#!/usr/bin/env python3
"""Cut the supporting cast out of frames of the film (assets/src/frame-*.png, from the
Wikimedia Commons 1080p copy of Animal Crackers): Margaret Dumont and Zeppo from the
waist up, and two faces from the crowd for the chorus, as paper ovals. They stand
behind a hedge at the back of the stage, so nothing below the waist is needed.

Each is cut in two at the mouth, as Groucho is: the top of the head, which flaps
open on a hinge at the side of the face when they sing, and the rest, with a dark
mouth behind. The white paper edge runs round the outline only. Writes
assets/art/cast_*.png and src/cast.h.

    tools/make_cast.py
"""
import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageOps

OUT, HDR = "assets/art", "src/cast.h"
BORDER = 4
# name, frame, outline (a polygon, or an oval as a box), the mouth's height, the
# hinge's x, and the top of the head (for scale): all in the frame's pixels
CAST = [
    ("dumont", "436.5", [(1110, 85), (1160, 72), (1205, 82), (1235, 110), (1250, 150), (1245, 190), (1232, 215),
                         (1240, 235), (1280, 262), (1305, 300), (1320, 345), (1330, 400), (1335, 460), (1330, 520),
                         (1320, 600), (1040, 600), (1035, 520), (1035, 440), (1033, 380), (1040, 330), (1055, 300),
                         (1085, 282), (1100, 262), (1105, 235), (1098, 200), (1094, 150), (1100, 105)], 205, 1100, 72),
    ("zeppo", "317.5", [(720, 250), (770, 245), (800, 262), (805, 300), (800, 340), (795, 380), (785, 400), (800, 405),
                        (860, 425), (870, 460), (880, 520), (888, 600), (870, 700), (600, 700), (578, 600), (575, 520), (580, 450), (600, 420),
                        (650, 400), (690, 395), (700, 380), (700, 330), (705, 280)], 374, 705, 245),
    ("woman", "475", (995, 252, 1125, 448), 426, 1000, 252),
    ("man", "475", (1222, 158, 1380, 392), 346, 1228, 158),
]

def main():
    lines = ["/* Made by tools/make_cast.py: the supporting cast, each in two pieces cut at the",
             " * mouth. For each: where the hinge is in its top piece and in its body piece, and",
             " * the height of its head (hair to mouth), in the pictures' pixels. */",
             "typedef struct { double top_x, top_y, body_x, body_y, head; } Cast;",
             "static const Cast CAST[] = {"]
    for name, frame, shape, mouth, hinge_x, crown in CAST:
        photo = ImageOps.autocontrast(Image.open(f"assets/src/frame-{frame}.png").convert("L"), cutoff=0.5)
        W, H = photo.size
        tint = Image.merge("RGB", [photo.point(lambda v: min(255, v * 1.04 + 6)), photo.point(lambda v: v * 0.98 + 2), photo.point(lambda v: v * 0.88)])
        rgb = np.asarray(tint)
        m = Image.new("L", (W, H), 0)
        d = ImageDraw.Draw(m)
        if isinstance(shape, tuple): d.ellipse(shape, fill=255)
        else: d.polygon(shape, fill=255)
        whole = np.asarray(m) > 0
        ys = np.arange(H)[:, None].repeat(W, 1)
        xs = np.arange(W)[None, :].repeat(H, 0)
        # the mouth: a dark half-oval in the middle of the face, behind the top
        cols = np.nonzero(whole[mouth])[0]
        x0, x1 = cols.min(), cols.max()
        cx, rx = (x0 + x1) / 2, (x1 - x0) * 0.28
        mouth_in = whole & (((xs - cx) / rx) ** 2 + ((ys - mouth) / 24.0) ** 2 < 1) & (ys < mouth - 3)
        lip = whole & (ys >= mouth - 3) & (ys < mouth)
        parts = {"top": whole & (ys < mouth), "body": (whole & (ys >= mouth)) | mouth_in | lip}
        boxes = {}
        for part, pm in parts.items():
            edge = Image.fromarray((pm * 255).astype(np.uint8)).filter(ImageFilter.MaxFilter(2 * BORDER + 1))
            e = (np.asarray(edge) > 0) & ~(whole & ~pm)            # edged on the outline only
            if part == "body": e &= ~(ys < mouth)                      # not round the mouth
            if part == "top": e &= ~(ys >= mouth)
            img = np.zeros((H, W, 4), np.uint8)
            img[e] = (250, 246, 236, 255)
            img[pm, :3] = rgb[pm]
            img[pm, 3] = 255
            if part == "body": img[mouth_in, :3] = (52, 16, 18)
            yy, xx = np.nonzero(img[..., 3])
            bx0, by0, bx1, by1 = xx.min(), yy.min(), xx.max() + 1, yy.max() + 1
            Image.fromarray(img[by0:by1, bx0:bx1], "RGBA").save(f"{OUT}/cast_{name}_{part}.png", optimize=True)
            boxes[part] = (bx0, by0)
            print(f"cast_{name}_{part}.png {bx1 - bx0}x{by1 - by0}")
        tx, ty = hinge_x - boxes["top"][0], mouth - boxes["top"][1]
        bx, by = hinge_x - boxes["body"][0], mouth - boxes["body"][1]
        lines.append(f"    /* {name:7} */ {{{tx}, {ty}, {bx}, {by}, {mouth - crown}}},")
    lines += ["};", ""]
    open(HDR, "w").write("\n".join(lines))
    print(HDR)

if __name__ == "__main__":
    main()
