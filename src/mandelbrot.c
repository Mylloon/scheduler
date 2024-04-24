#include "../includes/mandelbrot.h"
#include "../includes/sched.h"

#include <assert.h>
#include <complex.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define WIDTH 3840
#define HEIGHT 2160
#define ITERATIONS 1000

#define SCALE (WIDTH / 4.0)
#define DX (WIDTH / 2)
#define DY (HEIGHT / 2)

struct mandelbrot_args {
    unsigned int *image;
    int x, y;
};

struct mandelbrot_args *
new_mandelbrot_args(unsigned int *image, int x, int y)
{
    struct mandelbrot_args *args;

    if(!(args = malloc(sizeof(struct mandelbrot_args)))) {
        perror("Mandelbrot parameters");
        return NULL;
    }

    args->image = image;
    args->x = x;
    args->y = y;

    return args;
}

int
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

double complex
toc(int x, int y, int dx, int dy, double scale)
{
    return ((x - dx) + I * (y - dy)) / scale;
}

unsigned int
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
    int x = args->x;
    int y = args->y;

    (void)s; // pas de nouvelle tâche dans le scheduler

    free(closure);

    pixel(image, SCALE, x, y, DX, DY, WIDTH);
}

void
draw(void *closure, struct scheduler *s)
{
    struct mandelbrot_args *args = (struct mandelbrot_args *)closure;
    unsigned int *image = args->image;

    free(closure);

    for(int y = args->y; y < HEIGHT; y++) {
        for(int x = args->x; x < WIDTH; x++) {
            int rc =
                sched_spawn(draw_pixel, new_mandelbrot_args(image, x, y), s);
            assert(rc >= 0);
        }
    }
}

void
draw_serial(unsigned int *image)
{
    for(int y = 0; y < HEIGHT; y++) {
        for(int x = 0; x < WIDTH; x++) {
            pixel(image, SCALE, x, y, DX, DY, WIDTH);
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
    int size = WIDTH * HEIGHT;

    if(!(image = malloc(size * sizeof(unsigned int)))) {
        perror("Image allocation");
        return 1;
    }

    clock_gettime(CLOCK_MONOTONIC, &begin);

    if(serial) {
        draw_serial(image);
    } else {
        rc = sched_init(nthreads, size, draw, new_mandelbrot_args(image, 0, 0));
        assert(rc >= 0);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    delay = end.tv_sec + end.tv_nsec / 1000000000.0 -
            (begin.tv_sec + begin.tv_nsec / 1000000000.0);

    free(image);
    return delay;
}
