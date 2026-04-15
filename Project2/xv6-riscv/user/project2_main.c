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

/* Prints compressed Gantt chart for lottery */
static void lottery_print_gantt(void) {
    printf("\n+--------------------------------------+\n");
    printf(  "|        GANTT CHART (Lottery)         |\n");
    printf(  "+--------------------------------------+\n");

    int comp_pid[LOTTERY_MAX_GANTT];
    int comp_start[LOTTERY_MAX_GANTT];
    int comp_end[LOTTERY_MAX_GANTT];
    int segs = 0;

    for (int t = 0; t < l_gantt_len; ) {
        int cur = l_gantt[t].pid, start = t;
        while (t < l_gantt_len && l_gantt[t].pid == cur) t++;
        comp_pid[segs]   = cur;
        comp_start[segs] = start;
        comp_end[segs]   = t;
        segs++;
    }

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
            printf("|P%-2d", comp_pid[s]);
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
        printf("%-*d", w + 2, comp_start[s]);
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
    for (int t = 0; t < max_t; t += 5) printf("%-5d", t);
    printf("\n");

    for (int i = 0; i < n; i++) {
        printf("  P%-5d: ", procs[i].pid);
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
    printf("  %-8s %-9s %-9s %-7s %-7s %-12s %-10s\n",
           "Process","Tickets","Arrival","Burst","Wait","Turnaround","Response");
    printf("  --------------------------------------------------------------\n");

    /* Use integer arithmetic to avoid float issues on xv6 */
    int sum_wait = 0, sum_tat = 0, sum_resp = 0;
    for (int i = 0; i < n; i++) {
        printf("  P%-7d %-9d %-9d %-7d %-7d %-12d %-10d\n",
               procs[i].pid, procs[i].tickets, procs[i].arrival_time,
               procs[i].burst_time, procs[i].waiting_time,
               procs[i].turnaround_time, procs[i].response_time);
        sum_wait += procs[i].waiting_time;
        sum_tat  += procs[i].turnaround_time;
        sum_resp += procs[i].response_time;
    }
    printf("  --------------------------------------------------------------\n");
    /* xv6 printf has no %f — print as integer + decimal manually */
    printf("  Average Waiting Time    : %d.%02d\n",
           sum_wait/n, (sum_wait*100/n) % 100);
    printf("  Average Turnaround Time : %d.%02d\n",
           sum_tat/n,  (sum_tat*100/n)  % 100);
    printf("  Average Response Time   : %d.%02d\n",
           sum_resp/n, (sum_resp*100/n) % 100);

    int idle = 0;
    for (int i = 0; i < l_gantt_len; i++) if (l_gantt[i].pid == -1) idle++;
    int total_time = l_gantt[l_gantt_len - 1].time + 1;
    int util_int  = (total_time - idle) * 100 / total_time;
    int util_frac = ((total_time - idle) * 10000 / total_time) % 100;
    printf("  CPU Utilization         : %d.%02d%%\n", util_int, util_frac);
    /* Throughput: n / total_time as fraction */
    printf("  Throughput              : %d/%d processes/unit time\n", n, total_time);
}
/* lottery output..*/
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

/* [MEMBER 4] — EDF scheduling: always picks process with earliest deadline */
static void edf_schedule(EDFProcess proc[], int n) {
    int completed = 0, time = 0;
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
        if (idx == -1) { edf_gantt[edf_gantt_time++] = -1; time++; continue; }
        if (proc[idx].start_time == -1) proc[idx].start_time = time;
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

/* [MEMBER 4] — EDF Gantt chart display */
static void edf_print_gantt(EDFProcess proc[], int n) {
    (void)proc; (void)n;
    printf("\n+--------------------------------------+\n");
    printf(  "|          GANTT CHART (EDF)           |\n");
    printf(  "+--------------------------------------+\n");

    int comp_pid[EDF_MAX_TIME];
    int comp_start[EDF_MAX_TIME];
    int comp_end[EDF_MAX_TIME];
    int segs = 0;

    for (int t = 0; t < edf_gantt_time; ) {
        int cur = edf_gantt[t], start = t;
        while (t < edf_gantt_time && edf_gantt[t] == cur) t++;
        comp_pid[segs]   = cur;
        comp_start[segs] = start;
        comp_end[segs]   = t;
        segs++;
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
        if (comp_pid[s] == -1) {
            printf("|IDL");
            print_repeat(' ', w - 1);
            printf("|");
        } else {
            printf("|P%-2d", comp_pid[s]);
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
        printf("%-*d", w + 2, comp_start[s]);
    }
    printf("%d\n", comp_end[segs - 1]);
}

/* [MEMBER 4] — EDF per-process timeline with deadline markers */
static void edf_print_timeline(EDFProcess proc[], int n) {
    printf("\n+--------------------------------------+\n");
    printf(  "|      PER-PROCESS TIMELINE (EDF)      |\n");
    printf(  "+--------------------------------------+\n");

    int max_t = edf_gantt_time > 60 ? 60 : edf_gantt_time;
    printf("  Time  : ");
    for (int t = 0; t < max_t; t += 5) printf("%-5d", t);
    printf("\n");

    for (int i = 0; i < n; i++) {
        printf("  P%-5d: ", proc[i].pid);
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

/* [MEMBER 4] — EDF results table with deadline miss/met status */
static void edf_print_results(EDFProcess proc[], int n) {
    printf("\n+--------------------------------------------------------------------------+\n");
    printf(  "|                    RESULTS TABLE (EDF)                                  |\n");
    printf(  "+--------------------------------------------------------------------------+\n");
    printf("  %-5s %-8s %-7s %-9s %-7s %-12s %-13s %-10s %-8s\n",
           "PID","Arrival","Burst","Deadline","Start","Completion","Turnaround","Waiting","Status");
    printf("  -------------------------------------------------------------------------\n");

    int total_tat = 0, total_wt = 0, missed = 0;
    for (int i = 0; i < n; i++) {
        const char *st = proc[i].missed_deadline ? "MISSED" : "MET";
        printf("  P%-4d %-8d %-7d %-9d %-7d %-12d %-13d %-10d %s\n",
               proc[i].pid, proc[i].arrival_time, proc[i].burst_time,
               proc[i].deadline, proc[i].start_time, proc[i].completion_time,
               proc[i].turnaround_time, proc[i].waiting_time, st);
        total_tat += proc[i].turnaround_time;
        total_wt  += proc[i].waiting_time;
        if (proc[i].missed_deadline) missed++;
    }
    printf("  -------------------------------------------------------------------------\n");
    printf("  Average Turnaround Time : %d.%02d\n",
           total_tat/n, (total_tat*100/n) % 100);
    printf("  Average Waiting Time    : %d.%02d\n",
           total_wt/n,  (total_wt*100/n)  % 100);

    int idle = 0;
    for (int t = 0; t < edf_gantt_time; t++) if (edf_gantt[t] == -1) idle++;
    int util = edf_gantt_time > 0 ? (100*(edf_gantt_time-idle)/edf_gantt_time) : 0;
    printf("  CPU Utilization         : %d%%\n", util);

    if (missed == 0)
        printf("  Deadline Miss Rate      : 0/%d (All deadlines MET!)\n", n);
    else
        printf("  Deadline Miss Rate      : %d/%d processes MISSED deadline\n", missed, n);
}

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

/* [MEMBER 5] — Queue helpers */
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

/* [MEMBER 5] — Initialize and cleanup the MLFQ scheduler */
static void mlfq_init(MLFQSched *s, int numCpus, int *quantums, int levels, int aging, int maxP) {
    s->numLevels = (levels > MLFQ_MAX_LEVELS) ? MLFQ_MAX_LEVELS : levels;
    for (int i = 0; i < s->numLevels; i++) {
        mq_init(&s->queues[i]);
        s->timeQuantums[i] = quantums[i];
    }
    s->agingThreshold = aging;
    s->currentTime    = 0;
    s->numCpus        = numCpus;
    s->cpus = (MCPU*)malloc(sizeof(MCPU) * numCpus);
    for (int i = 0; i < numCpus; i++) {
        s->cpus[i].id             = i;
        s->cpus[i].currentProcess = 0;
        s->cpus[i].activeTime     = 0;
        s->cpus[i].idleTime       = 0;
    }
    s->totalCapacity  = maxP;
    s->allProcesses   = (MProcess**)malloc(sizeof(MProcess*) * maxP);
    s->processCount   = 0;
    s->completedProcesses = 0;
}

static void mlfq_cleanup(MLFQSched *s) {
    if (s->cpus) free(s->cpus);
    if (s->allProcesses) {
        for (int i = 0; i < s->processCount; i++) free(s->allProcesses[i]);
        free(s->allProcesses);
    }
}

static void mlfq_add(MLFQSched *s, MProcess *p) {
    if (s->processCount < s->totalCapacity)
        s->allProcesses[s->processCount++] = p;
}

/* [MEMBER 5] — Handle newly arrived processes */
static void mlfq_arrivals(MLFQSched *s) {
    for (int i = 0; i < s->processCount; i++) {
        MProcess *p = s->allProcesses[i];
        if (p->arrivalTime == s->currentTime &&
            p->state == MLFQ_READY && p->firstExecutionTime == -2) {
            p->firstExecutionTime = -1;
            p->currentQueueLevel  = 0;
            p->timeSpentInCurrentQueueWaiting = 0;
            printf("[Time %3d] P%d arrived -> Queue 0\n", s->currentTime, p->pid);
            mq_enqueue(&s->queues[0], p);
        }
    }
}

/* [MEMBER 5] — Aging: promote starving processes to higher-priority queues */
static void mlfq_aging(MLFQSched *s) {
    for (int i = 1; i < s->numLevels; i++) {
        int sz = s->queues[i].size;
        for (int k = 0; k < sz; k++) {
            MProcess *p = mq_dequeue(&s->queues[i]);
            p->timeSpentInCurrentQueueWaiting++;
            if (p->timeSpentInCurrentQueueWaiting >= s->agingThreshold) {
                printf("[Time %3d] P%d starving -> promoted Q%d->Q%d\n",
                       s->currentTime, p->pid,
                       p->currentQueueLevel, p->currentQueueLevel - 1);
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

/* [MEMBER 5] — Handle completions and time-slice expirations */
static void mlfq_completions(MLFQSched *s) {
    for (int i = 0; i < s->numCpus; i++) {
        MCPU *cpu = &s->cpus[i];
        if (!cpu->currentProcess) continue;
        MProcess *p = cpu->currentProcess;
        if (p->remainingTime == 0) {
            printf("[Time %3d] P%d completed on CPU%d\n",
                   s->currentTime, p->pid, cpu->id);
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
            printf("[Time %3d] P%d slice expired -> Queue %d\n",
                   s->currentTime, p->pid, p->currentQueueLevel);
            mq_enqueue(&s->queues[p->currentQueueLevel], p);
            cpu->currentProcess = 0;
        }
    }
}

/* [MEMBER 5] — Assign processes to free CPUs, with preemption */
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
                    printf("[Time %3d] CPU%d <- P%d (Q%d)\n",
                           s->currentTime, cpu->id, p->pid, p->currentQueueLevel);
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
                printf("[Time %3d] CPU%d preempting P%d(Q%d) for Q%d\n",
                       s->currentTime, cpu->id, rp->pid, rq, hq);
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
                printf("[Time %3d] CPU%d <- P%d (Q%d)\n",
                       s->currentTime, cpu->id, np->pid, np->currentQueueLevel);
            }
        }
    }
}

/* [MEMBER 5] — Advance simulation clock by one tick */
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

/* [MEMBER 5] — Main MLFQ simulation loop */
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

