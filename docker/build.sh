#!/usr/bin/env bash
# ============================================================
# OmniFace Docker 镜像构建脚本
#
# 用法:
#   ./docker/build.sh              # 构建镜像（本地使用）
#   ./docker/build.sh export       # 构建并导出 omniface-windows.tar.gz
#   ./docker/build.sh push         # 构建并推送到 Docker Hub（需先配置）
# ============================================================

set -euo pipefail

IMAGE_NAME="${IMAGE_NAME:-omniface}"
IMAGE_TAG="${IMAGE_TAG:-latest}"
ONNX_VERSION="${ONNX_VERSION:-1.20.1}"
DOCKERHUB_USER="${DOCKERHUB_USER:-}"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

info()  { echo -e "${GREEN}[INFO]${NC}  $*"; }
warn()  { echo -e "${YELLOW}[WARN]${NC}  $*"; }
error() { echo -e "${RED}[ERROR]${NC} $*"; exit 1; }

# 检查 Docker 是否可用
check_docker() {
    if ! command -v docker &>/dev/null; then
        error "未找到 Docker，请先安装 Docker"
    fi
    if ! docker info &>/dev/null; then
        error "Docker 未运行或权限不足，请启动 Docker 服务"
    fi
}

# 构建镜像
build_image() {
    info "开始构建镜像: ${IMAGE_NAME}:${IMAGE_TAG}"
    info "ONNX Runtime 版本: ${ONNX_VERSION}"
    echo ""

    cd "$PROJECT_DIR"
    docker build \
        --build-arg ONNX_VERSION="${ONNX_VERSION}" \
        --tag "${IMAGE_NAME}:${IMAGE_TAG}" \
        --file Dockerfile \
        .

    echo ""
    info "镜像构建成功: ${IMAGE_NAME}:${IMAGE_TAG}"

    # 显示镜像信息
    docker image inspect "${IMAGE_NAME}:${IMAGE_TAG}" \
        --format '  大小: {{.Size}} | 创建时间: {{.Created}}' 2>/dev/null || true
}

# 导出为 tar.gz（用于离线分发）
export_image() {
    local output_file="${PROJECT_DIR}/omniface-v${IMAGE_TAG}-docker.tar.gz"

    if [[ -f "$output_file" ]]; then
        warn "文件已存在: ${output_file}"
        read -r -p "是否覆盖？[y/N] " yn
        if [[ ! "$yn" =~ ^[Yy]$ ]]; then
            info "已取消"
            return
        fi
    fi

    info "导出镜像..."
    docker save "${IMAGE_NAME}:${IMAGE_TAG}" | gzip > "$output_file"

    local size
    size=$(du -h "$output_file" | cut -f1)
    info "导出完成: ${output_file} (${size})"
    echo ""
    echo "Windows 用户使用方式:"
    echo "  1. 将 ${output_file##*/} 和 docker-compose.yml 复制到 Windows"
    echo "  2. 在 Windows 终端中运行:"
    echo "     docker load < omniface-v${IMAGE_TAG}-docker.tar.gz"
    echo "     docker compose up -d"
    echo "  3. 打开浏览器访问 http://localhost:8080"
}

# 推送到 Docker Hub
push_image() {
    if [[ -z "$DOCKERHUB_USER" ]]; then
        error "请设置 DOCKERHUB_USER 环境变量: DOCKERHUB_USER=yourname ./docker/build.sh push"
    fi

    local remote="${DOCKERHUB_USER}/${IMAGE_NAME}:${IMAGE_TAG}"
    info "推送镜像: ${remote}"

    docker tag "${IMAGE_NAME}:${IMAGE_TAG}" "$remote"
    docker push "$remote"

    info "推送完成: ${remote}"
    echo ""
    echo "用户使用方式:"
    echo "  docker pull ${remote}"
    echo "  docker compose up -d"
}

# 显示使用帮助
show_help() {
    echo "OmniFace Docker 构建脚本"
    echo ""
    echo "用法: docker/build.sh [命令]"
    echo ""
    echo "命令:"
    echo "  build     构建镜像（默认）"
    echo "  export    构建并导出为 .tar.gz（离线分发用）"
    echo "  push      构建并推送到 Docker Hub"
    echo "  help      显示此帮助"
    echo ""
    echo "环境变量:"
    echo "  IMAGE_NAME        镜像名称（默认: omniface）"
    echo "  IMAGE_TAG         镜像标签（默认: latest）"
    echo "  ONNX_VERSION      ONNX Runtime 版本（默认: 1.20.1）"
    echo "  DOCKERHUB_USER    Docker Hub 用户名（push 时需要）"
    echo ""
    echo "示例:"
    echo "  ./docker/build.sh                      # 构建"
    echo "  ./docker/build.sh export               # 构建并导出离线包"
    echo "  IMAGE_TAG=v1.0.0 ./docker/build.sh export  # 指定版本号导出"
}

# ---- 主流程 ----
check_docker

case "${1:-build}" in
    build)
        build_image
        ;;
    export)
        build_image
        export_image
        ;;
    push)
        build_image
        push_image
        ;;
    help|--help|-h)
        show_help
        ;;
    *)
        error "未知命令: $1，使用 'help' 查看帮助"
        ;;
esac
