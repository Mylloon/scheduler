#include "../includes/sched.h"

#include <errno.h>
#include <stdio.h>

static struct scheduler sched;

/* Lance une tâche de la pile */
void *worker_routine(void *arg) {
  struct scheduler *s = (struct scheduler *)arg;

  while (1) {
    // Attente d'un changement d'état
    pthread_cond_wait(&s->cond, &s->mutex);

    pthread_mutex_lock(&s->mutex);
    if (s->top == -1) {
      // Il n'y a plus de tâches à exécuter
      pthread_mutex_unlock(&s->mutex);
      break;
    }

    // Extrait la tâche de la pile
    taskfunc f = s->tasks[s->top];
    void *closure = s->closures[s->top];
    s->top--;

    pthread_mutex_unlock(&s->mutex);

    // Exécute la tâche
    f(closure, s);

    // Signale que la tâche est terminée
    pthread_cond_signal(&s->cond);
  }

  return NULL;
}

int sched_init(int nthreads, int qlen, taskfunc f, void *closure) {
  if (nthreads == 0) {
    nthreads = sched_default_threads();
  }

  // Actuellement on n'utilises pas qlen
  // => On utilise une pile de taille fixe
  (void)qlen;

  sched.top = -1;

  if (pthread_mutex_init(&sched.mutex, NULL) != 0) {
    fprintf(stderr, "Can't init mutex\n");
    return -1;
  }

  if (pthread_cond_init(&sched.cond, NULL) != 0) {
    fprintf(stderr, "Can't init condition variable\n");
    return -1;
  }

  pthread_t threads[nthreads];
  for (int i = 0; i < nthreads; ++i) {
    if (pthread_create(&threads[i], NULL, worker_routine, &sched) != 0) {
      fprintf(stderr, "Can't create threads\n");
      return -1;
    }
  }

  if (sched_spawn(f, closure, &sched) != 0) {
    fprintf(stderr, "Can't create a new task\n");
    return -1;
  }

  for (int i = 0; i < nthreads; ++i) {
    if ((pthread_join(threads[i], NULL) != 0)) {
      fprintf(stderr, "Can't wait the thread %d\n", i);
      return -1;
    }
  }

  return 1;
}

int sched_spawn(taskfunc f, void *closure, struct scheduler *s) {
  pthread_mutex_lock(&s->mutex);

  if (s->top + 1 >= MAX_TASKS) {
    pthread_mutex_unlock(&s->mutex);
    errno = EAGAIN;
    fprintf(stderr, "Stack full\n");
    return -1;
  }

  s->top++;
  s->tasks[s->top] = f;
  s->closures[s->top] = closure;

  pthread_mutex_unlock(&s->mutex);
  pthread_cond_signal(&s->cond);

  return 0;
}
