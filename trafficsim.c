#include <stdio.h>
#include <stdlib.h>
#include <linux/unistd.h>
#include <sys/mman.h>
#include <time.h>

//create the semaphore node and queue
struct semNode{
	struct semNode* next;
	struct task_struct* task;
};

struct cs1550_sem{
	int value;
	struct semNode* head;
	struct semNode* tail;
};

//create the syscall macros
void down(struct cs1550_sem *sem) {
    syscall(441, sem);
}

void up(struct cs1550_sem *sem){
    syscall(442, sem);
}


int main(void){

    srand(time(NULL));

    int buffer_size = 12;
    int semaphores = 6;
    int *awake;
    int *north_cars_in;
    int *south_cars_in;
    int *direction;
    int *north_cars_out;
    int *south_cars_out;
    time_t *start_time;
    // (# of semaphores * sizeof the semaphore struct) + (my 3 int variables * sizeof an int)
    void *sem_ptr = mmap(NULL, (semaphores*sizeof(struct cs1550_sem)), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);
    void *num_ptr = mmap(NULL, (6*sizeof(int)) + (sizeof(time_t)), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);

    printf("\nTraffic Simulation is starting... Creating car queues...\n\n");
    printf("The Flagperson is now asleep\n\n");

    //treat the cars as two queues
    struct cs1550_sem *north_empty = (struct cs1550_sem*)sem_ptr;
    north_empty->value = buffer_size;
    north_empty->head = NULL;
    north_empty->tail = NULL;

    struct cs1550_sem *north_full = north_empty+1;
    north_full->value = 0;
    north_full->head = NULL;
    north_full->tail = NULL;

    struct cs1550_sem *south_empty = north_full + 1;
    south_empty->value = buffer_size;
    south_empty->head = NULL;
    south_empty->tail = NULL;

    struct cs1550_sem *south_full = south_empty +1;
    south_full->value = 0;
    south_full->head = NULL;
    south_full->tail = NULL;

    //create a mutex to prevent deadlock
    struct cs1550_sem *mutex = south_full + 1;
    mutex->value = 1;
    mutex->head = NULL;
    mutex->tail = NULL;

    //semaphore for the road to prevent more than one car travelling
    struct cs1550_sem *road = mutex + 1;
    road->value = 0;
    road->head = NULL;
    road->tail = NULL;

    //our honk signal
    awake = (int*)num_ptr;
    *awake = 0;

    north_cars_in = awake + 1;
    *north_cars_in = 0;

    south_cars_in = north_cars_in + 1;
    *south_cars_in = 0;

    //direction = 0 == north
    //direction = 1 == south
    direction = south_cars_in + 1;
    *direction = 0;
    
    //keeps track of the number of cars let through the intersection from the north
    north_cars_out = direction + 1;
    *north_cars_out = 0;

    //keeps track of the number of cars let through the intersection from the south
    south_cars_out = north_cars_out + 1;
    *south_cars_out = 0;

    start_time = south_cars_out + 1;
    *start_time = time(NULL);

    if(fork() == 0){
        //north producer
        if (fork() == 0){

            while(1){
                down(north_empty);
                down(mutex);

                *north_cars_in += 1;

                //if flagger is asleep, honk to wake him up
                if (*awake == 0){
                    //wake up the flagger
                    *awake = 1;
                    //tell flagger to let in the north cars
                    *direction = 0;
                    printf("Car %d coming from the NORTH direction, blew their horn at time %d\n\n", *north_cars_in, time(NULL) - *start_time);
                    printf("The flagperson is now AWAKE\n\n");
                
                //If flagger is awake
                }else{
                    printf("Car %d coming from the NORTH direction arrived in the queue at time %d\n\n", *north_cars_in, time(NULL) - *start_time);
                }

                up(mutex);
                up(north_full);
                up(road);

                if(rand()%10 >= 7){
                    printf("No new cars to the NORTH, sleeping for 10 seconds...\n\n");
                    sleep(10);
                }
            }
        //south producer
        }else{
            while(1){
                down(south_empty);
                down(mutex);

                *south_cars_in += 1;

                //if flagger is asleep, honk to wake him up
                if (*awake == 0){
                    //wake up the flagger
                    *awake = 1;
                    //tell the flagger which side to let through
                    *direction = 1;
                    printf("Car %d coming from the SOUTH direction, blew their horn at time %d\n\n", *south_cars_in, time(NULL) - *start_time);
                    printf("The flagperson is now AWAKE\n\n");
                
                //If flagger is awake
                }else{
                    printf("Car %d coming from the SOUTH direction arrived in the queue at time %d\n\n", *south_cars_in, time(NULL) - *start_time);
                }

                up(mutex);
                up(south_full);
                up(road);

                if(rand()%10 >= 7){
                    printf("No new cars to the SOUTH, sleeping for 10 seconds...\n\n");
                    sleep(10);
                }
            }
        }
    //flagperson consumer
    }else{
        while(1){

            down(road);
            down(mutex);
            
            //check both sides for cars
            if (*north_cars_in - *north_cars_out == 0 && *south_cars_in - *south_cars_out== 0 && *awake == 1){
                printf("The flagperson is now ASLEEP\n\n");
                *awake = 0;
                up(mutex);
            }else if(*north_cars_in - *north_cars_out > 0 || *south_cars_in - *south_cars_out > 0){
            
                //if the other side is full
                if (*direction == 0 && *south_cars_in - *south_cars_out >= 12){
                    *direction = 1;
                }else if (*direction == 1 && *north_cars_in - *north_cars_out >= 12){
                    *direction = 0;
                }

                //let the north cars through
                if(*direction == 0){

                    down(north_full);

                    //track the number of the next car through
                    *north_cars_out += 1;
                    //sleep(1);
                    printf("Car %d coming from the NORTH direction left the construction zone at time %d\n\n", *north_cars_out, time(NULL) - *start_time);
                    //debug print statement
                    //printf("There are %d north cars and %d south cars\n\n", *north_cars_in - *north_cars_out, *south_cars_in - *south_cars_out);
                    sleep(1);
                    //if the north is empty or south reaches 12 cars, switch sides
                    if (*north_cars_in - *north_cars_out == 0 || *south_cars_in - *south_cars_out == 12){
                        *direction = 1;
                    }

                    up(mutex);
                    up(north_empty);
                    
                //let the south cars through
                }else if (*direction == 1){

                    down(south_full);

                    //track the number of the next car through
                    *south_cars_out += 1;
                    
                    printf("Car %d coming from the SOUTH direction left the construction zone at time %d\n\n", *south_cars_out, time(NULL) - *start_time);
                    //debug print statement
                    //printf("There are %d north cars and %d south cars\n\n", *north_cars_in - *north_cars_out, *south_cars_in - *south_cars_out);
                    sleep(1);
                    //if the south is empty or north reaches 12 cars, switch sides
                    if (*south_cars_in - *south_cars_out == 0 || *north_cars_in - *north_cars_out == 12){
                        *direction = 0;
                    }
                    up(mutex);
                    up(south_empty);
                }
            }else up(mutex);
        }
    }
}