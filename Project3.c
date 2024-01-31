#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>

// Global Variable declarations
int num_std, num_tutors, num_chairs, max_help;
int total_sessions = 0, total_requests = 0, std_helped = 0;
int *waiting_area, *std_pr, *tutoring_by;
sem_t mutex, sem_std, sem_tutor, sem_coor;
pthread_t *std, *tutors, coordinator;

// Thread declarations
void *student_thread(void *arg);
void *tutor_thread(void *arg);
void *coordinator_thread(void *arg);

int main(int argc, char *argv[]) {
    // parse and store the input students, tutors, chairs and help
    num_std = atoi(argv[1]);
    num_tutors = atoi(argv[2]);
    num_chairs = atoi(argv[3]);
    max_help = atoi(argv[4]);

    // Memory Allocation for arrays
    waiting_area = (int *)malloc(num_chairs * sizeof(int));
    std_pr= (int *)malloc(num_std * sizeof(int));
    tutoring_by = (int *)malloc(num_std * sizeof(int));
    std = (pthread_t *)malloc(num_std * sizeof(pthread_t));
    tutors = (pthread_t *)malloc(num_tutors * sizeof(pthread_t));

    // Initialize semaphores for synchronization
    sem_init(&mutex, 0, 1);
    sem_init(&sem_std, 0, 0);
    sem_init(&sem_tutor, 0, 0);
    sem_init(&sem_coor, 0, 0);

    for (int i = 0; i < num_chairs; i++) {
        waiting_area[i] = -1; // Indicates empty chair
    }

    for (int i = 0; i < num_std; i++) {
        std_pr[i] = 0; // Initialize priority
        tutoring_by[i] = -1; // Indicates no tutor assigned
        pthread_create(&std[i], NULL, student_thread, (void *)(long)i);  // create student threads
    }

    // create coordinator thread
    pthread_create(&coordinator, NULL, coordinator_thread, NULL);

    // create tutor threads
    for (int i = 0; i < num_tutors; i++) {
        pthread_create(&tutors[i], NULL, tutor_thread, (void *)(long)i);
    }

    // join student threads
    for (int i = 0; i < num_std; i++) {
        pthread_join(std[i], NULL);
    }

    sem_post(&sem_coor); // Signal coordinator to finish and then join coordinator
    pthread_join(coordinator, NULL);

    for (int i = 0; i < num_tutors; i++) {
        sem_post(&sem_tutor); // Signal tutors to finish
    }

    // join the tutor threads
    for (int i = 0; i < num_tutors; i++) {
        pthread_join(tutors[i], NULL);
    }

    // free memory
    free(waiting_area);
    free(std_pr);
    free(tutoring_by);
    free(std);
    free(tutors);

    // destroy semaphores
    sem_destroy(&mutex);
    sem_destroy(&sem_std);
    sem_destroy(&sem_tutor);
    sem_destroy(&sem_coor);

    return 0;
}

void *student_thread(void *arg) {
    int id = (int)(long)arg;
    int helped_count = 0;

    while (helped_count < max_help) {
        usleep(rand() % 2000);  // simulate student programming

        sem_wait(&mutex); // acquire lock for checking available chairs
        if (total_requests - total_sessions == num_chairs) { // No empty chair
            printf("S: Student %d found no empty chair. Will try again later.\n", id + 1);
            sem_post(&mutex);
        } else {
            waiting_area[total_requests % num_chairs] = id; // Take a seat
            std_pr[id] = helped_count; // Update priority based on number of times helped
            total_requests++;
            printf("S: Student %d takes a seat. Empty chairs = %d.\n", id + 1, num_chairs - (total_requests - total_sessions));
            sem_post(&mutex); // release lock
            sem_post(&sem_std); // Notify coordinator

            sem_wait(&sem_tutor); // Wait for a tutor
            helped_count++;
            if (tutoring_by[id] != -1) {
                printf("S: Student %d received help from Tutor %d.\n", id + 1, tutoring_by[id] + 1);
                tutoring_by[id] = -1;
            }
        }
    }

    pthread_exit(NULL);
}


void *coordinator_thread(void *arg) {
    while (1) {
        sem_wait(&sem_std); // Wait for a student

        sem_wait(&mutex);
        // check if students have received max help
        if (total_requests >= num_std * max_help) {
            sem_post(&mutex);
            break;  // exit if they have received max help
        }
        
	int waiting_students = total_requests - total_sessions;
        int next_student_index = total_sessions % num_chairs;
        int student_id = waiting_area[next_student_index]; // Get the ID of the next student
        int student_priority = std_pr[student_id]; // Get the priority of the student

        printf("C: Student %d with priority %d added to the queue. Waiting students now = %d. Total requests = %d\n", 
               student_id + 1, student_priority, waiting_students, total_requests);
        sem_post(&mutex);
	/*int waiting_students = total_requests - total_sessions;
        printf("C: Student with priority added to the queue. Waiting students now = %d. Total requests = %d\n", waiting_students, total_requests);
        sem_post(&mutex);
	*/

        sem_post(&sem_coor); // Notify a tutor a student is waiting for help
    }

    // Notify remaining tutors coordinator is done
    for (int i = 0; i < num_tutors; i++) {
        sem_post(&sem_coor);
    }

    pthread_exit(NULL);
}


void *tutor_thread(void *arg) {
    int id = (int)(long)arg;  // tutor id

    while (1) {
        sem_wait(&sem_coor); // Wait for a student(coordinator signals student is waiting)

        sem_wait(&mutex);  // check if students have received max amount of help
        if (total_sessions >= num_std * max_help) {
            sem_post(&mutex);
            break;
        }

        int student_id = waiting_area[total_sessions % num_chairs];  // id of student in waiting area
        waiting_area[total_sessions % num_chairs] = -1; // Empty the chair  // empty chair in waiting area
        total_sessions++;
        std_helped++;
	printf("T: Student %d tutored by Tutor %d. Students tutored now = %d. Total sessions tutored = %d\n",student_id + 1,id + 1 , std_helped, total_sessions);


        sem_post(&mutex);

        tutoring_by[student_id] = id; // Assign tutor ID to student
        sem_post(&sem_tutor); // Notify student

        usleep(200); // Simulate tutoring

        sem_wait(&mutex);
        std_helped--;
        sem_post(&mutex);
    }

    pthread_exit(NULL);
}


