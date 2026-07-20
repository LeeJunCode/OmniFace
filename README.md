# OmniFace

目标人物实时面部分析系统。输入一段视频和一张目标人物照片，系统自动检测、跟踪、识别目标人物，并实时输出其面部信息：头部姿态、视线方向、面部动作单元（AU）。

支持两条分析路线：

- **OpenFace 2.0**：CLNF 关键点拟合，一次完成姿态 + AU + 视线估计
- **ONNX**：三个独立模型（MediaPipe FaceMesh / L2CS-Net / MEFARG），可任意组合开关

## 整体架构

```
视频帧
  → 人脸检测 (SCRFD, InsightFace, ONNX)
    → 多目标跟踪 (ByteTrack + Kalman 滤波)
      → 人脸识别 (MobileFaceNet, 512维特征, 余弦相似度匹配)
        → 锁定目标人物
          → 面部分析 (仅分析目标):
              OpenFace:  CLNF → 姿态 + 视线 + AU (warm/cold start)
              ONNX:      FaceMesh → EPNP+LM精炼 → 头部姿态
                         人脸对齐 → L2CS-Net → 视线方向
                         人脸对齐 → MEFARG → 41个AU
            → 可视化叠加
              → 视频输出 / Web界面MJPEG推流
```

## 依赖环境

| 依赖 | 版本 | 用途 |
|---|---|---|
| C++ 编译器 | GCC 13+ / Clang 17+ | C++20 |
| CMake | 3.21+ | 构建系统 |
| ONNX Runtime | 1.27+ | ONNX 模型推理 |
| OpenCV | 4.8+ | 图像读写、处理、可视化 |
| Eigen3 | 任意 | Kalman 滤波线性代数 |
| OpenBLAS | 任意 | OpenFace 线性代数 |
| dlib | 19.24+ | OpenFace 机器学习 |
| Boost | filesystem, system | OpenFace 文件系统 |

## 编译

### 第一步：安装系统依赖

```bash
sudo apt install libopencv-dev libeigen3-dev libopenblas-dev libdlib-dev libboost-filesystem-dev libboost-system-dev
```

### 第二步：安装 ONNX Runtime

从 https://github.com/microsoft/onnxruntime/releases 下载预编译包并配置环境变量。

### 第三步：编译 OpenFace（仅首次）

```bash
cd third_party/OpenFace
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cd ../..
```

### 第四步：编译 OmniFace

```bash
cmake -B build
cmake --build build
```

## 快速开始

```bash
./build/app/omniface --serve          # 启动Web界面 http://127.0.0.1:8080
./build/app/omniface --serve --port 9000

./build/app/omniface                  # CLI模式 (OpenCV窗口)
./build/app/omniface --config my_config.yaml
./build/app/omniface --backend onnx   # 切换到ONNX后端
```

## 模型文件

### 模型文件下载
检测（buffalo_sc/det_500m.onnx）与跟踪模型（buffalo_sc/w600k_mbf.onnx）均来自于 insightface 项目，可以通过官方项目进行下载：https://github.com/deepinsight/insightface

或者也能够通过 python 的 pip 管理包直接下载 insightface 项目，使用 “pip install insightface” 命令下载 insightface 项目，然后会在 .insightface/ 目录下下载 insightface 系列模型

视线估计模型（l2cs-net）下载：https://github.com/yakhyo/gaze-estimation

OpenGprahAU 模型下载：https://github.com/lingjivoo/OpenGraphAU

mediapipe 模型下载：https://github.com/PINTO0309/facemesh_onnx_tensorrt/releases/download/1.0.0/face_mesh_Nx3x192x192_post.onnx

canonical_face_model.obj 下载：https://github.com/google-ai-edge/mediapipe/blob/master/mediapipe/modules/face_geometry/data/canonical_face_model.obj

将 ONNX 模型放置到 `models/` 目录下：

```
models/
  buffalo_sc/
    det_500m.onnx          # SCRFD 人脸检测
    w600k_mbf.onnx         # MobileFaceNet 人脸识别
  mediapipe/
    face_mesh_Nx3x192x192_post.onnx  # 468点面部网格
    canonical_face_model.obj         # 标准3D人脸模型
  l2cs-net/
    resnet50_gaze.onnx     # L2CS-Net 视线估计
  OpenGprahAU/
    mefarg_resnet18_au.onnx  # MEFARG AU识别
```

## 配置说明

所有可调参数集中在 `config.yaml`，启动时自动加载。命令行参数 `--reference`、`--input`、`--output`、`--backend` 可覆盖配置文件。

### 分析后端

```yaml
analysis:
  backend: "openface"    # "openface" 或 "onnx"
```

### ONNX 后端参数

```yaml
onnx:
  enable_pose: true      # 头部姿态估计
  enable_gaze: true      # 视线估计
  enable_au: true        # AU表情识别
  align_faces: true      # 关键点引导的人脸对齐（提升准确度）
  au_temporal_window: 5  # AU时间窗口平滑帧数
```

### 阈值设置

```yaml
thresholds:
  match_threshold: 0.3         # 目标匹配余弦相似度阈值
  detection_confidence: 0.5    # 检测置信度
  target_recheck_interval: 30  # 身份复检间隔（帧），防止跟踪切换
```

### 跟踪参数

```yaml
tracking:
  track_buffer: 30       # 丢失轨迹保留帧数
  high_threshold: 0.5    # 高分检测阈值
  match_threshold: 0.5   # 首次匹配IoU阈值
```

## Web 界面

`--serve` 启动内嵌 HTTP 服务，浏览器打开对应地址即可使用。包含三个视图：

- **新建任务**：选择目标人物照片和输入视频，配置分析参数，管理预设方案
- **运行监控**：双路 MJPEG 实时画面（原始 + 分析），实时图表（注意力评分、AU激活、姿态/视线曲线），目标出现时间线
- **任务历史**：任务列表、详细报告（处理帧数、目标出现率、平均/最高相似度）、注意力与 AU 时间线图表（点击可跳转视频帧）、结果下载

## 输出文件详解

每次分析任务在 `data/output/<任务ID>/` 下生成以下文件。任务 ID 格式为 `YYYYMMDD_HHMMSS`（任务启动时间）。

### 文件清单

| 文件 | 内容 |
|---|---|
| `annotated.mp4` | 标注后的输出视频（检测框、ID、姿态角、视线箭头、AU标签） |
| `tracking.csv` | 逐帧跟踪日志 |
| `onnx_analysis.csv` | ONNX 后端逐帧面部分析数据（仅 ONNX 模式） |
| `metrics.jsonl` | 逐帧完整指标（JSON 行格式，供前端图表使用） |
| `task.json` | 任务元信息、参数、统计摘要 |
| `openface/` | OpenFace 全量输出目录（仅 OpenFace 模式，含 CSV 等） |

---

### tracking.csv — 逐帧跟踪日志

每一行代表视频的一帧，无论目标是否出现。

| 列名 | 类型 | 含义 |
|---|---|---|
| `frame` | int | 帧序号（从 0 开始） |
| `time_sec` | float | 视频时间戳（秒），`frame / fps` |
| `faces` | int | 当前帧检测到的人脸总数 |
| `target` | 0/1 | 目标人物是否出现（1=当前帧锁定目标） |
| `track_id` | int | 目标人物的 ByteTrack 轨迹 ID（-1 表示目标未出现） |
| `similarity` | float | 目标人脸与参考照片的余弦相似度（0~1，越高越像） |
| `openface_row` | int | OpenFace 输出 CSV 中对应的行号（-1 表示该帧未送入 OpenFace，仅 OpenFace 模式有效） |

---

### onnx_analysis.csv — ONNX 面部分析数据

仅 ONNX 后端（`analysis.backend: "onnx"`）生成。**仅在目标出现的帧写入一行**，目标未出现时跳过。

列分为四组：

#### 基础信息

| 列名 | 类型 | 含义 |
|---|---|---|
| `frame` | int | 帧序号 |
| `time_sec` | float | 视频时间戳（秒） |

#### 头部姿态（单位：度）

| 列名 | 含义 | 方向说明 |
|---|---|---|
| `pitch` | 俯仰角 | 正值 = 抬头，负值 = 低头 |
| `yaw` | 偏航角 | 正值 = 右转，负值 = 左转 |
| `roll` | 翻滚角 | 正值 = 右倾，负值 = 左倾 |

三列在姿态估计未启用或失败时为空。

#### 视线方向（单位：弧度）

| 列名 | 含义 | 方向说明 |
|---|---|---|
| `gaze_yaw` | 水平视线角 | 正值 = 向右看，负值 = 向左看 |
| `gaze_pitch` | 垂直视线角 | 正值 = 向上看，负值 = 向下看 |

两列在视线估计未启用或失败时为空。数值已经过 3 帧中值滤波 + EMA 平滑处理。

#### 面部动作单元 AU（27 列，值域 -1~1）

每个 AU 列存储的是模型输出的**余弦相似度**，值越接近 1 表示该 AU 激活越强。默认阈值 0.5 判定激活。

| 列名 | FACS 含义 | 面部表现 |
|---|---|---|
| `AU1` | 眉毛内角抬起 | 眉心上扬 |
| `AU2` | 眉毛外角抬起 | 眉尾上扬 |
| `AU4` | 眉毛整体压低 | 皱眉、眉毛下压 |
| `AU5` | 上眼睑抬起 | 瞪眼、上眼睑上提 |
| `AU6` | 脸颊抬起 | 笑眼、眼周肌肉收缩 |
| `AU7` | 眼睑收紧 | 眯眼、眼睑紧张 |
| `AU9` | 鼻根皱起 | 鼻子皱起 |
| `AU10` | 上唇抬起 | 上唇上提、露上牙 |
| `AU11` | 鼻唇沟加深 | 法令纹加深 |
| `AU12` | 嘴角向外上扬 | 微笑、嘴角上拉 |
| `AU13` | 嘴角急剧上扬 | 咧嘴 |
| `AU14` | 嘴角收紧（酒窝） | 抿嘴、酒窝 |
| `AU15` | 嘴角下压 | 嘴角下拉 |
| `AU16` | 下唇下压 | 下唇下翻 |
| `AU17` | 下巴抬起 | 下巴上提 |
| `AU18` | 嘴唇缩拢 | 嘴唇聚拢 |
| `AU19` | 舌头露出 | 伸舌 |
| `AU20` | 嘴角横向拉伸 | 嘴角水平拉开 |
| `AU22` | 嘴唇外翻（漏斗状） | 嘴唇外翻 |
| `AU23` | 嘴唇收紧 | 嘴唇变薄、紧绷 |
| `AU24` | 嘴唇压紧 | 双唇紧压 |
| `AU25` | 双唇分开 | 张嘴（非下颌动作） |
| `AU26` | 下颌下坠 | 下颌放松下垂 |
| `AU27` | 嘴巴张大 | 大幅张口 |
| `AU32` | 咬唇 | 咬下唇 |
| `AU38` | 鼻孔扩张 | 鼻孔张开 |
| `AU39` | 鼻孔收紧 | 鼻孔收缩 |

数值已经过配置的滑动窗口平均处理（默认 5 帧），以减少逐帧噪声。

---

### metrics.jsonl — 逐帧完整指标

每行一个 JSON 对象，包含该帧的全部分析结果，供前端图表和报告使用。字段：

```json
{
  "f": 30,           // 帧号
  "found": true,     // 目标是否出现
  "tid": 1,          // 目标 track_id
  "sim": 0.85,       // 相似度
  "pose_valid": true,
  "pose": [5.2, -12.3, 1.1],   // [pitch, yaw, roll] 度
  "gaze_valid": true,
  "gaze": [0.15, -0.08],        // [yaw, pitch] 弧度
  "aus": [["AU1",0.12], ["AU2",0.05], ...],  // AU名称+余弦相似度
  "tracks": [[1,0.85,1], ...]   // [track_id, similarity, is_target] 当前帧所有轨迹
}
```

---

### task.json — 任务摘要

```json
{
  "id": "20260720_143052",
  "state": "finished",
  "backend": "onnx",
  "reference": "data/img/target.jpg",
  "video": "data/video/input.mp4",
  "params": { "match_threshold": "0.3", ... },
  "stats": {
    "frames": 1800,            // 已处理帧数
    "total_frames": 1800,      // 视频总帧数
    "fps": 30.0,               // 视频帧率
    "target_frames": 1200,     // 目标出现的帧数
    "avg_sim": 0.782,          // 平均相似度
    "max_sim": 0.951,          // 最高相似度
    "intervals": [[100,500], [700,1500]]  // 目标出现的帧区间
  },
  "outputs": {
    "video": "data/output/.../annotated.mp4",
    "csv": "data/output/.../tracking.csv",
    "onnx_csv": "data/output/.../onnx_analysis.csv",
    "metrics": "data/output/.../metrics.jsonl"
  }
}
```

---

### OpenFace 输出（仅 OpenFace 模式）

OpenFace 后端会在 `data/output/<任务ID>/openface/` 下生成详细的全量 CSV，包含 2D/3D 关键点坐标、头部姿态参数、AU 回归强度和分类结果、视线方向向量、HOG 特征等 700+ 列。具体列名和含义见 OpenFace 官方文档 https://github.com/TadasBaltrusaitis/OpenFace。

```
face: 检测到的人脸的ID。

confidence: 面部检测的置信度。

gaze_0_x, gaze_0_y, gaze_0_z: 左眼注视方向的3D向量。

gaze_1_x, gaze_1_y, gaze_1_z: 右眼注视方向的3D向量。

gaze_angle_x: 头部姿态补偿后的平均注视角度（水平方向）。这个值表示人脸在水平方向上的注视角度。正值表示向右看，负值表示向左看。

gaze_angle_y: 头部姿态补偿后的平均注视角度（垂直方向）。这个值表示人脸在垂直方向上的注视角度。正值表示向上看，负值表示向下看。

eye_lmk_x_0 到 eye_lmk_x_55: 这56列代表了检测到的左眼和右眼眼球上的关键特征点的2D x坐标（以像素为单位）。前28个点（0-27）对应于一个眼睛（左眼），后28个点（28-55）对应于另一个眼睛（右眼）。

eye_lmk_y_0 到 eye_lmk_y_55: 这56列代表了对应于 eye_lmk_x_0 到 eye_lmk_x_55 这些特征点的2D y坐标（以像素为单位）。

eye_lmk_X_0 到 eye_lmk_X_55: 这56列代表了检测到的左眼和右眼眼球上的关键特征点的3D X坐标（相对于相机坐标系，单位通常是毫米）。同样，前28个点对应一个眼睛，后28个点对应另一个眼睛。

eye_lmk_Y_0 到 eye_lmk_Y_55: 这56列代表了对应于 eye_lmk_X_0 到 eye_lmk_X_55 这些特征点的3D Y坐标。

eye_lmk_Z_0 到 eye_lmk_Z_55: 这56列代表了对应于 eye_lmk_X_0 到 eye_lmk_X_55 这些特征点的3D Z坐标。Z轴通常指向远离相机的方向。

pose_Tx, pose_Ty, pose_Tz: 头部的平移向量。这三个数值表示检测到的人脸中心在3D空间中相对于相机坐标系的位置（x, y, z平移量）。

pose_Rx, pose_Ry, pose_Rz: 头部的旋转角度。这三个数值表示头部在3D空间中的旋转角度，分别对应绕X轴（俯仰角）、Y轴（偏航角）和Z轴（滚转角）的旋转）。

x_0 到 x_67: 这68列代表了检测到的面部68个关键特征点的2D x坐标（以像素为单位）。这些特征点覆盖了眉毛、眼睛、鼻子、嘴巴和下巴轮廓等面部区域。

y_0 到 y_67: 这68列代表了对应于 x_0 到 x_67 这些特征点的2D y坐标（以像素为单位）。

X_0 到 X_67: 这68列代表了检测到的面部68个关键特征点的3D X坐标（相对于相机坐标系，单位通常是毫米）。

Y_0 到 Y_67: 这68列代表了对应于 X_0 到 X_67 这些特征点的3D Y坐标。

Z_0 到 Z_67: 这68列代表了对应于 X_0 到 X_67 这些特征点的3D Z坐标。

p_scale: 检测到的人脸的尺度因子。

p_rx, p_ry, p_rz: 估计的头部旋转角度（与 pose_Rx, pose_Ry, pose_Rz 类似）。

p_tx, p_ty: 估计的头部平移量（与 pose_Tx, pose_Ty 类似）。

p_0 到 p_33: 这些列与3D人脸模型的参数相关，用于拟合检测到的2D特征点，代表了形状参数或纹理参数等。

AU01_r 到 AU45_r: 这些列表示面部动作单元 (Action Units - AUs) 的强度 (intensity)。数值通常在0到5之间，表示对应AU的激活程度。例如，AU01代表内眉，AU12代表嘴角外拉,并非所有的AU都会被估计。

AU01_c 到 AU28_c, AU45_c: 这些列表示面部动作单元是否被激活 (presence)。数值是0或1，1表示对应的AU被激活，0表示未激活。

```

## CLI 模式

```bash
./build/app/omniface --reference 目标.jpg --input 视频.mp4 --output 结果.mp4
```

按 `q` 或 `ESC` 退出。

## 项目结构

```
OmniFace/
  CMakeLists.txt              # 顶层构建
  config.yaml                 # 配置参数
  app/                        # 入口 main.cc
  include/omniface/
    types.h                   # 核心数据类型
    core/
      detection/              # SCRFD 人脸检测器
      tracking/               # ByteTrack + Kalman + LAPJV
      recognition/            # MobileFaceNet + 余弦匹配
      analysis/
        openface_analyzer.h   # OpenFace 2.0 封装
        pose_estimator.h      # MediaPipe FaceMesh + PnP
        gaze_estimator.h      # L2CS-Net 视线
        au_analyzer.h         # MEFARG AU
      detail/
        face_aligner.h        # 关键点引导的面部对齐
        onnx_session.h        # ONNX Runtime 会话封装
        preprocess.h          # 图像预处理
    pipeline/                 # 主管线
    server/                   # 内嵌 HTTP 服务
    viz/                      # 可视化叠加
  src/                        # 实现代码（与 include 对应）
  web/                        # Web 前端（SPA, 原生 JS ES模块）
    js/
      app.js                  # 入口
      api.js                  # API 通信
      attention.js            # 注意力评分算法
      charts.js               # Chart.js 图表封装
      views/
        config.js             # 新建任务视图
        monitor.js            # 运行监控视图
        history.js            # 历史报告视图
    csv.html                  # CSV 数据查看器
  models/                     # ONNX 模型文件
  data/                       # 图片、视频、输出
  third_party/OpenFace/       # OpenFace 2.0 源码及预编译库
```

## 实现细节

### 模块总览

项目分为 10 个 C++ 静态库模块和 1 个 Web 前端模块：

| 模块 | 职责 | 核心依赖 |
|---|---|---|
| `omniface_detail` | ONNX Runtime 会话封装、图像预处理（4种归一化策略） | ONNX Runtime, OpenCV |
| `omniface_detection` | SCRFD 人脸检测，3 尺度 anchor，距离解码 + NMS | omniface_detail |
| `omniface_tracking` | ByteTrack + Kalman 8维匀速滤波 + LAPJV 线性指派 | omniface_detail, Eigen3 |
| `omniface_recognition` | MobileFaceNet 512维特征提取 + 余弦相似度匹配 | omniface_detail |
| `omniface_pose` | MediaPipe FaceMesh 468关键点 + EPNP+LM PnP 姿态解算 | omniface_detail, OpenCV |
| `omniface_gaze` | L2CS-Net 90-bin Softmax 期望值解码视线方向 | omniface_detail |
| `omniface_au` | MEFARG ResNet-18 41 AU 余弦相似度 + 阈值判定 | omniface_detail |
| `omniface_openface` | OpenFace 2.0 CLNF 封装（warm/cold start） | OpenFace 预编译库 |
| `omniface_viz` | 标注绘制：检测框、姿态轴、3D线框、视线箭头、AU标签 | OpenCV |
| `omniface_pipeline` | 主管线：帧循环、检测→跟踪→识别→分析→输出 | 以上全部 |
| `omniface_server` | 内嵌 HTTP 服务：REST API + MJPEG 双路推流 + 任务管理 | cpp-httplib |

---

### 人脸检测 (SCRFD)

InsightFace `det_500m.onnx` 模型，输入 640×640。采用 Letterbox 预处理保持宽高比，灰色填充，`(x-127.5)/128` 归一化。三尺度 anchor（stride 8/16/32）覆盖不同大小的人脸。每个 anchor 输出距离偏移 + 5 点关键点偏移，解码后经 OpenCV NMS 去重。

### 人脸跟踪 (ByteTrack)

8 步匹配管线：高/低分检测分离 → Kalman 预测所有活跃轨迹 → 首次 IoU 匹配（高分检测 × 全部轨迹）→ 二次 IoU 匹配（低分检测 × 剩余轨迹）→ 未确认轨迹匹配 → 新轨迹创建 → 过期轨迹清理 → 状态迁移去重。Kalman 滤波器为 8 维匀速模型 `[cx,cy,a,h, vx,vy,va,vh]`，使用 Mahalanobis 距离门控（卡方 95% 阈值）过滤不可行匹配。LAPJV 算法求解线性指派问题。

### 人脸识别与目标锁定

参考照片 → SCRFD 检测人脸 → ARCface 5 点仿射对齐（112×112）→ MobileFaceNet 提取 512 维特征 → L2 归一化存储。视频中每帧检测到的人脸同样提取特征，计算余弦相似度。track_id 缓存机制避免同一轨迹重复识别。

目标锁定状态机：最高相似度超过阈值且 track_id 不变时维持锁定。每 N 帧（默认 30）强制重新提取特征验证身份，连续 2 次失败解除锁定，防止跟踪器身份切换导致分析错误对象。

### 头部姿态估计 (ONNX)

MediaPipe FaceMesh 模型输入 192×192，输出 468 个 3D 关键点的图像坐标。与预加载的标准 3D 人脸模型（`canonical_face_model.obj`）组成 2D-3D 对应点对。仅选取骨骼稳定区域的关键点（眼眶、鼻梁、眉骨、额头轮廓，排除下颌和脸颊软组织），先以 EPNP 算法求闭式解作为初值，再用 Levenberg-Marquardt 迭代精炼最小化重投影误差，最后将旋转矩阵分解为 ZYX 欧拉角（pitch/yaw/roll）。

### 视线估计 (ONNX)

L2CS-Net ResNet-50 模型输入 448×448，ImageNet 分通道归一化，输出 yaw/pitch 各 90-bin logits。通过 Softmax 期望值解码为连续角度：`angle = Σ(prob[i]×i) × 4° - 180°`。当姿态估计启用时，利用 FaceMesh 关键点计算眼睛中心的相似变换，将人脸精确对齐（眼睛固定在 30%/70% 水平位置，仅去平面内旋转，保留头姿信息）后送入模型，提升准确度。后处理采用两步平滑：3 帧中值滤波剔除单帧跳变，再接 EMA（α=0.6）平滑残余抖动。

### 面部动作单元识别 (ONNX)

MEFARG ResNet-18 模型输入 224×224，输出 41 维 L2 归一化余弦相似度（27 个主 AU + 14 个左右不对称子 AU）。默认阈值 0.5 判定激活，支持 per-AU 自定义阈值。当姿态估计启用时，面部经全相似对齐（眼睛固定在 30%/70% 水平 + 35% 垂直位置）后送入模型。5 帧滑动窗口平均降噪：维护最近 5 帧的原始 cosine_sim，逐 AU 取平均值后再做阈值判定。

### OpenFace 后端

封装 OpenFace 2.0 的 CLNF（Constrained Local Neural Field）关键点拟合。Warm start 保留上一帧表情参数（2-3 次迭代收敛），Cold start 从 bbox 重新初始化（5-8 次迭代，仅在目标 track_id 变更时触发）。CLNF 一次拟合同时输出 68+ 关键点、头部姿态、视线方向（3D 眼球模型）和 AU 回归强度（时序 SVM on HOG 特征）。每帧通过 `SetFacePreference` 告知 OpenFace 目标人脸的归一化坐标，确保多人场景下选择正确的人脸。

---

### 注意力评分算法

前端 `attention.js` 实现了"无罪推定"注意力评分：默认基线 80%（假设专注），仅在有明确分心证据时扣分。

**扣分项**（累计上限 65%）：

| 证据 | 条件 | 最大扣分 |
|---|---|---|
| 视线剧烈不稳定 | gaze std > 8° | -30 |
| 头部突然抖动 | head jerk > 3° | -15 |
| 闭眼/疲劳 | AU43 ≥ 0.5 | -20 |
| 头眼反向运动 | 头眼变化方向相反 | -10 |
| 面部长期完全放松 | AU4 < 0.15 且 AU7 < 0.1（持续20帧+） | -5 |

**加分项**（累计上限 12%）：

| 证据 | 条件 | 加分 |
|---|---|---|
| 视线极其稳定 | gaze std < 3° | +6 |
| 头部极其静止 | head std < 5° | +5 |
| 专注微表情 | AU4 或 AU7 ≥ 0.4 | +4 |
| 头眼协调追踪 | 头眼同向运动且幅度 > 2° | +3 |

最终分数经 EMA（α=0.3，约 10 帧窗口）平滑后转为百分制：≥65% 为"集中"，40-65% 为"部分分散"，<40% 为"分散"。历史窗口 75 帧（2.5秒@30fps）。

---

### Web 前端

纯原生 JavaScript ES 模块，零框架依赖，仅引入 Chart.js 用于图表渲染。7 个模块按职责拆分：`api.js`（通信层）、`attention.js`（注意力算法）、`charts.js`（图表管理）、`config.js`（新建任务视图）、`monitor.js`（运行监控视图）、`history.js`（历史报告视图）、`app.js`（入口 + 视图切换）。MJPEG 双路推流通过 `<img>` 标签原生支持实现，断连自动重连。
