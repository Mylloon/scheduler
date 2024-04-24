#include "../includes/mandelbrot.h"
#include "../includes/images_io.h"
#include "../includes/sched.h"

#include <assert.h>
#include <complex.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define ITERATIONS 1000

struct mandelbrot_args {
    unsigned int *image;
    double scale;
    int x, y, dx, dy, width, height;
};

struct mandelbrot_args *
new_mandelbrot_args(unsigned int *image, double scale, int x, int y, int dx,
                    int dy, int width, int height)
{
    struct mandelbrot_args *args;

    if(!(args = malloc(sizeof(struct mandelbrot_args)))) {
        perror("Mandelbrot parameters");
        return NULL;
    }

    args->image = image;
    args->scale = scale;
    args->x = x;
    args->y = y;
    args->dx = dx;
    args->dy = dy;
    args->width = width;
    args->height = height;

    return args;
}

static int
mandel(double complex c)
{
    double complex z = 0.0;
    int i = 0;
    while(i < ITERATIONS && creal(z) * creal(z) + cimag(z) * cimag(z) <= 4.0) {
        z = z * z + c;
        i++;
    }
    return i;
}

static double complex
toc(int x, int y, int dx, int dy, double scale)
{
    return ((x - dx) + I * (y - dy)) / scale;
}

static unsigned int
torgb(int n)
{
    unsigned char r, g, b;

    if(n < 128) {
        int v = 2 * n;
        r = v;
        g = 0;
        b = 255 - v;
    } else if(n < 256) {
        int v = 2 * (n - 128);
        r = 0;
        g = v;
        b = 255 - v;
    } else if(n < 512) {
        int v = n - 256;
        r = 255 - v;
        g = v;
        b = 0;
    } else if(n < 1024) {
        int v = (n - 512) / 2;
        g = 255;
        r = b = v;
    } else {
        r = g = b = 255;
    }

    return r << 16 | g << 8 | b;
}

void
pixel(unsigned int *image, double scale, int x, int y, int dx, int dy,
      int width)
{
    unsigned rgb = torgb(mandel(toc(x, y, dx, dy, scale)));
    image[y * width + x] = rgb;
}

void
draw_pixel(void *closure, struct scheduler *s)
{
    struct mandelbrot_args *args = (struct mandelbrot_args *)closure;
    unsigned int *image = args->image;
    double scale = args->scale;
    int x = args->x;
    int y = args->y;
    int dx = args->dx;
    int dy = args->dy;
    int width = args->width;

    (void)s; // pas de nouvelle tâche dans le scheduler

    free(closure);

    pixel(image, scale, x, y, dx, dy, width);
}

void
draw(void *closure, struct scheduler *s)
{
    struct mandelbrot_args *args = (struct mandelbrot_args *)closure;
    unsigned int *image = args->image;
    double scale = args->scale;
    int dx = args->dx;
    int dy = args->dy;
    int width = args->width;
    int height = args->height;

    free(closure);

    for(int y = 0; y < height; y++) {
        for(int x = 0; x < width; x++) {
            int rc = sched_spawn(
                draw_pixel,
                new_mandelbrot_args(image, scale, x, y, dx, dy, width, height),
                s);
            assert(rc >= 0);
        }
    }
}

void
draw_serial(unsigned int *image, double scale, int dx, int dy, int width,
            int height)
{
    for(int y = 0; y < height; y++) {
        for(int x = 0; x < width; x++) {
            pixel(image, scale, x, y, dx, dy, width);
        }
    }
}

double
benchmark_mandelbrot(int serial, int nthreads)
{
    unsigned int *image;
    struct timespec begin, end;
    double delay;
    int rc;
    int width = 3840;
    int height = 2160;
    double scale = width / 4.0;
    int dx = width / 2;
    int dy = height / 2;

    if(!(image = malloc(width * height * sizeof(unsigned int)))) {
        perror("Image allocation");
        return 1;
    }

    clock_gettime(CLOCK_MONOTONIC, &begin);

    if(serial) {
        draw_serial(image, scale, dx, dy, width, height);
    } else {
        rc = sched_init(
            nthreads, width * height, draw,
            new_mandelbrot_args(image, scale, 0, 0, dx, dy, width, height));
        assert(rc >= 0);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    delay = end.tv_sec + end.tv_nsec / 1000000000.0 -
            (begin.tv_sec + begin.tv_nsec / 1000000000.0);

    imageSaveBMP("mandelbrot.bmp", (unsigned char *)image, width, height, 3, 8);
    free(image);
    return delay;
}
