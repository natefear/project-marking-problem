/*
 * "the project marking problem". Your task is to synchronise the threads
 * correctly.
 */

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>

/*
 * Parameters of the program. The constraints are D < T and
 * S*K <= M*N.
 */

struct demo_parameters {
    int S;   /* Number of students */
    int M;   /* Number of markers */
    int K;   /* Number of markers per demo */
    int N;   /* Number of demos per marker */
    int T;   /* Length of session (minutes) */
    int D;   /* Length of demo (minutes) */
};

/* Global object holding the demo parameters. */
struct demo_parameters parameters;

/* The demo start time, set in the main function. Do not modify this. */
struct timeval starttime;

/* 
 * global variables here. 
 */

struct availableMarkerList{//linked list 
    int markerID;//id of available marker
    struct availableMarkerList *next;//ptr to next element of list
};
struct availableMarkerList *head = NULL;//start of linked list

pthread_mutex_t availableMarkerLock = PTHREAD_MUTEX_INITIALIZER;//lock for linked list
pthread_mutex_t mLocks[100];//lock for each inUseMarker index 

pthread_cond_t waitForMarker = PTHREAD_COND_INITIALIZER;//condition variables 
pthread_cond_t grabMarker[100];
pthread_cond_t demoFinished[100];

int inUseMarker[100];//ids of students being marked stored at index of marker id
int availableMarkers = 0;//list size

/*
 * timenow(): returns current simulated time in "minutes" (cs).
 * Assumes that starttime has been set already.
 * This function is safe to call in the student and marker threads as
 * starttime is set in the run() function.
 */
int timenow() {
    struct timeval now;
    gettimeofday(&now, NULL);
    return (now.tv_sec - starttime.tv_sec) * 100 + (now.tv_usec - starttime.tv_usec) / 10000;
}

/* delay(t): delays for t "minutes" (cs) */
void delay(int t) {
    struct timespec rqtp, rmtp;
    t *= 10;
    rqtp.tv_sec = t / 1000;
    rqtp.tv_nsec = 1000000 * (t % 1000);
    nanosleep(&rqtp, &rmtp);
}

/* panic(): simulates a student's panicking activity */
void panic() {
    delay(random() % (parameters.T - parameters.D));
}

/* demo(): simulates a demo activity */
void demo() {
    delay(parameters.D);
}

/*
 * A marker thread.
 * The parameter arg is the number of the current marker and the function
 * doesn't need to return any values.
 */
 
void *marker(void *arg) {
    /* The ID of the current marker, error, job number and student id */
    int markerID = *(int *)arg, err, job = 0, studentID;
    
    /*1. Enter the lab.*/
    printf("%d marker %d: enters lab\n", timenow(), markerID);

    /*A marker marks up to N projects.*/
    while(job < parameters.N) {//repeat n times

        int markerGrabbed = 0;//false

        err = pthread_mutex_lock(&availableMarkerLock);//lock avail list
        if(err){printf("%d Marker: %d availablity lock failed\n",timenow(),markerID); abort();}//catch error

        //allocate mem
        struct availableMarkerList *link = (struct availableMarkerList*) malloc(sizeof(struct availableMarkerList));
        link->markerID = markerID;//add available marker to list
       	link->next = head;//point new marker to previous list item
       	head = link;//update head to the new item/marker
       	availableMarkers++;//keep track of list size
       
        err = pthread_cond_signal(&waitForMarker);//signal any waiting student
        if(err){printf("%d Marker: %d available signal failed\n",timenow(),markerID); abort();}//catch error

        while(markerGrabbed == 0) {//wait for student
            
            if(timenow() + parameters.D >= parameters.T){break;}//time out 
 
            err = pthread_cond_wait(&grabMarker[markerID], &availableMarkerLock);//wait to be grabbed
            if(err){printf("%d Marker: %d grab wait failed\n",timenow(),markerID); abort();}//catch error

            err = pthread_mutex_lock(&mLocks[markerID]);//lock decoupling lock
            if(err){printf("%d Marker: %d grab check lock failed\n",timenow(),markerID); abort();}//catch error

            if(inUseMarker[markerID] >= 0) {//if marker has student id allocated
                markerGrabbed = 1;//marker grabbed
                studentID = inUseMarker[markerID];//set local student id
                break; //if grabbed release mlock on demo wait instead
            }

            err = pthread_mutex_unlock(&mLocks[markerID]);//unlock decoupling lock
            if(err){printf("%d Marker: %d grab check unlock failed\n",timenow(),markerID); abort();}//catch error 
   
        }

        err = pthread_mutex_unlock(&availableMarkerLock);//unlock avail list
        if(err){printf("%d Marker: %d availability unlock failed\n",timenow(),markerID); abort();}//catch error
       
        if(markerGrabbed == 1) {//grabbed true
            job++;//increment job num
            printf("%d marker %d: grabbed by student %d (job %d)\n", timenow(), markerID, studentID, job);//marker grabbed 
            int demoEnd = 0;//false

            //mlock obtained and retained in previous statement if grabbed
	
            while(demoEnd == 0) {//wait for demo to end

    	        err = pthread_cond_wait(&demoFinished[markerID], &mLocks[markerID]);//wait for end signal
                if(err){printf("%d Marker: %d demo wait failed\n",timenow(),markerID); abort();}//catch error

                if(inUseMarker[markerID] == -2){//demo done val
                    demoEnd = 1;//true	
                }
            }

            err = pthread_mutex_unlock(&mLocks[markerID]);//unlock decoupling lock
            if(err){printf("%d Marker: %d demo done unlock failed\n",timenow(),markerID); abort();}//catch error

            printf("%d marker %d: finished with student %d (job %d)\n", timenow(), markerID, studentID, job);//demo over

        }
        if(job == parameters.N) {//if all jobs done
            printf("%d marker %d: exits lab (finished %d jobs)\n", timenow(), markerID, parameters.N);//finish
            break;//exit
        }
        else if(timenow() + parameters.D >= parameters.T) {
            printf("%d marker %d: exits lab (timeout)\n", timenow(), markerID);//timeout
            break;//exit
        } 
      

    }

    return NULL;

}

/*
 * A student thread
 */
 
void *student(void *arg) {
    /* The ID of the current student, array of grabbed markers, error */
    int studentID = *(int *)arg, localInUseMarker[parameters.K], err;

    /* 1. Panic! */
    printf("%d student %d: starts panicking\n", timenow(), studentID);
    panic();

    /* 2. Enter the lab. */
    printf("%d student %d: enters lab\n", timenow(), studentID);

    /* 3. Grab K markers. */
    err = pthread_mutex_lock(&availableMarkerLock);//lock list
    if(err){printf("%d Student: %d availability lock failed\n",timenow(),studentID); abort();}//catch error

    while(availableMarkers < parameters.K) {//not enough markers wait
        if(timenow() + parameters.D >= parameters.T){break;}//time out
        err = pthread_cond_wait(&waitForMarker, &availableMarkerLock);//wait for marker
        if(err){printf("%d Student: %d wait for marker failed\n",timenow(),studentID); abort();}//catch error
    }
    
    for(int i = 0; i < parameters.K; i++) {//grab k markers

        if(timenow() + parameters.D >= parameters.T){break;}//time out
        if(head == NULL){printf("%d Student: %d tried to read empty list\n",timenow(),studentID); abort();}

        struct availableMarkerList *tmp = head;//save current as tmp before iterating
        localInUseMarker[i] = head->markerID;//save id locally

        head = head->next;//iterate to next list item
        free(tmp);//delete current grabbed marker

        availableMarkers--;//decrement num available
        
    }

    err = pthread_mutex_unlock(&availableMarkerLock);//unlock list
    if(err){printf("%d Student: %d availability unlock failed\n",timenow(),studentID); abort();}//catch error

    if(timenow() + parameters.D < parameters.T) {//enough time

        for(int i = 0; i < parameters.K; i++) {//for each grabbed marker
            
            err = pthread_mutex_lock(&mLocks[localInUseMarker[i]]);//use decoupling lock
            if(err){printf("%d Student: %d marker pairing lock failed\n",timenow(),studentID); abort();}//catch error

            inUseMarker[localInUseMarker[i]] = studentID;//update unique global marker ind with grab val

            err = pthread_mutex_unlock(&mLocks[localInUseMarker[i]]);//unlock decoupling lock
            if(err){printf("%d Student: %d marker pairing unlock failed\n",timenow(),studentID); abort();}//catch error

        }

        for(int i = 0; i < parameters.K; i++) {//for each grabbed marker
            err = pthread_cond_signal(&grabMarker[localInUseMarker[i]]);//signal grabbed marker
            if(err){printf("%d Student: %d grab signal failed\n",timenow(),studentID); abort();}//catch error
        }

        /* 4. Demo! */
        printf("%d student %d: starts demo\n", timenow(), studentID);
    	demo();
    	printf("%d student %d: ends demo\n", timenow(), studentID);
        
        for(int i = 0; i < parameters.K; i++) {//for each grabbed marker

            err = pthread_mutex_lock(&mLocks[localInUseMarker[i]]);//use decoupling lock
            if(err){printf("%d Student: %d demo end lock failed\n",timenow(),studentID); abort();}//catch error

            inUseMarker[localInUseMarker[i]] = -2;//update unique global marker ind with demo end val

            err = pthread_mutex_unlock(&mLocks[localInUseMarker[i]]);//unlock decoupling lock
            if(err){printf("%d Student: %d demo end unlock failed\n",timenow(),studentID); abort();}//catch error

        }
        
        for(int i = 0; i < parameters.K; i++) {//for each grabbed marker
            err = pthread_cond_signal(&demoFinished[localInUseMarker[i]]);//signal grabbed marker
            if(err){printf("%d Student: %d demo end signal failed\n",timenow(),studentID); abort();}//catch error
        }

        /* 5. Exit the lab. */
    	printf("%d student %d: exits lab (finished)\n", timenow(), studentID);//finished

    }
    else{
        printf("%d student %d: exits lab (timeout)\n", timenow(), studentID);//timeout
    }

    return NULL;

}

/* The function that runs the session. */

void run(){

    int markerID[100], studentID[100], err, i;
    pthread_t markerT[100], studentT[100];

    printf("S=%d M=%d K=%d N=%d T=%d D=%d\n",parameters.S, parameters.M, parameters.K, parameters.N, parameters.T, parameters.D);
    gettimeofday(&starttime, NULL);  /* Save start of simulated time */

    availableMarkers = 0;//reset to 0 important for bulk looping test

    /* Create M marker threads */
    for(i = 0; i<parameters.M; i++) {
        markerID[i] = i;//set thread id
        inUseMarker[i] = -1;//set val for not inuse
        err = pthread_create(&markerT[i], NULL, marker, &markerID[i]);//create thread
        if(err){printf("%d Marker: %d thread create failed\n",timenow(),i); abort();}//catch error
    }

    /* Create S student threads */
    for(i = 0; i<parameters.S; i++) {
        studentID[i] = i;//set thread id
        err = pthread_create(&studentT[i], NULL, student, &studentID[i]);//create thread
        if(err){printf("%d Student: %d thread create failed\n",timenow(),i); abort();}//catch error
    }

    /* With the threads now started, the session is in full swing ... */
    delay(parameters.T - parameters.D);

    /* 
     * When we reach here, this is the latest time a new demo could start.
     * You might want to do something here or soon after.
     */
   
    //time up signal all waiting students  
    err = pthread_cond_broadcast(&waitForMarker);//signal all waiting student
    if(err){printf("%d Student time up broadcast failed\n",timenow()); abort();}//catch error

    //time up signal all waiting markers
    for(i = 0; i<parameters.M; i++) {
        err = pthread_cond_signal(&grabMarker[i]);//signal each waiting marker
        if(err){printf("%d Marker: %d time up signal failed\n",timenow(),i); abort();}//catch error
    }
    
    /* Wait for student threads to finish */
    for(i = 0; i<parameters.S; i++) {
        err = pthread_join(studentT[i], NULL);//join thread
        if(err){printf("%d Student: %d thread join failed\n",timenow(),i); abort();}//catch error
    }
      
    /* Wait for marker threads to finish */
    for(i = 0; i<parameters.M; i++) {
        err = pthread_join(markerT[i], NULL);//join thread
        if(err){printf("%d Marker: %d thread join failed\n",timenow(),i); abort();}//catch error
    }
 
}

/*
 * main() checks that the parameters are ok. 
 */
int main(int argc, char *argv[]) {

    if(argc < 6) {
        puts("Usage: demo S M K N T D\n");
        exit(1);
    }

    parameters.S = atoi(argv[1]);
    parameters.M = atoi(argv[2]);
    parameters.K = atoi(argv[3]);
    parameters.N = atoi(argv[4]);
    parameters.T = atoi(argv[5]);
    parameters.D = atoi(argv[6]);

    if(parameters.M > 100 || parameters.S > 100) {
        puts("Maximum 100 markers and 100 students allowed.\n");
        exit(1);
    }

    if(parameters.D >= parameters.T) {
        puts("Constraint D < T violated.\n");
        exit(1);
    }

    if(parameters.S*parameters.K > parameters.M*parameters.N) {
        puts("Constraint S*K <= M*N violated.\n");
        exit(1);
    }

    // We're good to go.
 
    run();

    return 0;
}

