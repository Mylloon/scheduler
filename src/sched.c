#include "../includes/sched.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct task_info {
    void *closure;
    taskfunc f;
};

struct scheduler {
    /* Indicateurs de changement d'état */
    pthread_cond_t *cond;

    /* Taille des piles */
    int qlen;

    /* Mutex qui protège les piles */
    pthread_mutex_t *mutex;

    /* Nombre de threads instanciés */
    int nthreads;

    /* Nombre de threads en attente */
    int nthsleep;

    /* Piles de tâches */
    struct task_info **tasks;

    /* Positions actuelle dans la pile */
    int *top;
};

/* Ordonnanceur partagé */
static struct scheduler sched;

/* Lance une tâche de la pile */
void *sched_worker(void *);

/* Nettoie les opérations effectuées par l'initialisation de l'ordonnanceur */
int sched_init_cleanup(int);

int
sched_init(int nthreads, int qlen, taskfunc f, void *closure)
{
    sched.cond = NULL;
    sched.mutex = NULL;
    sched.tasks = NULL;
    sched.top = NULL;

    if(qlen <= 0) {
        fprintf(stderr, "qlen must be greater than 0\n");
        return -1;
    }
    sched.qlen = qlen;

    if(nthreads < 0) {
        fprintf(stderr, "nthreads must be greater than 0\n");
        return -1;
    } else if(nthreads == 0) {
        nthreads = sched_default_threads();
    }
    sched.nthreads = nthreads;

    sched.nthsleep = 0;

    // Initialisation des mutex de chaque processus
    if(!(sched.mutex = malloc(sched.nthreads * sizeof(pthread_mutex_t)))) {
        perror("Mutexes");
        return sched_init_cleanup(-1);
    }
    for(int i = 0; i < sched.nthreads; ++i) {
        if(pthread_mutex_init(&sched.mutex[i], NULL) != 0) {
            fprintf(stderr, "Can't init mutex for thread %d\n", i);
            return sched_init_cleanup(-1);
        }
    }

    // Initialisation des variables de conditions de chaque processus
    if(!(sched.cond = malloc(sched.nthreads * sizeof(pthread_cond_t)))) {
        perror("Variable conditions");
        return sched_init_cleanup(-1);
    }
    for(int i = 0; i < sched.nthreads; ++i) {
        if(pthread_cond_init(&sched.cond[i], NULL) != 0) {
            fprintf(stderr, "Can't init condition variable for thread %d\n", i);
            return sched_init_cleanup(-1);
        }
    }

    // Initialisation du curseur suivant l'état de la pile de chaque processus
    if(!(sched.top = malloc(sched.nthreads * sizeof(int)))) {
        perror("Cursor top stack\n");
        return sched_init_cleanup(-1);
    }
    for(int i = 0; i < sched.nthreads; ++i) {
        sched.top[i] = -1;
    }

    // Allocation mémoire pour la pile de chaque processus
    if(!(sched.tasks = malloc(sched.nthreads * sizeof(struct task_info *)))) {
        perror("Stack list");
        return sched_init_cleanup(-1);
    }
    for(int i = 0; i < sched.nthreads; ++i) {
        if(!(sched.tasks[i] = malloc(qlen * sizeof(struct task_info)))) {
            fprintf(stderr, "Stack for thread %d: %s\n", i, strerror(errno));
            return sched_init_cleanup(-1);
        }
    }

    pthread_t threads[nthreads];
    for(int i = 0; i < nthreads; ++i) {
        if(pthread_create(&threads[i], NULL, sched_worker, &sched) != 0) {
            fprintf(stderr, "Can't create the thread %d\n", i);

            if(i > 0) {
                fprintf(stderr, ", cancelling already created threads...\n");
                for(int j = 0; j < i; ++j) {
                    if(pthread_cancel(threads[j]) != 0) {
                        fprintf(stderr, "Can't cancel the thread %d\n", j);
                    }
                }
            } else {
                fprintf(stderr, "\n");
            }

            return sched_init_cleanup(-1);
        }
    }

    if(sched_spawn(f, closure, &sched) < 0) {
        fprintf(stderr, "Can't create the initial task\n");
        return sched_init_cleanup(-1);
    }

    for(int i = 0; i < nthreads; ++i) {
        if((pthread_join(threads[i], NULL) != 0)) {
            fprintf(stderr, "Can't wait the thread %d\n", i);
            return sched_init_cleanup(-1);
        }
    }

    return sched_init_cleanup(1);
}

int
sched_init_cleanup(int ret_code)
{
    if(sched.mutex) {
        for(int i = 0; i < sched.nthreads; ++i) {
            pthread_mutex_destroy(&sched.mutex[i]);
        }

        free(sched.mutex);
        sched.mutex = NULL;
    }

    if(sched.cond) {
        free(sched.cond);
        sched.cond = NULL;
    }

    if(sched.tasks) {
        for(int i = 0; i < sched.nthreads; ++i) {
            if(sched.tasks[i]) {
                free(sched.tasks[i]);
                sched.tasks[i] = NULL;
            }
        }

        free(sched.tasks);
        sched.tasks = NULL;
    }

    if(sched.top) {
        free(sched.top);
        sched.top = NULL;
    }

    return ret_code;
}

int
sched_spawn(taskfunc f, void *closure, struct scheduler *s)
{
    pthread_mutex_lock(&s->mutex[0]);

    if(s->top[0] + 1 >= s->qlen) {
        pthread_mutex_unlock(&s->mutex[0]);
        errno = EAGAIN;
        fprintf(stderr, "Stack is full\n");
        return -1;
    }

    s->tasks[0][++s->top[0]] = (struct task_info){closure, f};

    pthread_cond_signal(&s->cond[0]);
    pthread_mutex_unlock(&s->mutex[0]);

    return 0;
}

void *
sched_worker(void *arg)
{
    struct scheduler *s = (struct scheduler *)arg;

    while(1) {
        pthread_mutex_lock(&s->mutex[0]);

        // S'il on a rien à faire
        if(s->top[0] == -1) {
            s->nthsleep++;
            if(s->nthsleep == s->nthreads) {
                // Signal a tout les threads que il n'y a plus rien à faire
                // si un thread attend une tâche
                pthread_cond_broadcast(&s->cond[0]);
                pthread_mutex_unlock(&s->mutex[0]);

                break;
            }

            pthread_cond_wait(&s->cond[0], &s->mutex[0]);
            s->nthsleep--;
            pthread_mutex_unlock(&s->mutex[0]);
            continue;
        }

        // Extrait la tâche de la pile
        taskfunc f = s->tasks[0][s->top[0]].f;
        void *closure = s->tasks[0][s->top[0]].closure;
        s->top[0]--;
        pthread_mutex_unlock(&s->mutex[0]);

        // Exécute la tâche
        f(closure, s);
    }

    return NULL;
}
