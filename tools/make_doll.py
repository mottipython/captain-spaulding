#!/usr/bin/env python3
"""Cut Captain Spaulding out of the 1930 publicity photo (assets/src/spaulding.jpg,
public domain) as a jointed paper doll: head, body, each arm (his sleeves: his hands
are behind his back), and each leg in two, thigh and shin, held together with brass
split pins at the neck, shoulders, hips and knees.

Each piece is cut along his outline (tools/outline.py), with a round tab at each pin
so that it can turn without a gap opening, a white paper border, and a light warm
tint, like a printed card. The pins themselves are drawn by the show, over the
pieces (sp_pin.png). Writes the pieces to assets/art/sp_*.png and where their
pins are to src/doll.h.

    tools/make_doll.py
"""
import numpy as np
from PIL import Image, ImageDraw, ImageEnhance, ImageFilter, ImageOps
from outline import OUTLINE

SRC, OUT, HDR = "assets/src/spaulding.jpg", "assets/art", "src/doll.h"
S = 800            # the outline is traced on the photo shrunk to this width
BORDER = 5         # the white paper edge, in pixels
# the pins, in the outline's coordinates
NECK, HIP_L, HIP_R, KNEE_L, KNEE_R = (382, 196), (318, 452), (444, 452), (282, 690), (447, 690)
SHOULDER_L, SHOULDER_R = (290, 245), (474, 245)
TAB = 26           # the radius of the round tab round a pin
# Each piece: which part of him it is (a test on the outline's coordinates), the pin
# it turns on (None for the body, which everything hangs from), and the pins it has
# on it for the pieces below.
# the sleeves, down the outside of his jacket from the shoulder to where his hands go
# behind his back
ARM_L = [(287, 215), (255, 240), (245, 280), (242, 325), (250, 380), (262, 420), (267, 465),
         (300, 462), (298, 420), (292, 380), (288, 330), (292, 285), (305, 250), (318, 228)]
ARM_R = [(480, 210), (495, 235), (505, 280), (509, 340), (500, 400), (492, 440), (500, 470),
         (468, 470), (462, 440), (468, 400), (470, 340), (466, 285), (458, 250), (450, 222)]
# the gap between his legs, where the tail of his coat shows behind: cut away
GAP = [(368, 590), (379, 586), (392, 590), (402, 600), (408, 652), (340, 652), (350, 602)]
# the seat of his trousers, showing below where his jacket opens: it goes with the legs,
# each taking its half, so that a leg's whole trouser leg swings when it kicks
SEAT = [(262, 488), (267, 460), (282, 442), (300, 432), (325, 424), (350, 414), (370, 406), (380, 401),
        (390, 407), (405, 420), (425, 427), (450, 435), (465, 445), (475, 465), (482, 488)]
CROTCH = 380
PIECES = [
    ("head", lambda x, y: y < NECK[1] + 2, NECK, []),
    ("body", lambda x, y: (y >= NECK[1] + 2) & (y < 488), None, [NECK, SHOULDER_L, SHOULDER_R, HIP_L, HIP_R]),   # less the seat and arms
    ("arm_l", None, SHOULDER_L, []),
    ("arm_r", None, SHOULDER_R, []),
    ("thigh_l", lambda x, y: (y < KNEE_L[1] + 14) & (x < CROTCH), HIP_L, [KNEE_L]),       # from y 488, and its half of the seat
    ("thigh_r", lambda x, y: (y < KNEE_R[1] + 14) & (x >= CROTCH), HIP_R, [KNEE_R]),
    ("shin_l", lambda x, y: (y >= KNEE_L[1] + 14) & (x < CROTCH), KNEE_L, []),
    ("shin_r", lambda x, y: (y >= KNEE_R[1] + 14) & (x >= CROTCH), KNEE_R, []),
]

def poly_mask(points, size, k):
    m = Image.new("L", size, 0)
    ImageDraw.Draw(m).polygon([(x * k, y * k) for x, y in points], fill=255)
    return np.asarray(m) > 0

def brass_pin(d, x, y, r):
    d.ellipse([x - r, y - r, x + r, y + r], fill=(150, 112, 40, 255), outline=(60, 40, 10, 255), width=2)
    d.ellipse([x - r * 0.55, y - r * 0.55, x + r * 0.35, y + r * 0.35], fill=(236, 204, 120, 255))
    d.line([x - r * 0.5, y, x + r * 0.5, y], fill=(90, 64, 20, 255), width=2)

def main():
    photo = Image.open(SRC).convert("L")
    photo = ImageOps.autocontrast(photo, cutoff=0.5)
    W, H = photo.size
    k = W / S
    # a light warm tint, like a print on card
    tint = Image.merge("RGB", [photo.point(lambda v: min(255, v * 1.04 + 6)), photo.point(lambda v: v * 0.98 + 2), photo.point(lambda v: v * 0.88)])
    rgb = np.asarray(tint)
    whole = poly_mask(OUTLINE, (W, H), k)
    ys, xs = np.mgrid[0:H, 0:W] / k
    arms = {"arm_l": poly_mask(ARM_L, (W, H), k) & whole, "arm_r": poly_mask(ARM_R, (W, H), k) & whole}
    gap = poly_mask(GAP, (W, H), k)
    whole &= ~gap
    seat = poly_mask(SEAT, (W, H), k) & whole
    # all the light trouser cloth below his jacket goes with the legs, not the jacket
    trouser = whole & (np.asarray(photo) > 150) & (ys > 420) & ~arms["arm_l"] & ~arms["arm_r"]
    legs = seat | (ys >= 488) | trouser

    pins_px = lambda p: (p[0] * k, p[1] * k)
    out_scale = 1.0 / k        # pieces are written at the outline's scale (800 wide)
    lines = ["/* Made by tools/make_doll.py: the paper doll's pieces and their pins. */",
             "/* For each piece: its picture, where its own pin is in it, and where in the whole",
             " * doll (at rest) that pin is. Pixels, at the scale of the pictures. */",
             "typedef struct { double pin_x, pin_y, at_x, at_y; } Piece;",
             "static const Piece PIECE[] = {"]
    for name, test, pin, kids in PIECES:
        if name in arms:
            m = arms[name]
        else:
            m = whole & test(xs, ys)
            if name == "body": m &= ~legs & ~arms["arm_l"] & ~arms["arm_r"]
            if name.startswith("thigh"): m = whole & legs & (xs < CROTCH if name == "thigh_l" else xs >= CROTCH) & (ys < (KNEE_L if name == "thigh_l" else KNEE_R)[1] + 14)
        if pin is not None:                            # its tab, round the pin it turns on,
            cx, cy = pins_px(pin)                      # of its own cloth
            disc = whole & (((xs * k - cx) ** 2 + (ys * k - cy) ** 2) < (TAB * k) ** 2)
            if name.startswith("thigh"): disc &= legs
            m |= disc
        # the piece, with its white paper edge round it
        mi = Image.fromarray((m * 255).astype(np.uint8))
        edge = mi.filter(ImageFilter.MaxFilter(2 * int(BORDER * k) + 1))
        ys_, xs_ = np.nonzero(np.asarray(edge))
        x0, y0, x1, y1 = xs_.min(), ys_.min(), xs_.max() + 1, ys_.max() + 1
        piece = np.zeros((H, W, 4), np.uint8)
        piece[np.asarray(edge) > 0] = (250, 246, 236, 255)
        piece[m, :3] = rgb[m]
        img = Image.fromarray(piece[y0:y1, x0:x1], "RGBA")
        sw, sh = max(1, round(img.width * out_scale)), max(1, round(img.height * out_scale))
        img = img.resize((sw, sh), Image.LANCZOS)
        d = ImageDraw.Draw(img)
        to_local = lambda p: (p[0] * k * out_scale - x0 * out_scale, p[1] * k * out_scale - y0 * out_scale)
        img.save(f"{OUT}/sp_{name}.png", optimize=True)
        p = pin if pin is not None else ((x0 + x1) / 2 / k, (y0 + y1) / 2 / k)
        lx, ly = to_local(p)
        lines.append(f"    /* {name:8} */ {{{lx:.1f}, {ly:.1f}, {p[0]:.1f}, {p[1]:.1f}}},")
        print(f"sp_{name}.png {sw}x{sh}")
    lines += ["};", ""]
    # the split pin, drawn over the joints by the show
    pin = Image.new("RGBA", (40, 40), (0, 0, 0, 0))
    brass_pin(ImageDraw.Draw(pin), 20, 20, 16)
    pin.resize((14, 14), Image.LANCZOS).save(f"{OUT}/sp_pin.png")
    open(HDR, "w").write("\n".join(lines))
    print(HDR)

if __name__ == "__main__":
    main()
