#include "bitmap_utils.h"

/* */

void clear_image(uint32_t *dst, uint32_t width, uint32_t height)
{
	memset((void *) dst, 0x0, width*height*4);
}

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

void draw_fancy_image(uint32_t *image, uint32_t width, uint32_t height)
{
	uint32_t t = (uint32_t ) time(NULL);
	const int halfh = height / 2;
	const int halfw = width / 2;
	int ir, or;
	uint32_t *pixel = image;
	int y;

	/* squared radii thresholds */
	or = (halfw < halfh ? halfw : halfh) - 8;
	ir = or - 32;
	or *= or;
	ir *= ir;

	for (y = 0; y < height; y++) {
		int x;
		int y2 = (y - halfh) * (y - halfh);

		for (x = 0; x < width; x++) {
			uint32_t v;

			/* squared distance from center */
			int r2 = (x - halfw) * (x - halfw) + y2;

			if (r2 < ir)
				v = (r2 / 32 + t / 64) * 0x0080401;
			else if (r2 < or)
				v = (y + t / 32) * 0x0080401;
			else
				v = (x + t / 16) * 0x0080401;
			v &= 0x00ffffff;

			/* cross if compositor uses X from XRGB as alpha */
			if (abs(x - y) > 6 && abs(x + y - height) > 6)
				v |= 0xff000000;

			*pixel++ = v;
		}
	}
}


