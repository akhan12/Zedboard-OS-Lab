/********************************************************************
Copyright 2010-2017 K.C. Wang, <kwang@eecs.wsu.edu>
This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
********************************************************************/
#include "type.h"
extern PROC *running;
extern PROC *readyQueue;
/***************
typedef struct mutex{
  int lock;      // 0=unlocked, 1=locked
  int priority;  // current priority
  PROC *owner;   // pointer to current owner
  PROC *queue;   // blocked PROCs
}MUTEX;
*****************/
#define NMUTEX 32
MUTEX mutex[NMUTEX];
int mutexMap; // bitmap of 32 mutexes
extern MUTEX *mp;

int mutex_init() {
  mutexMap = 0; // all 32 mutexes are FREE initially
}

MUTEX *get_mutex() {
  int i;
  for (i = 0; i < 32; i++) {
    if ((mutexMap & (1 << i)) == 0) { // found a FREE mutex i
      mutexMap |= (1 << i);           // set bit i to 1;
      return &mutex[i];
    }
  }
  printf("out of mutex\n");
  return 0;
}

int put_mutex(MUTEX *mp) {
  int i;
  for (i = 0; i < NMUTEX; i++) {
    if (mp == &mutex[i]) {
      mutexMap &= ~(1 << i);
      return 1;
    }
  }
  return 0;
}

MUTEX *mutex_create() {
  // allocates a MUTEX
  MUTEX *p;
  p = get_mutex(); // MUTEX mutex[NMUTEX] in a link list OR as a bitmap
  if (p == 0) {
    printf("out of MUTEX\n");
    return 0;
  }
  // initialize mutex
  p->lock = 0;
  p->owner = p->queue = (PROC *)0;
  return p;
}

int mutex_destroy(MUTEX *m) {
  if (m == 0)
    return 0;
  put_mutex(m);
  return 1;
}

int mutex_lock(MUTEX *m) {
  PROC *p, *q, *temp; // current owner if mutex is locked

  int SR = int_off();
  if (m == mp)
    printf("TASK%d locking mutex\n", running->pid);

  if (m->lock == 0) { // mutex is unlocked
    m->lock = 1;
    m->owner = running;
  } else {                     // mutex is locked
    if (m->owner == running) { // no recursive mutex locking
      printf("mutex already locked by you!\n");
      return 1;
    }
    printf("TASK%d BLOCK ON MUTEX: owner=%d\n", running->pid, m->owner->pid);
    running->status = BLOCK;
    enqueue(&m->queue, running); // interrupts are off => no need to protect
    tswitch();
  }
  int_on(SR);
  return 1;
}

int mutex_unlock(MUTEX *m) {
  PROC *p;
  int SR = int_off();
  if (m == mp)
    printf("task%d unlocking mutex\n", running->pid);
  if (m->lock == 0 || m->owner != running) {
    printf("mutex_unlock error\n");
    int_on(SR);
    return -1;
  }
  // is owner and mutex was locked
  if (m->queue == 0) { // mutex has no waiters
    m->lock = 0;       // clear lock
    m->owner = 0;      // clear owner
  } else {             // mutex has waiters: unblock one as new owner
    p = dequeue(&m->queue);
    p->status = READY;
    m->owner = p;
    printf("%d mutex_unlock: new owner=task%d\n", running->pid, p->pid);

    // mutex_lock(&readyQueuelock);
    enqueue(&readyQueue, p); // interrupts off => no need to protect
    // mutex_unlock(&readyQueuelock);
    reschedule();
    /*
    if (p->priority > running->priority){
      printf("%d PREEMPT %d NOW!\n", p->pid, running->pid);
      tswitch();
    }
    */
  }
  int_on(SR);
  return 1;
}
