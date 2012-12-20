#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <error.h>
#include <errno.h>
#include <time.h>

/* */

void draw_test_image(uint32_t *addr, uint32_t width, uint32_t height);
void draw_fancy_image(uint32_t *image, uint32_t width, uint32_t height);
