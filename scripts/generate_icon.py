"""生成 OmniFace 应用图标 — 极简设计：O 形环 + 双眼 = 人脸"""
import struct
import zlib
import math
import os


def crc32(data):
    c = 0xFFFFFFFF
    for b in data:
        c ^= b
        for _ in range(8):
            c = (c >> 1) ^ 0xEDB88320 if c & 1 else c >> 1
    return c ^ 0xFFFFFFFF


def make_png_chunk(ctype, data):
    raw = ctype + data
    return struct.pack('>I', len(data)) + raw + struct.pack('>I', crc32(raw))


def create_png(w, h, pixels):
    sig = b'\x89PNG\r\n\x1a\n'
    ihdr = make_png_chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0))
    raw = bytearray()
    for y in range(h):
        raw.append(0)
        for x in range(w):
            raw.extend(pixels[y * w + x])
    idat = make_png_chunk(b'IDAT', zlib.compress(bytes(raw)))
    iend = make_png_chunk(b'IEND', b'')
    return sig + ihdr + idat + iend


def create_ico(sizes_pixels):
    n = len(sizes_pixels)
    header = struct.pack('<HHH', 0, 1, n)
    entries, body = bytearray(), bytearray()
    off = 6 + 16 * n
    for w, h, px in sizes_pixels:
        png = create_png(w, h, px)
        entries.extend(struct.pack('<BBBBHHII', w if w < 256 else 0,
            h if h < 256 else 0, 0, 0, 1, 32, len(png), off))
        body.extend(png)
        off += len(png)
    return bytes(header) + bytes(entries) + bytes(body)


def lerp_c(c1, c2, t):
    return tuple(int(c1[i] + (c2[i] - c1[i]) * t) for i in range(4))


def draw_icon(S):
    """极简 OmniFace 图标

    设计：
      - 深蓝圆角方形背景（象征专业/技术）
      - 白色圆环 = 字母 O（Omni）+ 人脸外轮廓
      - 环内上方两个圆点 = 眼睛（让 O 变成人脸符号）
      - 环下方一抹青色弧线 = 微笑 + 科技感扫描线
    """
    P = []
    cx = cy = S / 2
    R = S * 0.42          # 环外半径
    Ri = R * 0.64         # 环内半径

    # 颜色
    BG_IN  = (30,  55,  110, 255)
    BG_OUT = (12,  18,  42,  255)
    WHITE  = (248, 250, 255, 255)
    CYAN   = (70, 195, 240, 255)

    for y in range(S):
        for x in range(S):
            dx, dy = x - cx, y - cy
            d = math.sqrt(dx * dx + dy * dy)

            # 圆角方形遮罩
            sq = (abs(dx / (R * 1.35)) ** 3.5 + abs(dy / (R * 1.35)) ** 3.5) ** (1 / 3.5)
            if sq > 1.0:
                P.append((0, 0, 0, 0))
                continue

            # 渐变背景
            c = lerp_c(BG_IN, BG_OUT, min(d / (R * 1.3), 1.0))

            # 白色圆环 (O)
            if Ri < d < R:
                c = WHITE

            # 双眼
            eye_r = R * 0.115
            eye_y = cy - R * 0.06
            for ex in [cx - R * 0.26, cx + R * 0.26]:
                ed = math.sqrt((x - ex) ** 2 + (y - eye_y) ** 2)
                if ed < eye_r:
                    c = WHITE
                # 瞳孔
                if ed < eye_r * 0.42:
                    c = BG_OUT

            # 底部微笑弧线（在环内部）
            # 用二次曲线模拟：y = k*(x-cx)^2 + const
            arc_y0 = cy + R * 0.15
            k = 0.018 / R
            arc_y = k * (x - cx) ** 2 + arc_y0
            if abs(y - arc_y) < R * 0.06 and abs(x - cx) < R * 0.55:
                # 线条颜色：青色渐变
                c = CYAN

            P.append(c)
    return P


def main():
    d = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'resources')
    os.makedirs(d, exist_ok=True)

    sp = []
    for s in [16, 24, 32, 48, 64, 128, 256]:
        print(f"{s}x{s}")
        sp.append((s, s, draw_icon(s)))

    ip = os.path.join(d, 'omniface.ico')
    with open(ip, 'wb') as f:
        f.write(create_ico(sp))
    print(f"ICO: {ip}")

    pp = os.path.join(d, 'omniface_preview.png')
    with open(pp, 'wb') as f:
        f.write(create_png(256, 256, draw_icon(256)))
    print(f"PNG: {pp}")


if __name__ == '__main__':
    main()
