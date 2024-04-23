#include "../includes/quicksort.h"
#include "../includes/sched.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int
partition(int *a, int lo, int hi)
{
    int pivot = a[lo];
    int i = lo - 1;
    int j = hi + 1;
    int t;
    while(1) {
        do {
            i++;
        } while(a[i] < pivot);

        do {
            j--;
        } while(a[j] > pivot);

        if(i >= j) {
            return j;
        }

        t = a[i];
        a[i] = a[j];
        a[j] = t;
    }
}

struct quicksort_args {
    int *a;
    int lo, hi;
};

struct quicksort_args *
new_args(int *a, int lo, int hi)
{
    struct quicksort_args *args = malloc(sizeof(struct quicksort_args));
    if(args == NULL) {
        return NULL;
    }

    args->a = a;
    args->lo = lo;
    args->hi = hi;
    return args;
}

void
quicksort_serial(int *a, int lo, int hi)
{
    int p;

    if(lo >= hi) {
        return;
    }

    p = partition(a, lo, hi);
    quicksort_serial(a, lo, p);
    quicksort_serial(a, p + 1, hi);
}

void
quicksort(void *closure, struct scheduler *s)
{
    struct quicksort_args *args = (struct quicksort_args *)closure;
    int *a = args->a;
    int lo = args->lo;
    int hi = args->hi;
    int p;
    int rc;

    free(closure);

    if(lo >= hi) {
        return;
    }

    if(hi - lo <= 128) {
        quicksort_serial(a, lo, hi);
        return;
    }

    p = partition(a, lo, hi);
    rc = sched_spawn(quicksort, new_args(a, lo, p), s);
    assert(rc >= 0);
    rc = sched_spawn(quicksort, new_args(a, p + 1, hi), s);
    assert(rc >= 0);
}

double
benchmark_quicksort(int serial, int nthreads)
{
    int *a;
    struct timespec begin, end;
    double delay;
    int rc;
    int n = 10 * 1024 * 1024;

    a = malloc(n * sizeof(int));

    unsigned long long s = 0;
    for(int i = 0; i < n; i++) {
        s = s * 6364136223846793005ULL + 1442695040888963407;
        a[i] = (int)((s >> 33) & 0x7FFFFFFF);
    }

    clock_gettime(CLOCK_MONOTONIC, &begin);

    if(serial) {
        quicksort_serial(a, 0, n - 1);
    } else {
        rc = sched_init(nthreads, (n + 127) / 128, quicksort,
                        new_args(a, 0, n - 1));
        assert(rc >= 0);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    delay = end.tv_sec + end.tv_nsec / 1000000000.0 -
            (begin.tv_sec + begin.tv_nsec / 1000000000.0);

    for(int i = 0; i < n - 1; i++) {
        assert(a[i] <= a[i + 1]);
    }

    free(a);
    return delay;
}
