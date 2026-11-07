#!/usr/bin/env python3
"""Generates every Delquen logo asset from code (needs: pip install pillow numpy).

Outputs (relative to the repository root):
  delquen.ico            multi-size application icon (embedded in the exe by delquen.rc)
  delquen_wizard.bmp     installer welcome image (164x314)
  delquen_small.bmp      installer small image (55x55)
  docs/logo.png          512x512 logo
  docs/banner.png        README banner
  docs/social-preview.png   GitHub social preview (1280x640)
The vector version is docs/logo.svg (same geometry, edited by hand).
"""
import io, os, struct, math, random
import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DOCS = os.path.join(ROOT, "docs")
os.makedirs(DOCS, exist_ok=True)

BG1, BG2 = (0x1E, 0x2A, 0x52), (0x0B, 0x10, 0x22)      # badge background (top-left -> bottom-right)
C1, C2 = (0x22, 0xD3, 0xEE), (0x4A, 0xDE, 0x80)         # cyan -> green (FPS "good")
FONT_DIR = "/usr/share/fonts/truetype/google-fonts/"


def font(name, size):
    for p in (FONT_DIR + name, "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"):
        try:
            return ImageFont.truetype(p, size)
        except OSError:
            pass
    return ImageFont.load_default()


def grad(w, h, c1, c2, box=None):
    """Diagonal gradient (top-left c1 -> bottom-right c2) over `box` = (x0, y0, x1, y1) in pixels."""
    x0, y0, x1, y1 = box or (0, 0, w, h)
    yy, xx = np.mgrid[0:h, 0:w]
    t = (np.clip((xx - x0) / max(1, x1 - x0), 0, 1) + np.clip((yy - y0) / max(1, y1 - y0), 0, 1)) / 2
    a = np.zeros((h, w, 3))
    for i in range(3):
        a[..., i] = c1[i] * (1 - t) + c2[i] * t
    return Image.fromarray(a.astype("uint8"), "RGB")


def render_logo(small=False):
    """1024x1024 RGBA logo (256-unit design space, 4x supersampled)."""
    k = 4
    N = 256 * k
    img = Image.new("RGBA", (N, N), (0, 0, 0, 0))

    # badge
    m = Image.new("L", (N, N), 0)
    ImageDraw.Draw(m).rounded_rectangle([8 * k, 8 * k, 248 * k, 248 * k], radius=58 * k, fill=255)
    bg = grad(N, N, BG1, BG2).convert("RGBA")
    bg.putalpha(m)
    img.alpha_composite(bg)
    bd = Image.new("RGBA", (N, N), (0, 0, 0, 0))
    ImageDraw.Draw(bd).rounded_rectangle([8 * k, 8 * k, 248 * k, 248 * k], radius=58 * k,
                                         outline=(0x3B, 0x4C, 0x8A, 115), width=k)
    img.alpha_composite(bd)

    # the "D": stem + top/bottom bars + half ring, drawn as one thick stroke
    if small:
        stem, top, bot, cx, sw = 80, 62, 194, 114, 40
    else:
        stem, top, bot, cx, sw = 86, 70, 186, 118, 30
    r = (bot - top) / 2
    cy = (top + bot) / 2
    w = sw * k
    hw = sw / 2
    dm = Image.new("L", (N, N), 0)
    d = ImageDraw.Draw(dm)
    P = lambda x, y: (x * k, y * k)
    d.line([P(stem, top), P(cx, top)], fill=255, width=w)
    d.line([P(stem, bot), P(cx, bot)], fill=255, width=w)
    d.line([P(stem, top), P(stem, bot)], fill=255, width=w)
    outer, inner = [], []
    for a in range(-90, 91):
        ar = math.radians(a)
        outer.append(P(cx + (r + hw) * math.cos(ar), cy + (r + hw) * math.sin(ar)))
        inner.append(P(cx + (r - hw) * math.cos(ar), cy + (r - hw) * math.sin(ar)))
    d.polygon(outer + inner[::-1], fill=255)
    for (x, y) in ((stem, top), (stem, bot)):                      # rounded outer corners
        d.ellipse([x * k - w / 2, y * k - w / 2, x * k + w / 2, y * k + w / 2], fill=255)

    # glow + shadow
    glow = dm.filter(ImageFilter.GaussianBlur(9 * k)).point(lambda v: int(v * 0.55))
    g = Image.new("RGBA", (N, N), C1 + (0,))
    g.putalpha(glow)
    img.alpha_composite(g)
    sh = Image.new("L", (N, N), 0)
    sh.paste(dm, (0, 6 * k))
    sh = sh.filter(ImageFilter.GaussianBlur(6 * k)).point(lambda v: int(v * 0.45))
    s = Image.new("RGBA", (N, N), (0, 0, 0, 0))
    s.putalpha(sh)
    img.alpha_composite(s)

    # gradient fill of the D
    box = ((stem - hw) * k, (top - hw) * k, (cx + r + hw) * k, (bot + hw) * k)
    fill = grad(N, N, C1, C2, tuple(int(v) for v in box)).convert("RGBA")
    fill.putalpha(dm)
    img.alpha_composite(fill)

    # frame-time pulse inside the bowl (large sizes only)
    if not small:
        pts = [(104, 128), (120, 128), (129, 102), (139, 154), (148, 128), (156, 128)]
        pm = Image.new("L", (N, N), 0)
        pd = ImageDraw.Draw(pm)
        pw = 9 * k
        pd.line([P(*p) for p in pts], fill=255, width=pw, joint="curve")
        for p in (pts[0], pts[-1]):
            pd.ellipse([p[0] * k - pw / 2, p[1] * k - pw / 2, p[0] * k + pw / 2, p[1] * k + pw / 2], fill=255)
        white = Image.new("RGBA", (N, N), (255, 255, 255, 0))
        white.putalpha(pm)
        img.alpha_composite(white)
    return img


BIG, SMALL = render_logo(False), render_logo(True)


def logo(size):
    """Logo scaled to `size` px (simplified mark for tiny sizes)."""
    src = SMALL if size <= 48 else BIG
    return src.resize((size, size), Image.LANCZOS)


def png_bytes(im):
    b = io.BytesIO()
    im.save(b, "PNG", optimize=True)
    return b.getvalue()


def write_ico(path, sizes):
    entries = [(s, png_bytes(logo(s))) for s in sizes]
    n = len(entries)
    head = struct.pack("<HHH", 0, 1, n)
    offset = 6 + 16 * n
    body = b""
    for s, png in entries:
        wh = 0 if s >= 256 else s
        head += struct.pack("<BBBBHHII", wh, wh, 0, 0, 1, 32, len(png), offset + len(body))
        body += png
    with open(path, "wb") as f:
        f.write(head + body)


def waveform(draw, w, h, y, amp, color, seed=7, width=2):
    """Decorative frame-time trace."""
    rnd = random.Random(seed)
    pts, x = [], 0
    while x <= w:
        spike = rnd.random() < 0.07
        yy = y + (-amp * (1.6 + rnd.random()) if spike else rnd.uniform(-amp * 0.25, amp * 0.25))
        pts.append((x, yy))
        x += rnd.randint(14, 30)
    draw.line(pts, fill=color, width=width, joint="curve")


def vgrad(w, h, c1, c2):
    a = np.zeros((h, w, 3))
    t = np.linspace(0, 1, h)[:, None]
    for i in range(3):
        a[..., i] = c1[i] * (1 - t) + c2[i] * t
    return Image.fromarray(a.astype("uint8"), "RGB")


def banner(w, h, center=False):
    im = vgrad(w, h, (0x0B, 0x10, 0x22), (0x16, 0x20, 0x44)).convert("RGBA")
    ov = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    od = ImageDraw.Draw(ov)
    waveform(od, w, h, int(h * 0.915), h * 0.035, C1 + (60,), seed=3, width=3)
    waveform(od, w, h, int(h * 0.955), h * 0.03, C2 + (45,), seed=11, width=2)
    im.alpha_composite(ov)
    d = ImageDraw.Draw(im)
    lg = int(h * (0.50 if center else 0.62))
    title_f = font("Poppins-Bold.ttf", int(h * (0.22 if center else 0.34)))
    tag_f = font("Poppins-Regular.ttf", int(h * (0.062 if center else 0.098)))
    chip_f = font("Poppins-Medium.ttf", int(h * (0.045 if center else 0.07)))
    tag = "Lightweight FPS overlay for Windows"
    chips = "FPS  ·  1% Low  ·  Frame time  ·  CPU  ·  GPU  ·  RAM  ·  VRAM"
    if center:
        im.alpha_composite(logo(lg), ((w - lg) // 2, int(h * 0.10)))
        for text, f, y, col in (("Delquen", title_f, int(h * 0.64), (255, 255, 255)),
                                (tag, tag_f, int(h * 0.80), (0xAF, 0xBD, 0xE3)),
                                (chips, chip_f, int(h * 0.88), (0x7C, 0x8B, 0xB8))):
            tw = d.textlength(text, font=f)
            d.text(((w - tw) / 2, y), text, font=f, fill=col)
    else:
        x0 = int(h * 0.19)
        im.alpha_composite(logo(lg), (x0, (h - lg) // 2 - int(h * 0.04)))
        tx = x0 + lg + int(h * 0.12)
        d.text((tx, int(h * 0.17)), "Delquen", font=title_f, fill=(255, 255, 255))
        d.text((tx + 4, int(h * 0.60)), tag, font=tag_f, fill=(0xAF, 0xBD, 0xE3))
        d.text((tx + 4, int(h * 0.74)), chips, font=chip_f, fill=(0x7C, 0x8B, 0xB8))
    return im.convert("RGB")


def wizard():
    w, h = 164, 314
    im = vgrad(w, h, (0x16, 0x20, 0x44), (0x0B, 0x10, 0x22)).convert("RGBA")
    ov = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    waveform(ImageDraw.Draw(ov), w, h, h - 44, 8, C1 + (70,), seed=5, width=2)
    waveform(ImageDraw.Draw(ov), w, h, h - 30, 6, C2 + (55,), seed=9, width=2)
    im.alpha_composite(ov)
    lg = 104
    im.alpha_composite(logo(lg), ((w - lg) // 2, 46))
    d = ImageDraw.Draw(im)
    f1, f2 = font("Poppins-Bold.ttf", 26), font("Poppins-Regular.ttf", 12)
    for text, f, y, col in (("Delquen", f1, 166, (255, 255, 255)), ("FPS overlay", f2, 204, (0xAF, 0xBD, 0xE3))):
        d.text(((w - d.textlength(text, font=f)) / 2, y), text, font=f, fill=col)
    return im.convert("RGB")


def small_img():
    im = Image.new("RGBA", (55, 55), (255, 255, 255, 255))
    im.alpha_composite(logo(47), (4, 4))
    return im.convert("RGB")


write_ico(os.path.join(ROOT, "delquen.ico"), [16, 20, 24, 32, 40, 48, 64, 128, 256])
wizard().save(os.path.join(ROOT, "delquen_wizard.bmp"))
small_img().save(os.path.join(ROOT, "delquen_small.bmp"))
logo(512).save(os.path.join(DOCS, "logo.png"))
banner(1280, 320).save(os.path.join(DOCS, "banner.png"))
banner(1280, 640, center=True).save(os.path.join(DOCS, "social-preview.png"))
for s in (16, 32, 64):                       # previews of the small sizes (not committed; for checking)
    logo(s).resize((s * 6, s * 6), Image.NEAREST).save(f"/tmp/preview_{s}.png")
print("ok")
