# OmniFace

> 实时人脸检测、跟踪、识别与行为分析系统
> Real-time Face Detection, Tracking, Recognition & Behavioral Analysis System

![Version](https://img.shields.io/badge/version-1.0.0-blue)
![C++](https://img.shields.io/badge/C%2B%2B-20-brightgreen)
![Qt](https://img.shields.io/badge/Qt-6.11-green)
![Platform](https://img.shields.io/badge/Platform-Windows%20x64-lightgrey)
![Inference](https://img.shields.io/badge/Inference-ONNX%20Runtime%201.23.2-orange)
![Build](https://img.shields.io/badge/Build-CMake%20%2B%20vcpkg-yellow)

OmniFace 是一个面向桌面端的实时人脸分析系统，基于 **Qt6 Widgets** 与 **C++20** 构建。它在一条统一的流水线中完成了 **人脸检测（SCRFD）→ 多目标跟踪（ByteTrack）→ 特征识别（ArcFace）→ 行为分析（OpenFace / MediaPipe / L2CS-Net / MEFARG-AU）→ 数据质量评估与清洗** 的完整闭环，并提供了媒体资源管理、任务记录、可视化报表等桌面应用功能。

除 OpenFace 外，全部深度学习模型均通过 **ONNX Runtime** 加载，支持 **CPU / DirectML / CUDA / TensorRT** 四种执行提供者。行为分析后端既可选择 OpenFace 传统计算机视觉方案，也可选择基于深度学习的 MediaPipe、L2CS-Net、MEFARG-AU 方案。

---

## 目录

- [OmniFace](#omniface)
  - [目录](#目录)
  - [功能特性](#功能特性)
    - [核心算法](#核心算法)
    - [数据与质量](#数据与质量)
    - [桌面应用](#桌面应用)
  - [技术栈](#技术栈)
  - [快速开始](#快速开始)
    - [环境要求](#环境要求)
    - [获取第三方库与模型（重要）](#获取第三方库与模型重要)
    - [安装 vcpkg 依赖](#安装-vcpkg-依赖)
    - [编译](#编译)
    - [运行](#运行)
    - [打包发布](#打包发布)
  - [使用指南](#使用指南)
    - [界面导航](#界面导航)
    - [处理流程](#处理流程)
    - [任务详情](#任务详情)
    - [配置文件](#配置文件)
  - [文档](#文档)
  - [系统架构](#系统架构)
  - [项目结构](#项目结构)
  - [模型说明](#模型说明)
  - [数据质量评估与清洗](#数据质量评估与清洗)
  - [数据库设计](#数据库设计)
  - [致谢](#致谢)
  - [贡献](#贡献)
  - [许可证](#许可证)
  - [版本历史](#版本历史)

---

## 功能特性

### 核心算法

- **人脸检测（SCRFD）**：输出边界框 + 5 个关键点（左眼/右眼/鼻尖/左嘴角/右嘴角），letterbox 预处理 + NMS 去重。
- **多目标跟踪（ByteTrack）**：卡尔曼滤波预测 + IoU 关联 + LAPJV 最优匹配；高分/低分两级检测分别处理，支持遮挡恢复。
- **目标识别（ArcFace）**：5 点仿射对齐到 112×112 标准模板，提取 512 维特征，余弦相似度匹配，可锁定特定目标持续追踪。
- **行为分析（4 种后端）**：
  - **OpenFace**：68 点 2D/3D 关键点、头部姿态、双眼视线向量、AU 动作单元、PDM 参数。
  - **MediaPipe**：468 点面部网格、头部姿态（EPnP 求解）、重投影误差质量指标。
  - **L2CS-Net**：视线角度（yaw/pitch）与 3D 视线向量，带 EMA 平滑。
  - **MEFARG-AU**：41 个动作单元（27 对称 + 14 左右区分）的概率与激活状态。

### 数据与质量

- **数据质量评估**：对分析结果逐帧计算 0–100 综合得分并映射为 **S/A/B/C/D 五级**，覆盖置信度、关键点可见性、姿态、视线、AU 稳定性、FACS 约束等多个维度。
- **数据清洗**：按设定的最低质量等级自动筛除低质量帧，输出干净数据子集并统计剔除数量。
- **可视化报表**：置信度直方图、姿态/视线曲线、等级分布饼图、AU 柱状图、维度记分卡（Qt Charts）。

### 桌面应用

- **媒体资源管理**：视频/图片入库（SQLite），元数据编辑，按名称/日期/置信度筛选，批量删除。
- **任务管理**：分析任务全生命周期记录（运行中/完成/失败），支持暂停 / 单步 / 继续 / 停止。
- **实时统计**：处理帧数、检测人数、匹配数、分析成功率、当前/峰值相似度、平均 FPS 与延迟。

---

## 技术栈

| 类别 | 选型 |
|------|------|
| 语言标准 | C++20（MSVC / GCC） |
| GUI 框架 | Qt 6.11.1（Widgets / Multimedia / Charts / Svg / Sql / Concurrent） |
| 构建系统 | CMake ≥ 3.22 |
| 推理引擎 | ONNX Runtime 1.23.2（CPU / DirectML / CUDA / TensorRT） |
| 计算机视觉 | OpenCV 4（core / imgproc / dnn / calib3d / videoio / highgui） |
| 包管理 | vcpkg（OpenCV / Eigen3 / spdlog / yaml-cpp / dlib / Boost） |
| 数据库 | SQLite（Qt6::Sql） |
| 日志 | spdlog（文件日志，最多保留 10 份） |
| 行为分析 | OpenFace（C++ CLM 库）/ MediaPipe / L2CS-Net / MEFARG-AU（ONNX） |
| 媒体后端 | Qt Multimedia FFmpeg（默认，可切换 windowsmedia） |

---

## 快速开始

### 环境要求

- **操作系统**：Windows 10/11 x64（当前面向 Windows）
- **编译器**：Visual Studio 2022（MSVC，要求 `/utf-8` 与 C++20）；或 MinGW
- **CMake**：≥ 3.22
- **Qt**：6.11.1（`msvc2022_64` 预编译套件）
- **vcpkg**：用于安装 OpenCV 等 C++ 依赖
- **显卡（可选）**：NVIDIA GPU（CUDA / TensorRT）或 DirectML 设备
- **Python（可选）**：仅用于模型导出脚本（`scripts/tools/*.py`）

### 获取第三方库与模型（重要）

> `.gitignore` 忽略了 `third_party/` 与 `models/`，**它们不随仓库分发**，请在 clone 后按以下说明手动准备。

1. **ONNX Runtime**：下载 GPU 版 1.23.2，解压到 `third_party/onnxruntime-win-x64-gpu-1.23.2`，目录需包含 `lib/onnxruntime.dll`、`lib/onnxruntime.lib` 与 `include/`。若使用 vcpkg 的 `onnxruntime` 包，将 `OMNIFACE_ONNXRUNTIME_ROOT` 置空即可。
2. **OpenFace**：从官方仓库获取并放入 `third_party/OpenFace`（提供 `LandmarkDetector`、`FaceAnalyser`、`GazeAnalyser`、`Utilities` 四个库），随后在 `third_party/OpenFace` 下单独执行 CMake 构建。
3. **L2CS-Net / MediaPipe / OpenGraphAU**：分别放入 `third_party/L2CS-Net`、`third_party/mediapipe`、`third_party/OpenGraphAU`（模型导出脚本依赖它们；推理运行不依赖，仅需 `models/` 中的 ONNX）。
4. **模型文件**：按 [模型说明](#模型说明) 下载放入 `models/`，运行必需。

### 安装 vcpkg 依赖

```bash
vcpkg install opencv4[core,dnn,highgui,calib3d] eigen3 spdlog yaml-cpp dlib openblas lapack-reference boost-filesystem boost-system
```

依赖清单见 `vcpkg.json`。若 vcpkg / Qt 安装路径与顶层 `CMakeLists.txt` 中的默认值不同，请修改 `CMAKE_TOOLCHAIN_FILE` 与 `CMAKE_PREFIX_PATH`（或构建时用 `-D` 覆盖）。

### 编译

```bash
# 1. 配置
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release

# 2. 编译
cmake --build build --config Release --target OmniFace
```

可选：开启 MSVC 整机优化（LTO）：

```bash
cmake -B build -DCMAKE_CXX_FLAGS_RELEASE="/O2 /Ob2 /DNDEBUG /GL" \
      -DCMAKE_SHARED_LINKER_FLAGS_RELEASE="/LTCG" \
      -DCMAKE_EXE_LINKER_FLAGS_RELEASE="/LTCG"
cmake --build build --config Release
```

构建后 post-build 钩子会自动：
- 用 `windeployqt` 部署 Qt 运行库（platforms / styles / imageformats 等插件目录）；
- 复制 ONNX Runtime DLL（含 CUDA / TensorRT 提供者）到输出目录；
- 复制 OpenFace 模型到 `openface_model/`。

### 运行

```bash
./build/Release/OmniFace.exe
```

程序启动后会在可执行文件同级 `data/` 下初始化：

| 目录 / 文件 | 用途 |
|------|------|
| `data/omniface.db` | SQLite 数据库（videos / images / tasks） |
| `data/log/` | spdlog 时间戳日志（保留最近 10 份） |
| `data/upload/` | 导入的原始视频 / 图片 |
| `data/output/` | 分析任务的输出视频、CSV、质量报表 |

### 打包发布

**免安装 zip：**

```powershell
powershell -ExecutionPolicy Bypass -File scripts/package.ps1
```

脚本从 git tag 读取版本号，收集可执行文件、运行库、Qt 插件、模型与配置，生成 `dist/OmniFace_v<version>.zip`。

**安装程序（Inno Setup）：**

```bash
iscc scripts\installer.iss /DAppVersion=1.0.0
```

输出 `dist/OmniFace_Setup_v1.0.0.exe`，支持桌面快捷方式、卸载、现代向导界面。

---

## 使用指南

### 界面导航

| 页面 | 功能 |
|------|------|
| 首页 | 系统概览与快捷入口，统计视频/图片/任务数量 |
| 处理页 | 核心分析工作流（见下） |
| 视频管理 | 视频入库、元数据编辑、按名称/录制时间筛选、删除 |
| 图片管理 | 图片入库（含人脸检测信息）、按名称/置信度筛选、作为识别参考 |
| 任务历史 | 所有分析任务记录，按后端/状态/时间筛选 |

### 处理流程

1. **选择媒体**：从文件或数据库选择视频 / 图片（图片会提取参考人脸特征用于目标锁定）。
2. **选择后端**：OpenFace / MediaPipe / L2CS-Net / MEFARG-AU / 仅检测跟踪。
3. **开始处理**：实时预览检测框、跟踪 ID、相似度、FPS 与延迟；支持暂停 / 单步 / 继续 / 停止。
4. **查看结果**：处理完成后自动进入任务详情页。

### 任务详情

- **原始信息**：源媒体元数据、任务参数、总帧数、平均 FPS、状态、输出路径。
- **输出视频**：播放带标注的结果视频（检测框、目标标记、相似度）。
- **分析数据**：后端特有结果可视化（68 关键点 / 468 网格 / 视线 / AU 状态）。
- **质量评估**：数据质量评分与各维度报表。
- **数据清洗**：按最低等级筛除低质量帧，生成干净数据子集。

### 配置文件

`config.yaml` 控制全部运行参数（可执行文件旁的 `config.yaml` 优先级最高），完整说明见 [文档](#文档)。

```yaml
analysis_backend: openface     # openface | mediapipe | l2cs(gaze) | mefarg_au | none

model:
  detector_model: "models/buffalo_sc/det_500m.onnx"    # SCRFD 检测模型
  recognizer_model: "models/buffalo_sc/w600k_mbf.onnx"  # ArcFace 识别模型
  detection_threads: 4
  recognition_threads: 4
  detection_input_width: 640
  detection_input_height: 640
  device: cpu                   # cpu | directml | cuda | tensorrt

thresholds:
  match_threshold: 0.30         # 人脸匹配相似度阈值
  detection_confidence: 0.5
  detection_nms: 0.4
  tracking_high: 0.5            # ByteTrack 高分检测阈值
  tracking_match: 0.4
  tracking_match_second: 0.5
  tracking_new: 0.7

openface:
  profile: standard             # standard | max_quality

onnx:                           # MediaPipe 后端
  landmark_model: "models/mediapipe/face_mesh_Nx3x192x192_post.onnx"
  canonical_model: "models/mediapipe/canonical_face_model.obj"
  margin: 0.25
  score_threshold: 0.95

gaze:                           # L2CS-Net 视线后端
  model: "mobilenetv2_gaze.onnx"  # mobilenetv2 | mobileone_s0 | resnet50

mefarg_au:                      # 表情识别后端
  model: "models/OpenGprahAU/mefarg_resnet18_au.onnx"
  threshold: 0.5
```

---

## 文档

| 文档 | 说明 |
|------|------|
| [models/MODELS.md](models/MODELS.md) | 全部 ONNX 模型的结构、输入输出、预处理/后处理流程、来源 |
| [config.yaml](config.yaml) | 默认配置（字段含中文注释） |
| [vcpkg.json](vcpkg.json) | 第三方依赖清单 |

---

## 系统架构

分层模块化设计，依赖方向自下而上：

```
┌────────────────────────────────────────────────────────────────────┐
│                            UI 层 (omniface_ui)                      │
│  首页 · 处理页 · 视频管理 · 图片管理 · 任务历史 · 详情页 · 报表控件     │
├────────────────────────────────────────────────────────────────────┤
│                        流水线层 (omniface_pipeline)                  │
│  BasePipeline 框架 ── 视频解码线程 ── 目标跟踪器 ── 输出管理 ── 配置   │
│  ├─ OpenFacePipeline    ├─ MediaPipePipeline    ├─ L2csPipeline     │
│  └─ MefargAuPipeline    └─ RecognitionPipeline（仅检测跟踪）         │
├────────────────────────────────────────────────────────────────────┤
│                   核心层 (omniface_core)                             │
│  ┌──────────┐ ┌──────────┐ ┌────────────┐ ┌──────────────────────┐  │
│  │ 检测      │ │ 跟踪      │ │ 识别        │ │ 分析（4 后端）         │  │
│  │ SCRFD     │ │ ByteTrack│ │ ArcFace    │ │ OpenFace             │  │
│  └──────────┘ └──────────┘ └────────────┘ │ MediaPipe FaceMesh    │  │
│  ┌──────────────────────┐ ┌──────────────┐ │ L2CS-Net 视线         │  │
│  │ 推理 (OnnxSession)    │ │ 质量评估/清洗 │ │ MEFARG-AU             │  │
│  │ 预处理 (FaceCrop)     │ │ S/A/B/C/D 分级│ └──────────────────────┘  │
│  └──────────────────────┘ └──────────────┘                            │
├────────────────────────────────────────────────────────────────────┤
│                    数据库层 (omniface_db) — SQLite (Qt6::Sql)        │
│  videos 表 · images 表 · tasks 表 · Snowflake 分布式 ID              │
└────────────────────────────────────────────────────────────────────┘
```

**单帧处理流水线**：

```
输入帧 → 缩放(可选) → SCRFD 检测 → 坐标回映射 → ByteTrack 跟踪
   → 目标匹配(参考人脸特征 vs 跟踪人脸) → 后端行为分析(关键点/姿态/视线/AU)
   → 叠加可视化(检测框/ID/相似度) → 写入输出视频 → 回调通知 UI 刷新
```

所有行为分析后端实现统一的 `IAnalyzer` 接口，流水线按 `analysis_backend` 配置动态创建实例，互不耦合。

---

## 项目结构

```
OmniFace/
├── CMakeLists.txt              # 顶层构建脚本（vcpkg / Qt / ONNX Runtime 集成）
├── config.yaml                 # 应用默认配置
├── vcpkg.json                  # vcpkg 依赖清单
├── include/omniface/           # 全部公共头文件
│   ├── core/                   #   核心模块
│   │   ├── inference/          #     ONNX Runtime 会话封装 + 图像预处理
│   │   ├── detection/          #     SCRFD 人脸检测
│   │   ├── tracking/           #     ByteTrack 跟踪（Kalman / LAPJV / STrack）
│   │   ├── recognition/        #     ArcFace 识别 + 余弦匹配 + 仿射对齐
│   │   ├── analysis/           #     4 个行为分析后端
│   │   └── quality/            #     质量评估与数据清洗
│   ├── pipeline/               #   流水线框架 + 各后端流水线 + 工作线程
│   ├── db/                     #   数据库仓库层
│   └── ui/                     #   UI 页面 / 对话框 / 控件
├── src/                        # 实现文件（与 include 一一对应）
├── models/                     # ONNX 模型（gitignore，需手动获取）
├── resources/                  # 图标、SVG 资源、RC 文件、预览图
├── scripts/                    # 打包、图标生成、模型导出等脚本
├── third_party/                # OpenFace / L2CS-Net / MediaPipe / OpenGraphAU / ONNX Runtime（gitignore）
├── build/                      # CMake 构建输出（自动生成）
└── dist/                       # 打包输出（zip / 安装程序）
```

---

## 模型说明

`models/` 目录中的全部 ONNX 模型（详细结构见 [models/MODELS.md](models/MODELS.md)）：

| 模型 | 文件 | 用途 | 输入尺寸 |
|------|------|------|---------|
| SCRFD | `buffalo_sc/det_500m.onnx` | 人脸检测 | 640×640（BGR 平面分离，letterbox） |
| ArcFace | `buffalo_sc/w600k_mbf.onnx` | 512 维特征提取 | 112×112（BGR 平面分离） |
| L2CS-Net | `l2cs-net/mobilenetv2_gaze.onnx` 等 | 视线估计 | 448×448（RGB CHW，ImageNet 归一化） |
| MediaPipe | `mediapipe/face_mesh_Nx3x192x192_post.onnx` | 468 关键点 + 姿态 | 192×192（RGB CHW + 裁剪参数） |
| MEFARG-AU | `OpenGprahAU/mefarg_resnet18_au.onnx` | 41 个动作单元 | 224×224（RGB CHW，中心裁剪） |

| 模型 | 来源 |
|------|------|
| SCRFD / ArcFace | InsightFace（buffalo_sc 模型包） |
| L2CS-Net | L2CS-Net（Gaze360 数据集训练） |
| MediaPipe | Google MediaPipe Face Landmarker（ONNX 转换） |
| MEFARG-AU | OpenGraphAU（ResNet-18 + 图卷积） |

AU 模型可从 PyTorch checkpoint 通过 `scripts/tools/export_au_onnx.py` 重新导出。

---

## 数据质量评估与清洗

对分析任务输出的 CSV 结果做后处理，为每一帧计算 **0–100 综合得分** 并映射为 **S/A/B/C/D 五级**：

| 等级 | 分数 | 含义 |
|------|------|------|
| S | ≥ 93 | 卓越 |
| A | 85–92 | 优秀 |
| B | 70–84 | 良好 |
| C | 50–69 | 一般 |
| D | < 50 | 较差 |

不同后端采用不同评估维度：

- **OpenFace**：检测置信度(30) + 关键点可见数(15) + 姿态角度(20) + 双眼视线一致性(15) + 角速度/稳定性(10) + AU 合理性与 FACS 约束(10)。
- **MediaPipe**：几何质量（重投影 RMS 误差 / 人脸尺寸，50）+ 姿态质量（姿态角与稳定性，50）。
- **L2CS / MEFARG-AU**：基于视线 / AU 输出的专用质量评估实现。

数据清洗器按设定的最低等级（如 `GradeCut::kB`）自动筛除低质量帧，保留干净子集并统计剔除数量。

---

## 数据库设计

SQLite 数据库（`data/omniface.db`），通过 Qt6::Sql 访问，主键由 Snowflake 算法生成（64 位时间戳 + 机器 + 序列，线程安全）。

**videos**（视频资源）：`id, name, description, notes, duration, resolution_width/height, frame_rate, record_time, record_location, file_size, saved_path(唯一), created_at`

**images**（图片资源，含人脸检测信息）：`id, name, description, notes, saved_path(唯一), face_confidence, bbox_x1/y1/x2/y2, keypoints(BLOB), embedding(BLOB, 512维), created_at`

**tasks**（分析任务）：`id, video_id(FK), image_id(FK), name, description, notes, analysis_backend, params_snapshot(BLOB), start_time, end_time, total_frames, avg_fps, output_dir, output_video_path, csv_path, output_types, status(running/completed/failed)`

索引覆盖名称、录制时间、创建时间、后端、状态等常用查询字段；迁移机制自动补充新增列。

---

## 致谢

本项目的算法与模型受益于以下开源项目：

- [InsightFace](https://github.com/deepinsight/insightface) — SCRFD 检测与 ArcFace 识别模型（buffalo_sc）
- [ByteTrack](https://github.com/ifzhang/ByteTrack) — 多目标跟踪算法
- [OpenFace](https://github.com/TadasBaltrusaitis/OpenFace) — 传统面部行为分析（关键点 / 视线 / AU）
- [L2CS-Net](https://github.com/Ahmednull/L2CS-Net) — 视线估计
- [MediaPipe](https://github.com/google-ai-edge/mediapipe) — 人脸网格与姿态估计
- [OpenGraphAU](https://github.com/lingyugong/OpenGraphAU) — 动作单元检测
- [ONNX Runtime](https://github.com/microsoft/onnxruntime) — 模型推理引擎
- [Qt](https://www.qt.io/)、[OpenCV](https://opencv.org/)、[vcpkg](https://github.com/microsoft/vcpkg)、[spdlog](https://github.com/gabime/spdlog)、[yaml-cpp](https://github.com/jbeder/yaml-cpp)

---

## 贡献

欢迎提交 Issue 与 Pull Request。若需修改代码，请遵循以下约定：

- 遵循项目现有代码风格与命名规范（`src/` / `include/` 一一对应，头文件位于 `include/omniface/` 下同名路径）。
- 新增功能请同步更新 `CMakeLists.txt` 中的模块目标与 `config.yaml` 默认配置。
- 涉及模型格式 / 输入输出的改动，请同步更新 `models/MODELS.md`。

---

## 许可证

本项目目前**未指定开源许可证**（默认保留所有权利）。如需开源，请先确定适用的许可证（如 MIT / Apache-2.0）并补充 `LICENSE` 文件。

---

## 版本历史

| 版本 | 日期 | 说明 |
|------|------|------|
| 1.0.0 | 2026-08 | 完整版本：修复视频全屏显示与数据库删除 bug，完成检测/跟踪/识别 + 四后端行为分析 + 质量评估与清洗全流程 |

---

*OmniFace — 基于 Qt6 / C++20 的实时人脸分析与质量评估系统*
