#include "../includes/sched.h"

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

/* Tâche */
struct task_info {
    void *closure;
    taskfunc f;
};

/* Structure de chaque thread */
struct worker {
    /* Premier élément du deque (dernier ajouter) */
    int bottom;

    /* Mutex qui protège cette structure */
    pthread_mutex_t mutex;

    /* Deque de tâches */
    struct task_info *tasks;

    /* Thread */
    pthread_t thread;

    /* Dernier élément du deque (premier ajouter) */
    int top;
};

/* Scheduler partagé */
struct scheduler {
    /* Condition threads dormant */
    pthread_cond_t cond;

    /* Mutex qui protège cette structure */
    pthread_mutex_t mutex;

    /* Nombre de threads instanciés */
    int nthreads;

    /* Compteur des threads dormants */
    int nthsleep;

    /* Taille deque */
    int qlen;

    /* Liste de workers par threads */
    struct worker *workers;
};

/* Lance une tâche de la pile */
void *sched_worker(void *);

/* Nettoie les opérations effectuées par l'initialisation de l'ordonnanceur */
int sched_init_cleanup(struct scheduler, int);

/* Récupère l'index du thread courant
 *
 * Assume que le mutex de l'ordonnanceur est verrouillé */
int current_thread(struct scheduler *);

int
sched_init(int nthreads, int qlen, taskfunc f, void *closure)
{
    static struct scheduler sched;
    sched.workers = NULL;

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

    // Initialisation variable de condition
    if(pthread_cond_init(&sched.cond, NULL) != 0) {
        fprintf(stderr, "Can't init condition variable\n");
        return sched_init_cleanup(sched, -1);
    }

    // Initialisation du mutex
    if(pthread_mutex_init(&sched.mutex, NULL) != 0) {
        fprintf(stderr, "Can't init mutex\n");
        return sched_init_cleanup(sched, -1);
    }

    sched.nthsleep = 0;

    // Initialize workers
    if(!(sched.workers = malloc(nthreads * sizeof(struct worker)))) {
        perror("Workers");
        return -1;
    }
    for(int i = 0; i < nthreads; ++i) {
        sched.workers[i].bottom = 0;
        sched.workers[i].top = 0;

        // Initialisation mutex
        if(pthread_mutex_init(&sched.workers[i].mutex, NULL) != 0) {
            fprintf(stderr, "Can't init mutex %d\n", i);
            return sched_init_cleanup(sched, -1);
        }

        // Allocation mémoire deque
        if(!(sched.workers[i].tasks =
                 malloc(sched.qlen * sizeof(struct task_info)))) {
            fprintf(stderr, "Thread %d: ", i);
            perror("Deque list");
            return sched_init_cleanup(sched, -1);
        }
    }

    // Initialise l'aléatoire
    srand(time(NULL));

    // Création des threads
    for(int i = 0; i < nthreads; ++i) {
        pthread_mutex_lock(&sched.mutex);
        if(pthread_create(&sched.workers[i].thread, NULL, sched_worker,
                          (void *)&sched) != 0) {
            fprintf(stderr, "Can't create thread %d\n", i);

            // Annule les threads déjà créer
            for(int j = 0; j < i; ++j) {
                pthread_cancel(sched.workers[j].thread);
            }

            return sched_init_cleanup(sched, -1);
        }

        sched.nthreads++;

        pthread_mutex_unlock(&sched.mutex);
    }

    // Ajoute la tâche initiale
    if(sched_spawn(f, closure, &sched) < 0) {
        fprintf(stderr, "Can't queue the initial task\n");
        return sched_init_cleanup(sched, -1);
    }

    // Attend la fin des threads
    for(int i = 0; i < nthreads; ++i) {
        if((pthread_join(sched.workers[i].thread, NULL) != 0)) {
            fprintf(stderr, "Can't wait the thread %d\n", i);

            // Quelque chose s'est mal passé, on annule les threads en cours
            for(int j = 0; j < nthreads; ++j) {
                if(j != i) {
                    pthread_cancel(sched.workers[j].thread);
                }
            }

            return sched_init_cleanup(sched, -1);
        }
    }

    return sched_init_cleanup(sched, 1);
}

int
sched_init_cleanup(struct scheduler s, int ret_code)
{
    pthread_cond_destroy(&s.cond);

    pthread_mutex_destroy(&s.mutex);

    if(s.workers) {
        for(int i = 0; i < s.nthreads; ++i) {
            pthread_mutex_destroy(&s.workers[i].mutex);

            free(s.workers[i].tasks);
            s.workers[i].tasks = NULL;
        }

        free(s.workers);
        s.workers = NULL;
    }

    return ret_code;
}

int
current_thread(struct scheduler *s)
{
    pthread_t current = pthread_self();

    for(int i = 0; i < s->nthreads; ++i) {
        if(pthread_equal(s->workers[i].thread, current)) {
            return i;
        }
    }

    return -1;
}

int
sched_spawn(taskfunc f, void *closure, struct scheduler *s)
{
    int th;

    pthread_mutex_lock(&s->mutex);
    if((th = current_thread(s)) < 0) {
        th = 0;
    }
    pthread_mutex_unlock(&s->mutex);

    pthread_mutex_lock(&s->workers[th].mutex);

    int next = (s->workers[th].bottom + 1) % s->qlen;
    if(next == s->workers[th].top) {
        pthread_mutex_unlock(&s->workers[th].mutex);
        fprintf(stderr, "Stack is full\n");
        errno = EAGAIN;
        return -1;
    }

    s->workers[th].tasks[s->workers[th].bottom] =
        (struct task_info){closure, f};
    s->workers[th].bottom = next;

    pthread_mutex_unlock(&s->workers[th].mutex);

    return 0;
}

void *
sched_worker(void *arg)
{
    struct scheduler *s = (struct scheduler *)arg;

    // Récupère le processus courant (index tableau)
    int curr_th;

    pthread_mutex_lock(&s->mutex);
    while((curr_th = current_thread(s)) < 0);
    pthread_mutex_unlock(&s->mutex);

    struct task_info task;
    int found;
    while(1) {
        found = 0;
        pthread_mutex_lock(&s->workers[curr_th].mutex);

        if(s->workers[curr_th].top != s->workers[curr_th].bottom) {
            found = 1;
            s->workers[curr_th].bottom =
                (s->workers[curr_th].bottom - 1 + s->qlen) % s->qlen;
            task = s->workers[curr_th].tasks[s->workers[curr_th].bottom];
        }
        pthread_mutex_unlock(&s->workers[curr_th].mutex);

        if(!found) {
            // Vol car aucune tâche trouvée
            pthread_mutex_lock(&s->mutex);
            int nthreads = s->nthreads;
            pthread_mutex_unlock(&s->mutex);

            for(int i = 0, k = rand() % (nthreads + 1), target; i < nthreads;
                ++i) {
                target = (i + k) % nthreads;

                pthread_mutex_lock(&s->workers[target].mutex);
                if(s->workers[target].top != s->workers[target].bottom) {
                    // Tâche trouvée
                    found = 1;
                    s->workers[target].bottom =
                        (s->workers[target].bottom - 1 + s->qlen) % s->qlen;
                    task = s->workers[target].tasks[s->workers[target].bottom];

                    pthread_mutex_unlock(&s->workers[target].mutex);
                    break;
                }
                pthread_mutex_unlock(&s->workers[target].mutex);
            }

            // Aucune tâche à faire
            if(!found) {
                pthread_mutex_lock(&s->mutex);
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
        pthread_cond_signal(&s->cond);

        // Exécute la tâche
        task.f(task.closure, s);
    }

    return NULL;
}
