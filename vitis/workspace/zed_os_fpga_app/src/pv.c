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

int P(struct semaphore *s)
{
  int SR = int_off();
  lock();
  s->value--;
  if (s->value < 0){
    running->status = BLOCK;
    enqueue(&s->queue, running);
    tswitch();
  }
  unlock();
}

int V(struct semaphore *s)
{
  PROC *p;
  int SR = int_off();
  s->value++;
  if (s->value <= 0){
    p = dequeue(&s->queue);
    p->status = READY;
    enqueue(&readyQueue, p);
  }
  int_on(SR);
}

int P_int(struct semaphore *s)
{

  s->value--;
  if (s->value < 0){
    running->status = BLOCK;
    enqueue(&s->queue, running);
    tswitch();
  }
}

int V_int(struct semaphore *s)
{
  PROC *p;
  s->value++;
  if (s->value <= 0){
    p = dequeue(&s->queue);
    p->status = READY;
    enqueue(&readyQueue, p);
  }

}
