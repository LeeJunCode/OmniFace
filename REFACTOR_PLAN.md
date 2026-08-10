# OmniFace 项目重构计划

## 一、当前结构 vs 目标结构

### 1.1 当前 `src/` → 目标 `src/`

```
当前 src/                                  目标 src/
────                                        ────
src/                                       src/
├── CMakeLists.txt                         ├── CMakeLists.txt
│                                          │
├── core/                                  ├── core/
│   ├── detail/                            │   ├── inference/              ← 改名，含义明确
│   │   ├── onnx_session.cc                │   │   ├── onnx_session.cc
│   │   └── preprocess.cc                  │   │   └── preprocess.cc
│   │                                      │   │
│   ├── detection/                         │   ├── detection/
│   │   └── face_detector.cc               │   │   └── face_detector.cc
│   │                                      │   │
│   ├── tracking/                          │   ├── tracking/
│   │   ├── bytetrack_engine.cc            │   │   ├── bytetrack_engine.cc
│   │   ├── face_tracker.cc                │   │   ├── face_tracker.cc
│   │   └── detail/                        │   │   └── internal/            ← 改名
│   │       ├── kalman_filter.cc           │   │       ├── kalman_filter.cc
│   │       ├── lapjv.cc                   │   │       ├── lapjv.cc
│   │       └── strack.cc                  │   │       └── strack.cc
│   │                                      │   │
│   ├── recognition/                       │   ├── recognition/
│   │   ├── face_matcher.cc                │   │   ├── face_matcher.cc
│   │   └── face_recognizer.cc             │   │   └── face_recognizer.cc
│   │                                      │   │
│   ├── analysis/                          │   ├── analysis/
│   │   ├── analyzer_interface.h ← 裸露   │   │   ├── interface/            ← 新：接口层
│   │   ├── openface/                      │   │   │   ├── analyzer_interface.h
│   │   │   ├── openface_analyzer.cc       │   │   │   └── csv_output_writer.cc/.h
│   │   │   └── openface_writer.cc         │   │   ├── openface/
│   │   ├── mediapipe/                     │   │   │   ├── openface_analyzer.cc
│   │   │   ├── mediapipe_analyzer.cc      │   │   │   ├── openface_writer.cc
│   │   │   ├── mediapipe_pose_estimator.cc│   │   │   └── openface_utility.cc
│   │   │   └── mediapipe_visualizer.cc    │   │   ├── mediapipe/
│   │   ├── l2cs/                          │   │   │   ├── mediapipe_analyzer.cc
│   │   │   └── l2cs_estimator.cc          │   │   │   ├── mediapipe_pose_estimator.cc
│   │   └── mefarg_au/                     │   │   │   └── mediapipe_visualizer.cc
│   │       └── mefarg_au_estimator.cc     │   │   ├── l2cs/
│   │                                      │   │   │   └── l2cs_estimator.cc
│   │                                      │   │   └── mefarg_au/
│   │                                      │   │       └── mefarg_au_estimator.cc
│   │                                      │   │
│   └── quality/                           │   ├── quality/
│       ├── base_quality_evaluator.h 裸露  │   │   ├── interface/            ← 新：基类层
│       ├── base_data_cleaner.h      裸露  │   │   │   ├── base_quality_evaluator.h
│       ├── quality_types.h          裸露  │   │   │   ├── base_data_cleaner.h
│       ├── openface/                      │   │   │   ├── quality_types.h
│       │   ├── openface_quality_e...      │   │   │   ├── quality_csv_reader.cc/.h
│       │   └── openface_data_cleaner.cc   │   │   │   └── quality_grade_utils.h
│       ├── mediapipe/                     │   │   ├── openface/
│       │   ├── mediapipe_quality_e...     │   │   │   ├── openface_quality_evaluator.cc
│       │   └── mediapipe_data_cleaner.cc  │   │   │   ├── openface_data_cleaner.cc
│       ├── l2cs/                          │   │   │   └── openface_quality_types.h
│       │   ├── l2cs_quality_evaluator.cc  │   │   ├── mediapipe/
│       │   └── l2cs_data_cleaner.cc       │   │   │   ├── mediapipe_quality_evaluator.cc
│       └── mefarg_au/                     │   │   │   ├── mediapipe_data_cleaner.cc
│           ├── mefarg_au_quality_e...     │   │   │   └── mediapipe_quality_types.h
│           └── mefarg_au_data_cleaner.cc  │   │   ├── l2cs/
│                                          │   │   │   ├── l2cs_quality_evaluator.cc
│                                          │   │   │   ├── l2cs_data_cleaner.cc
│                                          │   │   │   └── l2cs_quality_types.h
│                                          │   │   └── mefarg_au/
│                                          │   │       ├── mefarg_au_quality_evaluator.cc
│                                          │   │       ├── mefarg_au_data_cleaner.cc
│                                          │   │       └── mefarg_au_quality_types.h
│                                          │   │
│                                          │   └── utils/                  ← 新：共享工具
│                                          │       ├── math_utils.h
│                                          │       ├── geometry_utils.h
│                                          │       └── csv_utils.h
│                                          │
├── pipeline/                              ├── pipeline/
│   ├── pipeline.cc  ← 待删除              │   ├── framework/              ← 新：框架层
│   ├── base_pipeline.cc ← 裸露           │   │   ├── base_pipeline.cc/.h
│   ├── openface_pipeline.cc ← 裸露       │   │   ├── pipeline_factory.cc/.h
│   ├── mediapipe_pipeline.cc ← 裸露      │   │   ├── config.cc/.h
│   ├── l2cs_pipeline.cc ← 裸露           │   │   ├── module_factory.cc/.h
│   ├── mefarg_au_pipeline.cc ← 裸露      │   │   └── bounded_queue.h
│   ├── recognition_pipeline.cc ← 裸露    │   ├── workers/                ← 新：运行时组件
│   ├── pipeline_factory.cc ← 裸露        │   │   ├── decode_worker.cc/.h
│   ├── config.cc ← 裸露                  │   │   ├── output_manager.cc/.h
│   ├── decode_worker.cc ← 裸露           │   │   └── target_tracker.cc/.h
│   ├── module_factory.cc ← 裸露          │   ├── openface/
│   ├── output_manager.cc ← 裸露          │   │   └── openface_pipeline.cc/.h
│   ├── target_tracker.cc ← 裸露          │   ├── mediapipe/
│   └── bounded_queue.h ← 裸露            │   │   └── mediapipe_pipeline.cc/.h
│                                          │   ├── l2cs/
│                                          │   │   └── l2cs_pipeline.cc/.h
│                                          │   ├── mefarg_au/
│                                          │   │   └── mefarg_au_pipeline.cc/.h
│                                          │   └── recognition/
│                                          │       └── recognition_pipeline.cc/.h
│                                          │
├── db/              (不变)                ├── db/
│   ├── database.cpp                       │   ├── database.cpp
│   ├── video_repo.cpp                     │   ├── video_repo.cpp
│   ├── image_repo.cpp                     │   ├── image_repo.cpp
│   ├── task_repo.cpp                      │   ├── task_repo.cpp
│   └── snowflake.cpp                      │   └── snowflake.cpp
│                                          │
└── ui/              (不变)                └── ui/
    ├── main.cpp                               ├── main.cpp
    ├── main_window.cpp                        ├── main_window.cpp
    ├── theme.cc                               ├── theme.cc
    ├── pages/                                 ├── pages/
    ├── dialogs/                               ├── dialogs/
    └── widgets/                               └── widgets/
```

### 1.2 目标 `include/` 镜像结构

```
include/omniface/                         include/omniface/
────                                      ────
├── types.h                               ├── types.h                    [M]
│                                         │
├── core/                                 ├── core/
│   ├── detail/                           │   ├── inference/              ← 改名
│   │   ├── onnx_session.h                │   │   ├── onnx_session.h
│   │   └── preprocess.h                  │   │   └── preprocess.h
│   ├── detection/                        │   ├── detection/
│   │   └── face_detector.h               │   │   └── face_detector.h
│   ├── tracking/                         │   ├── tracking/
│   │   ├── bytetrack_engine.h            │   │   ├── bytetrack_engine.h
│   │   ├── face_tracker.h                │   │   ├── face_tracker.h
│   │   └── detail/                       │   │   └── internal/
│   │       ├── kalman_filter.h           │   │       ├── kalman_filter.h
│   │       ├── lapjv.h                   │   │       ├── lapjv.h
│   │       └── strack.h                  │   │       └── strack.h
│   ├── recognition/                      │   ├── recognition/
│   │   ├── align.h                       │   │   ├── align.h
│   │   ├── face_matcher.h                │   │   ├── face_matcher.h
│   │   └── face_recognizer.h             │   │   └── face_recognizer.h
│   ├── analysis/                         │   ├── analysis/
│   │   ├── analyzer_interface.h ← 裸露  │   │   ├── interface/
│   │   ├── openface/                     │   │   │   ├── analyzer_interface.h
│   │   │   ├── openface_analyzer.h       │   │   │   └── csv_output_writer.h
│   │   │   └── openface_writer.h         │   │   ├── openface/
│   │   ├── mediapipe/                    │   │   │   ├── openface_analyzer.h
│   │   │   ├── mediapipe_analyzer.h      │   │   │   └── openface_writer.h
│   │   │   ├── mediapipe_pose_estimator.h│   │   ├── mediapipe/
│   │   │   └── mediapipe_visualizer.h    │   │   │   ├── mediapipe_analyzer.h
│   │   ├── l2cs/                         │   │   │   ├── mediapipe_pose_estimator.h
│   │   │   └── l2cs_estimator.h          │   │   │   └── mediapipe_visualizer.h
│   │   └── mefarg_au/                    │   │   ├── l2cs/
│   │       └── mefarg_au_estimator.h     │   │   │   └── l2cs_estimator.h
│   │                                     │   │   └── mefarg_au/
│   │                                     │   │       └── mefarg_au_estimator.h
│   │                                     │   │
│   └── quality/                          │   ├── quality/
│       ├── base_quality_evaluator.h 裸露 │   │   ├── interface/
│       ├── base_data_cleaner.h     裸露  │   │   │   ├── base_quality_evaluator.h
│       ├── quality_types.h         裸露  │   │   │   ├── base_data_cleaner.h
│       ├── openface/                     │   │   │   ├── quality_types.h
│       │   ├── openface_quality_...h     │   │   │   ├── quality_csv_reader.h
│       │   ├── openface_quality_types.h  │   │   │   └── quality_grade_utils.h
│       │   └── openface_data_cleaner.h   │   │   ├── openface/
│       ├── mediapipe/                    │   │   │   ├── openface_quality_evaluator.h
│       │   ├── mediapipe_quality_...h    │   │   │   ├── openface_quality_types.h
│       │   ├── mediapipe_quality_types.h │   │   │   └── openface_data_cleaner.h
│       │   └── mediapipe_data_cleaner.h  │   │   ├── mediapipe/
│       ├── l2cs/                         │   │   │   ├── mediapipe_quality_evaluator.h
│       │   ├── l2cs_quality_evaluator.h  │   │   │   ├── mediapipe_quality_types.h
│       │   ├── l2cs_quality_types.h      │   │   │   └── mediapipe_data_cleaner.h
│       │   └── l2cs_data_cleaner.h       │   │   ├── l2cs/
│       └── mefarg_au/                    │   │   │   ├── l2cs_quality_evaluator.h
│           ├── mefarg_au_quality_...h    │   │   │   ├── l2cs_quality_types.h
│           ├── mefarg_au_quality_types.h │   │   │   └── l2cs_data_cleaner.h
│           └── mefarg_au_data_cleaner.h  │   │   └── mefarg_au/
│                                         │   │       ├── mefarg_au_quality_evaluator.h
│                                         │   │       ├── mefarg_au_quality_types.h
│                                         │   │       └── mefarg_au_data_cleaner.h
│                                         │   │
│                                         │   └── utils/
│                                         │       ├── math_utils.h
│                                         │       ├── geometry_utils.h
│                                         │       └── csv_utils.h
│                                         │
├── pipeline/                             ├── pipeline/
│   ├── pipeline.h  ← 待删除              │   ├── framework/
│   ├── base_pipeline.h ← 裸露           │   │   ├── base_pipeline.h
│   ├── openface_pipeline.h ← 裸露       │   │   ├── pipeline_factory.h
│   ├── mediapipe_pipeline.h ← 裸露      │   │   ├── config.h
│   ├── l2cs_pipeline.h ← 裸露           │   │   ├── module_factory.h
│   ├── mefarg_au_pipeline.h ← 裸露      │   │   └── bounded_queue.h
│   ├── recognition_pipeline.h ← 裸露    │   ├── workers/
│   ├── pipeline_factory.h ← 裸露        │   │   ├── decode_worker.h
│   ├── config.h ← 裸露                  │   │   ├── output_manager.h
│   ├── decode_worker.h ← 裸露           │   │   └── target_tracker.h
│   ├── module_factory.h ← 裸露          │   ├── openface/
│   ├── output_manager.h ← 裸露          │   │   └── openface_pipeline.h
│   ├── target_tracker.h ← 裸露          │   ├── mediapipe/
│   └── bounded_queue.h ← 裸露           │   │   └── mediapipe_pipeline.h
│                                         │   ├── l2cs/
│                                         │   │   └── l2cs_pipeline.h
│                                         │   ├── mefarg_au/
│                                         │   │   └── mefarg_au_pipeline.h
│                                         │   └── recognition/
│                                         │       └── recognition_pipeline.h
│                                         │
├── db/              (不变)               ├── db/
│   ├── database.h                        │   ├── database.h
│   ├── video_repo.h                      │   ├── video_repo.h
│   ├── image_repo.h                      │   ├── image_repo.h
│   ├── task_repo.h                       │   ├── task_repo.h
│   └── snowflake.h                       │   └── snowflake.h
│                                         │
└── ui/              (不变)               └── ui/
    ├── main_window.h                         ├── main_window.h
    ├── theme.h                               ├── theme.h
    ├── pages/                                ├── pages/
    ├── dialogs/                              ├── dialogs/
    └── widgets/                              └── widgets/
```

---

## 二、每个目录的模块含义

| 目录 | 模块含义 | 包含内容 |
|------|----------|----------|
| `core/inference/` | ONNX 推理基础设施 | `OnnxSession` 封装、`FaceCropPreprocessor` 预处理 |
| `core/detection/` | 人脸检测模块 | SCRFD `FaceDetector` |
| `core/tracking/` | 多目标跟踪模块 | `ByteTrackEngine`、`FaceTracker`、`KalmanFilter`、`STrack`、`Lapjv` |
| `core/tracking/internal/` | 跟踪算法内部实现 | Kalman 滤波、匈牙利匹配等底层算法（不对外暴露） |
| `core/recognition/` | 人脸识别模块 | `FaceRecognizer`（ArcFace 特征提取）、`FaceMatcher`（余弦匹配）、`AlignFace` |
| `core/analysis/interface/` | 分析模块公共接口 | `IAnalyzer` 抽象基类、统一 `FrameAnalysisResult`、`CsvOutputWriter` |
| `core/analysis/openface/` | OpenFace 分析后端 | CE-CLM 关键点、AU、视线、头部姿态 |
| `core/analysis/mediapipe/` | MediaPipe 分析后端 | FaceMesh 468 关键点、PnP 头部姿态估计 |
| `core/analysis/l2cs/` | L2CS-Net 视线分析后端 | 视线方向估计（偏航/俯仰角+3D向量） |
| `core/analysis/mefarg_au/` | MEFARG 动作单元分析后端 | 41 类 AU 检测 |
| `core/quality/interface/` | 质量评估公共接口 | `BaseQualityEvaluator`、`BaseDataCleaner`、`QualityGrade` 工具、CSV 读取骨架 |
| `core/quality/openface/` | OpenFace 数据质量评估 | 置信度/关键点/姿态/视线/AU 五维度评分 |
| `core/quality/mediapipe/` | MediaPipe 数据质量评估 | 重投影误差/姿态/角速度评分 |
| `core/quality/l2cs/` | L2CS 数据质量评估 | 视线置信度/稳定性评分 |
| `core/quality/mefarg_au/` | MEFARG 数据质量评估 | 判决策略距离/脉冲噪声/AU 翻转率评分 |
| `core/utils/` | 核心工具函数 | 数学（Softmax/RotationToEuler）、几何（BoundingBox 转换）、CSV 工具 |
| `pipeline/framework/` | 管道框架层 | `BasePipeline` 基类、`PipelineFactory`、`Config`、`ModuleFactory`、`BoundedQueue` |
| `pipeline/workers/` | 管道运行时组件 | `DecodeWorker`（视频解码）、`OutputManager`（输出管理）、`TargetTracker`（目标追踪） |
| `pipeline/openface/` | OpenFace 管道 | `OpenFacePipeline` |
| `pipeline/mediapipe/` | MediaPipe 管道 | `MediaPipePipeline` |
| `pipeline/l2cs/` | L2CS 管道 | `L2csPipeline` |
| `pipeline/mefarg_au/` | MEFARG 管道 | `MefargAuPipeline` |
| `pipeline/recognition/` | 纯识别管道 | `RecognitionPipeline` |
| `db/` | 数据库持久层 | SQLite 仓库（video/image/task）、Snowflake ID |
| `ui/` | Qt 用户界面 | 页面、对话框、组件、主题 |

---

## 三、变更总结

### 新增目录（8 个）

| 新目录 | 目的 |
|--------|------|
| `core/analysis/interface/` | 收纳裸露的 `analyzer_interface.h`，后续加 `csv_output_writer` |
| `core/quality/interface/` | 收纳裸露的 3 个基类头文件 + 新提取的 CSV/grade 工具 |
| `core/utils/` | 收纳从各模块提取的共享工具（math/geometry/csv） |
| `pipeline/framework/` | 管道框架层：base、factory、config、module_factory |
| `pipeline/workers/` | 管道运行时：decode_worker、output_manager、target_tracker |
| `pipeline/openface/` | OpenFace 管道实现 |
| `pipeline/mediapipe/` | MediaPipe 管道实现 |
| `pipeline/l2cs/` | L2CS 管道实现 |
| `pipeline/mefarg_au/` | MEFARG 管道实现 |
| `pipeline/recognition/` | 纯识别管道实现 |

### 重命名目录（3 个）

| 旧名 | 新名 | 原因 |
|------|------|------|
| `core/detail/` | `core/inference/` | "detail" 含义模糊，"inference" 明确表达 ONNX 推理职责 |
| `core/tracking/detail/` | `core/tracking/internal/` | "detail" 是 C++ 惯用语但在此项目中不一致，改为 "internal" |

### 删除文件（2 个）

| 文件 | 原因 |
|------|------|
| `src/pipeline/pipeline.cc` | 旧整体式 Pipeline 类，已被 BasePipeline 子类模式替代 |
| `include/omniface/pipeline/pipeline.h` | 同上 |

---

## 四、第二轮审查新发现的重复代码

### 4.1 `face_recognizer.cc` — ExtractBatch 自重复

`Extract`（行67-93）调用 `Preprocess()` 做 BGR→planar 预处理，但 `ExtractBatch`（行95-165）**内联**了完全相同的预处理逻辑（行112-141），包括相同的归一化常量 `kScale=1.0f/127.5f` 和 `kOffset=-1.0f`。→ `ExtractBatch` 改为循环调用 `Preprocess()`

### 4.2 `target_tracker.cc` — keep_cached lambda 重复定义

`keep_cached_on_recheck`（行128-137）与 `keep_cached`（行286-295）功能完全相同：从缓存读取相似度并写入 `out_matches`。→ 提取为私有方法 `ApplyCachedMatch()`

### 4.3 `mefarg_au_quality_evaluator.cc` — 开发注释残留

`Evaluate()` 方法中行136-178包含大段开发者思路注释（"Actually, let me change..."、"No, that's bad..."）。→ 删除这些注释

### 4.4 `kAuNames` 数组重复

41个AU名称数组在 `mefarg_au_estimator.cc` 和 `mefarg_au_quality_evaluator.cc` 中各定义一次。→ 提取到 `mefarg_au_quality_types.h` 或共享头文件

### 4.5 `ZeroDist()` 重复

`mediapipe_visualizer.cc::ZeroDist()` 返回 `cv::Mat::zeros(4,1,CV_64F)`，此模式在 `mediapipe_pose_estimator.cc` 行211和行341中直接内联使用。→ 统一使用 `ZeroDist()` 或提取到共享头文件

### 4.6 `FaceDetector::Config` 创建重复

`module_factory.cc` 行19-26 创建 `FaceDetector::Config` 的代码在 `base_pipeline.cc::UpdateThresholds` 行72-78 完全重复。→ 提取为 `module_factory.h` 中的内联工厂函数

### 4.7 `mediapipe_pose_estimator.cc` — EstimatePose/EstimatePoseFromCrop 后处理重复

`EstimatePose`（行128-254）和 `EstimatePoseFromCrop`（行256-380）的后处理部分（ONNX输出解析→关键点提取→PnP求解→Rodrigues→Euler→重投影RMS）约60行完全重复。→ 提取为私有方法 `PostProcessLandmarks()`

### 4.8 `FrameResult`/`FrameCallback` 绑定到废弃 `pipeline.h`

`base_pipeline.h` 引用 `pipeline.h` 仅为了 `FrameResult` 和 `FrameCallback` 类型。→ 将这两个类型移到 `base_pipeline.h` 自身，删除 `pipeline.h` 上的依赖

---

## 五、重构步骤

### 阶段一：文件迁移（纯目录重组，不改代码逻辑）

1. `core/detail/` → `core/inference/`（src + include）
2. `core/tracking/detail/` → `core/tracking/internal/`
3. `pipeline/` 下按 framework / workers / openface / mediapipe / l2cs / mefarg_au / recognition 拆分
4. 更新所有 `#include` 路径和 `CMakeLists.txt` 源文件路径

### 阶段二：消除裸露文件 + 删除废弃代码

5. `analyzer_interface.h` → `core/analysis/interface/`
6. `base_quality_evaluator.h`、`base_data_cleaner.h`、`quality_types.h` → `core/quality/interface/`
7. 将 `FrameResult`/`FrameCallback` 迁移到 `base_pipeline.h`，删除 `pipeline.cc` + `pipeline.h`

### 阶段三：提取重复代码（不影响结构，只减代码量）

8. 提取 `CsvOutputWriter` → `core/analysis/interface/`
9. 提取共享工具函数 → `core/utils/`（math_utils.h, geometry_utils.h, csv_utils.h）
10. 提取 quality 共享逻辑 → `core/quality/interface/`（quality_csv_reader, quality_grade_utils）
11. 合并 `mediapipe_pose_estimator.cc` 两个方法的公共后处理逻辑（4.7）
12. 统一 `AnalyzerKind` / `AnalysisBackend` 枚举
13. `face_recognizer.cc`: `ExtractBatch` 改为调用 `Preprocess()`（4.1）
14. `target_tracker.cc`: 提取 `ApplyCachedMatch()` 消除 lambda 重复（4.2）
15. `mefarg_au_*`: 提取共享 `kAuNames` 到类型头文件（4.4）
16. `module_factory.cc`: 提取 `MakeDetectorConfig()` 工厂函数（4.6）
17. 清理 `mefarg_au_quality_evaluator.cc` 开发注释（4.3）

### 阶段四：大文件拆分

18. 拆分 `openface_quality_evaluator.cc` 的 `AssignFrameGrades`
19. 拆分 `openface_analyzer.cc` 的 `Impl::Analyze`
20. 修正 `OpenFaceWriter` 命名空间

---

## 六、变更量

| 类型 | 数量 |
|------|------|
| 新增目录 | 10 |
| 重命名目录 | 2 |
| 新增文件 | 5（csv_output_writer, math_utils, geometry_utils, csv_utils, quality_grade_utils） |
| 删除文件 | 2（pipeline.cc, pipeline.h） |
| 移动文件 | ~18（pipeline 子目录 + analysis/quality 裸露文件） |
| 修改文件 | ~30（include 路径更新 + 代码修改 + 去重） |
| 净代码 | **约 -1200 行** |
