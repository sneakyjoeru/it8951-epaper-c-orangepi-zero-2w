/*
 * it8951_image.c — Image display for IT8951 using stb_image
 *
 * Loads an image, converts to grayscale, auto-scales to fit screen
 * preserving aspect ratio, centers on background, applies brightness,
 * Floyd-Steinberg dithers to 16 levels, sends as 8bpp.
 */
#include "it8951_driver.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Bilinear resize */
static void resize_bilinear_gray(const uint8_t *src, int sw, int sh,
                                 uint8_t *dst, int dw, int dh)
{
    for (int dy = 0; dy < dh; dy++) {
        float sy = (float)dy * sh / dh;
        int sy0 = (int)sy;
        int sy1 = (sy0 + 1 < sh) ? sy0 + 1 : sy0;
        float fy = sy - sy0;
        for (int dx = 0; dx < dw; dx++) {
            float sx = (float)dx * sw / dw;
            int sx0 = (int)sx;
            int sx1 = (sx0 + 1 < sw) ? sx0 + 1 : sx0;
            float fx = sx - sx0;
            float v = (1-fx)*(1-fy) * src[sy0*sw+sx0] +
                      fx*(1-fy)     * src[sy0*sw+sx1] +
                      (1-fx)*fy     * src[sy1*sw+sx0] +
                      fx*fy         * src[sy1*sw+sx1];
            dst[dy*dw+dx] = (uint8_t)(v + 0.5);
        }
    }
}

int it8951_display_image(it8951_t *dev, const char *path,
                         uint8_t bg_color, float brightness, uint16_t mode)
{
    int img_w, img_h, img_ch;
    unsigned char *img = stbi_load(path, &img_w, &img_h, &img_ch, 1); /* force grayscale */
    if (!img) {
        fprintf(stderr, "Failed to load image: %s\n", path);
        return -1;
    }

    printf("Original size: %d x %d\n", img_w, img_h);

    int screen_w = dev->info.panel_w;
    int screen_h = dev->info.panel_h;

    /* Auto-scale to fit screen, preserving aspect ratio */
    float scale = fminf((float)screen_w / img_w, (float)screen_h / img_h);
    int new_w, new_h;
    if (scale < 1.0) {
        new_w = (int)(img_w * scale);
        new_h = (int)(img_h * scale);
    } else {
        new_w = img_w;
        new_h = img_h;
    }

    /* Resize image */
    uint8_t *resized = malloc(new_w * new_h);
    if (new_w != img_w || new_h != img_h) {
        resize_bilinear_gray(img, img_w, img_h, resized, new_w, new_h);
    } else {
        memcpy(resized, img, new_w * new_h);
    }

    /* Create canvas with background color */
    uint8_t *canvas = malloc(screen_w * screen_h);
    memset(canvas, bg_color, screen_w * screen_h);

    /* Center image on canvas */
    int off_x = (screen_w - new_w) / 2;
    int off_y = (screen_h - new_h) / 2;

    /* Paste and apply brightness boost to non-background pixels */
    for (int y = 0; y < new_h; y++) {
        int py = off_y + y;
        if (py < 0 || py >= screen_h) continue;
        for (int x = 0; x < new_w; x++) {
            int px = off_x + x;
            if (px < 0 || px >= screen_w) continue;
            uint8_t val = resized[y * new_w + x];
            if (brightness != 1.0 && abs(val - bg_color) > 2) {
                int boosted = (int)(val * brightness);
                val = (uint8_t)(boosted > 255 ? 255 : boosted);
            }
            canvas[py * screen_w + px] = val;
        }
    }

    /* Floyd-Steinberg dithering to 16 levels (multiples of 17) */
    float *arr = malloc(screen_w * screen_h * sizeof(float));
    for (int i = 0; i < screen_w * screen_h; i++)
        arr[i] = canvas[i];

    uint8_t *out = malloc(screen_w * screen_h);
    float step = 255.0 / 15;

    for (int row = 0; row < screen_h; row++) {
        for (int col = 0; col < screen_w; col++) {
            int idx = row * screen_w + col;
            float cur = arr[idx];
            float q = round(cur / step) * step;
            if (q < 0) q = 0;
            if (q > 255) q = 255;
            out[idx] = (uint8_t)q;
            float err = cur - q;

            if (col + 1 < screen_w)
                arr[idx + 1] += err * 7 / 16;
            if (row + 1 < screen_h) {
                if (col > 0)
                    arr[idx + screen_w - 1] += err * 3 / 16;
                arr[idx + screen_w] += err * 5 / 16;
                if (col + 1 < screen_w)
                    arr[idx + screen_w + 1] += err * 1 / 16;
            }
        }
    }

    /* Display as 8bpp */
    it8951_display_8bpp(dev, out, 0, 0, screen_w, screen_h, mode);

    free(out);
    free(arr);
    free(canvas);
    free(resized);
    stbi_image_free(img);

    return 0;
}