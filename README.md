# Elevator

## Overview

I created an elevator program in C to practice and present my skills with
concurrency and parallel programming. The program extensively uses mutexes,
semaphores and condition variables to create the overall system.

My long-term goals with the project are to tighten the algorithm for calling
and sending elevators to minimize the average wait-time per person.

## Details

Elevator loop:
- Posts a **semaphore** to indicate it's ready
- Waits on `call_sig` **condition variable** for it to be called
- Once called, enters an active loop
    - Determines the direction it must go to reach its target
        - If no target, exit loop
    - Opens doors along the way (signal the `doors` **cond_var**)
    - If there are no more calls in the direction it is traveling, reset
      direction to 0
    - Wait for clock pulse **cond_var** (a 2s timer, see below)
    - Loop
- Loop

Person thread:
- Wait on **semaphore** until people available to leave floor `src`
- Call elevator to `src`
    - Try to find one on it's way already
    - If nothing on the way, signal `call_sig` **cond_var**
- Add `src` to list of targets
    - Wait for elevator using `doors` **cond_var**
- Once in elevator, add `dest` to list of targets
    - Wait for elevator using `doors` **cond_var**
- Post **semaphore** to increment # of people on floor `dest`
- Person arrived at floor `dest`

__Aside__:
- The elevators move in sync using a clock signal, which controls the speed of
  the elevators. While I could make them move on their own clocks, I decided to
  keep it this way so it's easier to understand what's happening.
- The semaphore in the person thread is used to keep track of how many people
  are on a specific floor. It won't allow someone to leave a floor if the # of
  people are 0.

## Sample run

`./simulation`:

```
Elevator (1) created!
Elevator (0) created!
Person (1 -> 0) created!
Person (3 -> 7) created!
Person (5 -> 0) created!
Elevator (1) called!
    ~clock E0:(0-) E1:(1^)~
Elevator (0) called!
    ~clock E0:(1^) E1:(2^)~
E0: Opening door
    ~clock E0:(1^) E1:(3^)~
E1: Opening door
    ~clock E0:(1-) E1:(3^)~
    ~clock E0:(0v) E1:(3^)~
E0: Opening door
Person (1 -> 0) arrived!
    ~clock E0:(0v) E1:(4^)~
    ~clock E0:(0-) E1:(5^)~
    ~clock E0:(0-) E1:(6^)~
Elevator (0) called!
    ~clock E0:(1^) E1:(7^)~
E1: Opening door
    ~clock E0:(2^) E1:(7^)~
Person (3 -> 7) arrived!
    ~clock E0:(3^) E1:(7-)~
    ~clock E0:(4^) E1:(7-)~
    ~clock E0:(5^) E1:(7-)~
E0: Opening door
    ~clock E0:(5^) E1:(7-)~
    ~clock E0:(5-) E1:(7-)~
    ~clock E0:(4v) E1:(7-)~
    ~clock E0:(3v) E1:(7-)~
    ~clock E0:(2v) E1:(7-)~
    ~clock E0:(2v) E1:(7-)~
    ~clock E0:(1v) E1:(7-)~
    ~clock E0:(0v) E1:(7-)~
    ~clock E0:(0v) E1:(7-)~
E0: Opening door
Person (5 -> 0) arrived!
    ~clock E0:(0v) E1:(7-)~
    ~clock E0:(0-) E1:(7-)~
    ~clock E0:(0-) E1:(7-)~
    ~clock E0:(0-) E1:(7-)~
    ~clock E0:(0-) E1:(7-)~
    ~clock E0:(0-) E1:(7-)~
```


## How to use

### Configuration

There are a number of configuration options defined as constants at the top of
the program, while the program may work with other configurations, it has not
been thoroughly tested.

```c
#define ELEVATORS (2)                   // num elevators
#define FLOORS (12)                     // num floors
#define PEOPLE_PER_FLOOR (6)            // num people per floor
#define ELEVATOR_TIMEOUT (5)            // inactive time before shutdown
#define CLOCK_TIME (2)                  // time between clock pulses
```

### Build and run

Simply type `make run` to build and run the simulation

### Input details (stdin)

The program begins with 3 people with pre-defined source and destination floors:
1. Person 1 wants to go from floor 5 to the ground floor (0)
2. Person 2 wants to go from floor 3 to floor 7
3. Person 3 wants to go from floor 1 to the ground floor (0)

While the program is running, you may enter a source integer, and dest integer
to create a person thread that wants to go from `src` to `dest`. A value of 0
represents the ground floor, while all other integers represent their respective
building floors.

__Aside:__
- There is a limit on the number of people that can *leave* a floor. Since there
  are only so many habitants, if you try to create a person thread leaving `src`
  but there's nobody on floor `src`, the thread will wait until someone arrives
  there. This is done using **semaphores**.
