/* scheduler.c
#AUTHOR: Jhi Morris (19173632)
#MODIFIED: 27-04-19
#PURPOSE: Main class for the scheduler simulator program.
*/

#include "scheduler.h"

/* main
#RETURN: int (return code)
#ARGUMENTS: int (argc), char* array (arguments)
#PURPOSE: Prepares to run scheduler routine by loading required files and
      handling associated errors.*/
int main(int argc, char *argv[])
{
  FILE *input, *output;
  ErrorFlag error = NONE;
  TaskQueue *queue = malloc(sizeof(TaskQueue));

  if(argc == 3) //no args are optional
  { //args: 1=task_file, 2=queue capacity
    queue->head = NULL;
    queue->spaceLeft = atoi(argv[2]); //done here because of args
    queue->maxCap = queue->spaceLeft;
    pthread_mutex_init(&queue->mutex, NULL);

    if(queue->spaceLeft > 0)
    {
      input = fopen(argv[1], "r");

      if(input != NULL)
      {
        output = fopen("simulation_log", "w");

        if(output != NULL)
        {
          scheduler(queue, input, output); //begin actual operation
        }
        else
        {
          error = BAD_LOG;
        }
      }
      else
      {
        error = BAD_INPUT;
      }
    }
    else
    {
      error = INVALID_CAP;
    }
  }
  else
  {
    error = WRONG_ARGS;
  }

  pthread_mutex_destroy(&queue->mutex);
  free(queue);

  switch(error)
  {
    case NONE:
      fclose(input);
      fclose(output);
      break;
    case BAD_INPUT:
      perror("Unable to load input file:");
      break;
    case BAD_LOG:
      perror("Unable to load log file:");
      break;
    case INVALID_CAP:
      fprintf(stderr, "Second argument not recognized. Expecting for queue size an integer greater than zero.\n");
      break;
    case WRONG_ARGS:
      fprintf(stderr, "Invalid arguments.\nExpecting form: scheduler \"/path/to the/task_file\" queueSize. Where queueSize is integer greater than zero.\n");
      break;
  }
}

/* scheduler
#RETURN: void
#ARGUMENTS: TaskQueue (Ready-Queue), FILE* (task_file), FILE* (log file)
#PURPOSE: Begins task() and cpu() threads, then handles wrap-up and logging.*/
void scheduler(TaskQueue *queue, FILE *input, FILE *output)
{
  int i;
  pthread_t reader;
  pthread_t cpus[NUM_CPUS];
  cpuData *cpuDatums[NUM_CPUS];
  int numTasks = 0;
  int totalWaitingTime = 0;
  int totalTurnaroundTime = 0;
  int *readFinish = malloc(sizeof(int)); //signal that task() is done
  taskData *read = malloc(sizeof(taskData));

  *readFinish = 0;

  read->log = malloc(sizeof(logAccess));
  read->log->output = output;
  pthread_mutex_init(&read->log->mutex, NULL);
  read->input = input;
  read->queue = queue;

  pthread_create(&reader, NULL, task, read);

  for(i = 0; i < NUM_CPUS; i++)
  {
    cpuDatums[i] = malloc(sizeof(cpuData));

    cpuDatums[i]->cpuNum = i;
    cpuDatums[i]->numTasks = 0;
    cpuDatums[i]->totalWaitingTime = 0;
    cpuDatums[i]->totalTurnaroundTime = 0;
    cpuDatums[i]->queue = queue;
    cpuDatums[i]->readFinish = readFinish;
    cpuDatums[i]->log = read->log;

    pthread_create(&cpus[i], NULL, cpu, cpuDatums[i]);
  }

  pthread_join(reader, NULL);
  *readFinish = 1; //now CPUs can end if queue is empty

  for(i = 0; i < NUM_CPUS; i++)
  { //slightly inefficient as it won't end a CPU thread until earlier CPU threads are done
    pthread_join(cpus[i], NULL);

    numTasks += cpuDatums[i]->numTasks;
    totalWaitingTime += cpuDatums[i]->totalWaitingTime;
    totalTurnaroundTime += cpuDatums[i]->totalTurnaroundTime;

    free(cpuDatums[i]);
  }

  /*log entry:
  Number of tasks: numTasks
  Average waiting time: totalWaitingTime/numTasks
  Average turn around time: totalTurnaroundTime/numTasks*/
  pthread_mutex_lock(&read->log->mutex);
  fprintf(read->log->output, "Number of tasks: %d\nAverage waiting time: %ld seconds\nAverage turn around time: %ld seconds\n", numTasks, (totalWaitingTime/(long int)numTasks), (totalTurnaroundTime/(long int)numTasks));
  pthread_mutex_unlock(&read->log->mutex);

  pthread_mutex_destroy(&read->log->mutex);
  free(readFinish);
  free(read->log);
  free(read);
}

/* task
#RETURN: void* (unused)
#ARGUMENTS: void* (expecting taskData struct pointer)
#PURPOSE: Uses configuration data contained within taskData struct to load
      process tasks from task_file, inserting them into the queue.*/
void *task(void *in)
{
  TaskQueueNode *node;
  taskData *data = (taskData*)in; //cast now so as not to repeat
  int numTasks = 0; //running total for logging purposes
  int unrecognizedLines = 0;
  int inTask, inBurst, ret, done;

  while((ret = fscanf(data->input, "%i %i", &inTask, &inBurst)) != EOF)
  {
    if(ret == 2 && inTask > 0 && inBurst > 0) //validate
    {
      done = 0;
      numTasks++;

      //create new process node
      node = malloc(sizeof(TaskQueueNode));
      node->next = NULL;
      node->taskNum = inTask;
      node->burstDuration = inBurst;
      node->arrivalTime = time(0);

      while(!done)
      { //TODO convert this to use cond
        //add to queue
        pthread_mutex_lock(&data->queue->mutex);

        if(data->queue->spaceLeft > 0)
        {
          insertQueue(data->queue, node);
          done = 1;
        } //if there's no space, unlock and try later

        pthread_mutex_unlock(&data->queue->mutex);
      }

      /*log entry:
      taskNum: burstDuration
      Arrival time: arrivalTime*/
      pthread_mutex_lock(&data->log->mutex);
      fprintf(data->log->output, "Task %d: %d\nArrival time: %d:%d:%d\n", node->taskNum, node->burstDuration, TIME(node->arrivalTime));
      pthread_mutex_unlock(&data->log->mutex);
    }
    else
    {
      unrecognizedLines++; //inc and skip line
    }
  }

  /*log entry:
  Number of tasks put into Ready-Queue: numTasks
  Terminate at time: time(0)*/
  pthread_mutex_lock(&data->log->mutex);
  fprintf(data->log->output, "Number of tasks put into Ready-Queue: %d\nInvalid lines skipped: %d\nTerminate at time: %d:%d:%d\n", numTasks, unrecognizedLines, TIME(time(0)));
  pthread_mutex_unlock(&data->log->mutex);

  return NULL;
}

/* cpu
#RETURN: void* (unused)
#ARGUMENTS: void* (expecting cpuData struct pointer)
#PURPOSE: Processes tasks from queue (sleeping for burstDuration seconds) and
      logs appropriate information.*/
void *cpu(void *in)
{
  cpuData *data = (cpuData*)in;
  TaskQueueNode *task;
  time_t serviceTime, completionTime;

  while(!(*(data->readFinish)) || (data->queue->maxCap != data->queue->spaceLeft))
  { //keep looping until task() says it is done and queue is empty
    pthread_mutex_lock(&data->queue->mutex); //need to lock before checking if empty, incase pre-empted between checking and locking

    if(data->queue->maxCap > data->queue->spaceLeft) //TODO change this to use cond
    { //ie if not empty
      task = popQueue(data->queue);

      pthread_mutex_unlock(&data->queue->mutex);

      serviceTime = time(0);
      data->totalWaitingTime += (serviceTime - task->arrivalTime);

      /*log entry:
      Statistics for CPU-data->cpuNum:
      Task task->taskNum
      Arrival time: task->arrivalTime
      Service time: serviceTime*/
      pthread_mutex_lock(&data->log->mutex);
      fprintf(data->log->output, "Statistics for CPU-%d:\nTask %d\nArrival time: %d:%d:%d\nService time: %d:%d:%d\n", data->cpuNum, task->taskNum, TIME(task->arrivalTime), TIME(serviceTime));
      pthread_mutex_unlock(&data->log->mutex);

      sleep(task->burstDuration); //"process" task

      completionTime = time(0);
      data->totalTurnaroundTime += (completionTime - task->arrivalTime);

      /*log entry:
      Statistics for CPU-data->cpuNum:
      Task task->taskNum
      Arrival time: task->arrivalTime
      Completion time: completionTime*/
      pthread_mutex_lock(&data->log->mutex);
      fprintf(data->log->output, "Statistics for CPU-%d:\nTask %d\nArrival time: %d:%d:%d\nService time: %d:%d:%d\n", data->cpuNum, task->taskNum, TIME(task->arrivalTime), TIME(completionTime));
      pthread_mutex_unlock(&data->log->mutex);

      free(task);
      data->numTasks++;
    } //if empty, skip activity and try again later
    else
    {
      pthread_mutex_unlock(&data->queue->mutex);
    }
  }

  /*log entry:
  CPU-data->cpuNum terminates after servicing data->numTasks tasks.*/
  pthread_mutex_lock(&data->log->mutex);
  fprintf(data->log->output, "CPU-%d terminates after servicing %d tasks.\n", data->cpuNum, data->numTasks);
  pthread_mutex_unlock(&data->log->mutex);

  return NULL;
}

/* insertQueue
#RETURN: void
#ARGUMENTS: TaskQueue* (queue), TaskQueueNode (node to be added)
#PURPOSE: Inserts node into tail of queue and decrements space left.*/
void insertQueue(TaskQueue *queue, TaskQueueNode *node)
{
  TaskQueueNode *next;

  if(queue->head == NULL)
  { //if the queue is empty, attach right away
    queue->head = node;
  }
  else
  {
    next = queue->head;

    while(next->next != NULL)
    { //traverse to find end of queue
      next = next->next;
    }

    next->next = node;
  }

  queue->spaceLeft--;
}

/* popQueue
#RETURN: TaskQueueNode* (node removed from queue)
#ARGUMENTS: TaskQueue* (queue)
#PURPOSE: Pops node from head of queue and increments space left.*/
TaskQueueNode *popQueue(TaskQueue *queue)
{
  TaskQueueNode *node;

  node = queue->head;
  queue->head = queue->head->next; //set head to second item in queue; removing first item

  queue->spaceLeft++;

  return node;
}
