#!/usr/bin/env python3
"""Penguin Dash logo generator — menu_title.png (512x256) + splash_1.png (512x512).

Run from the repo root; regenerates the two logo textures in data/textures/.

Style keyed to the game: pc_20.ttf wordmark, orange/blue two-tone like the
original ETR logo, thick white strokes, soft slate-blue drop shadow, and a
simple belly-sliding penguin on a snow swoosh. Everything drawn at 4x and
downsampled for clean edges.
"""
from PIL import Image, ImageDraw, ImageFont, ImageFilter
import math

FONT = "data/fonts/pc_20.ttf"
SS = 4  # supersample factor

ORANGE_TOP, ORANGE_BOT = (255, 214, 92), (232, 118, 22)
BLUE_TOP, BLUE_BOT = (120, 190, 245), (18, 74, 158)
SHADOW = (38, 64, 110)


def gradient_text(text, px, top, bot, stroke_px, rot=0.0):
    """Render `text` with vertical gradient fill + white stroke + shadow.
    Returns an RGBA layer cropped to content (at supersample scale)."""
    font = ImageFont.truetype(FONT, px)
    pad = stroke_px * 3 + px // 3
    d = ImageDraw.Draw(Image.new("RGBA", (1, 1)))
    x0, y0, x1, y1 = d.textbbox((0, 0), text, font=font, stroke_width=stroke_px)
    w, h = x1 - x0 + 2 * pad, y1 - y0 + 2 * pad
    org = (pad - x0, pad - y0)

    # white stroke + fill mask
    base = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    ImageDraw.Draw(base).text(org, text, font=font, fill=(255, 255, 255, 255),
                              stroke_width=stroke_px, stroke_fill=(255, 255, 255, 255))
    fill_mask = Image.new("L", (w, h), 0)
    ImageDraw.Draw(fill_mask).text(org, text, font=font, fill=255)

    grad = Image.new("RGBA", (w, h))
    gpx = grad.load()
    ys = fill_mask.getbbox()
    gy0, gy1 = (ys[1], ys[3]) if ys else (0, h)
    for y in range(h):
        t = min(max((y - gy0) / max(gy1 - gy0, 1), 0.0), 1.0)
        c = tuple(int(a + (b - a) * t) for a, b in zip(top, bot))
        for x in range(w):
            gpx[x, y] = c + (255,)
    base.paste(grad, (0, 0), fill_mask)

    # soft shadow behind
    sh = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    ImageDraw.Draw(sh).text((org[0] + stroke_px * 1.2, org[1] + stroke_px * 1.6),
                            text, font=font, fill=SHADOW + (170,),
                            stroke_width=stroke_px, stroke_fill=SHADOW + (170,))
    sh = sh.filter(ImageFilter.GaussianBlur(stroke_px * 0.9))
    out = Image.alpha_composite(sh, base)
    if rot:
        out = out.rotate(rot, expand=True, resample=Image.BICUBIC)
    return out.crop(out.getbbox())


def draw_penguin(size):
    """Belly-sliding penguin, facing right, drawn from ellipses. RGBA layer."""
    s = size
    im = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    BLACK = (24, 24, 28, 255)
    WHITE = (250, 250, 252, 255)
    ORANGE = (255, 170, 40, 255)
    ORANGE_D = (225, 130, 20, 255)

    # body: long low ellipse
    d.ellipse((s*0.06, s*0.42, s*0.78, s*0.80), fill=BLACK)
    # head: round, front-right, slightly higher
    d.ellipse((s*0.58, s*0.28, s*0.90, s*0.60), fill=BLACK)
    # belly
    d.ellipse((s*0.14, s*0.52, s*0.66, s*0.79), fill=WHITE)
    # cheek patch
    d.ellipse((s*0.66, s*0.38, s*0.86, s*0.56), fill=WHITE)
    # eye
    d.ellipse((s*0.735, s*0.375, s*0.795, s*0.435), fill=WHITE)
    d.ellipse((s*0.757, s*0.395, s*0.787, s*0.425), fill=BLACK)
    # beak: triangle pointing right
    d.polygon([(s*0.86, s*0.44), (s*1.00, s*0.485), (s*0.86, s*0.53)], fill=ORANGE)
    # wing: swept back along the body
    d.ellipse((s*0.16, s*0.40, s*0.52, s*0.56), fill=(40, 40, 46, 255))
    # feet trailing behind
    d.ellipse((s*0.00, s*0.60, s*0.16, s*0.70), fill=ORANGE_D)
    d.ellipse((s*0.02, s*0.68, s*0.18, s*0.78), fill=ORANGE)
    return im


def snow_swoosh(w, h):
    """White motion swoosh the penguin rides on."""
    im = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.ellipse((0, h*0.30, w, h*0.95), fill=(255, 255, 255, 235))
    d.ellipse((w*0.06, h*0.05, w*0.75, h*0.65), fill=(214, 232, 248, 200))
    d.ellipse((w*0.12, h*0.28, w*0.66, h*0.62), fill=(255, 255, 255, 235))
    return im.filter(ImageFilter.GaussianBlur(h*0.05))


def speed_lines(w, h, n=3):
    im = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    for i in range(n):
        y = h * (0.25 + 0.22 * i)
        d.rounded_rectangle((0, y, w * (0.55 - 0.12 * i), y + h*0.07),
                            radius=h*0.035, fill=(255, 255, 255, 210))
    return im


def make_menu_title():
    W, H = 512 * SS, 256 * SS
    canvas = Image.new("RGBA", (W, H), (0, 0, 0, 0))

    peng_zone = int(H * 0.98)
    sw = snow_swoosh(int(peng_zone * 1.35), int(peng_zone * 0.5))
    peng = draw_penguin(peng_zone).rotate(14, expand=True, resample=Image.BICUBIC)

    word1 = gradient_text("Penguin", int(H * 0.34), ORANGE_TOP, ORANGE_BOT,
                          stroke_px=int(H * 0.024), rot=3)
    word2 = gradient_text("Dash!", int(H * 0.46), BLUE_TOP, BLUE_BOT,
                          stroke_px=int(H * 0.026), rot=3)

    # layout: penguin left, words right
    lines = speed_lines(int(W*0.28), int(H*0.30))
    canvas.alpha_composite(lines, (int(W*0.005), int(H*0.42)))
    canvas.alpha_composite(sw, (int(-W*0.02), int(H*0.52)))
    canvas.alpha_composite(peng, (int(W*0.02), int(H*0.05)))
    canvas.alpha_composite(word1, (int(W*0.40), int(H*0.08)))
    canvas.alpha_composite(word2, (int(W*0.44), int(H*0.42)))

    return canvas.resize((512, 256), Image.LANCZOS)


def draw_flag(size):
    """Tiny course flag: yellow pole, red pennant."""
    im = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.line((size*0.5, size*0.15, size*0.42, size*0.95), fill=(232, 198, 60, 255),
           width=max(2, size // 12))
    d.polygon([(size*0.5, size*0.15), (size*0.05, size*0.32), (size*0.47, size*0.45)],
              fill=(212, 40, 34, 255))
    return im


def make_splash():
    W, H = 512 * SS, 512 * SS
    canvas = Image.new("RGBA", (W, H), (0, 0, 0, 0))

    # big snowball backdrop like the original splash
    ball = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(ball)
    cx, cy, r = W // 2, int(H * 0.55), int(W * 0.415)
    d.ellipse((cx - r, cy - r, cx + r, cy + r), fill=(240, 246, 252, 255))
    # shading: darker bottom-left crescent + texture arcs
    sh = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    ds = ImageDraw.Draw(sh)
    ds.ellipse((cx - r, cy - r, cx + r, cy + r), fill=(176, 200, 226, 255))
    ds.ellipse((cx - r + int(r*0.28), cy - r - int(r*0.18), cx + r + int(r*0.3), cy + r - int(r*0.12)),
               fill=(240, 246, 252, 255))
    sh = sh.filter(ImageFilter.GaussianBlur(W * 0.012))
    mask = Image.new("L", (W, H), 0)
    ImageDraw.Draw(mask).ellipse((cx - r, cy - r, cx + r, cy + r), fill=255)
    ball.paste(sh, (0, 0), mask)
    for fr, wdt in [(0.62, 0.012), (0.78, 0.010), (0.9, 0.008)]:
        rr = int(r * fr)
        ImageDraw.Draw(ball).arc((cx - r, cy - int(rr*0.9), cx + r, cy + rr),
                                 start=15, end=168,
                                 fill=(198, 216, 236, 255), width=int(W * wdt))
    canvas.alpha_composite(ball)

    # course flags planted on the upper rim (base sits on the circle)
    for fs, ang, frot in [(0.10, 138, 22), (0.085, 108, 6),
                          (0.09, 66, -8), (0.10, 40, -24)]:
        f = draw_flag(int(H * fs)).rotate(frot, expand=True, resample=Image.BICUBIC)
        bx = cx + r * math.cos(math.radians(ang))
        by = cy - r * math.sin(math.radians(ang))
        canvas.alpha_composite(f, (int(bx - f.width * 0.45), int(by - f.height * 0.82)))

    # sliding trail behind (left of) the penguin, hugging the ball's crown
    trail = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    ImageDraw.Draw(trail).arc((cx - int(r*0.80), cy - int(r*0.92), cx + int(r*0.80), cy + int(r*0.7)),
                              start=200, end=252, fill=(255, 255, 255, 235),
                              width=int(W * 0.018))
    canvas.alpha_composite(trail.filter(ImageFilter.GaussianBlur(W * 0.005)))

    peng_zone = int(H * 0.46)
    peng = draw_penguin(peng_zone).rotate(16, expand=True, resample=Image.BICUBIC)
    canvas.alpha_composite(peng, (int(W*0.27), int(H*0.085)))

    word1 = gradient_text("Penguin", int(H * 0.155), ORANGE_TOP, ORANGE_BOT,
                          stroke_px=int(H * 0.012), rot=3)
    word2 = gradient_text("Dash!", int(H * 0.20), BLUE_TOP, BLUE_BOT,
                          stroke_px=int(H * 0.013), rot=3)
    canvas.alpha_composite(word1, ((W - word1.width) // 2, int(H * 0.545)))
    canvas.alpha_composite(word2, ((W - word2.width) // 2, int(H * 0.70)))

    return canvas.resize((512, 512), Image.LANCZOS)


if __name__ == "__main__":
    out = "data/textures"
    make_menu_title().save(f"{out}/menu_title.png")
    make_splash().save(f"{out}/splash_1.png")
    print("done")
