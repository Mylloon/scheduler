#include "../includes/mandelbrot.h"
#include "../includes/quicksort.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int
main(int argc, char *argv[])
{
    int serial = 0;
    int nthreads = -1;

    int quicksort = 0;
    int mandelbrot = 0;
    double delay;

    int opt;
    while((opt = getopt(argc, argv, "qmst:")) != -1) {
        if(opt < 0) {
            goto usage;
        }

        switch(opt) {
        case 'q':
            quicksort = 1;
            break;
        case 'm':
            mandelbrot = 1;
            break;
        case 's':
            serial = 1;
            break;
        case 't':
            nthreads = atoi(optarg);
            break;
        default:
            goto usage;
        }
    }
    if(nthreads < 0 && !serial) {
        goto usage;
    }

    if(quicksort) {
        delay = benchmark_quicksort(serial, nthreads);
    } else if(mandelbrot) {
        delay = benchmark_mandelbrot(serial, nthreads);
    } else {
        goto usage;
    }

    assert(delay >= 0.0);
    printf("Done in %lf seconds.\n", delay);

    return 0;

usage:
    printf("Usage: %s -q|m [-t threads] [-s]\n", argv[0]);
    return 1;
}
