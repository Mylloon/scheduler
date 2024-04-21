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

struct robber {
    pthread_cond_t cond;
    pthread_mutex_t mutex;
};

struct scheduler {
    /* Taille des piles */
    int qlen;

    /* Mutex qui protège les piles */
    pthread_mutex_t *mutex;

    /* Nombre de threads instanciés */
    int nthreads;

    /* Piles de tâches */
    struct task_info **tasks;

    /* Liste des threads */
    pthread_t *threads;

    /* Positions actuelle dans la pile */
    int *top;

    /* Infos pour le vol */
    struct robber rob;
};

/* Ordonnanceur partagé */
static struct scheduler sched;

/* Lance une tâche de la pile */
void *sched_worker(void *);

/* Nettoie les opérations effectuées par l'initialisation de l'ordonnanceur */
int sched_init_cleanup(int);

/* sched_spawn sur un thread spécifique */
int sched_spawn_core(taskfunc, void *, struct scheduler *, int);

/* Récupère l'index du thread courant */
int current_thread(struct scheduler *);

int
sched_init(int nthreads, int qlen, taskfunc f, void *closure)
{
    sched.mutex = NULL;
    sched.tasks = NULL;
    sched.threads = NULL;
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

    // Initialisation des infos de vol
    sched.rob =
        (struct robber){PTHREAD_COND_INITIALIZER, PTHREAD_MUTEX_INITIALIZER};

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

    // Ajoute la tâche initiale
    if(sched_spawn_core(f, closure, &sched, 0) < 0) {
        fprintf(stderr, "Can't create the initial task\n");
        return sched_init_cleanup(-1);
    }

    // Créer les threads
    if(!(sched.threads = malloc(sched.nthreads * sizeof(pthread_t *)))) {
        perror("Threads");
        return sched_init_cleanup(-1);
    }
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
    if(sched.mutex) {
        for(int i = 0; i < sched.nthreads; ++i) {
            pthread_mutex_destroy(&sched.mutex[i]);
        }

        free(sched.mutex);
        sched.mutex = NULL;
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

    if(sched.threads) {
        free(sched.threads);
        sched.threads = NULL;
    }

    if(sched.top) {
        free(sched.top);
        sched.top = NULL;
    }

    pthread_cond_destroy(&sched.rob.cond);
    pthread_mutex_destroy(&sched.rob.mutex);

    return ret_code;
}

int
sched_spawn(taskfunc f, void *closure, struct scheduler *s)
{
    int core;
    if((core = current_thread(s)) < 0) {
        fprintf(stderr, "Thread not in list, who am I?\n");
        return -1;
    }

    // On ajoute la tâche sur la pile du thread courant
    return sched_spawn_core(f, closure, s, core);
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
sched_spawn_core(taskfunc f, void *closure, struct scheduler *s, int core)
{
    pthread_mutex_lock(&s->mutex[core]);

    if(s->top[core] + 1 >= s->qlen) {
        pthread_mutex_unlock(&s->mutex[core]);
        errno = EAGAIN;
        fprintf(stderr, "Stack is full\n");
        return -1;
    }

    s->top[core]++;
    s->tasks[core][s->top[core]] = (struct task_info){closure, f};

    pthread_mutex_unlock(&s->mutex[core]);

    return 0;
}

void *
sched_worker(void *arg)
{
    struct scheduler *s = (struct scheduler *)arg;

    // Récupère le processus courant (index tableau)
    int curr_th;
    if((curr_th = current_thread(s)) < 0) {
        fprintf(stderr, "Worker thread not tracked, exiting...\n");
        return NULL;
    }

    while(1) {
        pthread_mutex_lock(&s->rob.mutex);
        pthread_mutex_lock(&s->mutex[curr_th]);

        // Si rien à faire
        if(s->top[curr_th] == -1) {
            // Cherche un thread (avec le + de tâches en attente) à voler
            int stolen = -1;

            pthread_cond_wait(&s->rob.cond, &s->rob.mutex);
            for(int i = 0, max_tasks = -1; i < s->nthreads; ++i) {
                if(i == curr_th) {
                    continue; // On ne se vole pas soi-même
                }

                // Verrouille le mutex du thread candidat
                /* pthread_mutex_lock(&s->mutex[i]); */

                if(s->top[i] > max_tasks) {
                    max_tasks = s->top[i];
                    stolen = i;
                }

                // Déverrouille le mutex du thread candidat
                /* pthread_mutex_unlock(&s->mutex[i]); */
            }

            // Vole une tâche à un autre thread
            if(stolen >= 0) {
                struct task_info theft;
                pthread_mutex_lock(&s->mutex[stolen]);

                // Actuellement on prend la tâche la plus ancienne en
                // inversant la première et la dernière
                // TODO: Récupérer la premiere tâche tout en respectant l'ordre
                theft = s->tasks[stolen][0];
                s->tasks[stolen][0] = s->tasks[stolen][s->top[stolen]];
                s->top[stolen]--;

                pthread_mutex_unlock(&s->mutex[stolen]);

                pthread_mutex_unlock(&s->mutex[curr_th]);
                pthread_mutex_unlock(&s->rob.mutex);

                // Rajoute la tâche sur notre pile
                sched_spawn_core(theft.f, theft.closure, s, curr_th);

                continue;
            }

            pthread_mutex_unlock(&s->mutex[curr_th]);
            pthread_cond_broadcast(&s->rob.cond);
            pthread_mutex_unlock(&s->rob.mutex);
            printf("%d se tire\n", curr_th);
            break;
        }

        // Extrait la tâche de la pile
        taskfunc f = s->tasks[curr_th][s->top[curr_th]].f;
        void *closure = s->tasks[curr_th][s->top[curr_th]].closure;
        s->top[curr_th]--;
        pthread_mutex_unlock(&s->mutex[curr_th]);

        // Exécute la tâche
        f(closure, s);

        pthread_cond_broadcast(&s->rob.cond);
        pthread_mutex_unlock(&s->rob.mutex);
    }

    return NULL;
}
