/*
 * ============================================================
 *   OS PROJECT 2 — Advanced Scheduling Algorithms
 *   Algorithms: Lottery Scheduler | EDF | MLFQ
 *
 *   FIXES APPLIED:
 *   1. read_int() now reads char-by-char, skipping leading
 *      non-digit characters (newlines, spaces) so batched
 *      input does not cause subsequent reads to return 0.
 *   2. Every malloc() call is followed by a NULL check with
 *      a graceful error message and exit(1).
 *   3. mlfq_init() n<1 guard added to prevent malloc(0).
 *   4. Minor: edf_gantt_time reset to 0 at start of
 *      edf_schedule() so repeated runs don't corrupt state.
 *   5. CRITICAL FIX: Large arrays in print_gantt made static
 *      to easily bypass xv6's strict 4KB user-stack limit.
 *   6. PRINTF FIX: xv6 does not support formatted printf padding
 *      like "%-5d" or "%02d". Manually implemented padding
 *      functions print_pad_int, print_pad_str, print_zero_pad_int
 *      to ensure correct alignment.
 * ============================================================
 */
#include "kernel/types.h"
#include "user/user.h"

/* INT_MAX — not available in xv6's headers */
#define INT_MAX  0x7fffffff

/* ----------------------------------------------------------------
 * Simple LCG pseudo-random number generator
 * ---------------------------------------------------------------- */
static unsigned int _lcg_seed = 12345;

static void lcg_srand(unsigned int s) { _lcg_seed = s; }

static int lcg_rand(void) {
    _lcg_seed = _lcg_seed * 1664525u + 1013904223u;
    return (int)((_lcg_seed >> 1) & 0x7fffffff);
}

/* Utility: print a repeated character */
static void print_repeat(char c, int n) {
    for (int i = 0; i < n; i++) {
        char buf[2]; buf[0] = c; buf[1] = '\0';
        printf("%s", buf);
    }
}

/* xv6 Printf padding helpers */
static void print_pad_str(const char *str, int width, int left_align) {
    int len = 0;
    while(str[len]) len++;
    int pad = width - len;
    if (pad < 0) pad = 0;
    if (!left_align) print_repeat(' ', pad);
    printf("%s", str);
    if (left_align) print_repeat(' ', pad);
}

static void print_pad_int(int val, int width, int left_align) {
    int temp = val;
    int len = 0;
    if (temp == 0) len = 1;
    else {
        if (temp < 0) { len++; temp = -temp; }
        while (temp > 0) { len++; temp /= 10; }
    }
    
    int pad = width - len;
    if (pad < 0) pad = 0;
    if (!left_align) print_repeat(' ', pad);
    printf("%d", val);
    if (left_align) print_repeat(' ', pad);
}

static void print_zero_pad_int(int val, int width) {
    int temp = val;
    int len = 0;
    if (temp == 0) len = 1;
    else {
        if (temp < 0) { len++; temp = -temp; }
        while (temp > 0) { len++; temp /= 10; }
    }
    int pad = width - len;
    if (pad < 0) pad = 0;
    if (val < 0) {
        printf("-");
        print_repeat('0', pad);
        printf("%d", -val);
    } else {
        print_repeat('0', pad);
        printf("%d", val);
    }
}

/* ----------------------------------------------------------------
 * FIX 1: read_int() — char-by-char, skips leading non-digits
 *
 * The original bulk read() grabbed multiple newline-terminated
 * lines in one call, leaving leftover bytes that caused the next
 * read_int() to see an empty buffer and return 0.  Reading one
 * byte at a time and skipping non-digit leading characters fixes
 * the batched-input problem entirely.
 * ---------------------------------------------------------------- */
static int read_int(void) {
    char c;
    int val = 0;

    /* Skip everything that is not a digit */
    while (read(0, &c, 1) == 1) {
        if (c >= '0' && c <= '9') {
            val = c - '0';
            break;
        }
    }
    /* Accumulate remaining digits */
    while (read(0, &c, 1) == 1 && c >= '0' && c <= '9')
        val = val * 10 + (c - '0');

    return val;
}

/* ----------------------------------------------------------------
 * FIX 2: safe_malloc() — wraps malloc with a NULL check
 * ---------------------------------------------------------------- */
static void *safe_malloc(int sz) {
    void *p = malloc(sz);
    if (!p) {
        printf("FATAL: malloc(%d) returned NULL — out of memory\n", sz);
        exit(1);
    }
    return p;
}

/* ============================================================
 * LOTTERY SCHEDULER
 * ============================================================ */
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

/* Main lottery simulation loop */
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
            if (l_gantt_len < LOTTERY_MAX_GANTT) {
                l_gantt[l_gantt_len].pid  = -1;
                l_gantt[l_gantt_len].time = current_time;
                l_gantt_len++;
            }
            current_time++;
            continue;
        }
        if (!procs[idx].first_run) {
            procs[idx].response_time = current_time - procs[idx].arrival_time;
            procs[idx].first_run = 1;
        }
        if (l_gantt_len < LOTTERY_MAX_GANTT) {
            l_gantt[l_gantt_len].pid  = procs[idx].pid;
            l_gantt[l_gantt_len].time = current_time;
            l_gantt_len++;
        }

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

/* Prints compressed Gantt chart for lottery */
static void lottery_print_gantt(void) {
    printf("\n+--------------------------------------+\n");
    printf(  "|        GANTT CHART (Lottery)         |\n");
    printf(  "+--------------------------------------+\n");

    /* FIX 5: Move large arrays from Stack to BSS (using static) to prevent overflow */
    static int comp_pid[LOTTERY_MAX_GANTT];
    static int comp_start[LOTTERY_MAX_GANTT];
    static int comp_end[LOTTERY_MAX_GANTT];
    int segs = 0;

    for (int t = 0; t < l_gantt_len; ) {
        int cur = l_gantt[t].pid, start = t;
        while (t < l_gantt_len && l_gantt[t].pid == cur) t++;
        comp_pid[segs]   = cur;
        comp_start[segs] = start;
        comp_end[segs]   = t;
        segs++;
    }

    if (segs == 0) { printf("  (no data)\n"); return; }

    /* Top border */
    printf("\n  ");
    for (int s = 0; s < segs; s++) {
        int w = comp_end[s] - comp_start[s];
        if (w > 6) w = 6;
        print_repeat('-', w + 2);
    }
    printf("\n  ");

    /* Labels */
    for (int s = 0; s < segs; s++) {
        int dur = comp_end[s] - comp_start[s];
        int w   = (dur > 6) ? 6 : dur;
        if (comp_pid[s] == -1) {
            printf("|IDL");
            print_repeat(' ', w - 1);
            printf("|");
        } else {
            printf("|P");
            print_pad_int(comp_pid[s], 2, 1);
            print_repeat(' ', w - 1);
            printf("|");
        }
    }
    printf("\n  ");

    /* Bottom border */
    for (int s = 0; s < segs; s++) {
        int w = comp_end[s] - comp_start[s];
        if (w > 6) w = 6;
        print_repeat('-', w + 2);
    }
    printf("\n  ");

    /* Time markers */
    for (int s = 0; s < segs; s++) {
        int dur = comp_end[s] - comp_start[s];
        int w   = (dur > 6) ? 6 : dur;
        print_pad_int(comp_start[s], w + 2, 1);
    }
    printf("%d\n", comp_end[segs - 1]);
}

/* Prints per-process execution timeline */
static void lottery_print_timeline(LotteryProcess procs[], int n) {
    printf("\n+--------------------------------------+\n");
    printf(  "|    PER-PROCESS TIMELINE (Lottery)    |\n");
    printf(  "+--------------------------------------+\n");

    int max_t = l_gantt_len > 60 ? 60 : l_gantt_len;
    printf("  Time  : ");
    for (int t = 0; t < max_t; t += 5) print_pad_int(t, 5, 1);
    printf("\n");

    for (int i = 0; i < n; i++) {
        printf("  P"); print_pad_int(procs[i].pid, 5, 1); printf(": ");
        for (int t = 0; t < max_t; t++) {
            if (l_gantt[t].pid == procs[i].pid)
                printf("#");
            else if (t < procs[i].arrival_time)
                printf(" ");
            else
                printf(".");
        }
        printf("\n");
    }
    if (l_gantt_len > 60)
        printf("  (truncated at t=60; total=%d)\n", l_gantt_len);
}

/* Prints results table with averages and CPU utilization */
static void lottery_print_results(LotteryProcess procs[], int n) {
    printf("\n+--------------------------------------------------------------+\n");
    printf(  "|                  RESULTS TABLE (Lottery)                    |\n");
    printf(  "+--------------------------------------------------------------+\n");
    
    printf("  ");
    print_pad_str("Process", 8, 1); printf(" ");
    print_pad_str("Tickets", 9, 1); printf(" ");
    print_pad_str("Arrival", 9, 1); printf(" ");
    print_pad_str("Burst", 7, 1); printf(" ");
    print_pad_str("Wait", 7, 1); printf(" ");
    print_pad_str("Turnaround", 12, 1); printf(" ");
    print_pad_str("Response", 10, 1); printf("\n");
    
    printf("  --------------------------------------------------------------\n");

    int sum_wait = 0, sum_tat = 0, sum_resp = 0;
    for (int i = 0; i < n; i++) {
        printf("  P"); print_pad_int(procs[i].pid, 7, 1); printf(" ");
        print_pad_int(procs[i].tickets, 9, 1); printf(" ");
        print_pad_int(procs[i].arrival_time, 9, 1); printf(" ");
        print_pad_int(procs[i].burst_time, 7, 1); printf(" ");
        print_pad_int(procs[i].waiting_time, 7, 1); printf(" ");
        print_pad_int(procs[i].turnaround_time, 12, 1); printf(" ");
        print_pad_int(procs[i].response_time, 10, 1); printf("\n");
        
        sum_wait += procs[i].waiting_time;
        sum_tat  += procs[i].turnaround_time;
        sum_resp += procs[i].response_time;
    }
    printf("  --------------------------------------------------------------\n");
    
    printf("  Average Waiting Time    : %d.", sum_wait/n);
    print_zero_pad_int((sum_wait*100/n) % 100, 2);
    printf("\n");
    
    printf("  Average Turnaround Time : %d.", sum_tat/n);
    print_zero_pad_int((sum_tat*100/n) % 100, 2);
    printf("\n");

    printf("  Average Response Time   : %d.", sum_resp/n);
    print_zero_pad_int((sum_resp*100/n) % 100, 2);
    printf("\n");

    int idle = 0;
    for (int i = 0; i < l_gantt_len; i++) if (l_gantt[i].pid == -1) idle++;
    int total_time = l_gantt[l_gantt_len - 1].time + 1;
    int util_int  = (total_time - idle) * 100 / total_time;
    int util_frac = ((total_time - idle) * 10000 / total_time) % 100;
    
    printf("  CPU Utilization         : %d.", util_int);
    print_zero_pad_int(util_frac, 2);
    printf("%%\n");
    printf("  Throughput              : %d/%d processes/unit time\n", n, total_time);
}

/* ============================================================
 * EDF SCHEDULER
 * ============================================================ */
#define EDF_MAX_PROC  20
#define EDF_MAX_TIME  500

typedef struct {
    int pid;
    int arrival_time;
    int burst_time;
    int deadline;
    int remaining_time;
    int completion_time;
    int waiting_time;
    int turnaround_time;
    int start_time;
    int is_completed;
    int missed_deadline;
} EDFProcess;

static int edf_gantt[EDF_MAX_TIME];
static int edf_gantt_time = 0;

static void edf_schedule(EDFProcess proc[], int n) {
    int completed = 0, time = 0;
    /* FIX 4: reset edf_gantt_time so repeated runs are safe */
    edf_gantt_time = 0;
    for (int i = 0; i < EDF_MAX_TIME; i++) edf_gantt[i] = -1;

    while (completed < n) {
        int idx = -1, earliest = INT_MAX;
        for (int i = 0; i < n; i++) {
            if (!proc[i].is_completed && proc[i].arrival_time <= time)
                if (proc[i].deadline < earliest) {
                    earliest = proc[i].deadline;
                    idx = i;
                }
        }
        if (idx == -1) {
            if (edf_gantt_time < EDF_MAX_TIME)
                edf_gantt[edf_gantt_time++] = -1;
            time++;
            continue;
        }
        if (proc[idx].start_time == -1) proc[idx].start_time = time;
        if (edf_gantt_time < EDF_MAX_TIME)
            edf_gantt[edf_gantt_time++] = proc[idx].pid;
        proc[idx].remaining_time--;
        time++;
        if (proc[idx].remaining_time == 0) {
            proc[idx].completion_time  = time;
            proc[idx].turnaround_time  = time - proc[idx].arrival_time;
            proc[idx].waiting_time     = proc[idx].turnaround_time - proc[idx].burst_time;
            proc[idx].missed_deadline  = (time > proc[idx].deadline) ? 1 : 0;
            proc[idx].is_completed     = 1;
            completed++;
        }
    }
    edf_gantt_time = time;
}

static void edf_print_gantt(EDFProcess proc[], int n) {
    (void)proc; (void)n;
    printf("\n+--------------------------------------+\n");
    printf(  "|          GANTT CHART (EDF)           |\n");
    printf(  "+--------------------------------------+\n");

    /* FIX 5: Move large arrays from Stack to BSS (using static) to prevent overflow */
    static int comp_pid[EDF_MAX_TIME];
    static int comp_start[EDF_MAX_TIME];
    static int comp_end[EDF_MAX_TIME];
    int segs = 0;

    for (int t = 0; t < edf_gantt_time; ) {
        int cur = edf_gantt[t], start = t;
        while (t < edf_gantt_time && edf_gantt[t] == cur) t++;
        comp_pid[segs]   = cur;
        comp_start[segs] = start;
        comp_end[segs]   = t;
        segs++;
    }

    if (segs == 0) { printf("  (no data)\n"); return; }

    printf("\n  ");
    for (int s = 0; s < segs; s++) {
        int w = comp_end[s] - comp_start[s];
        if (w > 6) w = 6;
        print_repeat('-', w + 2);
    }
    printf("\n  ");
    for (int s = 0; s < segs; s++) {
        int dur = comp_end[s] - comp_start[s];
        int w   = (dur > 6) ? 6 : dur;
        if (comp_pid[s] == -1) {
            printf("|IDL");
            print_repeat(' ', w - 1);
            printf("|");
        } else {
            printf("|P"); print_pad_int(comp_pid[s], 2, 1);
            print_repeat(' ', w - 1);
            printf("|");
        }
    }
    printf("\n  ");
    for (int s = 0; s < segs; s++) {
        int w = comp_end[s] - comp_start[s];
        if (w > 6) w = 6;
        print_repeat('-', w + 2);
    }
    printf("\n  ");
    for (int s = 0; s < segs; s++) {
        int dur = comp_end[s] - comp_start[s];
        int w   = (dur > 6) ? 6 : dur;
        print_pad_int(comp_start[s], w + 2, 1);
    }
    printf("%d\n", comp_end[segs - 1]);
}

static void edf_print_timeline(EDFProcess proc[], int n) {
    printf("\n+--------------------------------------+\n");
    printf(  "|      PER-PROCESS TIMELINE (EDF)      |\n");
    printf(  "+--------------------------------------+\n");

    int max_t = edf_gantt_time > 60 ? 60 : edf_gantt_time;
    printf("  Time  : ");
    for (int t = 0; t < max_t; t += 5) print_pad_int(t, 5, 1);
    printf("\n");

    for (int i = 0; i < n; i++) {
        printf("  P"); print_pad_int(proc[i].pid, 5, 1); printf(": ");
        for (int t = 0; t < max_t; t++) {
            if (edf_gantt[t] == proc[i].pid)
                printf("#");
            else if (t < proc[i].arrival_time)
                printf(" ");
            else
                printf(".");
        }
        if (proc[i].deadline <= max_t)
            printf(" D=%d", proc[i].deadline);
        printf("\n");
    }
    if (edf_gantt_time > 60)
        printf("  (truncated at t=60; total=%d)\n", edf_gantt_time);
}

static void edf_print_results(EDFProcess proc[], int n) {
    printf("\n+--------------------------------------------------------------------------+\n");
    printf(  "|                    RESULTS TABLE (EDF)                                  |\n");
    printf(  "+--------------------------------------------------------------------------+\n");
    
    printf("  ");
    print_pad_str("PID", 5, 1); printf(" ");
    print_pad_str("Arrival", 8, 1); printf(" ");
    print_pad_str("Burst", 7, 1); printf(" ");
    print_pad_str("Deadline", 9, 1); printf(" ");
    print_pad_str("Start", 7, 1); printf(" ");
    print_pad_str("Completion", 12, 1); printf(" ");
    print_pad_str("Turnaround", 13, 1); printf(" ");
    print_pad_str("Waiting", 10, 1); printf(" ");
    print_pad_str("Status", 8, 1); printf("\n");
    
    printf("  -------------------------------------------------------------------------\n");

    int total_tat = 0, total_wt = 0, missed = 0;
    for (int i = 0; i < n; i++) {
        const char *st = proc[i].missed_deadline ? "MISSED" : "MET";
        printf("  P"); print_pad_int(proc[i].pid, 4, 1); printf(" ");
        print_pad_int(proc[i].arrival_time, 8, 1); printf(" ");
        print_pad_int(proc[i].burst_time, 7, 1); printf(" ");
        print_pad_int(proc[i].deadline, 9, 1); printf(" ");
        print_pad_int(proc[i].start_time, 7, 1); printf(" ");
        print_pad_int(proc[i].completion_time, 12, 1); printf(" ");
        print_pad_int(proc[i].turnaround_time, 13, 1); printf(" ");
        print_pad_int(proc[i].waiting_time, 10, 1); printf(" ");
        printf("%s\n", st);
        
        total_tat += proc[i].turnaround_time;
        total_wt  += proc[i].waiting_time;
        if (proc[i].missed_deadline) missed++;
    }
    printf("  -------------------------------------------------------------------------\n");
    
    printf("  Average Turnaround Time : %d.", total_tat/n);
    print_zero_pad_int((total_tat*100/n) % 100, 2);
    printf("\n");
    
    printf("  Average Waiting Time    : %d.", total_wt/n);
    print_zero_pad_int((total_wt*100/n) % 100, 2);
    printf("\n");

    int idle = 0;
    for (int t = 0; t < edf_gantt_time; t++) if (edf_gantt[t] == -1) idle++;
    int util = edf_gantt_time > 0 ? (100*(edf_gantt_time-idle)/edf_gantt_time) : 0;
    printf("  CPU Utilization         : %d%%\n", util);

    if (missed == 0)
        printf("  Deadline Miss Rate      : 0/%d (All deadlines MET!)\n", n);
    else
        printf("  Deadline Miss Rate      : %d/%d processes MISSED deadline\n", missed, n);
}

/* ============================================================
 * MLFQ SCHEDULER
 * ============================================================ */
#define MLFQ_MAX_PROC    20
#define MLFQ_MAX_LEVELS   4

typedef enum { MLFQ_READY, MLFQ_RUNNING, MLFQ_COMPLETED } MState;

typedef struct {
    int    pid;
    int    arrivalTime;
    int    burstTime;
    int    remainingTime;
    int    completionTime;
    int    waitingTime;
    int    turnaroundTime;
    int    responseTime;
    int    firstExecutionTime;
    int    currentQueueLevel;
    int    timeSliceConsumed;
    int    timeSpentInCurrentQueueWaiting;
    MState state;
} MProcess;

typedef struct {
    MProcess *processes[MLFQ_MAX_PROC];
    int front, rear, size;
} MQueue;

typedef struct {
    int       id;
    MProcess *currentProcess;
    int       activeTime;
    int       idleTime;
} MCPU;

typedef struct {
    MQueue   queues[MLFQ_MAX_LEVELS];
    int      timeQuantums[MLFQ_MAX_LEVELS];
    int      numLevels;
    int      agingThreshold;
    int      currentTime;
    MCPU    *cpus;
    int      numCpus;
    MProcess **allProcesses;
    int      processCount;
    int      totalCapacity;
    int      completedProcesses;
} MLFQSched;

/* Queue helpers */
static void mq_init(MQueue *q)    { q->front=0; q->rear=-1; q->size=0; }
static int  mq_empty(MQueue *q)   { return q->size==0; }
static void mq_enqueue(MQueue *q, MProcess *p) {
    if (q->size < MLFQ_MAX_PROC) {
        q->rear = (q->rear+1) % MLFQ_MAX_PROC;
        q->processes[q->rear] = p;
        q->size++;
    }
}
static MProcess *mq_dequeue(MQueue *q) {
    if (q->size > 0) {
        MProcess *p = q->processes[q->front];
        q->front = (q->front+1) % MLFQ_MAX_PROC;
        q->size--;
        return p;
    }
    return 0;
}

/* FIX 2+3: mlfq_init uses safe_malloc; guards n<1 */
static void mlfq_init(MLFQSched *s, int numCpus, int *quantums, int levels, int aging, int maxP) {
    /* FIX 3: prevent malloc(0) if caller passes bad maxP */
    if (maxP < 1) maxP = 1;
    if (numCpus < 1) numCpus = 1;

    s->numLevels = (levels > MLFQ_MAX_LEVELS) ? MLFQ_MAX_LEVELS : levels;
    for (int i = 0; i < s->numLevels; i++) {
        mq_init(&s->queues[i]);
        s->timeQuantums[i] = quantums[i];
    }
    s->agingThreshold = aging;
    s->currentTime    = 0;
    s->numCpus        = numCpus;

    /* FIX 2: NULL-checked allocation */
    s->cpus = (MCPU*)safe_malloc(sizeof(MCPU) * numCpus);
    for (int i = 0; i < numCpus; i++) {
        s->cpus[i].id             = i;
        s->cpus[i].currentProcess = 0;
        s->cpus[i].activeTime     = 0;
        s->cpus[i].idleTime       = 0;
    }
    s->totalCapacity  = maxP;

    /* FIX 2: NULL-checked allocation */
    s->allProcesses = (MProcess**)safe_malloc(sizeof(MProcess*) * maxP);
    s->processCount   = 0;
    s->completedProcesses = 0;
}

static void mlfq_cleanup(MLFQSched *s) {
    if (s->cpus) { free(s->cpus); s->cpus = 0; }
    if (s->allProcesses) {
        for (int i = 0; i < s->processCount; i++)
            if (s->allProcesses[i]) free(s->allProcesses[i]);
        free(s->allProcesses);
        s->allProcesses = 0;
    }
}

static void mlfq_add(MLFQSched *s, MProcess *p) {
    if (s->processCount < s->totalCapacity)
        s->allProcesses[s->processCount++] = p;
}

static void mlfq_arrivals(MLFQSched *s) {
    for (int i = 0; i < s->processCount; i++) {
        MProcess *p = s->allProcesses[i];
        if (p->arrivalTime == s->currentTime &&
            p->state == MLFQ_READY && p->firstExecutionTime == -2) {
            p->firstExecutionTime = -1;
            p->currentQueueLevel  = 0;
            p->timeSpentInCurrentQueueWaiting = 0;
            printf("[Time "); print_pad_int(s->currentTime, 3, 0);
            printf("] P%d arrived -> Queue 0\n", p->pid);
            mq_enqueue(&s->queues[0], p);
        }
    }
}

static void mlfq_aging(MLFQSched *s) {
    for (int i = 1; i < s->numLevels; i++) {
        int sz = s->queues[i].size;
        for (int k = 0; k < sz; k++) {
            MProcess *p = mq_dequeue(&s->queues[i]);
            p->timeSpentInCurrentQueueWaiting++;
            if (p->timeSpentInCurrentQueueWaiting >= s->agingThreshold) {
                printf("[Time "); print_pad_int(s->currentTime, 3, 0);
                printf("] P%d starving -> promoted Q%d->Q%d\n",
                       p->pid, p->currentQueueLevel, p->currentQueueLevel - 1);
                p->currentQueueLevel--;
                p->timeSpentInCurrentQueueWaiting = 0;
                mq_enqueue(&s->queues[p->currentQueueLevel], p);
            } else {
                mq_enqueue(&s->queues[i], p);
            }
        }
    }
    int sz = s->queues[0].size;
    for (int k = 0; k < sz; k++) {
        MProcess *p = mq_dequeue(&s->queues[0]);
        p->timeSpentInCurrentQueueWaiting++;
        mq_enqueue(&s->queues[0], p);
    }
}

static void mlfq_completions(MLFQSched *s) {
    for (int i = 0; i < s->numCpus; i++) {
        MCPU *cpu = &s->cpus[i];
        if (!cpu->currentProcess) continue;
        MProcess *p = cpu->currentProcess;
        if (p->remainingTime == 0) {
            printf("[Time "); print_pad_int(s->currentTime, 3, 0);
            printf("] P%d completed on CPU%d\n", p->pid, cpu->id);
            p->state           = MLFQ_COMPLETED;
            p->completionTime  = s->currentTime;
            p->turnaroundTime  = p->completionTime - p->arrivalTime;
            p->waitingTime     = p->turnaroundTime - p->burstTime;
            cpu->currentProcess = 0;
            s->completedProcesses++;
        } else if (p->timeSliceConsumed >= s->timeQuantums[p->currentQueueLevel]) {
            p->state = MLFQ_READY;
            p->timeSliceConsumed = 0;
            p->timeSpentInCurrentQueueWaiting = 0;
            if (p->currentQueueLevel < s->numLevels - 1) p->currentQueueLevel++;
            printf("[Time "); print_pad_int(s->currentTime, 3, 0);
            printf("] P%d slice expired -> Queue %d\n",
                   p->pid, p->currentQueueLevel);
            mq_enqueue(&s->queues[p->currentQueueLevel], p);
            cpu->currentProcess = 0;
        }
    }
}

static void mlfq_assign(MLFQSched *s) {
    for (int i = 0; i < s->numCpus; i++) {
        MCPU *cpu = &s->cpus[i];
        if (!cpu->currentProcess) {
            for (int j = 0; j < s->numLevels; j++) {
                if (!mq_empty(&s->queues[j])) {
                    MProcess *p = mq_dequeue(&s->queues[j]);
                    if (p->firstExecutionTime == -1) {
                        p->firstExecutionTime = s->currentTime;
                        p->responseTime = s->currentTime - p->arrivalTime;
                    }
                    p->state = MLFQ_RUNNING;
                    p->timeSpentInCurrentQueueWaiting = 0;
                    cpu->currentProcess = p;
                    printf("[Time "); print_pad_int(s->currentTime, 3, 0);
                    printf("] CPU%d <- P%d (Q%d)\n",
                           cpu->id, p->pid, p->currentQueueLevel);
                    break;
                }
            }
        } else {
            MProcess *rp = cpu->currentProcess;
            int rq = rp->currentQueueLevel, hq = -1;
            for (int j = 0; j < s->numLevels; j++) {
                if (!mq_empty(&s->queues[j])) { hq = j; break; }
            }
            if (hq != -1 && hq < rq) {
                printf("[Time "); print_pad_int(s->currentTime, 3, 0);
                printf("] CPU%d preempting P%d(Q%d) for Q%d\n",
                       cpu->id, rp->pid, rq, hq);
                rp->state = MLFQ_READY;
                rp->timeSpentInCurrentQueueWaiting = 0;
                mq_enqueue(&s->queues[rp->currentQueueLevel], rp);
                MProcess *np = mq_dequeue(&s->queues[hq]);
                if (np->firstExecutionTime == -1) {
                    np->firstExecutionTime = s->currentTime;
                    np->responseTime = s->currentTime - np->arrivalTime;
                }
                np->state = MLFQ_RUNNING;
                np->timeSpentInCurrentQueueWaiting = 0;
                cpu->currentProcess = np;
                printf("[Time "); print_pad_int(s->currentTime, 3, 0);
                printf("] CPU%d <- P%d (Q%d)\n",
                       cpu->id, np->pid, np->currentQueueLevel);
            }
        }
    }
}

static void mlfq_tick(MLFQSched *s) {
    for (int i = 0; i < s->numCpus; i++) {
        MCPU *cpu = &s->cpus[i];
        if (cpu->currentProcess) {
            cpu->currentProcess->remainingTime--;
            cpu->currentProcess->timeSliceConsumed++;
            cpu->activeTime++;
        } else {
            cpu->idleTime++;
        }
    }
}

static void mlfq_simulate(MLFQSched *s) {
    printf("\n=== MLFQ Simulation ===\n");
    printf("CPUs: %d | Levels: %d | Aging: %d\n",
           s->numCpus, s->numLevels, s->agingThreshold);
    printf("Time Quantums: ");
    for (int i = 0; i < s->numLevels; i++) printf("%d ", s->timeQuantums[i]);
    printf("\n\n");

    while (s->completedProcesses < s->processCount) {
        mlfq_arrivals(s);
        mlfq_aging(s);
        mlfq_completions(s);
        mlfq_assign(s);
        mlfq_tick(s);
        s->currentTime++;
        if (s->currentTime > 100000) {
            printf("Time limit exceeded!\n");
            break;
        }
    }
    printf("[Time %d] MLFQ simulation complete.\n\n", s->currentTime);
}

static void mlfq_print_metrics(MLFQSched *s) {
    printf("\n+--------------------------------------------------------------+\n");
    printf(  "|                  RESULTS TABLE (MLFQ)                       |\n");
    printf(  "+--------------------------------------------------------------+\n");
    
    printf("  ");
    print_pad_str("PID", 6, 1); printf(" ");
    print_pad_str("Arrival", 10, 1); printf(" ");
    print_pad_str("Burst", 8, 1); printf(" ");
    print_pad_str("Completion", 12, 1); printf(" ");
    print_pad_str("Turnaround", 12, 1); printf(" ");
    print_pad_str("Waiting", 10, 1); printf(" ");
    print_pad_str("Response", 10, 1); printf("\n");
    printf("  ------------------------------------------------------------------\n");

    int tw = 0, tt = 0, tr = 0;
    int n = s->processCount;
    for (int i = 0; i < n; i++) {
        MProcess *p = s->allProcesses[i];
        printf("  P"); print_pad_int(p->pid, 5, 1); printf(" ");
        print_pad_int(p->arrivalTime, 10, 1); printf(" ");
        print_pad_int(p->burstTime, 8, 1); printf(" ");
        print_pad_int(p->completionTime, 12, 1); printf(" ");
        print_pad_int(p->turnaroundTime, 12, 1); printf(" ");
        print_pad_int(p->waitingTime, 10, 1); printf(" ");
        print_pad_int(p->responseTime, 10, 1); printf("\n");
        
        tw += p->waitingTime;
        tt += p->turnaroundTime;
        tr += p->responseTime;
    }
    printf("  ------------------------------------------------------------------\n");
    
    printf("  Average Waiting Time    : %d.", tw/n);
    print_zero_pad_int((tw*100/n) % 100, 2);
    printf("\n");
    
    printf("  Average Turnaround Time : %d.", tt/n);
    print_zero_pad_int((tt*100/n) % 100, 2);
    printf("\n");
    
    printf("  Average Response Time   : %d.", tr/n);
    print_zero_pad_int((tr*100/n) % 100, 2);
    printf("\n");
    
    printf("  Throughput              : %d/%d processes/unit time\n", n, s->currentTime);

    printf("\n  CPU Utilization:\n");
    for (int i = 0; i < s->numCpus; i++) {
        MCPU *cpu = &s->cpus[i];
        int util = s->currentTime > 0
                   ? (cpu->activeTime * 100 / s->currentTime) : 0;
        printf("  CPU %d: %d%% (Active: %d, Idle: %d)\n",
               cpu->id, util, cpu->activeTime, cpu->idleTime);
    }
}

/* ============================================================
 * Top-level runners
 * ============================================================ */
void run_lottery_scheduler(void) {
    lcg_srand(12345);
    LotteryProcess procs[LOTTERY_MAX_PROC];
    int n;

    printf("\n+------------------------------------------+\n");
    printf(  "|       LOTTERY SCHEDULER SIMULATION       |\n");
    printf(  "+------------------------------------------+\n");
    printf("\nEnter number of processes (max %d): ", LOTTERY_MAX_PROC);
    n = read_int();
    if (n < 1 || n > LOTTERY_MAX_PROC) { printf("Invalid count.\n"); return; }

    for (int i = 0; i < n; i++) {
        procs[i].pid = i + 1;
        printf("\n--- Process P%d ---\n", i + 1);
        printf("  Arrival Time : ");
        procs[i].arrival_time = read_int();
        printf("  Burst Time   : ");
        procs[i].burst_time = read_int();
        printf("  Tickets      : ");
        procs[i].tickets = read_int();
    }

    run_lottery(procs, n);
    lottery_print_gantt();
    lottery_print_timeline(procs, n);
    lottery_print_results(procs, n);
    printf("\n[Lottery Simulation Complete]\n");
}

void run_edf_scheduler(void) {
    EDFProcess proc[EDF_MAX_PROC];
    int n;

    printf("\n+------------------------------------------+\n");
    printf(  "|  EDF (Earliest Deadline First) Scheduler |\n");
    printf(  "+------------------------------------------+\n");
    printf("\nEnter number of processes (max %d): ", EDF_MAX_PROC);
    n = read_int();
    if (n < 1 || n > EDF_MAX_PROC) { printf("Invalid count.\n"); return; }

    for (int i = 0; i < n; i++) {
        proc[i].pid = i + 1;
        printf("\n  P%d -> ", i + 1);
        printf("Arrival : ");
        proc[i].arrival_time = read_int();
        printf("         Burst   : ");
        proc[i].burst_time = read_int();
        printf("         Deadline: ");
        proc[i].deadline = read_int();
        proc[i].remaining_time  = proc[i].burst_time;
        proc[i].completion_time = 0;
        proc[i].waiting_time    = 0;
        proc[i].turnaround_time = 0;
        proc[i].start_time      = -1;
        proc[i].is_completed    = 0;
        proc[i].missed_deadline = 0;
    }

    edf_schedule(proc, n);
    edf_print_gantt(proc, n);
    edf_print_timeline(proc, n);
    edf_print_results(proc, n);
    printf("\n[EDF Simulation Complete]\n");
}

void run_mlfq_scheduler(void) {
    MLFQSched sched;
    int numCpus, numLevels, aging;

    printf("\n+------------------------------------------+\n");
    printf(  "|  MLFQ (Multi-Level Feedback Queue) Sched |\n");
    printf(  "+------------------------------------------+\n");

    printf("Number of CPUs       : ");
    numCpus = read_int();
    if (numCpus < 1) numCpus = 1;

    printf("Number of queue levels (max %d): ", MLFQ_MAX_LEVELS);
    numLevels = read_int();
    if (numLevels < 1 || numLevels > MLFQ_MAX_LEVELS) numLevels = 3;

    int quantums[MLFQ_MAX_LEVELS];
    printf("Enter time quantum for each level:\n");
    for (int i = 0; i < numLevels; i++) {
        printf("  Level %d quantum: ", i);
        quantums[i] = read_int();
        if (quantums[i] < 1) quantums[i] = 1;   /* guard against 0 quantum */
    }

    printf("Aging threshold (time units before promotion): ");
    aging = read_int();
    if (aging < 1) aging = 10;   /* sensible default if 0 entered */

    int n;
    printf("Number of processes (max %d): ", MLFQ_MAX_PROC);
    n = read_int();
    if (n < 1 || n > MLFQ_MAX_PROC) { printf("Invalid count.\n"); return; }

    /* FIX 2+3: safe_malloc inside mlfq_init, n validated above */
    mlfq_init(&sched, numCpus, quantums, numLevels, aging, n);

    for (int i = 0; i < n; i++) {
        /* FIX 2: NULL-checked allocation for each process */
        MProcess *p = (MProcess*)safe_malloc(sizeof(MProcess));
        p->pid = i + 1;
        printf("\n--- Process P%d ---\n", i + 1);
        printf("  Arrival Time: ");
        p->arrivalTime = read_int();
        printf("  Burst Time  : ");
        p->burstTime = read_int();
        p->remainingTime  = p->burstTime;
        p->completionTime = 0;
        p->waitingTime    = 0;
        p->turnaroundTime = 0;
        p->responseTime   = 0;
        p->firstExecutionTime = -2;
        p->currentQueueLevel  = 0;
        p->timeSliceConsumed  = 0;
        p->timeSpentInCurrentQueueWaiting = 0;
        p->state = MLFQ_READY;
        mlfq_add(&sched, p);
    }

    mlfq_simulate(&sched);
    mlfq_print_metrics(&sched);
    mlfq_cleanup(&sched);
    printf("\n[MLFQ Simulation Complete]\n");
}

int main(void) {
    int choice;

    while (1) {
        printf("\n+----------------------------------------------+\n");
        printf(  "|    OS PROJECT 2 — Scheduling Algorithms     |\n");
        printf(  "+----------------------------------------------+\n");
        printf(  "|  1. Lottery Scheduler                        |\n");
        printf(  "|  2. EDF (Earliest Deadline First)            |\n");
        printf(  "|  3. MLFQ (Multi-Level Feedback Queue)        |\n");
        printf(  "|  0. Exit                                     |\n");
        printf(  "+----------------------------------------------+\n");
        printf("Select algorithm: ");
        choice = read_int();

        switch (choice) {
            case 1: run_lottery_scheduler(); break;
            case 2: run_edf_scheduler();     break;
            case 3: run_mlfq_scheduler();    break;
            case 0:
                printf("\nGoodbye!\n");
                return 0;
            default:
                printf("Invalid choice. Try again.\n");
        }
    }
}

