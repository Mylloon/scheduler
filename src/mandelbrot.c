#include "../includes/mandelbrot.h"
#include "../includes/images_io.h"
#include "../includes/sched.h"

#include <assert.h>
#include <complex.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define WIDTH 3840
#define HEIGHT 2160
#define ITERATIONS 1000
#define CHUNK_SIZE 8

#define SCALE (WIDTH / 4.0)
#define DX (WIDTH / 2)
#define DY (HEIGHT / 2)

struct mandelbrot_args {
    unsigned int *image;
    int start_x, start_y, end_x, end_y;
};

struct mandelbrot_args *
new_mandelbrot_args(unsigned int *image, int start_x, int start_y, int end_x,
                    int end_y)
{
    struct mandelbrot_args *args;

    if(!(args = malloc(sizeof(struct mandelbrot_args)))) {
        perror("Mandelbrot parameters");
        return NULL;
    }

    args->image = image;
    args->start_x = start_x;
    args->start_y = start_y;
    args->end_x = end_x;
    args->end_y = end_y;

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
toc(int x, int y)
{
    return ((x - (int)DX) + I * (y - (int)DY)) / SCALE;
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
pixel(unsigned int *image, int x, int y)
{
    unsigned rgb = torgb(mandel(toc(x, y)));
    image[y * WIDTH + x] = rgb;
}

void
draw(void *closure, struct scheduler *s)
{
    struct mandelbrot_args *args = (struct mandelbrot_args *)closure;
    unsigned int *image = args->image;
    int start_x = args->start_x;
    int start_y = args->start_y;
    int end_x = args->end_x;
    int end_y = args->end_y;

    free(closure);

    if((end_x - start_x) < CHUNK_SIZE && (end_y - start_y) < CHUNK_SIZE) {
        // Si le morceau est petit alors on dessine
        for(int y = start_y; y < end_y; y++) {
            for(int x = start_x; x < end_x; x++) {
                pixel(image, x, y);
            }
        }
    } else {
        // Sinon on recoupe le morceau
        int mid_x = (start_x + end_x) / 2;
        int mid_y = (start_y + end_y) / 2;

        int rc1 = sched_spawn(
            draw, new_mandelbrot_args(image, start_x, start_y, mid_x, mid_y),
            s);
        int rc2 = sched_spawn(
            draw, new_mandelbrot_args(image, mid_x, start_y, end_x, mid_y), s);
        int rc3 = sched_spawn(
            draw, new_mandelbrot_args(image, start_x, mid_y, mid_x, end_y), s);
        int rc4 = sched_spawn(
            draw, new_mandelbrot_args(image, mid_x, mid_y, end_x, end_y), s);

        assert(rc1 >= 0 && rc2 >= 0 && rc3 >= 0 && rc4 >= 0);
    }
}

void
draw_serial(unsigned int *image)
{
    for(int y = 0; y < HEIGHT; y++) {
        for(int x = 0; x < WIDTH; x++) {
            pixel(image, x, y);
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
        rc = sched_init(nthreads, size, draw,
                        new_mandelbrot_args(image, 0, 0, WIDTH, HEIGHT));
        assert(rc >= 0);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    delay = end.tv_sec + end.tv_nsec / 1000000000.0 -
            (begin.tv_sec + begin.tv_nsec / 1000000000.0);

    // Sauvegarde l'image pour voir si ça fonctionne correctement
    imageSaveBMP("mandelbrot.bmp", (unsigned char *)image, WIDTH, HEIGHT, 3, 8);

    free(image);
    return delay;
}
