#include "../includes/sched.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

struct task_info {
    void *closure;
    taskfunc f;
};

struct scheduler {
    /* Indicateur de changement d'état */
    pthread_cond_t *cond;

    /* Taille de la pile */
    int qlen;

    /* Mutex qui protège la structure */
    pthread_mutex_t mutex;

    /* Nombre de threads instanciés */
    int nthreads;

    /* Nombre de threads en attente */
    int nthsleep;

    /* Pile de tâches */
    struct task_info *tasks;

    /* Position actuelle dans la pile */
    int top;
};

/* Ordonnanceur partagé */
static struct scheduler sched;

/* Lance une tâche de la pile */
void *sched_worker(void *);

/* Nettoie les opérations effectuées par l'initialisation de l'ordonnanceur */
int cleanup_init(int);

int
sched_init(int nthreads, int qlen, taskfunc f, void *closure)
{
    sched.cond = NULL;
    sched.tasks = NULL;

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

    if(pthread_mutex_init(&sched.mutex, NULL) != 0) {
        fprintf(stderr, "Can't init mutex\n");
        return -1;
    }

    // Initialisation des variables de conditions de chaque processus
    if(!(sched.cond = malloc(sched.nthreads * sizeof(pthread_cond_t)))) {
        perror("Variable conditions");
        return cleanup_init(-1);
    }
    for(int i = 0; i < sched.nthreads; ++i) {
        if(pthread_cond_init(&sched.cond[i], NULL) != 0) {
            fprintf(stderr, "Can't init condition variable for thread %d\n", i);
            return cleanup_init(-1);
        }
    }

    sched.top = -1;
    if((sched.tasks = malloc(qlen * sizeof(struct task_info))) == NULL) {
        perror("Stack");
        return cleanup_init(-1);
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

            return cleanup_init(-1);
        }
    }

    if(sched_spawn(f, closure, &sched) < 0) {
        fprintf(stderr, "Can't create the initial task\n");
        return cleanup_init(-1);
    }

    for(int i = 0; i < nthreads; ++i) {
        if((pthread_join(threads[i], NULL) != 0)) {
            fprintf(stderr, "Can't wait the thread %d\n", i);
            return cleanup_init(-1);
        }
    }

    return cleanup_init(1);
}

int
cleanup_init(int ret_code)
{
    if(!sched.cond) {
        free(sched.cond);
        sched.cond = NULL;
    }

    if(!sched.tasks) {

        free(sched.tasks);
        sched.tasks = NULL;
    }

    return ret_code;
}

int
sched_spawn(taskfunc f, void *closure, struct scheduler *s)
{
    pthread_mutex_lock(&s->mutex);

    if(s->top + 1 >= s->qlen) {
        pthread_mutex_unlock(&s->mutex);
        errno = EAGAIN;
        fprintf(stderr, "Stack is full\n");
        return -1;
    }

    s->tasks[++s->top] = (struct task_info){closure, f};

    pthread_cond_signal(&s->cond[0]);
    pthread_mutex_unlock(&s->mutex);

    return 0;
}

void *
sched_worker(void *arg)
{
    struct scheduler *s = (struct scheduler *)arg;

    while(1) {
        pthread_mutex_lock(&s->mutex);

        // S'il on a rien à faire
        if(s->top == -1) {
            s->nthsleep++;
            if(s->nthsleep == s->nthreads) {
                // Signal a tout les threads que il n'y a plus rien à faire
                // si un thread attend une tâche
                pthread_cond_broadcast(&s->cond[0]);
                pthread_mutex_unlock(&s->mutex);

                break;
            }

            pthread_cond_wait(&s->cond[0], &s->mutex);
            s->nthsleep--;
            pthread_mutex_unlock(&s->mutex);
            continue;
        }

        // Extrait la tâche de la pile
        taskfunc f = s->tasks[s->top].f;
        void *closure = s->tasks[s->top].closure;
        s->top--;
        pthread_mutex_unlock(&s->mutex);

        // Exécute la tâche
        f(closure, s);
    }

    return NULL;
}
