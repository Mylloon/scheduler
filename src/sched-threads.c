#include "../includes/sched.h"

#include <errno.h>
#include <pthread.h>
#include <stdio.h>

int
sched_init(int nthreads, int qlen, taskfunc f, void *closure)
{
    sched_spawn(f, closure, NULL);
    return 0;
}

int
sched_spawn(taskfunc f, void *closure, struct scheduler *s)
{
    pthread_t thread;
    int err;

    // Création d'un thread pour la tâche
    if((err = pthread_create(&thread, NULL, (void *(*)(void *))f, closure)) !=
       0) {
        fprintf(stderr, "pthread_create error %d\n", errno);
        return -1;
    }

    // Attend la fin du thread
    if((err = pthread_join(thread, NULL)) != 0) {
        fprintf(stderr, "pthread_join error %d\n", errno);
        return -1;
    }

    return 0;
}
