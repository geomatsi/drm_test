#include "bitmap_utils.h"

/* */

void draw_test_image(uint32_t *dst, uint32_t w, uint32_t h)
{

    uint32_t color32[] = {
        0x000000FF, 0x0000FF00, 0x00FF0000,
        0x00FF00FF, 0x00FFFF00, 0x0000FFFF,
    };

    uint32_t color;
    int i, j;

    for(i = 0; i < h; i++ ){

        color = color32[6*i/h];

        for(j = 0; j < w; j++) {
            *(dst + i*w + j) = color;
        }
    }


}
