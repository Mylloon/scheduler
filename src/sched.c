#include "../includes/sched.h"

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct task_info {
    void *closure;
    taskfunc f;
};

struct scheduler {
    /* Taille des piles */
    int qlen;

    /* Variable de conditions pour reveillé les threads au besoin */
    pthread_cond_t cond;

    /* Mutex qui protège les piles */
    pthread_mutex_t mutex;

    /* Nombre de threads instanciés */
    int nthreads;

    /* Piles de tâches */
    struct task_info **tasks;

    /* Liste des threads */
    pthread_t *threads;

    /* Compteur des threads dormants */
    int nthsleep;

    /* Stack sous forme de dequeu pour gérer la récupération
     * du premier élément ajouté */
    int *top;
    int *bottom;
};

/* Ordonnanceur partagé */
static struct scheduler sched;

/* Lance une tâche de la pile */
void *sched_worker(void *);

/* Nettoie les opérations effectuées par l'initialisation de l'ordonnanceur */
int sched_init_cleanup(int);

/* Récupère l'index du thread courant */
int current_thread(struct scheduler *);

int
sched_init(int nthreads, int qlen, taskfunc f, void *closure)
{
    sched.tasks = NULL;
    sched.threads = NULL;
    sched.top = NULL;
    sched.bottom = NULL;

    if(qlen <= 0) {
        fprintf(stderr, "qlen must be greater than 0\n");
        return -1;
    }
    sched.qlen = qlen + 1; // circular buffer

    if(nthreads < 0) {
        fprintf(stderr, "nthreads must be greater than 0\n");
        return -1;
    } else if(nthreads == 0) {
        nthreads = sched_default_threads();
    }
    sched.nthreads = 0;

    sched.nthsleep = 0;

    // Initialisation du mutex
    if(pthread_mutex_init(&sched.mutex, NULL) != 0) {
        fprintf(stderr, "Can't init mutex\n");
        return sched_init_cleanup(-1);
    }

    // Initialisation variable de condition
    if(pthread_cond_init(&sched.cond, NULL) != 0) {
        fprintf(stderr, "Can't init varcond\n");
        return sched_init_cleanup(-1);
    }

    // Initialisation du curseur suivant l'état de la pile de chaque processus
    if(!(sched.top = malloc(nthreads * sizeof(int)))) {
        perror("Cursor top stack");
        return sched_init_cleanup(-1);
    }
    if(!(sched.bottom = malloc(nthreads * sizeof(int)))) {
        perror("Cursor bottom stack");
        return sched_init_cleanup(-1);
    }
    for(int i = 0; i < nthreads; ++i) {
        sched.top[i] = 0;
        sched.bottom[i] = 0;
    }

    // Allocation mémoire pour la pile de chaque processus
    if(!(sched.tasks = malloc(nthreads * sizeof(struct task_info *)))) {
        perror("Stack list");
        return sched_init_cleanup(-1);
    }
    for(int i = 0; i < nthreads; ++i) {
        if(!(sched.tasks[i] = malloc(qlen * sizeof(struct task_info)))) {
            fprintf(stderr, "Stack for thread %d: %s\n", i, strerror(errno));
            return sched_init_cleanup(-1);
        }
    }

    // Initialise l'aléatoire
    srand(time(NULL));

    // Créer les threads
    if(!(sched.threads = malloc(nthreads * sizeof(pthread_t)))) {
        perror("Threads");
        return sched_init_cleanup(-1);
    }

    // Ajoute la tâche initiale
    if(sched_spawn(f, closure, &sched) < 0) {
        fprintf(stderr, "Can't create the initial task\n");
        return sched_init_cleanup(-1);
    }

    // Démarre les threads
    for(int i = 0; i < nthreads; ++i) {
        if(pthread_create(&sched.threads[i], NULL, sched_worker, &sched) != 0) {
            fprintf(stderr, "Can't create the thread %d\n", i);

            if(i > 0) {
                fprintf(stderr, ", cancelling already created threads...\n");
                for(int j = 0; j < i; ++j) {
                    if(pthread_cancel(sched.threads[j]) != 0) {
                        fprintf(stderr, "Can't cancel the thread %d\n", j);
                    }
                }
            } else {
                fprintf(stderr, "\n");
            }

            return sched_init_cleanup(-1);
        }
        sched.nthreads++;
    }

    for(int i = 0; i < nthreads; ++i) {
        if((pthread_join(sched.threads[i], NULL) != 0)) {
            fprintf(stderr, "Can't wait the thread %d\n", i);
            return sched_init_cleanup(-1);
        }
    }

    return sched_init_cleanup(1);
}

int
sched_init_cleanup(int ret_code)
{
    pthread_mutex_destroy(&sched.mutex);

    pthread_cond_destroy(&sched.cond);

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

    if(sched.threads) {
        free(sched.threads);
        sched.threads = NULL;
    }

    if(sched.top) {
        free(sched.top);
        sched.top = NULL;
    }

    if(sched.bottom) {
        free(sched.bottom);
        sched.bottom = NULL;
    }

    return ret_code;
}

int
current_thread(struct scheduler *s)
{
    pthread_t current = pthread_self();
    for(int i = 0; i < s->nthreads; i++) {
        if(pthread_equal(s->threads[i], current)) {
            return i;
        }
    }

    return -1;
}

int
sched_spawn(taskfunc f, void *closure, struct scheduler *s)
{
    int th;
    if((th = current_thread(s)) < 0) {
        th = 0;
    }

    pthread_mutex_lock(&s->mutex);

    int next = (s->top[th] + 1) % s->qlen;
    if(next == s->bottom[th]) {
        pthread_mutex_unlock(&s->mutex);
        fprintf(stderr, "Stack is full\n");
        errno = EAGAIN;
        return -1;
    }

    s->tasks[th][s->top[th]] = (struct task_info){closure, f};
    s->top[th] = next;

    pthread_mutex_unlock(&s->mutex);

    return 0;
}

void *
sched_worker(void *arg)
{
    struct scheduler *s = (struct scheduler *)arg;

    // Récupère le processus courant (index tableau)
    int curr_th;
    while((curr_th = current_thread(s)) < 0);

    struct task_info task;
    int found;
    while(1) {
        found = 0;
        pthread_mutex_lock(&s->mutex);

        if(s->bottom[curr_th] != s->top[curr_th]) {
            found = 1;
            s->top[curr_th] = (s->top[curr_th] - 1 + s->qlen) % s->qlen;
            task = s->tasks[curr_th][s->top[curr_th]];
        }

        if(!found) {
            // Vol car aucune tâche trouvée
            for(int i = 0, k = rand() % (s->nthreads + 1), target;
                i < s->nthreads; ++i) {
                target = (i + k) % s->nthreads;

                if(s->bottom[target] != s->top[target]) {
                    // Tâche trouvée
                    found = 1;
                    s->top[target] = (s->top[target] - 1 + s->qlen) % s->qlen;
                    task = s->tasks[target][s->top[target]];
                    break;
                }
            }

            // Aucune tâche à faire
            if(!found) {
                s->nthsleep++;
                // Ne partir que si tout le monde dort
                if(s->nthsleep >= s->nthreads) {
                    pthread_cond_broadcast(&s->cond);
                    pthread_mutex_unlock(&s->mutex);

                    break;
                }
                pthread_cond_wait(&s->cond, &s->mutex);
                s->nthsleep--;
                pthread_mutex_unlock(&s->mutex);
                continue;
            }
        }

        pthread_cond_broadcast(&s->cond);
        pthread_mutex_unlock(&s->mutex);

        // Exécute la tâche
        task.f(task.closure, s);
    }

    return NULL;
}
