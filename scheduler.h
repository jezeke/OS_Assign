/* scheduler.h
#AUTHOR: Jhi Morris (19173632)
#MODIFIED: 27-04-19
#PURPOSE: Header file for scheduler.c.
*/

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <stdint.h>
#include <pthread.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <unistd.h>

//number of CPUs. Default is 3
#define NUM_CPUS 3

//conversion from unix time to hh:mm:ss
#define HOUR(n) (int)(((n)%86400)/3600)
#define MIN(n) (int)(((n)%3600)/60)
#define SEC(n) (int)(((n)%3600)%60)

//janky shortcut used for printf-like arguments. pairs with "%d:%d:%d" mask
#define TIME(n) HOUR(n), MIN(n), SEC(n)

/* ErrorFlag
#PURPOSE: Used by main() to handle error flags more cleanly.*/
typedef enum ErrorFlag {NONE, BAD_INPUT, BAD_LOG, INVALID_CAP, WRONG_ARGS} ErrorFlag;

/* logAccess
#PURPOSE: Stores data required to make log entries.*/
typedef struct logAccess
{
  FILE *output;
  pthread_mutex_t mutex;
} logAccess;

/* TaskQueueNode
#PURPOSE: Stores information about task as a queue node.*/
typedef struct TaskQueueNode
{
  struct TaskQueueNode *next;
  int taskNum;
  int burstDuration;
  time_t arrivalTime;
} TaskQueueNode;

/* TaskQueue
#PURPOSE: Stores data required to access task queue safely.*/
typedef struct TaskQueue
{
  TaskQueueNode *head;
  int spaceLeft;
  int maxCap;
  pthread_mutex_t mutex;
} TaskQueue;

/* taskData
#PURPOSE: Stores data required by task() thread to load from task_file into
      Ready-Queue and to log data.*/
typedef struct taskData
{
  FILE *input;
  TaskQueue *queue;
  int *readFinish;
  logAccess *log;
} taskData;

/* cpuData
#PURPOSE: Stores data required by cpu() thread to process task and to log data.*/
typedef struct cpuData
{
  int cpuNum;
  int numTasks;
  time_t totalWaitingTime;
  time_t totalTurnaroundTime;
  int *readFinish;
  TaskQueue *queue;
  logAccess *log;
} cpuData;

void scheduler(TaskQueue *queue, FILE *input, FILE *output);

void *task(void *in);

void *cpu(void *in);

void insertQueue(TaskQueue *queue, TaskQueueNode *node);

TaskQueueNode *popQueue(TaskQueue *queue);
