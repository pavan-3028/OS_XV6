/*
 * ============================================================
 *   OS PROJECT 2 — Advanced Scheduling Algorithms
 *   Algorithms: Lottery Scheduler | EDF | MLFQ
 * ============================================================
 *
 */
#include "kernel/types.h"
#include "user/user.h"
 
/* INT_MAX — not available in xv6's headers */
#define INT_MAX  0x7fffffff
 
/* ----------------------------------------------------------------
 * Simple LCG pseudo-random number generator
 * (replaces rand()/srand() which xv6 does not provide)
 * ---------------------------------------------------------------- */
static unsigned int _lcg_seed = 12345;
 
static void lcg_srand(unsigned int s) { _lcg_seed = s; }
 
static int lcg_rand(void) {
    _lcg_seed = _lcg_seed * 1664525u + 1013904223u;
    return (int)((_lcg_seed >> 1) & 0x7fffffff);
}
 
/* Simple strlen — available in xv6 user lib but kept explicit */
/* (xv6 user.h already declares strlen, so we just use it)      */
 
/* Utility: print a repeated character */
static void print_repeat(char c, int n) {
    for (int i = 0; i < n; i++) {
        char buf[2]; buf[0] = c; buf[1] = '\0';
        printf("%s", buf);
    }
}

#define LOTTERY_MAX_PROC   10
#define LOTTERY_MAX_GANTT  500

typedef struct {
    int pid;
    int arrival_time;
    int burst_time;
    int tickets;
    int remaining_time;
    int finish_time;
    int waiting_time;
    int turnaround_time;
    int response_time;
    int first_run;
} LotteryProcess;

typedef struct {
    int pid;
    int time;
} GanttEntry;

static GanttEntry l_gantt[LOTTERY_MAX_GANTT];
static int        l_gantt_len = 0;

/* Randomly selects a winning process based on ticket count */
static int lottery_draw(LotteryProcess procs[], int n, int current_time) {
    int total = 0;
    for (int i = 0; i < n; i++)
        if (procs[i].arrival_time <= current_time && procs[i].remaining_time > 0)
            total += procs[i].tickets;
    if (total == 0) return -1;

    int winning = (lcg_rand() % total) + 1;
    int counter = 0;
    for (int i = 0; i < n; i++) {
        if (procs[i].arrival_time <= current_time && procs[i].remaining_time > 0) {
            counter += procs[i].tickets;
            if (counter >= winning) return i;
        }
    }
    return -1;
}

/*Main lottery simulation loop; fills l_gantt[] */
static void run_lottery(LotteryProcess procs[], int n) {
    int current_time = 0, completed = 0;
    l_gantt_len = 0;

    for (int i = 0; i < n; i++) {
        procs[i].remaining_time = procs[i].burst_time;
        procs[i].first_run      = 0;
        procs[i].response_time  = -1;
    }

    while (completed < n) {
        int idx = lottery_draw(procs, n, current_time);
        if (idx == -1) {
            l_gantt[l_gantt_len].pid  = -1;
            l_gantt[l_gantt_len].time = current_time;
            l_gantt_len++;
            current_time++;
            continue;
        }
        if (!procs[idx].first_run) {
            procs[idx].response_time = current_time - procs[idx].arrival_time;
            procs[idx].first_run = 1;
        }
        l_gantt[l_gantt_len].pid  = procs[idx].pid;
        l_gantt[l_gantt_len].time = current_time;
        l_gantt_len++;

        procs[idx].remaining_time--;
        current_time++;

        if (procs[idx].remaining_time == 0) {
            procs[idx].finish_time     = current_time;
            procs[idx].turnaround_time = current_time - procs[idx].arrival_time;
            procs[idx].waiting_time    = procs[idx].turnaround_time - procs[idx].burst_time;
            completed++;
        }
    }
}
