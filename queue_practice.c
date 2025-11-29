#include <stdio.h>
#include <signal.h>

#define QSZ 20

int global_array[QSZ];

unsigned int RIDX = 0, WIDX = 0, nitems = 0;  

int enqueue(int val) {
    if (nitems >= QSZ) {
        return -1;
    }
    
    global_array[WIDX] = val;
    WIDX++;
    WIDX %= QSZ;
    nitems++;

    return 0;
}

int dequeue() {
    if (nitems == 0) {
	return -1;
    }

    int data = global_array[RIDX];
    RIDX++;
    RIDX %= QSZ;  
    nitems--;
    
    return data;
}

void sighandler(int sig) {
   static int data = 0;
   printf("Got Signal %d\n", sig);
   enqueue(data++);
}

int main(int argc, char **argv) {
   signal(SIGINT, sighandler);
   while(1) {
      int data = dequeue();
      if (data != -1) {
         printf("[main] Dequeued %d\n");
      }
   }
   return 0;
}


