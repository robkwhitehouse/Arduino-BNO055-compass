#include <LinkedList.h>

/*
 * Simple RealTimeScheduler for Arduino
 * R K Whitehouse - Jan 2021
 *
 * This is actually a "best endeavours" real time scheduler.
 * The only guarantee is the the task will not be executed *before* the scheduled time
 * but it might be delayed if the CPU is overloaded
 */

// --- MAX_TASKS can be as big as you need but obviously it uses memory
#define MAX_TASKS 20

// --- Individual task descriptor structure
struct Task { 
  void (*proc)();               // Pointer to the procedure 
  boolean repeating = false;    // 
  unsigned long period = 0;     // Num microseconds between executions
  unsigned long when = 0;       // time of next execution (dynamically updated)
};






class Scheduler {

  private:
// -- List of tasks - will be chronologically ordered (soonest task first) 
// -- The LinkedList class uses new() and delete() to manage memory
    LinkedList<Task*> taskList = LinkedList<Task*>();   
       
  public:
    unsigned add(Task *task) {
      int i;
      int listSize; 
      
      listSize = taskList.size();
      if (listSize >= MAX_TASKS) {
        Serial.println("Error: Too many tasks");
      } else {
        //Find where to insert the task
        for ( i=0; i < listSize; i++ ) { 
            Task* iTask = taskList.get(i);
            if (iTask->when > task->when) break;
        }       
        taskList.add(i,task);      
      }
      return i;
    }

    void begin() { 
    }

    void dispatch() {
      Task* task;
      register byte i;
      unsigned long now;

      now = micros();
      for (i = 0; i < taskList.size(); i++) {    
        task = taskList.get(i);
        if ( now < task->when ) break; //not yet time to run next task in list, break out of loop
        else {
          task->proc();                //Run the task
          taskList.remove(i);          //Always remove it from the schedule - this NOT calling Dispatcher::remove() below 
          if (task->repeating) {
             task->when = now + task->period;
            add(task);                 //Re-insert the Task so that it appears in the right chrono order
          } else delete(task);         // One off task - delete after run (prevent memory leak)   
        }
      }
    }
    
    // N.B. This method is NOT used by the dispatcher (above) which calls LinkedList.remove() directly
    // Intended for removing repeating tasks that are no longer required
    void remove(int taskIndex) {
      delete(taskList.get(taskIndex)); //free up the task object memory
      taskList.remove(taskIndex);      //free up the linked list item
    }
};
