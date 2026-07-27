/*
 * it8951_diff.c — Regional differential update for IT8951 e-paper displays.
 *
 * Compares a new image against the last displayed image (stored on disk),
 * finds changed regions, expands them by a configurable border, applies
 * Floyd-Steinberg dithering at the edges for smooth transitions, and
 * sends only the changed region to the display.
 *
 * Two refresh modes:
 *   --hard   White-flash the region first (GC16), then draw new content.
 *            Produces cleanest result but visible white blink.
 *   --soft   Draw new content directly over old (GC16 only, no pre-clear).
 *            No blinking, but may leave faint ghosting in areas where
 *            old content was dark and new content is light.
 *
 * --border-smooth N   Expand changed region by N pixels on each side (default: 20).
 *                      Dithering is applied in this border zone to blend
 *                      old and new pixel values smoothly.
 *
 * Image storage: /tmp/it8951_last.png (PNG, 8bpp grayscale).
 */
#include "it8951_driver.h"

/* stb_image is already implemented in it8951_image.c — just include headers */
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/stat.h>

#define DEFAULT_BORDER 20
#define LAST_IMG_PATH "/tmp/it8951_last.png"

/* ---- Floyd-Steinberg dithering at region border ---- */

/*
 * Blend old and new images in the border zone using Floyd-Steinberg
 * error diffusion. Pixels in the inner region get the new value directly;
 * pixels in the border zone get a dithered blend of old and new.
 *
 * blend_factor: 0.0 = all old, 1.0 = all new. Varies from 0 at the outer
 * edge to 1 at the inner edge of the border zone.
 */
static void apply_border_dither(uint8_t *region_data,
                                const uint8_t *old_full,
                                const uint8_t *new_full,
                                int rx, int ry, int rw, int rh,
                                int screen_w, int screen_h,
                                int border)
{
    /* Copy pixel values from the new image for the entire region.
       The border zone blends old→new for smooth transition.
       No explicit dithering — let the GC16 hardware handle 16-level quantization
       to preserve 8bpp antialiasing. */
    for (int y = 0; y < rh; y++) {
        for (int x = 0; x < rw; x++) {
            int gx = rx + x;
            int gy = ry + y;
            if (gx < 0 || gx >= screen_w || gy < 0 || gy >= screen_h) {
                region_data[y * rw + x] = 255; /* white for out-of-bounds */
                continue;
            }

            int old_val = old_full[gy * screen_w + gx];
            int new_val = new_full[gy * screen_w + gx];

            /* Distance from region edge */
            int dx = (x < rw - 1 - x) ? x : (rw - 1 - x);
            int dy = (y < rh - 1 - y) ? y : (rh - 1 - y);
            int dist = (dx < dy) ? dx : dy;

            if (dist >= border) {
                /* Inner region: use new value directly (full 8bpp) */
                region_data[y * rw + x] = (uint8_t)new_val;
            } else {
                /* Border zone: linear blend old→new based on distance */
                float blend = (float)dist / (float)border;
                int val = (int)(old_val * (1.0f - blend) + new_val * blend + 0.5f);
                if (val < 0) val = 0;
                if (val > 255) val = 255;
                region_data[y * rw + x] = (uint8_t)val;
            }
        }
    }
}

/* ---- Find bounding box of changed region ---- */

static int find_changed_region(const uint8_t *old_img, const uint8_t *new_img,
                               int w, int h, int threshold,
                               int *out_x, int *out_y, int *out_w, int *out_h)
{
    int min_x = w, min_y = h, max_x = -1, max_y = -1;

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            if (abs((int)old_img[y * w + x] - (int)new_img[y * w + x]) > threshold) {
                if (x < min_x) min_x = x;
                if (x > max_x) max_x = x;
                if (y < min_y) min_y = y;
                if (y > max_y) max_y = y;
            }
        }
    }

    if (max_x < 0)
        return -1; /* No changes */

    *out_x = min_x;
    *out_y = min_y;
    *out_w = max_x - min_x + 1;
    *out_h = max_y - min_y + 1;
    return 0;
}

/* ---- Main API: display with differential regional update ---- */

/*
 * it8951_display_diff — Compare new image to stored last image,
 * find changed region, expand by border, dither edges, send to display.
 *
 * Parameters:
 *   dev         — IT8951 device handle
 *   new_data    — New full-screen 8bpp grayscale image (w*h bytes)
 *   w, h        — Image dimensions (should match panel size)
 *   mode        — Hard or soft refresh
 *   border      — Border expansion in pixels (for dithering)
 *   threshold   — Pixel difference threshold to detect changes
 *
 * Returns 0 on success, -1 on error or no changes.
 */
int it8951_display_diff(it8951_t *dev, const uint8_t *new_data,
                        int w, int h, int mode, int border, int threshold)
{
    int screen_w = dev->info.panel_w;
    int screen_h = dev->info.panel_h;

    if (w != screen_w || h != screen_h) {
        fprintf(stderr, "diff: image size %dx%d doesn't match panel %dx%d\n",
                w, h, screen_w, screen_h);
        return -1;
    }

    /* Load last image from disk (if exists) */
    uint8_t *old_data = NULL;
    int old_w, old_h, old_ch;
    old_data = stbi_load(LAST_IMG_PATH, &old_w, &old_h, &old_ch, 1);

    if (!old_data || old_w != screen_w || old_h != screen_h) {
        /* No previous image — full refresh */
        if (old_data) stbi_image_free(old_data);
        printf("diff: no previous image, doing full refresh\n");
        it8951_display_8bpp(dev, new_data, 0, 0, screen_w, screen_h, GC16_MODE);
        /* Save current as last */
        stbi_write_png(LAST_IMG_PATH, screen_w, screen_h, 1, new_data, screen_w);
        return 0;
    }

    /* Find changed region */
    int cx, cy, cw, ch;
    if (find_changed_region(old_data, new_data, screen_w, screen_h, threshold,
                            &cx, &cy, &cw, &ch) < 0) {
        printf("diff: no changes detected\n");
        stbi_image_free(old_data);
        return 0; /* No changes — nothing to do */
    }

    /* Expand region by border, clamped to screen bounds */
    int rx = cx - border;
    int ry = cy - border;
    int rw = cw + 2 * border;
    int rh = ch + 2 * border;
    if (rx < 0) rx = 0;
    if (ry < 0) ry = 0;
    if (rx + rw > screen_w) rw = screen_w - rx;
    if (ry + rh > screen_h) rh = screen_h - ry;

    /* IT8951 8bpp requires even-width regions (data sent as 16-bit words).
       Align x to even and round width up to even. */
    if (rx % 2 != 0) { rx--; rw++; }
    if (rw % 2 != 0) { rw++; }
    if (rx + rw > screen_w) rw = screen_w - rx;  /* re-clamp */
    if (rw % 2 != 0) rw--;  /* must stay even */

    printf("diff: changed region %d,%d %dx%d → expanded %d,%d %dx%d (border=%d, even-aligned)\n",
           cx, cy, cw, ch, rx, ry, rw, rh, border);

    /* Build region data with border dithering */
    uint8_t *region = malloc(rw * rh);
    if (!region) {
        stbi_image_free(old_data);
        return -1;
    }

    apply_border_dither(region, old_data, new_data,
                        rx, ry, rw, rh,
                        screen_w, screen_h, border);

    /* Send to display */
    if (mode == DIFF_MODE_HARD) {
        /* Hard refresh: white-flash region first, then draw */
        printf("diff: hard refresh (white flash + GC16)\n");
        /* Clear region to white (255=white in 8bpp) */
        uint8_t *white = malloc(rw * rh);
        memset(white, 255, rw * rh);
        it8951_display_8bpp(dev, white, rx, ry, rw, rh, GC16_MODE);
        free(white);
        /* Now draw the actual content */
        it8951_display_8bpp(dev, region, rx, ry, rw, rh, GC16_MODE);
    } else if (mode == DIFF_MODE_SMOOTH) {
        /* Smooth refresh: A2 fast mode, 1-bit dithered, no flash */
        printf("diff: smooth refresh (A2, 1-bit dithered)\n");
        /* Convert region to 1-bit using Floyd-Steinberg dithering */
        uint8_t *bw = malloc(rw * rh);
        float *fs_errors = calloc(rw * rh, sizeof(float));
        for (int y = 0; y < rh; y++) {
            for (int x = 0; x < rw; x++) {
                float val = (float)region[y * rw + x] + fs_errors[y * rw + x];
                uint8_t q = (val < 128) ? 0 : 255;
                bw[y * rw + x] = q;
                float err = val - (float)q;
                if (x + 1 < rw) fs_errors[y * rw + (x+1)] += err * 7.0f / 16.0f;
                if (y + 1 < rh) {
                    if (x > 0)      fs_errors[(y+1) * rw + (x-1)] += err * 3.0f / 16.0f;
                                    fs_errors[(y+1) * rw + x]   += err * 5.0f / 16.0f;
                    if (x + 1 < rw) fs_errors[(y+1) * rw + (x+1)] += err * 1.0f / 16.0f;
                }
            }
        }
        free(fs_errors);
        it8951_display_8bpp(dev, bw, rx, ry, rw, rh, A2_MODE);
        free(bw);
    } else {
        /* Soft refresh: just draw over old content (GC16) */
        printf("diff: soft refresh (GC16 only)\n");
        it8951_display_8bpp(dev, region, rx, ry, rw, rh, GC16_MODE);
    }

    /* Save current image as last */
    stbi_write_png(LAST_IMG_PATH, screen_w, screen_h, 1, new_data, screen_w);

    free(region);
    stbi_image_free(old_data);
    return 0;
}

/* ---- Convenience: display image file with diff ---- */

int it8951_display_image_diff(it8951_t *dev, const char *path,
                              uint8_t bg_color, float brightness,
                              int mode, int border)
{
    int img_w, img_h, img_ch;
    unsigned char *img = stbi_load(path, &img_w, &img_h, &img_ch, 1);
    if (!img) {
        fprintf(stderr, "Failed to load image: %s\n", path);
        return -1;
    }

    int screen_w = dev->info.panel_w;
    int screen_h = dev->info.panel_h;

    /* Auto-scale to fit screen */
    float scale = fminf((float)screen_w / img_w, (float)screen_h / img_h);
    int new_w, new_h;
    if (scale < 1.0) {
        new_w = (int)(img_w * scale);
        new_h = (int)(img_h * scale);
    } else {
        new_w = img_w;
        new_h = img_h;
    }

    /* Resize */
    uint8_t *resized = malloc(new_w * new_h);
    if (new_w != img_w || new_h != img_h) {
        /* Bilinear resize */
        for (int dy = 0; dy < new_h; dy++) {
            float sy = (float)dy * img_h / new_h;
            int sy0 = (int)sy;
            int sy1 = (sy0 + 1 < img_h) ? sy0 + 1 : sy0;
            float fy = sy - sy0;
            for (int dx = 0; dx < new_w; dx++) {
                float sx = (float)dx * img_w / new_w;
                int sx0 = (int)sx;
                int sx1 = (sx0 + 1 < img_w) ? sx0 + 1 : sx0;
                float fx = sx - sx0;
                float v = (1-fx)*(1-fy) * img[sy0*img_w+sx0] +
                          fx*(1-fy)     * img[sy0*img_w+sx1] +
                          (1-fx)*fy     * img[sy1*img_w+sx0] +
                          fx*fy         * img[sy1*img_w+sx1];
                resized[dy*new_w+dx] = (uint8_t)(v + 0.5);
            }
        }
    } else {
        memcpy(resized, img, new_w * new_h);
    }

    /* Create full-screen canvas */
    uint8_t *canvas = malloc(screen_w * screen_h);
    memset(canvas, bg_color, screen_w * screen_h);

    int off_x = (screen_w - new_w) / 2;
    int off_y = (screen_h - new_h) / 2;

    /* Gamma LUT */
    uint8_t gamma_lut[256];
    if (brightness != 1.0f) {
        float inv_gamma = 1.0f / brightness;
        for (int i = 0; i < 256; i++) {
            float norm = (float)i / 255.0f;
            float corrected = powf(norm, inv_gamma);
            int v = (int)(corrected * 255.0f + 0.5f);
            gamma_lut[i] = (v < 0) ? 0 : (v > 255 ? 255 : (uint8_t)v);
        }
    } else {
        for (int i = 0; i < 256; i++) gamma_lut[i] = (uint8_t)i;
    }

    for (int y = 0; y < new_h; y++) {
        int py = off_y + y;
        if (py < 0 || py >= screen_h) continue;
        for (int x = 0; x < new_w; x++) {
            int px = off_x + x;
            if (px < 0 || px >= screen_w) continue;
            uint8_t val = resized[y * new_w + x];
            if (abs(val - bg_color) > 2) {
                val = gamma_lut[val];
            }
            canvas[py * screen_w + px] = val;
        }
    }

    /* Differential update */
    int ret = it8951_display_diff(dev, canvas, screen_w, screen_h,
                                  mode, border, 5);

    free(canvas);
    free(resized);
    stbi_image_free(img);
    return ret;
}