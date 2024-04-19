#include "../includes/sched.h"

#include <errno.h>
#include <stdio.h>

static struct scheduler sched;

/* Lance une tâche de la pile */
void *
worker_routine(void *arg)
{
    struct scheduler *s = (struct scheduler *)arg;

    while(1) {
        pthread_mutex_lock(&s->mutex);

        if(s->exit == 1) {
            pthread_mutex_unlock(&s->mutex);
            printf("finalement..rien a faire, ciao\n");

            break;
        }

        // S'il on a rien à faire
        if(s->top == -1) {
            // printf("attend..\n");
            pthread_cond_wait(&s->cond, &s->mutex);
            pthread_mutex_unlock(&s->mutex);
            // printf("reveillé!\n");
            continue;
        }
        // printf("lancement tâche #%d\n", s->top);

        // Extrait la tâche de la pile
        taskfunc f = s->tasks[s->top].f;
        void *closure = s->tasks[s->top].closure;
        s->top--;
        pthread_mutex_unlock(&s->mutex);

        // Exécute la tâche
        f(closure, s);

        // Signale s'il n'y a plus rien à faire
        if(s->top == -1) {
            printf("va falloir partir\n");
            pthread_mutex_lock(&s->mutex);
            s->exit = 1;
            pthread_cond_broadcast(&s->cond);
            pthread_mutex_unlock(&s->mutex);
        }
    }

    return NULL;
}

int
sched_init(int nthreads, int qlen, taskfunc f, void *closure)
{
    if(nthreads == 0) {
        nthreads = sched_default_threads();
    }

    // TODO : Actuellement on n'utilises pas qlen
    // => On utilise une pile de taille fixe
    (void)qlen;

    sched.top = -1;
    sched.exit = 0;

    if(pthread_mutex_init(&sched.mutex, NULL) != 0) {
        fprintf(stderr, "Can't init mutex\n");
        return -1;
    }

    if(pthread_cond_init(&sched.cond, NULL) != 0) {
        fprintf(stderr, "Can't init condition variable\n");
        return -1;
    }

    pthread_t threads[nthreads];
    for(int i = 0; i < nthreads; ++i) {
        if(pthread_create(&threads[i], NULL, worker_routine, &sched) != 0) {
            fprintf(stderr, "Can't create threads\n");
            return -1;
        }
    }

    if(sched_spawn(f, closure, &sched) != 0) {
        fprintf(stderr, "Can't create the initial task\n");
        return -1;
    }

    for(int i = 0; i < nthreads; ++i) {
        if((pthread_join(threads[i], NULL) != 0)) {
            fprintf(stderr, "Can't wait the thread %d\n", i);
            return -1;
        }
    }

    return 1;
}

int
sched_spawn(taskfunc f, void *closure, struct scheduler *s)
{
    pthread_mutex_lock(&s->mutex);

    if(s->top + 1 >= MAX_TASKS) {
        pthread_mutex_unlock(&s->mutex);
        errno = EAGAIN;
        fprintf(stderr, "Stack is full\n");
        return -1;
    }

    s->tasks[++s->top] = (taskinfo){f, closure};

    pthread_cond_signal(&s->cond);
    pthread_mutex_unlock(&s->mutex);

    return 0;
}
