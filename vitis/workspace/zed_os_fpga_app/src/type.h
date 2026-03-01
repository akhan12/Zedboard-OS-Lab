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

#ifndef TYPE_H
#define TYPE_H

// defines.h file

#include "xil_types.h"
#include "xil_printf.h"
#include "xil_exception.h"
#define  SSIZE 1024

#define  FREE   0
#define  READY  1
#define  SLEEP  2
#define  BLOCK  3
#define  ZOMBIE 4
#define  PAUSE  5
#define  printf  kprintf

#define BLUE   0
#define GREEN  1
#define RED    2
#define CYAN   3
#define YELLOW 4
#define PURPLE 5
#define WHITE  6

typedef struct semaphore{
  int value;
  struct proc *queue;
}SEMAPHORE;

// to support priority inversion, each PROC must have a pointer to a MUTEX or
// SEMAPHORE, which points back to the PROC.
typedef struct proc{
  struct proc *next;
  int    *ksp;
  int    status;
  int    pid;

  int    priority;
 
  int    ppid;
  struct proc *parent;
  int    event;
  int    exitCode;
  int    pause; 
  int    kstack[SSIZE];
}PROC;

extern PROC *running, *freeList, *readyQueue, *sleepList, *pauseList;

#endif // TYPE_H
