# OmniFace — Docker 使用指南 (Windows)

## 什么是 OmniFace？

目标人物实时面部分析系统。上传一段视频和目标人物照片，自动检测、跟踪、识别目标人物，实时输出头部姿态、视线方向、面部动作单元（AU）分析结果。

## Docker 版本说明

Docker 版本以 Web 服务模式运行（`--serve`），通过浏览器访问 `http://localhost:8080` 使用全部功能：
- 上传视频和目标人物照片
- 实时双路 MJPEG 画面（原始 + 分析标注）
- 实时图表（注意力评分、AU 激活、姿态/视线曲线）
- 任务历史查看与结果下载

> **注意**：Docker 版本不支持 OpenCV GUI 窗口模式（CLI 模式），请使用 Web 界面。

---

## 快速开始（3 步）

### 第 1 步：安装 Docker Desktop

从 https://www.docker.com/products/docker-desktop/ 下载安装。

安装完成后重启电脑，确认 Docker Desktop 正在运行（任务栏右下角鲸鱼图标）。

### 第 2 步：获取镜像

**方式 A — 离线安装包**（推荐，无需网络）

如果你收到了 `omniface-windows.tar.gz` 文件：

```powershell
# 在解压后的目录中打开 PowerShell，运行：
docker load < omniface-windows.tar.gz
```

**方式 B — 在线拉取**

```powershell
docker pull yourname/omniface:latest
```

### 第 3 步：启动

将 `docker-compose.yml` 放到你想存放数据的目录，然后：

```powershell
# 创建所需目录
mkdir uploads\img uploads\video output presets

# 启动
docker compose up -d
```

打开浏览器，访问 **http://localhost:8080** 开始使用。

---

## 目录结构

启动后，你的工作目录如下：

```
工作目录/
├── docker-compose.yml        # 启动配置
├── uploads/
│   ├── img/                  # 把目标人物照片放这里
│   └── video/                # 把待分析视频放这里
├── output/                   # 分析结果自动保存到这里
│   └── 20260720_143052/      # 每次任务一个目录（时间戳命名）
│       ├── annotated.mp4     # 标注后的视频
│       ├── tracking.csv      # 逐帧跟踪数据
│       ├── onnx_analysis.csv # 面部分析数据
│       ├── metrics.jsonl     # 逐帧完整指标
│       └── task.json         # 任务摘要
└── presets/                  # 保存的参数预设方案
```

---

## 使用流程

1. 将目标人物照片放入 `uploads/img/`
2. 将待分析视频放入 `uploads/video/`
3. 浏览器打开 `http://localhost:8080`
4. 在「新建任务」页面选择参考照片和视频，配置参数
5. 点击「开始分析」
6. 在「运行监控」页面查看实时进度和图表
7. 分析完成后在「任务历史」查看报告和下载结果

---

## 常用命令

```powershell
# 启动
docker compose up -d

# 查看日志
docker compose logs -f

# 停止
docker compose down

# 重启
docker compose restart

# 更新镜像后重启
docker compose down
docker compose up -d
```

---

## 自定义配置

把 `config.yaml` 放到工作目录，然后在 `docker-compose.yml` 中取消注释：

```yaml
volumes:
  - ./config.yaml:/app/config.yaml:ro
```

重启容器生效：`docker compose restart`

---

## 自定义端口

默认使用 8080 端口。修改 `docker-compose.yml`：

```yaml
ports:
  - "9000:8080"    # 使用 9000 端口
```

---

## 注意事项

1. **首次构建**：Docker 镜像约 1-2GB（含模型文件），构建时间约 15-30 分钟
2. **分析后端**：默认使用 ONNX 后端（支持姿态、视线、AU 三个分析器）。OpenFace 后端需要额外挂载模型文件
3. **大视频上传**：默认支持最大 500MB 文件上传，可在配置中修改 `max_upload_mb`
4. **性能**：Docker 在 Windows 上的 CPU 性能为原生的 95-98%（通过 WSL2），几乎无损耗
5. **GPU 加速**：如需 GPU 加速，在 `docker-compose.yml` 中添加：
   ```yaml
   deploy:
     resources:
       reservations:
         devices:
           - driver: nvidia
             count: 1
             capabilities: [gpu]
   ```

---

## 常见问题

**Q: 启动后浏览器打不开页面？**
A: 确认 Docker Desktop 正在运行（任务栏鲸鱼图标），等待几秒后刷新。如果端口被占用，更换端口。

**Q: 上传文件后分析报错？**
A: 检查视频格式是否支持（mp4/avi/mkv），图片是否正常（jpg/png）。查看日志：`docker compose logs`

**Q: 分析很慢？**
A: 大视频处理需要时间。可以在 Web 界面调整参数：降低 `detection_interval`、启用 `async_recognition`、设置 `max_frame_width`。

**Q: 如何更新到新版本？**
A: 收到新的镜像文件后：
```powershell
docker load < omniface-new.tar.gz
docker compose down
docker compose up -d
```
