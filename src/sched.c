#include "../includes/sched.h"

#include <pthread.h>
#include <stdio.h>

int sched_init(int nthreads, int qlen, taskfunc f, void *closure) {
  sched_spawn(f, closure, NULL);
  return 0;
}

int sched_spawn(taskfunc f, void *closure, struct scheduler *s) {
  pthread_t thread;
  int errno;

  // Création d'un thread pour la tâche
  if ((errno = pthread_create(&thread, NULL, (void *)f, closure)) != 0) {
    fprintf(stderr, "pthread_create error %d\n", errno);
    return -1;
  }

  // Attend la fin du thread
  if ((errno = pthread_join(thread, NULL)) != 0) {
    fprintf(stderr, "error %d\n", errno);
    return -1;
  }

  return 0;
}
