/* Made by tools/make_doll.py: the paper doll's pieces and their pins. */
/* For each piece: its picture, where its own pin is in it, and where in the whole
 * doll (at rest) that pin is. Pixels, at the scale of the pictures. */
typedef struct { double pin_x, pin_y, at_x, at_y; } Piece;
static const Piece PIECE[] = {
    /* head     */ {91.1, 170.1, 382.0, 196.0},
    /* body     */ {138.4, 149.8, 375.4, 343.2},
    /* tail     */ {38.9, 85.9, 373.9, 569.0},
    /* thigh_l  */ {92.7, 55.7, 318.0, 452.0},
    /* thigh_r  */ {68.6, 55.3, 444.0, 452.0},
    /* shin_l   */ {59.9, 30.3, 282.0, 690.0},
    /* shin_r   */ {40.9, 30.3, 447.0, 690.0},
};
