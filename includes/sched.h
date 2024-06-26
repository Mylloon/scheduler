#pragma once

#include <unistd.h>

struct scheduler;

typedef void (*taskfunc)(void *, struct scheduler *);

/* Renvoie le nombre de coeurs disponible. */
static inline int
sched_default_threads(void)
{
    return sysconf(_SC_NPROCESSORS_ONLN);
}

/* Lance l'ordonnanceur
 * - nthreads : nombre de threads créer par l'ordonnanceur.
 *   Si 0, le nombre de threads sera égal au nombre de coeurs de votre machine
 *
 * - qlen : nombre minimum de tâches simultanées que l’ordonnanceur devra
 *   supporter.
 *   Retourne une erreur si l'utilisateur dépasse qlen
 *
 * - f, closure : tâche initiale
 *
 * Renvoie 1 quand elle a terminé, -1 en cas d'échec d'initialisation
 */
int sched_init(int nthreads, int qlen, taskfunc f, void *closure);

/* Enfile une nouvelle tâche (f, closure) à l'ordonanceur (s)
 *
 * Peut renvoyer -1 avec errno = EAGAIN quand on dépasse la capacité de
 * l'ordonanceur
 */
int sched_spawn(taskfunc f, void *closure, struct scheduler *s);
