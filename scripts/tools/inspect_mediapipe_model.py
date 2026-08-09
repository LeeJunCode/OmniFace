"""Inspect MediaPipe Face Landmarker ONNX model to determine score output range.

Also test with face-like synthetic patterns, gradients, and natural images.
"""
import numpy as np
import onnxruntime as ort
import sys
import os
import cv2

project_root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
model_path = os.path.join(project_root, "models", "mediapipe", "face_mesh_Nx3x192x192_post.onnx")

print(f"Loading model: {model_path}")
session = ort.InferenceSession(model_path, providers=['CPUExecutionProvider'])

print("\n=== MODEL INPUTS ===")
for inp in session.get_inputs():
    print(f"  {inp.name}: shape={inp.shape}, type={inp.type}")

print("\n=== MODEL OUTPUTS ===")
for out in session.get_outputs():
    print(f"  {out.name}: shape={out.shape}, type={out.type}")


def run_inference(img_rgb_uint8, x1=0, y1=0, w=192, h=192):
    """Run model inference and return score."""
    # Resize to 192x192
    img = cv2.resize(img_rgb_uint8, (192, 192))
    # Convert to float and normalize to [0, 1] or similar
    img_f = img.astype(np.float32) / 255.0  # Standard normalization
    img_tensor = img_f.transpose(2, 0, 1)[np.newaxis, ...]  # NCHW

    inputs = {
        'input': img_tensor.astype(np.float32),
        'crop_x1': np.array([[x1]], dtype=np.int32),
        'crop_y1': np.array([[y1]], dtype=np.int32),
        'crop_width': np.array([[w]], dtype=np.int32),
        'crop_height': np.array([[h]], dtype=np.int32),
    }
    outputs = session.run(None, inputs)
    return float(outputs[0].flatten()[0])


# Test 1: Random noise
print("\n=== TEST 1: Random noise (no face) ===")
scores = []
for _ in range(20):
    img = (np.random.rand(192, 192, 3) * 255).astype(np.uint8)
    scores.append(run_inference(img))
scores = np.array(scores)
print(f"  Range: [{scores.min():.4f}, {scores.max():.4f}], Mean: {scores.mean():.4f}")

# Test 2: Solid color images
print("\n=== TEST 2: Solid colors ===")
for color_name, color in [("black", (0,0,0)), ("white", (255,255,255)),
                           ("gray", (128,128,128)), ("skin", (200,170,140))]:
    img = np.full((192, 192, 3), color, dtype=np.uint8)
    s = run_inference(img)
    print(f"  {color_name}: score = {s:.4f}")

# Test 3: Gradient images
print("\n=== TEST 3: Gradients ===")
for name in ["horizontal", "vertical", "radial"]:
    x = np.linspace(0, 1, 192)
    y = np.linspace(0, 1, 192)
    if name == "horizontal":
        g = np.tile(x, (192, 1))
    elif name == "vertical":
        g = np.tile(y[:, None], (1, 192))
    else:
        xx, yy = np.meshgrid(x, y)
        g = np.sqrt((xx-0.5)**2 + (yy-0.5)**2)
        g = 1.0 - g / g.max()
    img = (g[:, :, None] * np.array([200, 170, 140]) + (1-g[:, :, None]) * 128).astype(np.uint8)
    s = run_inference(img)
    print(f"  {name} gradient: score = {s:.4f}")

# Test 4: Face-like oval pattern
print("\n=== TEST 4: Synthetic face-like pattern ===")
xx, yy = np.meshgrid(np.linspace(-1, 1, 192), np.linspace(-1, 1, 192))
face_mask = ((xx*1.3)**2 + yy**2) < 0.5  # oval
# Eye regions
left_eye = ((xx+0.25)**2 + (yy+0.15)**2) < 0.02
right_eye = ((xx-0.25)**2 + (yy+0.15)**2) < 0.02
# Mouth region
mouth = ((xx*1.5)**2 + (yy-0.35)**2) < 0.03

img = np.full((192, 192, 3), 128, dtype=np.uint8)
img[face_mask] = [210, 170, 140]  # skin color
img[left_eye] = [60, 40, 30]     # dark eyes
img[right_eye] = [60, 40, 30]
img[mouth] = [120, 70, 60]       # dark mouth
s = run_inference(img)
print(f"  Simple face pattern: score = {s:.4f}")

# Test 5: Try with different normalizations
print("\n=== TEST 5: Effect of input normalization ===")
img = np.full((192, 192, 3), 200, dtype=np.uint8)
for norm_name, norm_fn in [
    ("/255.0", lambda x: x.astype(np.float32) / 255.0),
    ("/127.5-1.0", lambda x: x.astype(np.float32) / 127.5 - 1.0),
    ("no norm (uint8 as float)", lambda x: x.astype(np.float32)),
    ("float [0,1] random", lambda x: np.random.rand(*x.shape).astype(np.float32)),
]:
    img_f = norm_fn(img)
    if len(img_f.shape) == 3:
        img_tensor = img_f.transpose(2, 0, 1)[np.newaxis, ...]
    else:
        img_tensor = img_f
    inputs = {
        'input': img_tensor.astype(np.float32),
        'crop_x1': np.array([[0]], dtype=np.int32),
        'crop_y1': np.array([[0]], dtype=np.int32),
        'crop_width': np.array([[192]], dtype=np.int32),
        'crop_height': np.array([[192]], dtype=np.int32),
    }
    outputs = session.run(None, inputs)
    s = float(outputs[0].flatten()[0])
    print(f"  {norm_name}: score = {s:.4f}")

# Test 6: Check if model expects specific preprocessing
print("\n=== TEST 6: Various normalization ranges ===")
for scale, shift, label in [
    (1.0/255.0, 0.0, "[0,1]"),
    (1.0/127.5, -1.0, "[-1,1]"),
    (1.0, -127.5/127.5, "[-1,1] alt"),
    (2.0/255.0, -1.0, "[-1,1] with 2/255"),
]:
    img = np.full((192, 192, 3), 200, dtype=np.uint8)
    img_f = img.astype(np.float32) * scale + shift
    img_tensor = img_f.transpose(2, 0, 1)[np.newaxis, ...]
    inputs = {
        'input': img_tensor.astype(np.float32),
        'crop_x1': np.array([[0]], dtype=np.int32),
        'crop_y1': np.array([[0]], dtype=np.int32),
        'crop_width': np.array([[192]], dtype=np.int32),
        'crop_height': np.array([[192]], dtype=np.int32),
    }
    outputs = session.run(None, inputs)
    s = float(outputs[0].flatten()[0])
    print(f"  {label}: score = {s:.4f}")

# Test 7: Try reading a real image if available
print("\n=== TEST 7: Try reading sample images ===")
sample_dirs = [
    os.path.join(project_root, "samples"),
    os.path.join(project_root, "data"),
    os.path.join(project_root, "test"),
    os.path.join(os.path.expanduser("~"), "Pictures"),
]
found_image = False
for d in sample_dirs:
    if os.path.isdir(d):
        for f in os.listdir(d)[:5]:
            fpath = os.path.join(d, f)
            if f.lower().endswith(('.jpg', '.jpeg', '.png', '.bmp')):
                try:
                    img = cv2.imread(fpath)
                    if img is not None and img.shape[0] > 50 and img.shape[1] > 50:
                        img = cv2.cvtColor(img, cv2.COLOR_BGR2RGB)
                        s = run_inference(img, 0, 0, img.shape[1], img.shape[0])
                        print(f"  {f}: score = {s:.4f} (size={img.shape[1]}x{img.shape[0]})")
                        found_image = True
                except:
                    pass
        if found_image:
            break
if not found_image:
    print("  No sample images found")
