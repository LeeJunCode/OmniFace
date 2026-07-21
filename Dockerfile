# ============================================================
# OmniFace Docker 镜像 — 多阶段构建
# 支持 OpenFace + ONNX 双后端，默认使用 ONNX
# ============================================================

# -------------------- 构建阶段 --------------------
FROM ubuntu:24.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

# 安装编译工具链和依赖
RUN apt-get update && apt-get install -y --no-install-recommends \
    g++ \
    gcc \
    cmake \
    make \
    git \
    wget \
    ca-certificates \
    libopencv-dev \
    libeigen3-dev \
    libopenblas-dev \
    libdlib-dev \
    libboost-filesystem-dev \
    libboost-system-dev \
    && rm -rf /var/lib/apt/lists/*

# 下载并安装 ONNX Runtime 预编译包
ARG ONNX_VERSION=1.20.1
RUN wget -q https://github.com/microsoft/onnxruntime/releases/download/v${ONNX_VERSION}/onnxruntime-linux-x64-${ONNX_VERSION}.tgz \
    && tar xzf onnxruntime-linux-x64-${ONNX_VERSION}.tgz -C /opt \
    && rm onnxruntime-linux-x64-${ONNX_VERSION}.tgz \
    && cd /opt/onnxruntime-linux-x64-${ONNX_VERSION} \
    && if [ ! -d lib64 ] && [ -d lib ]; then ln -sf lib lib64; fi \
    && if [ ! -d include/onnxruntime ] && [ -f include/onnxruntime_cxx_api.h ]; then mkdir -p include/onnxruntime && mv include/*.h include/onnxruntime/ 2>/dev/null; fi \
    && ls -la include/ && ls -la lib/ | head -5

ENV ONNXRUNTIME_ROOT=/opt/onnxruntime-linux-x64-${ONNX_VERSION}
ENV CMAKE_PREFIX_PATH=${ONNXRUNTIME_ROOT}

WORKDIR /build

# 先复制 third_party，编译 OpenFace（利用 Docker 层缓存）
COPY third_party/ third_party/

RUN cd third_party/OpenFace \
    && cmake -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build -j$(nproc)

# 复制源码
COPY CMakeLists.txt .
COPY app/ app/
COPY include/ include/
COPY src/ src/

# 编译 OmniFace
RUN cmake -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build -j$(nproc)

# -------------------- 运行阶段 --------------------
FROM ubuntu:24.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive

# 安装运行时依赖（使用 -dev 包以确保版本兼容性）
RUN apt-get update && apt-get install -y --no-install-recommends \
    libopencv-dev \
    libopenblas-dev \
    libdlib-dev \
    libboost-filesystem-dev \
    libboost-system-dev \
    zip \
    && rm -rf /var/lib/apt/lists/*

# 复制 ONNX Runtime 共享库
ARG ONNX_VERSION=1.20.1
COPY --from=builder /opt/onnxruntime-linux-x64-${ONNX_VERSION}/lib/libonnxruntime.so* /usr/lib/

# 复制编译产物
COPY --from=builder /build/build/app/omniface /app/omniface
RUN chmod +x /app/omniface

# 复制 OpenFace 运行时文件（编译产物 + 模型文件）
COPY --from=builder /build/third_party/OpenFace/build/bin/ /app/third_party/OpenFace/build/bin/
COPY --from=builder /build/third_party/OpenFace/lib/local/ /app/third_party/OpenFace/lib/local/

# 复制 Web 前端
COPY web/ /app/web/

# 复制模型文件
COPY models/ /app/models/

# 复制配置文件
COPY config.yaml /app/config.yaml

# Docker 容器需要监听 0.0.0.0
RUN sed -i 's|host: "127.0.0.1"|host: "0.0.0.0"|' /app/config.yaml

# 复制数据文件
COPY data/ /app/data/

# 创建数据目录（确保所有子目录存在）
RUN mkdir -p /app/data/img /app/data/video /app/data/output /app/data/presets

WORKDIR /app
EXPOSE 8080

# 默认以 Web 服务模式启动
ENTRYPOINT ["./omniface", "--serve"]
