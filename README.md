# project-marking-problem
A toy POSIX threading problem written in C in which S students have a project which must be marked by K markers and M markers must mark up to N projects.

To use:
- Run the make command.
- Run the compiled demo.c specifying the number of students, number of markers, number of markers required to mark a student, number of students a mark can mark, the time in which the entire marking session lasts as well as the duration that each student's project marking will last as parameters e.g. './demo 5 3 2 4 15 2'.

Report 

1. Solved because:

The threads were synchronized using mutual exclusion and condition variables. The solution uses a single shared mutex for operations involving the availability list this ensures that only one thread is able to add or remove from the list at a time because threads without the lock will wait if another thread holds the lock. This is crucial because without this measure two students may check the number of markers simultaneously when there is only just enough markers for one to proceed resulting in error.

A set of M mutexes has been used for an array of M indexes. These mutexes and array indexes are decoupled in to sets of K so that each student and their pool of K markers can achieve synchronisation regarding whether a marker has really been grabbed and when a demo has finished. This is able to occur safely because there is an array index for each of the markers and access to these indexes is only ever shared between a single student who is being marked and the marker to which the index belongs to using each markers unique mLocks[id] mutex.

The pthread_cond_wait function allows markers to wait for students to grab them and student to wait for markers to become available by going to sleep, releasing the held mutex and waiting for a condition variable to be signalled before waking again. This wait is performed in a loop that checks a predicate that indicates whether it should really be awake because sometimes a thread will wake when it hasn't received a signal. Threads are woken using signals however these signals are only able to wake waiting threads, to overcome this when the markers add themselves to the list they must hold on to the availability list lock during signalling their availability and until they wait so that when the students wake from the signal they are blocked and wait for the marker to release the mutex before checking there's enough markers.

This technique is used again to ensure that the markers are waiting before the student can signal that the demo has been completed by making the marker hold on to the markers unique mLocks[id] from the point that they are grabbed until they wait on the demo completion. So that when the demo's finished the the student will wait on the K mLocks[id] it needs before indicating demo completion.

At the end of the session time the markers are signalled on the grab marked condition variable and the students are singalled on the wait for marker condition variable, so that they wake up and realise that the session time has end, exit the lab and time out. This is so that all the threads can join and the program can terminate.

Overall this use of mutexes, waiting and condition variables provides synchronisation and maintain consistency by ensuring that threads block if another thread is within a critical section, so that data inconsistencies cannot occur as well as to ensure that all threads are waiting for one another before signalling as this prevents threads from surpassing one another causing them to becoming out of sync and to hang.

2. Correct because:

- The student cannot proceed without grabbing K markers.
- Each student enters the lab, grabs K markers and starts their demo in the same time step. 
- Each student also finishes their demo and exit the lab in the same time step that the student's grabbed markers are finished with the student.
- Each student either completes their demo and exits the lab or does not complete the demo and times out.
- Markers wait for the student to signal that the demo is finished.
- Each marker either completes all their jobs and exits the lab or times out.
- The threads always join and terminate - tested several thousand times on random parameters.
- Does not deadlock and no signs of data races.
- Checks return value of pthread functions.

Synchronization is successful because all threads complete their objective according to the specification at the correct time step with error handling and without falling in to pitfalls.

3. Efficient because:

- Does not use busy waiting.
- Goes to sleep on wait freeing resources.
- Linked list dynamic memory allocation.
- Frees memory.
- Once marking, each student with its set of K markers are decoupled making each set independent of all other threads.
- Each student only ever signals K markers - no wasted awakening.

This can be considered efficient because the solution manages its resources and because instead of using a single lock for operations on the in use marker array it decouples each student and their set of K grabbed markers with K of M mutexes for accesses to K indexes of the array of M in use markers. This reduces the amount of mutex related blocking that occurs by making each set independent of one another, so more operations can occur concurrently. I tried to further improve the efficiency by using dynamic data structures for the rest of the arrays, however this lead to an issue where the after thousands of runs the wait function would fail to release a mutex upon waiting.
