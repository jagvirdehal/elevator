#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>

// Constants
#define ELEVATORS (2)                   // num elevators
#define FLOORS (12)                     // num floors
#define PEOPLE_PER_FLOOR (6)            // num people per floor
#define ELEVATOR_TIMEOUT (5)            // inactive time before shutdown
#define CLOCK_TIME (0.5)                  // time between clock pulses

// Macros
#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#define MAX(a,b) (((a) > (b)) ? (a) : (b))
#define ABS(a) ((a) >= 0 ? (a) : -(a))
#define DIST(a,b) (ABS((a) - (b)))
#define LEQ(bottom, middle, top) ( ((bottom) <= (middle)) && ((middle) <= (top)) )


// Elevator locks
pthread_cond_t call_sig;                    // person -> elevator signal to exit ready state
pthread_mutex_t elevator_lock[ELEVATORS];   // lock to access elevator state
pthread_cond_t elevator_doors[ELEVATORS];   // elevator -> person signal to open doors for enter/exit
sem_t elevators_rdy;                        // indicates # of ready elevators

// Elevator state
volatile const int * elevator_pos[ELEVATORS]; 
volatile const int * elevator_dir[ELEVATORS];
volatile bool elevator_buttons[ELEVATORS][FLOORS];

// Floor locks
sem_t floor_people[FLOORS];                 // indicates # of people on floor

// Clock for elevators
pthread_cond_t clock_sig;                   // used so all elevators wait on a clock pulse every N seconds
bool terminate = false;                     // only ever set to true, so each elevator can eventually terminate


// Elevator thread
void * elevator_thr(void *arg) {
    int * param = (int *) arg;
    int id = param[0];
    free(arg);
    printf("Elevator (%d) created!\n", id);

    // Local state
    int curr_floor = 0;
    int direction = 0;

    pthread_mutex_lock(&elevator_lock[id]);

    // External state setup
    elevator_pos[id] = &curr_floor;             // set pointer to curr floor
    elevator_dir[id] = &direction;              // set pointer to curr direction

    for ( int f = 0; f < FLOORS; f++ ) {
        elevator_buttons[id][f] = false;
    }

    for (;;) {              // main loop
        sem_post(&elevators_rdy);               // indicate an elevator is ready
        pthread_cond_wait(&call_sig, &elevator_lock[id]);  // wait till called
      if ( terminate ) { break; }
        printf("Elevator (%d) called!\n", id);

        for (;;) {          // active loop
            if ( direction == 0 ) {             // if no direction, try to set one
                for ( int f = 0; f < FLOORS; f++ ) {
                    if ( elevator_buttons[id][f] ) {
                        direction = ( curr_floor < f ) ? 1 : -1;
                  break;
                    }
                }
          if ( direction == 0 ) { break; }      // no buttons active, break to ready
            }

            if ( elevator_buttons[id][curr_floor] ) {               // if button is pressed, open door
                printf("E%d: Opening door\n", id);
                elevator_buttons[id][curr_floor] = false;           // set button to off
                pthread_cond_broadcast(&elevator_doors[id]);        // doors open to get on/off
                pthread_cond_wait(&clock_sig, &elevator_lock[id]);  // wait for extra clock sig
            }

            // Keep old direction if elevator has buttons left, otherwise reset
            int d = direction;
            direction = 0;
            for ( int f = curr_floor; f >= 0 && f < FLOORS; f += d ) { 
                if ( elevator_buttons[id][f] ) { direction = d; break; }
            }

            // Elevator go up/down
            curr_floor += direction;

            pthread_cond_wait(&clock_sig, &elevator_lock[id]);      // wait for clock sig
        }
    }

    pthread_mutex_unlock(&elevator_lock[id]);

    return NULL;
}

int call_elevator(int floor, int dir ) {
    // First see if an active elevator can come to us
    for ( int e = 0; e < ELEVATORS; e++ ) {
        if ( pthread_mutex_trylock(&elevator_lock[e]) == 0 ) {      // lock acquired
            // same direction?
            if ( dir == *elevator_dir[e] ) {
                // are we on the way?
                if ( ( dir < 0 && floor <= *elevator_pos[e] ) || 
                    ( dir > 0 && floor >= *elevator_pos[e] ) ) {
                    elevator_buttons[e][floor] = true;
                    while ( elevator_buttons[e][floor] ) {
                        pthread_cond_wait(&elevator_doors[e], &elevator_lock[e]);
                    }
                    return e;
                }
            }
            pthread_mutex_unlock(&elevator_lock[e]);
        }
    }

    // Call a new elevator
    for (;;) {
        sem_wait(&elevators_rdy);
        for ( int e = 0; e < ELEVATORS; e++ ) {
            if ( pthread_mutex_trylock(&elevator_lock[e]) == 0 ) {      // lock acquired
                if ( *elevator_dir[e] == 0 ) {
                    elevator_buttons[e][floor] = true;
                    pthread_cond_signal(&call_sig);
                    while ( elevator_buttons[e][floor] ) {
                        pthread_cond_wait(&elevator_doors[e], &elevator_lock[e]);
                    }
                    return e;
                }
                pthread_mutex_unlock(&elevator_lock[e]);
            }
        }
    }
}

void * person_thr(void *arg) {
    int * params = (int *) arg;
    int src = params[0];
    int dest = params[1];
    free(arg);

    // Parameter checking
    if ( src == dest ) { return NULL; }
    if ( src < 0 || dest < 0 ) { return NULL; }
    if ( src >= FLOORS || dest >= FLOORS ) { return NULL; }

    sem_wait(&floor_people[src]);               // decrement # of people on floor
    printf("Person (%d -> %d) created!\n", src, dest);

    int direction = src < dest ? 1 : -1;
    int e = call_elevator(src, direction);

    elevator_buttons[e][dest] = true;
    while ( elevator_buttons[e][dest] ) {
        pthread_cond_wait(&elevator_doors[e], &elevator_lock[e]);
    }

    pthread_mutex_unlock(&elevator_lock[e]);
    sem_post(&floor_people[dest]);
    printf("Person (%d -> %d) arrived!\n", src, dest);

    return NULL;
}

void init() {
    // Initialize clock signal
    pthread_cond_init(&clock_sig, 0);

    // Initialize locks for elevators
    sem_init(&elevators_rdy, 0, 0);
    for ( int i = 0; i < ELEVATORS; i++ ) {
        pthread_cond_init(&call_sig, 0);
        pthread_mutex_init(&elevator_lock[i], 0);
        pthread_cond_init(&elevator_doors[i], 0);
    }

    // Initialize population of each floor
    sem_init(&floor_people[0], 0, 0);           // ground floor begins with 0
    for ( int i = 1; i < FLOORS; i++ ) {
        sem_init(&floor_people[i], 0, PEOPLE_PER_FLOOR);
    }
}

pthread_t create_person(int src, int dest) {
    pthread_t person;

    int * params = malloc(2 * sizeof(int));
    params[0] = src;
    params[1] = dest;

    pthread_create(&person, NULL, person_thr, params);
    return person;
}

pthread_t create_elevator(int id) {
    pthread_t elevator;

    int * params = malloc(sizeof(int));
    params[0] = id;

    pthread_create(&elevator, NULL, elevator_thr, params);
    return elevator;
}

void * spawner_thr(void * args) {
    int src;
    int dest;

    while ( terminate == false ) {
        while ( scanf("%d", &src) != 1 ) {}
        while ( scanf("%d", &dest) != 1 ) {}
        printf("Attempting to spawn (%d -> %d)\n", src, dest);
        create_person(src, dest);
    }

    return NULL;
}

int main() {
    // Initialize
    init();

    // Create elevator threads
    pthread_t ev_threads[ELEVATORS];
    for (int e = 0; e < ELEVATORS; e++) {
        ev_threads[e] = create_elevator(e);
    }

    // Wait 2 seconds to ensure elevators have initalized their own state
    sleep(2);

    // Create default people
    pthread_t person = create_person(5, 0);
    pthread_t person2 = create_person(3, 7);
    pthread_t person3 = create_person(1, 0);

    // Create a spawner thread that looks for user input and creates new people
    pthread_t spawner;
    pthread_create(&spawner, NULL, spawner_thr, NULL);

    // Main clock loop for elevators
    int inactive_time = 0;
    for ( ;; ) {
        sleep(CLOCK_TIME);              // controls the time between clock pulses
        
        // Get # of ready elevators
        int num_ready = 0;
        sem_getvalue(&elevators_rdy, &num_ready);
        if ( num_ready == ELEVATORS ) { inactive_time += 1; }
        else { inactive_time = 0; }

        // Terminate if inactive too long
        if ( inactive_time >= ELEVATOR_TIMEOUT ) {
            terminate = true;
            pthread_cond_broadcast(&call_sig);
            pthread_cond_broadcast(&clock_sig);
      break;
        }

        // Print current state
        printf("    ~clock");
        for ( int e = 0; e < ELEVATORS; e++ ) {
            int pos = *elevator_pos[e];
            int dir = *elevator_dir[e];
            char dir_char = (dir == 0) ? '-' : ( (dir > 0) ? '^' : 'v' );

            printf(" E%d:(%d%c)", e, pos, dir_char );
        }
        printf("~\n");

        // Pulse the clock
        pthread_cond_broadcast(&clock_sig);
      if ( terminate ) { break; }
    }

    // If terminated, program will exit immediately (should prob do cleanup)
}
