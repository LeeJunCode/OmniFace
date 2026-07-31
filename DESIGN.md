# OmniFace 系统设计

## 项目概述

OmniFace — 基于 C++20 的实时人脸检测、识别与分析系统，支持 CPU/GPU 推理，提供 Qt6 桌面界面。

**核心管线**：视频帧 → 人脸检测 → 人脸跟踪 → 人脸识别 → 面部分析（姿态 / 视线）

**约束条件**：
- ONNX Runtime 推理（CPU: OpenVINO / Dnnl EP；GPU: CUDA / TensorRT EP）
- 实时性能（目标 ≥15 fps）
- 人脸检测与识别模型来源：InsightFace

## 分析后端

系统支持三种分析后端，通过 `analysis_backend` 配置切换。每种后端实现 `IAnalyzer` 统一接口，产出 `FrameAnalysisResult`：

| 后端 | 枚举值 | 命名空间 | 用途 |
|------|--------|---------|------|
| OpenFace | `openface` | `analysis::openface` | CE-CLM 关键点 + 头部姿态 + 视线 + AU（OpenFace 2.0 管线） |
| MediaPipe | `mediapipe` | `analysis::mediapipe` | FaceMesh 468 关键点 + PnP 头部姿态 |
| L2CS-Net | `l2cs` | `analysis::l2cs` | 视线估计（90-bin softmax 分类回归） |
| None | `none` | — | 仅检测 + 识别 + 跟踪，不做面部分析 |

配置键兼容旧值：`"onnx"` 自动映射为 `"mediapipe"`，`"gaze"` 自动映射为 `"l2cs"`。

---

## 模型清单

| 模型 | 用途 | 输入 | 输出 | 大小 |
|------|------|------|------|------|
| `det_500m.onnx` | 人脸检测（SCRFD） | 动态 | score + bbox + 5 点关键点 × 3 stride | 2.5 MB |
| `w600k_mbf.onnx` | 人脸特征提取（MobileFaceNet） | 112×112 | 512-d embedding | 13 MB |
| `face_mesh_Nx3x192x192_post.onnx` | 468 点 3D 面部网格（MediaPipe） | 192×192 | 468 点 (x,y,z) + score | 2.4 MB |
| `canonical_face_model.obj` | MediaPipe 标准 3D 人脸模型 | — | 468 个 3D 顶点 | — |
| `mobilenetv2_gaze.onnx` | 视线方向估计（L2CS-Net） | 448×448 | yaw [1,90] + pitch [1,90] logits | 9.4 MB |

**模型来源**：
- 检测 + 识别：InsightFace buffalo_sc 包
- 面部网格：PINTO0309/facemesh_onnx_tensorrt（Google MediaPipe 转换）
- 视线估计：Ahmednull/L2CS-Net（Gaze360 数据集训练），模型来自 yakhyo/gaze-estimation

---

## 检测模型输出结构（det_500m）

以 640×640 输入为例，SCRFD 模型输出 9 个 tensor，按 stride 8/16/32 分组：

| Stride | 网格 | 锚点/格 | 总位置 | score 形状 | bbox 形状 | kps 形状 |
|--------|------|---------|--------|-----------|----------|----------|
| 8 | 80×80 | 2 | 12800 | [12800, 1] | [12800, 4] | [12800, 10] |
| 16 | 40×40 | 2 | 3200 | [3200, 1] | [3200, 4] | [3200, 10] |
| 32 | 20×20 | 2 | 800 | [800, 1] | [800, 4] | [800, 10] |

模型内部已做 Sigmoid（ONNX 图中 score 输出前驱为 Sigmoid 节点），后处理无需再次 Sigmoid。

### 解码公式（distance2bbox / distance2kps）

```
// bbox 解码（距离偏移：left, top, right, bottom）
bbox_offset = bbox_raw × stride
x1 = anchor_x - bbox_offset[0]
y1 = anchor_y - bbox_offset[1]
x2 = anchor_x + bbox_offset[2]
y2 = anchor_y + bbox_offset[3]

// 关键点解码（5 点 × 2 坐标 = 10 个值）
kps_offset = kps_raw × stride
kp_x[k] = anchor_x + kps_offset[2k]
kp_y[k] = anchor_y + kps_offset[2k+1]
```

### 后处理流程

1. 遍历 3 个 stride，生成各 stride 的 anchor center 网格
2. 从 9 个输出中取 score、bbox（× stride）、kps（× stride）
3. `score ≥ confidence_threshold` 的保留
4. distance2bbox / distance2kps 解码，坐标除以 `det_scale` 映射回原图
5. 收集所有候选框 → NMS（IoU 阈值默认 0.4）

---

## 系统架构

```
                        Pipeline 数据流

  视频帧 ──▶ [DecodeWorker] ──▶ [Detection] ──▶ [Tracking] ──▶ [Recognition]
               (有界队列)            │                  │                │
                                BoundingBox[]     TrackedFace[]    MatchResult[]
                                    │                  │                │
                                    └────── 共用 ──────┘                │
                                       omniface/types.h          FaceMatcher
                                                                (余弦相似度)
                                         │
                               [TargetTracker]
                               (embedding 缓存 +
                                复查机制)
                                         │
                               ┌─────────┼─────────┐
                               │         │         │
                          [MediaPipe]  [L2CS]   [OpenFace]
                             头部姿态    视线      完整分析
                               │         │         │
                               └─────────┼─────────┘
                                         │
                                FrameAnalysisResult
                                  (IAnalyzer 统一接口)
                                         │
                                    [OutputManager]
                                   (视频/图像输出)

  ┌─────────────────────────────────────────────────────────────┐
  │                     桌面界面 (Qt6)                           │
  │  MainWindow → ProcessPage / DetailPage / HistoryPage        │
  │  OnnxAnalysisPanel / AUPanel / ProcessingDashboard          │
  │  SettingsDialog / VideoPlayerWidget                         │
  └─────────────────────────────────────────────────────────────┘
```

---

## 模块划分

### 模块 1：人脸检测（`core/detection/`）

| 文件 | 职责 |
|------|------|
| `face_detector.h/cc` | 封装 SCRFD ONNX 模型，提供 `Detect()` 接口 |

**预处理管线**：
1. Letterbox 保持宽高比缩放 → 放入 `input_size` 零填充画布
2. BGR→RGB + 归一化：`(pixel - 127.5) / 128.0`
3. HWC→CHW 转为 NCHW（batch=1）

**宽帧优化**：当帧宽度超过 `max_frame_width` 时先降采样检测，再将坐标映射回原图。

---

### 模块 2：人脸跟踪（`core/tracking/`）

| 文件 | 职责 |
|------|------|
| `face_tracker.h/cc` | 封装 ByteTrack 引擎，提供 `Update()` 接口 |
| `bytetrack_engine.h/cc` | ByteTrack 核心算法实现 |
| `detail/kalman_filter.h/cc` | 8 状态 Kalman 滤波器（x, y, a, h + 速度分量） |
| `detail/lapjv.h/cc` | Jonker-Volgenant 线性分配算法（稠密矩阵，O(n³)） |
| `detail/strack.h/cc` | 单条跟踪轨道（状态机：New→Tracked→Lost→Removed） |

**ByteTrack 算法流程**：

```
第1步: Kalman 预测所有 tracklet 的新位置
第2步: 将检测框按置信度分为高分和低分两组
第3步: 高分检测 ↔ 所有 tracklet 做 LAPJV 匹配（基于 IoU）
       - 匹配成功: Kalman 更新 tracklet
       - 未匹配的高分检测: 进入第三轮
       - 未匹配的 tracklet: 标记丢失
第4步: 低分检测 ↔ 未匹配 tracklet 做二次匹配
       - ByteTrack 精髓：利用低置信度检测挽救遮挡/模糊目标
第5步: 未匹配高分检测 ↔ 未确认轨道做第三次匹配（代价阈值 0.3）
第6步: 剩余高分检测初始化为新轨道（需 score ≥ high_threshold）
第7步: 清理超时丢失轨道（超过 track_buffer 帧）
```

**Kalman 滤波器**：8 维状态 `[x, y, a, h, vx, vy, va, vh]`，匀速运动模型，观测为 4 维 `[x, y, a, h]`。使用 xyah 坐标系（中心坐标 + 宽高比 + 高度），比 tlwh 更适合 Kalman 建模。

---

### 模块 3：人脸识别（`core/recognition/`）

| 文件 | 职责 |
|------|------|
| `face_recognizer.h/cc` | MobileFaceNet ONNX 推理，输出 512 维 embedding |
| `face_matcher.h/cc` | 余弦相似度比对，支持参考人脸注册 |
| `align.h` | ARCface 5 点关键点相似变换对齐 |

**人脸对齐**（与 InsightFace `face_align.norm_crop()` 一致）：
1. 使用 SCRFD 的 5 点关键点 + ARCface 标准模板估算相似变换
2. `cv::estimateAffinePartial2D(src, dst, RANSAC)` — 仅旋转+缩放+平移
3. `cv::warpAffine(frame, M, 112×112)` — 对齐 + 缩放一步完成
4. 关键点退化时自动回退到 bbox 裁剪 + resize

**识别归一化**：`(pixel - 127.5) / 127.5`（mean=127.5, std=127.5，注意与检测模型不同）

**Embedding 输出**：512 维原始向量，未 L2 归一化。余弦相似度需自行计算 `dot(A,B)/(||A||·||B||)`。

---

### 模块 4：Pipeline 管线（`pipeline/`）

| 文件 | 职责 |
|------|------|
| `pipeline.h/cc` | 核心管线：组装所有模块，逐帧处理 |
| `config.h/cc` | YAML 配置加载/保存，值域裁剪 |
| `module_factory.h/cc` | 根据配置创建各模块实例 |
| `decode_worker.h/cc` | 视频解码工作线程（有界队列） |
| `target_tracker.h/cc` | 目标跟踪：embedding 缓存 + 复查机制 |
| `output_manager.h/cc` | 视频/图像输出管理 |
| `bounded_queue.h` | 线程安全有界队列模板 |

**TargetTracker 设计**：
- 按 track_id 缓存 embedding，避免重复识别
- 复查机制：连续命中时降低复查频率（sim ≥0.6 不再复查，sim ≥0.2 每 60 帧复查）
- 质量把关：过滤宽高比异常、关键点越界、置信度不足的检测
- 两次验证失败后降级目标

**EMA 平滑**：
- 视线角度平滑：`α = 0.2`，减少帧间抖动
- 边界框平滑：`α = 0.25`，减少裁切抖动（视线抖动的根本原因）

---

### 模块 5：面部分析（`core/analysis/`）

三种后端各自独立，实现 `IAnalyzer` 统一接口。

#### 5.0 统一接口

| 文件 | 职责 |
|------|------|
| `analyzer_interface.h` | `IAnalyzer` 抽象接口 + `FrameAnalysisResult` 统一结果 + `AnalyzerKind` 枚举 |

```cpp
enum class AnalyzerKind { kOpenFace, kMediaPipe, kL2cs };

struct FrameAnalysisResult {
    std::optional<HeadPose> head_pose;
    std::optional<float> gaze_yaw, gaze_pitch;
    std::optional<cv::Vec3f> gaze_vector;
    std::vector<cv::Point2f> landmarks;          // 468 点（MediaPipe）
    std::array<float, 136> landmarks_2d{};       // 68 点（OpenFace）
    std::vector<std::pair<std::string, double>> action_units; // AU（OpenFace）
    // ... 各后端特有字段
    AnalyzerKind backend = AnalyzerKind::kOpenFace;
    bool valid = false;
    int track_id = -1;
};

class IAnalyzer {
public:
    virtual ~IAnalyzer() = default;
    virtual AnalyzerKind Kind() const = 0;
    virtual FrameAnalysisResult Analyze(const cv::Mat& frame,
                                         const BoundingBox& bbox,
                                         int track_id = -1) = 0;
    virtual bool IsValid() const = 0;
    virtual void Reset() {}
};
```

#### 5.1 头部姿态估计（MediaPipe 后端）

| 文件 | 职责 |
|------|------|
| `mediapipe/mediapipe_pose_estimator.h/cc` | FaceMesh ONNX 推理 + EPNP 姿态解算 |
| `mediapipe/mediapipe_visualizer.h/cc` | 静态可视化：关键点、3D 线框、面部法线 |
| `mediapipe/mediapipe_analyzer.h/cc` | 封装 PoseEstimator，实现 IAnalyzer 接口 |

**预处理**：
```
bbox 四边扩展 25% margin → crop → resize 192×192
  → BGR→RGB + /255.0 → [0, 1]
  → HWC→CHW
  → 构造 5 个输入 tensor（image + crop_x1/y1/width/height int32）
```

**输出处理**：
1. 从 `final_landmarks` [1, 468, 3] 提取 468 个 (x, y, z) 坐标
2. score < threshold 则返回 invalid
3. `cv::solvePnP(object_pts, image_pts, K, 0, rvec, tvec, false, SOLVEPNP_EPNP)`
4. Rodrigues 旋转向量 → 旋转矩阵 → 坐标系转换
5. canonical 坐标系 (X=右, Y=上) → 头部姿态 (X=右, Y=下, Z=前)：`R * diag(1, -1, -1)`
6. ZYX 欧拉角分解 → pitch / yaw / roll（度）

**相机内参**：`f = max(img_w, img_h)`，主点在图像中心。

#### 5.2 视线估计（L2CS 后端）

| 文件 | 职责 |
|------|------|
| `l2cs/l2cs_estimator.h/cc` | L2CS-Net ONNX 推理，实现 IAnalyzer 接口 |

**模型架构**：
```
MobileNetV2 Backbone → GlobalAveragePool → 1280-d
  → ├─ fc_yaw:  Gemm (1280→90) → yaw logits
     └─ fc_pitch: Gemm (1280→90) → pitch logits
```

**预处理**：
```
bbox → crop 人脸区域（无 margin，与训练条件一致）
  → resize 448×448（不保持宽高比）
  → /255.0 + ImageNet 标准化（减均值除标准差）
  → BGR→RGB → NCHW
```

**后处理**：
```
1. Softmax(yaw_logits), Softmax(pitch_logits)
2. 期望值解码: angle = Σ(prob[i] × i) × 4.0 - 180.0
   90 个 bin，bin 宽度 4°，范围 [-180°, +180°]
3. 弧度转换: rad = deg × π/180
4. 3D 视线向量: x = -cos(pitch)×sin(yaw), y = -sin(pitch), z = -cos(pitch)×cos(yaw)
```

**已知局限**：侧脸不稳定（Gaze360 训练集以正面为主）；帧间抖动（通过 EMA 平滑缓解）。

#### 5.3 OpenFace 后端

| 文件 | 职责 |
|------|------|
| `openface/openface_analyzer.h/cc` | 封装 OpenFace 2.0 CE-CLM 管线，实现 IAnalyzer 接口 |
| `openface/openface_writer.h/cc` | OpenFace 格式 CSV + 对齐人脸图像输出 |

支持 standard 和 max_quality 两种 profile，通过 CLNF 关键点 + PDM 模型联合估计姿态和表情参数，SVR/SVM 分类器做 AU 回归。

---

### 模块 6：桌面界面（`ui/`）

| 文件 | 职责 |
|------|------|
| `main_window.h/cpp` | 主窗口：菜单栏、QStackedWidget 页面导航 |
| `process_page.h/cpp` | 处理页面：视频上传、参数配置、实时分析 |
| `detail_page.h/cpp` | 详情页面：逐帧数据浏览、CSV 解析、视频回放 |
| `history_page.h/cpp` | 历史页面：批量管理历史分析记录 |
| `saved_media_page.h/cpp` | 已保存媒体页面 |
| `settings_dialog.h/cpp` | 设置对话框：所有阈值和模型参数 |
| `processing_dashboard.h/cpp` | 实时仪表盘：FPS 颜色编码、延迟显示 |
| `onnx_analysis_panel.h/cpp` | ONNX 分析结果面板：姿态角 + 关键点信息 |
| `au_panel.h/cpp` | AU 面板：动作单元激活状态显示 |
| `video_player_widget.h/cpp` | 视频播放器（QMediaPlayer，支持逐帧导航） |
| `reference_face_service.h/cpp` | 异步参考人脸注册服务 |
| `app_state.h` | 全局应用状态单例 |
| `image_utils.h` | 图像工具函数 |
| `stylesheet.h` | 全局样式表定义 |

---

### 模块 7：基础设施

| 文件 | 职责 |
|------|------|
| `core/detail/onnx_session.h/cc` | ONNX Runtime 会话封装（平台 EP 选择、宽路径处理） |
| `core/detail/preprocess.h/cc` | 人脸裁剪预处理（统一 NormKind：None/ImageNet/InsightFace） |

---

## 全局类型定义（`include/omniface/types.h`）

```cpp
struct BoundingBox {
    float x1, y1, x2, y2;
    float confidence;
    std::array<float, 10> keypoints;  // 5 点 (x,y 交替)
};

struct FaceEmbedding {
    std::vector<float> features;  // 512 维
    bool valid = false;
};

struct TrackedFace {
    int id;
    BoundingBox box;
    int consecutive_hit_count;
    int consecutive_miss_count;
};

struct HeadPose {
    float pitch = 0.0f;  // 俯仰角（度），低头为正
    float yaw = 0.0f;    // 偏航角（度），右转为正
    float roll = 0.0f;   // 翻滚角（度），右倾为正
};

struct MatchResult {
    BoundingBox box;
    float similarity;
    bool is_match;
    bool valid = false;
    int tracked_id = -1;
};
```

---

## 目录结构

```
OmniFace/
├── DESIGN.md
├── config.yaml                 # 默认配置
├── CMakeLists.txt
├── vcpkg.json                  # 依赖声明（vcpkg 清单模式）
│
├── app/
│   └── main.cpp                # 应用入口
│
├── include/omniface/
│   ├── types.h                 # 全局类型定义
│   ├── pipeline/
│   │   ├── pipeline.h          # 核心管线
│   │   ├── config.h            # 配置结构体 + YAML 解析
│   │   ├── module_factory.h    # 模块工厂
│   │   ├── decode_worker.h     # 视频解码器
│   │   ├── target_tracker.h    # 目标跟踪 + embedding 缓存
│   │   ├── output_manager.h    # 输出管理
│   │   └── bounded_queue.h     # 有界队列
│   ├── core/
│   │   ├── detection/
│   │   │   └── face_detector.h
│   │   ├── tracking/
│   │   │   ├── face_tracker.h
│   │   │   ├── bytetrack_engine.h
│   │   │   └── detail/
│   │   │       ├── kalman_filter.h
│   │   │       ├── lapjv.h
│   │   │       └── strack.h
│   │   ├── recognition/
│   │   │   ├── face_recognizer.h
│   │   │   ├── face_matcher.h
│   │   │   └── align.h
│   │   ├── analysis/
│   │   │   ├── analyzer_interface.h     # IAnalyzer 统一接口
│   │   │   ├── openface/
│   │   │   │   ├── openface_analyzer.h
│   │   │   │   └── openface_writer.h
│   │   │   ├── mediapipe/
│   │   │   │   ├── mediapipe_analyzer.h
│   │   │   │   ├── mediapipe_pose_estimator.h
│   │   │   │   └── mediapipe_visualizer.h
│   │   │   └── l2cs/
│   │   │       └── l2cs_estimator.h
│   │   └── detail/
│   │       ├── onnx_session.h      # ONNX Runtime 会话封装
│   │       └── preprocess.h        # 人脸裁剪预处理器
│   └── ui/
│       ├── main_window.h
│       ├── process_page.h
│       ├── detail_page.h
│       ├── history_page.h
│       ├── saved_media_page.h
│       ├── settings_dialog.h
│       ├── processing_dashboard.h
│       ├── onnx_analysis_panel.h
│       ├── au_panel.h
│       ├── video_player_widget.h
│       ├── reference_face_service.h
│       ├── app_state.h
│       ├── image_utils.h
│       └── stylesheet.h
│
├── src/                         # 镜像 include/ 结构
├── ui/                          # Qt UI 实现（.cpp）
├── models/
│   ├── buffalo_sc/              # 检测 + 识别模型
│   ├── mediapipe/               # 面部网格模型 + canonical 模型
│   └── l2cs-net/                # 视线估计模型
├── openface_model/              # OpenFace CE-CLM 模型文件
├── scripts/
│   ├── OmniFace.bat             # 启动脚本
│   ├── generate_icon.py         # 图标生成脚本
│   └── package.ps1              # 打包脚本
└── third_party/                 # 第三方库（不纳入本设计文档范围）
```

---

## 构建系统

- **构建工具**：CMake + Visual Studio（Windows）
- **C++ 标准**：C++20
- **包管理**：vcpkg（清单模式，`vcpkg.json`）
- **编码规范**：Google C++ Style Guide（`.h/.cc` 扩展名，snake_case 变量，PascalCase 函数）

### 外部依赖

| 依赖 | 获取方式 | 用途 |
|------|---------|------|
| ONNX Runtime | vcpkg | 模型推理 |
| OpenCV | vcpkg（dnn/highgui/calib3d） | 图像 I/O 与预处理 |
| Qt6 | vcpkg（base/multimedia） | 桌面界面 |
| spdlog | vcpkg | 日志 |
| yaml-cpp | vcpkg | YAML 配置解析 |
| Eigen3 | vcpkg | Kalman 滤波器矩阵运算 |
| dlib | vcpkg | OpenFace CE-CLM 人脸特征点检测 |
| OpenBLAS + LAPACK | vcpkg | OpenFace 线性代数运算 |
| Boost | vcpkg（filesystem/system） | OpenFace 文件系统操作 |

---

## 数据流与接口

### Pipeline 接口

```cpp
class Pipeline {
public:
    explicit Pipeline(const std::string& cfg_path, FrameCallback callback = {});
    explicit Pipeline(const PipelineConfig& cfg, FrameCallback callback = {});

    void Run();                        // 阻塞运行至视频结束或 ESC
    void Stop();                       // 请求停止
    FrameResult ProcessFrame(cv::Mat& bgr_frame, int frame_index);  // 逐帧处理
    void OpenVideo(const std::string& path);
    bool PollFrame(cv::Mat& frame);

    void UpdateThresholds(const ThresholdConfig& thresholds);
    void EnrollReference(const FaceEmbedding& embedding);

    const PipelineConfig& Config() const;
    double GetFps() const;
    std::string GetOutputDir() const;
};
```

### FrameResult 结构

```cpp
struct FrameResult {
    int frame_index = 0;
    int face_count = 0;
    double total_latency_ms = 0.0;
    double recognition_latency_ms = 0.0;
    double analysis_latency_ms = 0.0;

    // 统一分析结果（各后端通过 IAnalyzer 接口产出）
    std::vector<analysis::FrameAnalysisResult> analysis_results;

    // 各后端原生结果（保留用于可视化/导出）
    FaceAnalysisResult analysis;                                          // OpenFace
    std::vector<analysis::mediapipe::MediaPipeAnalysisResult> onnx_results; // MediaPipe
    std::vector<analysis::l2cs::L2csEstimationResult> l2cs_results;        // L2CS-Net

    std::vector<TrackedFace> tracked_faces;
    std::vector<MatchResult> match_results;
    cv::Mat annotated_frame;
};
```

---

## 已知局限

- **侧脸视线不稳定**：L2CS-Net 在 Gaze360 数据集训练，侧脸样本稀少，极端角度误差增大
- **帧间视线抖动**：通过 EMA 平滑（α=0.2）和边界框平滑（α=0.25）缓解
- **AU 分析未实现**：`onnx.enable_au` 配置项预留给 OpenGraphAU，尚未集成
- **仅处理目标人脸**：MediaPipe、L2CS 和 OpenFace 分析仅对匹配到的目标人脸执行，非全部检测人脸
- **OpenFace 后端性能**：CE-CLM 首次初始化需 20-30ms（cold start），后续跟踪帧降至 ~10ms（warm start）
