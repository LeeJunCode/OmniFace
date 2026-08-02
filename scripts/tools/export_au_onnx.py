"""
将 OpenGprahAU MEFARG (ResNet-18, stage 1) 模型导出为 ONNX 格式。

模型来源: third_party/OpenGraphAU/model/ANFL.py
输出: models/OpenGprahAU/mefarg_resnet18_au.onnx
"""

import sys
import os
import argparse
import numpy as np
from collections import OrderedDict

import torch

# 将 third_party/OpenGraphAU 加入路径, 以便导入 model 模块
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'third_party', 'OpenGraphAU'))

# 修补 resnet18 必须在导入 ANFL 之前:
# model/__init__.py 和 model/ANFL.py 都通过 `from .resnet import resnet18` 获取函数引用,
# 在 ANFL 导入前将 resnet18 替换为 pretrained=False 的版本, 避免查找不存在的预训练权重文件。
import model.resnet as resnet_module

_orig_resnet18 = resnet_module.resnet18


def _resnet18_no_pretrain(**kwargs):
    return _orig_resnet18(pretrained=False, **kwargs)


resnet_module.resnet18 = _resnet18_no_pretrain

# stage 1 使用 ANFL 中的 MEFARG (不带 GEM edge extractor)
from model.ANFL import MEFARG  # noqa: E402


def strip_module_prefix(state_dict: dict) -> OrderedDict:
    """去除 DataParallel 包装产生的 'module.' 前缀"""
    new_sd = OrderedDict()
    for k, v in state_dict.items():
        name = k[7:] if k.startswith('module.') else k
        new_sd[name] = v
    return new_sd


def export_model(checkpoint_path: str, output_path: str, opset: int = 17) -> None:
    # ---------- 1. 创建模型 ----------
    print(f"[1/5] 创建 MEFARG 模型 (ResNet-18 backbone)...")
    model = MEFARG(
        num_main_classes=27,
        num_sub_classes=14,
        backbone='resnet18',
        neighbor_num=4,
        metric='dots',
    )

    # ---------- 2. 加载 checkpoint ----------
    print(f"[2/5] 加载 checkpoint: {checkpoint_path}")
    ckpt = torch.load(checkpoint_path, map_location='cpu')
    state_dict = strip_module_prefix(ckpt['state_dict'])
    epoch = ckpt.get('epoch', 'unknown')
    print(f"       checkpoint epoch: {epoch}")

    missing, unexpected = model.load_state_dict(state_dict, strict=False)
    if missing:
        print(f"       缺失的 key ({len(missing)}): {missing[:5]}...")
    if unexpected:
        print(f"       多余的 key ({len(unexpected)}): {unexpected[:5]}...")
    print(f"       加载完成")

    model.eval()

    # ---------- 3. 验证 PyTorch 输出 ----------
    print(f"[3/5] 验证 PyTorch 推理...")
    dummy_input = torch.randn(1, 3, 224, 224)
    with torch.no_grad():
        torch_output = model(dummy_input)
    print(f"       输出 shape: {torch_output.shape}")
    print(f"       输出范围: [{torch_output.min().item():.4f}, {torch_output.max().item():.4f}]")

    assert torch_output.shape == (1, 41), f"期望 shape (1, 41), 实际 {torch_output.shape}"

    # ---------- 4. 导出 ONNX ----------
    print(f"[4/5] 导出 ONNX (opset {opset})...")
    torch.onnx.export(
        model,
        dummy_input,
        output_path,
        input_names=['input'],
        output_names=['au_logits'],
        opset_version=opset,
        dynamic_axes={
            'input': {0: 'batch'},
            'au_logits': {0: 'batch'},
        },
        do_constant_folding=True,
        export_params=True,
    )
    print(f"       导出到: {output_path}")

    file_size = os.path.getsize(output_path) / (1024 * 1024)
    print(f"       文件大小: {file_size:.1f} MB")

    # ---------- 5. 验证 ONNX ----------
    print(f"[5/5] 验证 ONNX 模型...")
    import onnxruntime as ort

    session = ort.InferenceSession(output_path, providers=['CPUExecutionProvider'])

    # 检查输入
    for inp in session.get_inputs():
        print(f"       IN:  {inp.name}, shape={inp.shape}, type={inp.type}")
    for out in session.get_outputs():
        print(f"       OUT: {out.name}, shape={out.shape}, type={out.type}")

    # 推理对比
    onnx_output = session.run(None, {'input': dummy_input.numpy()})
    diff = np.abs(onnx_output[0] - torch_output.numpy())
    print(f"       max diff: {diff.max():.6f}")
    print(f"       mean diff: {diff.mean():.6f}")

    if diff.max() < 1e-4:
        print(f"\n*** ONNX 导出成功! max diff = {diff.max():.6f} < 1e-4 ***")
    else:
        print(f"\n*** 警告: max diff = {diff.max():.6f} >= 1e-4, 请检查导出质量 ***")

    # 验证 ONNX 结构
    try:
        import onnx
        onnx_model = onnx.load(output_path)
        onnx.checker.check_model(onnx_model)
        print(f"       ONNX 模型结构验证通过")
    except ImportError:
        print(f"       (跳过 onnx.checker: onnx 模块不可用)")


def main():
    project_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

    default_checkpoint = os.path.join(
        project_root, 'models', 'OpenGprahAU', 'OpenGprahAU-ResNet18_first_stage.pth'
    )
    default_output = os.path.join(
        project_root, 'models', 'OpenGprahAU', 'mefarg_resnet18_au.onnx'
    )

    parser = argparse.ArgumentParser(description='导出 OpenGprahAU 模型到 ONNX')
    parser.add_argument('--checkpoint', default=default_checkpoint, help='PyTorch checkpoint 路径')
    parser.add_argument('--output', default=default_output, help='ONNX 输出路径')
    parser.add_argument('--opset', type=int, default=17, help='ONNX opset 版本')
    args = parser.parse_args()

    if not os.path.exists(args.checkpoint):
        print(f"错误: checkpoint 不存在: {args.checkpoint}")
        sys.exit(1)

    os.makedirs(os.path.dirname(args.output), exist_ok=True)
    export_model(args.checkpoint, args.output, args.opset)


if __name__ == '__main__':
    main()
