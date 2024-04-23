#include "../includes/sched.h"

#include <pthread.h>
#include <stdio.h>

/* Routine d'un thread */
void *sched_worker(void *);

int
sched_init(int nthreads, int qlen, taskfunc f, void *closure)
{
    // Paramètres inutilisés
    (void)nthreads;
    (void)qlen;

    sched_spawn(f, closure, NULL);
    return 0;
}

int
sched_spawn(taskfunc f, void *closure, struct scheduler *s)
{
    // Paramètres inutilisés
    (void)f;
    (void)closure;

    pthread_t thread;
    int err;

    // Création d'un thread pour la tâche
    if((err = pthread_create(&thread, NULL, sched_worker, &s)) != 0) {
        fprintf(stderr, "pthread_create error %d\n", err);
        return -1;
    }

    // Attend la fin du thread
    if((err = pthread_join(thread, NULL)) != 0) {
        fprintf(stderr, "pthread_join error %d\n", err);
        return -1;
    }

    return 0;
}

void *
sched_worker(void *arg)
{
    (void)arg;

    return NULL;
}
