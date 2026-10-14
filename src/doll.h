/* Made by tools/make_doll.py: the paper doll's pieces and their pins. */
/* For each piece: its picture, where its own pin is in it, and where in the whole
 * doll (at rest) that pin is. Pixels, at the scale of the pictures. */
typedef struct { double pin_x, pin_y, at_x, at_y; } Piece;
static const Piece PIECE[] = {
    /* head     */ {11.7, 70.2, 338.0, 150.0},
    /* hat      */ {82.9, 61.9, 373.9, 87.9},
    /* jaw      */ {55.3, 71.8, 382.0, 196.0},
    /* body     */ {123.2, 148.2, 383.5, 341.6},
    /* arm_l    */ {52.9, 35.1, 290.0, 245.0},
    /* arm_r    */ {25.8, 39.8, 474.0, 245.0},
    /* thigh_l  */ {92.7, 51.0, 318.0, 452.0},
    /* thigh_r  */ {68.6, 50.6, 444.0, 452.0},
    /* shin_l   */ {59.9, 25.6, 282.0, 690.0},
    /* shin_r   */ {40.9, 25.6, 447.0, 690.0},
};
