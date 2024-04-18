#include "../includes/sched.h"

#include <errno.h>
#include <stdio.h>

static struct scheduler sched;

/* Lance une tâche de la pile */
void *worker_routine(void *arg) {
  struct scheduler *s = (struct scheduler *)arg;

  while (1) {
    pthread_mutex_lock(&s->mutex);

    // S'il n'y a plus de tâches à exécuter
    if (s->top == -1) {
      // printf("rien a faire, on attend\n");
      pthread_cond_wait(&s->cond, &s->mutex);
      pthread_mutex_unlock(&s->mutex);
      continue;
    }
    // printf("on a un truc a faire!! (top: %d)\n", s->top);

    // Extrait la tâche de la pile
    taskfunc f = s->tasks[s->top].f;
    void *closure = s->tasks[s->top].closure;
    s->top--;

    pthread_mutex_unlock(&s->mutex);

    // Exécute la tâche
    // printf("lance la tache\n");
    f(closure, s);

    // Signale que la tâche est terminée
    // printf("tache terminée\n");
    pthread_cond_signal(&s->cond);
  }

  return NULL;
}

int sched_init(int nthreads, int qlen, taskfunc f, void *closure) {
  if (nthreads == 0) {
    nthreads = sched_default_threads();
  }

  // TODO : Actuellement on n'utilises pas qlen
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
    fprintf(stderr, "Can't create the initial task\n");
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
  // printf("attend spawn\n");
  pthread_mutex_lock(&s->mutex);

  if (s->top + 1 >= MAX_TASKS) {
    pthread_mutex_unlock(&s->mutex);
    errno = EAGAIN;
    fprintf(stderr, "Stack is full\n");
    return -1;
  }

  s->tasks[++s->top] = (taskinfo){f, closure};

  pthread_mutex_unlock(&s->mutex);

  pthread_cond_signal(&s->cond);

  return 0;
}
