/* Made by tools/make_cast.py: the supporting cast, each in two pieces cut at the
 * mouth. For each: where the hinge is in its top piece and in its body piece, and
 * the height of its head (hair to mouth), in the pictures' pixels. */
typedef struct { double top_x, top_y, body_x, body_y, head; } Cast;
static const Cast CAST[] = {
    /* dumont  */ {10, 137, 71, 23, 133},
    /* zeppo   */ {9, 133, 134, 23, 129},
    /* woman   */ {9, 178, -13, 23, 174},
    /* man     */ {10, 192, -5, 23, 188},
};
