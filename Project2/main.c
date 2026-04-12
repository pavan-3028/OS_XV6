/*
 * ============================================================
 *   OS PROJECT 2 — Advanced Scheduling Algorithms
 *   Algorithms: Lottery Scheduler | EDF | MLFQ
 * ============================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>

/* ============================================================
   ANSI COLOR CODES
   ============================================================ */
#define RESET     "\033[0m"
#define BOLD      "\033[1m"
#define RED       "\033[31m"
#define GREEN     "\033[32m"
#define YELLOW    "\033[33m"
#define BLUE      "\033[34m"
#define MAGENTA   "\033[35m"
#define CYAN      "\033[36m"
#define WHITE     "\033[37m"
#define BG_GREEN  "\033[42m"
#define BG_BLUE   "\033[44m"
#define BG_RED    "\033[41m"
#define BG_YELLOW "\033[43m"
#define BG_CYAN   "\033[46m"
#define BG_MAGENTA "\033[45m"

static const char *process_colors[] = {
    "\033[42m", "\033[44m", "\033[41m", "\033[43m",
    "\033[46m", "\033[45m", "\033[47m", "\033[100m"
};
static const char *process_text_colors[] = {
    "\033[32m", "\033[34m", "\033[31m", "\033[33m",
    "\033[36m", "\033[35m", "\033[37m", "\033[90m"
};

/* ============================================================
   SECTION 1 — LOTTERY SCHEDULER
   ============================================================ */
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

static int lottery_draw(LotteryProcess procs[], int n, int current_time) {
    int total = 0;
    for (int i = 0; i < n; i++)
        if (procs[i].arrival_time <= current_time && procs[i].remaining_time > 0)
            total += procs[i].tickets;
    if (total == 0) return -1;

    int winning = (rand() % total) + 1;
    int counter = 0;
    for (int i = 0; i < n; i++) {
        if (procs[i].arrival_time <= current_time && procs[i].remaining_time > 0) {
            counter += procs[i].tickets;
            if (counter >= winning) return i;
        }
    }
    return -1;
}

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
            procs[idx].finish_time      = current_time;
            procs[idx].turnaround_time  = current_time - procs[idx].arrival_time;
            procs[idx].waiting_time     = procs[idx].turnaround_time - procs[idx].burst_time;
            completed++;
        }
    }
}

static void lottery_print_gantt(void) {
    printf(BOLD CYAN "\n╔══════════════════════════════════════╗\n");
    printf(          "║        GANTT CHART (Lottery)         ║\n");
    printf(          "╚══════════════════════════════════════╝\n" RESET);

    /* Compress consecutive identical pids */
    int comp_pid[LOTTERY_MAX_GANTT], comp_start[LOTTERY_MAX_GANTT], comp_end[LOTTERY_MAX_GANTT];
    int segs = 0;
    for (int t = 0; t < l_gantt_len; ) {
        int cur = l_gantt[t].pid, start = t;
        while (t < l_gantt_len && l_gantt[t].pid == cur) t++;
        comp_pid[segs]   = cur;
        comp_start[segs] = start;
        comp_end[segs]   = t;
        segs++;
    }

    printf("\n  ");
    for (int s = 0; s < segs; s++) {
        int w = comp_end[s] - comp_start[s]; if (w > 6) w = 6;
        for (int k = 0; k < w + 2; k++) printf("─");
    }
    printf("\n  ");
    for (int s = 0; s < segs; s++) {
        int dur = comp_end[s] - comp_start[s];
        int w   = (dur > 6) ? 6 : dur;
        int ci  = (comp_pid[s] > 0) ? ((comp_pid[s] - 1) % 8) : 7;
        if (comp_pid[s] == -1) {
            printf(BG_YELLOW " IDL  " RESET);
        } else {
            char lbl[16]; snprintf(lbl, sizeof(lbl), " P%-2d", comp_pid[s]);
            int pad = w + 2 - (int)strlen(lbl);
            printf("%s%s", process_colors[ci], lbl);
            for (int k = 0; k < pad; k++) printf(" ");
            printf(RESET);
        }
    }
    printf("\n  ");
    for (int s = 0; s < segs; s++) {
        int w = comp_end[s] - comp_start[s]; if (w > 6) w = 6;
        for (int k = 0; k < w + 2; k++) printf("─");
    }
    printf("\n  ");
    for (int s = 0; s < segs; s++) {
        int dur = comp_end[s] - comp_start[s];
        int w   = (dur > 6) ? 6 : dur;
        printf(YELLOW "%-*d" RESET, w + 2, comp_start[s]);
    }
    printf(YELLOW "%d\n" RESET, comp_end[segs - 1]);
}

static void lottery_print_results(LotteryProcess procs[], int n) {
    printf(BOLD CYAN "\n╔══════════════════════════════════════════════════════════════╗\n");
    printf(          "║                  RESULTS TABLE (Lottery)                    ║\n");
    printf(          "╚══════════════════════════════════════════════════════════════╝\n" RESET);
    printf(BOLD WHITE "  %-8s %-9s %-9s %-7s %-7s %-12s %-10s\n" RESET,
           "Process","Tickets","Arrival","Burst","Wait","Turnaround","Response");
    printf("  ──────────────────────────────────────────────────────────────\n");

    float avg_wait = 0, avg_tat = 0, avg_resp = 0;
    for (int i = 0; i < n; i++) {
        printf(CYAN "  P%-7d" RESET " %-9d %-9d %-7d %-7d %-12d %-10d\n",
               procs[i].pid, procs[i].tickets, procs[i].arrival_time,
               procs[i].burst_time, procs[i].waiting_time,
               procs[i].turnaround_time, procs[i].response_time);
        avg_wait += procs[i].waiting_time;
        avg_tat  += procs[i].turnaround_time;
        avg_resp += procs[i].response_time;
    }
    printf("  ──────────────────────────────────────────────────────────────\n");
    printf(GREEN "  Average Waiting Time    : %.2f\n" RESET, avg_wait / n);
    printf(GREEN "  Average Turnaround Time : %.2f\n" RESET, avg_tat  / n);
    printf(GREEN "  Average Response Time   : %.2f\n" RESET, avg_resp / n);

    int idle = 0;
    for (int i = 0; i < l_gantt_len; i++) if (l_gantt[i].pid == -1) idle++;
    int total_time = l_gantt[l_gantt_len - 1].time + 1;
    printf(CYAN  "  CPU Utilization         : %.2f%%\n" RESET,
           ((float)(total_time - idle) / total_time) * 100.0f);
    printf(CYAN  "  Throughput              : %.4f processes/unit time\n" RESET,
           (float)n / total_time);
}

static void lottery_print_timeline(LotteryProcess procs[], int n) {
    printf(BOLD CYAN "\n╔══════════════════════════════════════╗\n");
    printf(          "║    PER-PROCESS TIMELINE (Lottery)    ║\n");
    printf(          "╚══════════════════════════════════════╝\n" RESET);

    int max_t = l_gantt_len > 60 ? 60 : l_gantt_len;
    printf("  Time  : ");
    for (int t = 0; t < max_t; t += 5) printf(YELLOW "%-5d" RESET, t);
    printf("\n");

    for (int i = 0; i < n; i++) {
        int ci = (procs[i].pid - 1) % 8;
        printf("  P%-5d: ", procs[i].pid);
        for (int t = 0; t < max_t; t++) {
            if (l_gantt[t].pid == procs[i].pid)
                printf("%s█" RESET, process_text_colors[ci]);
            else if (t < procs[i].arrival_time)
                printf(" ");
            else
                printf("·");
        }
        printf("\n");
    }
    if (l_gantt_len > 60)
        printf(YELLOW "  (truncated at t=60; total=%d)\n" RESET, l_gantt_len);
}

void run_lottery_scheduler(void) {
    srand((unsigned)time(NULL));
    LotteryProcess procs[LOTTERY_MAX_PROC];
    int n;

    printf(BOLD CYAN "\n╔══════════════════════════════════════════╗\n");
    printf(          "║       LOTTERY SCHEDULER SIMULATION       ║\n");
    printf(          "╚══════════════════════════════════════════╝\n" RESET);
    printf(YELLOW "\nEnter number of processes (max %d): " RESET, LOTTERY_MAX_PROC);
    scanf("%d", &n);
    if (n < 1 || n > LOTTERY_MAX_PROC) { printf(RED "Invalid count.\n" RESET); return; }

    for (int i = 0; i < n; i++) {
        procs[i].pid = i + 1;
        printf(GREEN "\n--- Process P%d ---\n" RESET, i + 1);
        printf("  Arrival Time : "); scanf("%d", &procs[i].arrival_time);
        printf("  Burst Time   : "); scanf("%d", &procs[i].burst_time);
        printf("  Tickets      : "); scanf("%d", &procs[i].tickets);
    }

    run_lottery(procs, n);
    lottery_print_gantt();
    lottery_print_timeline(procs, n);
    lottery_print_results(procs, n);
    printf(GREEN "\n[Lottery Simulation Complete]\n" RESET);
}

/* ============================================================
   SECTION 2 — EDF SCHEDULER
   ============================================================ */
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
    memset(edf_gantt, -1, sizeof(edf_gantt));

    while (completed < n) {
        int idx = -1, earliest = INT_MAX;
        for (int i = 0; i < n; i++) {
            if (!proc[i].is_completed && proc[i].arrival_time <= time)
                if (proc[i].deadline < earliest) { earliest = proc[i].deadline; idx = i; }
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

static void edf_print_gantt(EDFProcess proc[], int n) {
    (void)proc; (void)n;
    printf(BOLD CYAN "\n╔══════════════════════════════════════╗\n");
    printf(          "║          GANTT CHART (EDF)           ║\n");
    printf(          "╚══════════════════════════════════════╝\n" RESET);

    int comp_pid[EDF_MAX_TIME], comp_start[EDF_MAX_TIME], comp_end[EDF_MAX_TIME];
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
        int w = comp_end[s] - comp_start[s]; if (w > 6) w = 6;
        for (int k = 0; k < w + 2; k++) printf("─");
    }
    printf("\n  ");
    for (int s = 0; s < segs; s++) {
        int dur = comp_end[s] - comp_start[s];
        int w   = (dur > 6) ? 6 : dur;
        int ci  = (comp_pid[s] > 0) ? ((comp_pid[s] - 1) % 8) : 7;
        if (comp_pid[s] == -1) {
            printf(BG_YELLOW " IDL  " RESET);
        } else {
            char lbl[16]; snprintf(lbl, sizeof(lbl), " P%-2d", comp_pid[s]);
            int pad = w + 2 - (int)strlen(lbl);
            printf("%s%s", process_colors[ci], lbl);
            for (int k = 0; k < pad; k++) printf(" ");
            printf(RESET);
        }
    }
    printf("\n  ");
    for (int s = 0; s < segs; s++) {
        int w = comp_end[s] - comp_start[s]; if (w > 6) w = 6;
        for (int k = 0; k < w + 2; k++) printf("─");
    }
    printf("\n  ");
    for (int s = 0; s < segs; s++) {
        int dur = comp_end[s] - comp_start[s];
        int w   = (dur > 6) ? 6 : dur;
        printf(YELLOW "%-*d" RESET, w + 2, comp_start[s]);
    }
    printf(YELLOW "%d\n" RESET, comp_end[segs - 1]);
}

static void edf_print_timeline(EDFProcess proc[], int n) {
    printf(BOLD CYAN "\n╔══════════════════════════════════════╗\n");
    printf(          "║      PER-PROCESS TIMELINE (EDF)      ║\n");
    printf(          "╚══════════════════════════════════════╝\n" RESET);

    int max_t = edf_gantt_time > 60 ? 60 : edf_gantt_time;
    printf("  Time  : ");
    for (int t = 0; t < max_t; t += 5) printf(YELLOW "%-5d" RESET, t);
    printf("\n");

    for (int i = 0; i < n; i++) {
        int ci = (proc[i].pid - 1) % 8;
        printf("  P%-5d: ", proc[i].pid);
        for (int t = 0; t < max_t; t++) {
            if (edf_gantt[t] == proc[i].pid)
                printf("%s█" RESET, process_text_colors[ci]);
            else if (t < proc[i].arrival_time)
                printf(" ");
            else
                printf("·");
        }
        if (proc[i].deadline <= max_t)
            printf(RED " D=%d" RESET, proc[i].deadline);
        printf("\n");
    }
    if (edf_gantt_time > 60)
        printf(YELLOW "  (truncated at t=60; total=%d)\n" RESET, edf_gantt_time);
}

static void edf_print_results(EDFProcess proc[], int n) {
    printf(BOLD CYAN "\n╔══════════════════════════════════════════════════════════════════════════╗\n");
    printf(          "║                    RESULTS TABLE (EDF)                                  ║\n");
    printf(          "╚══════════════════════════════════════════════════════════════════════════╝\n" RESET);
    printf(BOLD WHITE "  %-5s %-8s %-7s %-9s %-7s %-12s %-13s %-10s %-8s\n" RESET,
           "PID","Arrival","Burst","Deadline","Start","Completion","Turnaround","Waiting","Status");
    printf("  ─────────────────────────────────────────────────────────────────────────\n");

    double total_tat = 0, total_wt = 0;
    int missed = 0;
    for (int i = 0; i < n; i++) {
        const char *sc = proc[i].missed_deadline ? RED : GREEN;
        const char *st = proc[i].missed_deadline ? "MISSED ✗" : "MET    ✓";
        printf(CYAN "  P%-4d" RESET " %-8d %-7d %-9d %-7d %-12d %-13d %-10d %s%s\n" RESET,
               proc[i].pid, proc[i].arrival_time, proc[i].burst_time, proc[i].deadline,
               proc[i].start_time, proc[i].completion_time,
               proc[i].turnaround_time, proc[i].waiting_time, sc, st);
        total_tat += proc[i].turnaround_time;
        total_wt  += proc[i].waiting_time;
        if (proc[i].missed_deadline) missed++;
    }
    printf("  ─────────────────────────────────────────────────────────────────────────\n");
    printf(GREEN "  Average Turnaround Time : %.2f\n" RESET, total_tat / n);
    printf(GREEN "  Average Waiting Time    : %.2f\n" RESET, total_wt  / n);

    int idle = 0;
    for (int t = 0; t < edf_gantt_time; t++) if (edf_gantt[t] == -1) idle++;
    printf(CYAN  "  CPU Utilization         : %.1f%%\n" RESET,
           edf_gantt_time > 0 ? (100.0 * (edf_gantt_time - idle) / edf_gantt_time) : 0.0);

    if (missed == 0)
        printf(GREEN "  Deadline Miss Rate      : 0/%d (All deadlines MET!)\n" RESET, n);
    else
        printf(RED   "  Deadline Miss Rate      : %d/%d processes MISSED deadline\n" RESET, missed, n);
}

void run_edf_scheduler(void) {
    EDFProcess proc[EDF_MAX_PROC];
    int n;

    printf(BOLD CYAN "\n╔══════════════════════════════════════════╗\n");
    printf(          "║  EDF (Earliest Deadline First) Scheduler ║\n");
    printf(          "╚══════════════════════════════════════════╝\n" RESET);
    printf(YELLOW "\nEnter number of processes (max %d): " RESET, EDF_MAX_PROC);
    scanf("%d", &n);
    if (n < 1 || n > EDF_MAX_PROC) { printf(RED "Invalid count.\n" RESET); return; }

    for (int i = 0; i < n; i++) {
        proc[i].pid = i + 1;
        printf(GREEN "\n  P%d -> " RESET, i + 1);
        printf("Arrival : "); scanf("%d", &proc[i].arrival_time);
        printf("         Burst   : "); scanf("%d", &proc[i].burst_time);
        printf("         Deadline: "); scanf("%d", &proc[i].deadline);
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
    printf(GREEN "\n[EDF Simulation Complete]\n" RESET);
}

/* ============================================================
   SECTION 3 — MLFQ SCHEDULER
   ============================================================ */
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

static void mq_init(MQueue *q)    { q->front=0; q->rear=-1; q->size=0; }
static int  mq_empty(MQueue *q)   { return q->size==0; }
static void mq_enqueue(MQueue *q, MProcess *p) {
    if (q->size < MLFQ_MAX_PROC) { q->rear=(q->rear+1)%MLFQ_MAX_PROC; q->processes[q->rear]=p; q->size++; }
}
static MProcess *mq_dequeue(MQueue *q) {
    if (q->size>0) { MProcess *p=q->processes[q->front]; q->front=(q->front+1)%MLFQ_MAX_PROC; q->size--; return p; }
    return NULL;
}

static void mlfq_init(MLFQSched *s, int numCpus, int *quantums, int levels, int aging, int maxP) {
    s->numLevels = (levels > MLFQ_MAX_LEVELS) ? MLFQ_MAX_LEVELS : levels;
    for (int i = 0; i < s->numLevels; i++) { mq_init(&s->queues[i]); s->timeQuantums[i] = quantums[i]; }
    s->agingThreshold = aging;
    s->currentTime = 0;
    s->numCpus = numCpus;
    s->cpus = (MCPU*)malloc(sizeof(MCPU)*numCpus);
    for (int i = 0; i < numCpus; i++) { s->cpus[i].id=i; s->cpus[i].currentProcess=NULL; s->cpus[i].activeTime=0; s->cpus[i].idleTime=0; }
    s->totalCapacity = maxP;
    s->allProcesses = (MProcess**)malloc(sizeof(MProcess*)*maxP);
    s->processCount = 0;
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
    if (s->processCount < s->totalCapacity) s->allProcesses[s->processCount++] = p;
}

static void mlfq_arrivals(MLFQSched *s) {
    for (int i = 0; i < s->processCount; i++) {
        MProcess *p = s->allProcesses[i];
        if (p->arrivalTime == s->currentTime && p->state == MLFQ_READY && p->firstExecutionTime == -2) {
            p->firstExecutionTime = -1;
            p->currentQueueLevel  = 0;
            p->timeSpentInCurrentQueueWaiting = 0;
            printf("[Time %3d] P%d arrived → Queue 0\n", s->currentTime, p->pid);
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
                printf("[Time %3d] P%d starving → promoted Q%d→Q%d\n",
                       s->currentTime, p->pid, p->currentQueueLevel, p->currentQueueLevel-1);
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
            printf("[Time %3d] P%d completed on CPU%d\n", s->currentTime, p->pid, cpu->id);
            p->state = MLFQ_COMPLETED;
            p->completionTime  = s->currentTime;
            p->turnaroundTime  = p->completionTime - p->arrivalTime;
            p->waitingTime     = p->turnaroundTime - p->burstTime;
            cpu->currentProcess = NULL;
            s->completedProcesses++;
        } else if (p->timeSliceConsumed >= s->timeQuantums[p->currentQueueLevel]) {
            p->state = MLFQ_READY;
            p->timeSliceConsumed = 0;
            p->timeSpentInCurrentQueueWaiting = 0;
            if (p->currentQueueLevel < s->numLevels-1) p->currentQueueLevel++;
            printf("[Time %3d] P%d slice expired → Queue %d\n", s->currentTime, p->pid, p->currentQueueLevel);
            mq_enqueue(&s->queues[p->currentQueueLevel], p);
            cpu->currentProcess = NULL;
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
                    printf("[Time %3d] CPU%d ← P%d (Q%d)\n", s->currentTime, cpu->id, p->pid, p->currentQueueLevel);
                    break;
                }
            }
        } else {
            MProcess *rp = cpu->currentProcess;
            int rq = rp->currentQueueLevel, hq = -1;
            for (int j = 0; j < s->numLevels; j++) { if (!mq_empty(&s->queues[j])) { hq=j; break; } }
            if (hq != -1 && hq < rq) {
                printf("[Time %3d] CPU%d preempting P%d(Q%d) for Q%d\n", s->currentTime, cpu->id, rp->pid, rq, hq);
                rp->state = MLFQ_READY; rp->timeSpentInCurrentQueueWaiting = 0;
                mq_enqueue(&s->queues[rp->currentQueueLevel], rp);
                MProcess *np = mq_dequeue(&s->queues[hq]);
                if (np->firstExecutionTime == -1) { np->firstExecutionTime = s->currentTime; np->responseTime = s->currentTime - np->arrivalTime; }
                np->state = MLFQ_RUNNING; np->timeSpentInCurrentQueueWaiting = 0;
                cpu->currentProcess = np;
                printf("[Time %3d] CPU%d ← P%d (Q%d)\n", s->currentTime, cpu->id, np->pid, np->currentQueueLevel);
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
    printf(BOLD BLUE "\n=== MLFQ Simulation ===\n" RESET);
    printf("CPUs: %d | Levels: %d | Aging: %d\n", s->numCpus, s->numLevels, s->agingThreshold);
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
        if (s->currentTime > 100000) { printf(RED "Time limit exceeded!\n" RESET); break; }
    }
    printf(GREEN "[Time %d] MLFQ simulation complete.\n\n" RESET, s->currentTime);
}

static void mlfq_print_metrics(MLFQSched *s) {
    printf(BOLD CYAN "\n╔══════════════════════════════════════════════════════════════╗\n");
    printf(          "║                  RESULTS TABLE (MLFQ)                       ║\n");
    printf(          "╚══════════════════════════════════════════════════════════════╝\n" RESET);
    printf(BOLD WHITE "  %-6s %-10s %-8s %-12s %-12s %-10s %-10s\n" RESET,
           "PID","Arrival","Burst","Completion","Turnaround","Waiting","Response");
    printf("  ──────────────────────────────────────────────────────────────────\n");

    double tw=0, tt=0, tr=0;
    for (int i = 0; i < s->processCount; i++) {
        MProcess *p = s->allProcesses[i];
        printf(CYAN "  P%-5d" RESET " %-10d %-8d %-12d %-12d %-10d %-10d\n",
               p->pid, p->arrivalTime, p->burstTime, p->completionTime,
               p->turnaroundTime, p->waitingTime, p->responseTime);
        tw += p->waitingTime; tt += p->turnaroundTime; tr += p->responseTime;
    }
    printf("  ──────────────────────────────────────────────────────────────────\n");
    int n = s->processCount;
    printf(GREEN "  Average Waiting Time    : %.2f\n" RESET, tw/n);
    printf(GREEN "  Average Turnaround Time : %.2f\n" RESET, tt/n);
    printf(GREEN "  Average Response Time   : %.2f\n" RESET, tr/n);
    printf(CYAN  "  Throughput              : %.5f processes/unit time\n" RESET, (double)n/s->currentTime);

    printf(BOLD MAGENTA "\n  CPU Utilization:\n" RESET);
    for (int i = 0; i < s->numCpus; i++) {
        MCPU *cpu = &s->cpus[i];
        double u = s->currentTime > 0 ? ((double)cpu->activeTime/s->currentTime)*100.0 : 0.0;
        printf("  CPU %d: %.2f%% (Active: %d, Idle: %d)\n", cpu->id, u, cpu->activeTime, cpu->idleTime);
    }
}

void run_mlfq_scheduler(void) {
    MLFQSched sched;
    int numCpus, numLevels, aging;

    printf(BOLD CYAN "\n╔══════════════════════════════════════════╗\n");
    printf(          "║  MLFQ (Multi-Level Feedback Queue) Sched ║\n");
    printf(          "╚══════════════════════════════════════════╝\n" RESET);

    printf(YELLOW "Number of CPUs       : " RESET); scanf("%d", &numCpus);
    if (numCpus < 1) numCpus = 1;
    printf(YELLOW "Number of queue levels (max %d): " RESET, MLFQ_MAX_LEVELS); scanf("%d", &numLevels);
    if (numLevels < 1 || numLevels > MLFQ_MAX_LEVELS) numLevels = 3;

    int quantums[MLFQ_MAX_LEVELS];
    printf(YELLOW "Enter time quantum for each level:\n" RESET);
    for (int i = 0; i < numLevels; i++) {
        printf("  Level %d quantum: ", i); scanf("%d", &quantums[i]);
    }
    printf(YELLOW "Aging threshold (time units before promotion): " RESET); scanf("%d", &aging);

    int n;
    printf(YELLOW "Number of processes (max %d): " RESET, MLFQ_MAX_PROC); scanf("%d", &n);
    if (n < 1 || n > MLFQ_MAX_PROC) { printf(RED "Invalid count.\n" RESET); return; }

    mlfq_init(&sched, numCpus, quantums, numLevels, aging, n);

    for (int i = 0; i < n; i++) {
        MProcess *p = (MProcess*)malloc(sizeof(MProcess));
        p->pid = i+1;
        printf(GREEN "\n--- Process P%d ---\n" RESET, i+1);
        printf("  Arrival Time: "); scanf("%d", &p->arrivalTime);
        printf("  Burst Time  : "); scanf("%d", &p->burstTime);
        p->remainingTime = p->burstTime;
        p->completionTime = p->waitingTime = p->turnaroundTime = p->responseTime = 0;
        p->firstExecutionTime = -2;   /* -2 = not yet arrived */
        p->currentQueueLevel  = 0;
        p->timeSliceConsumed  = 0;
        p->timeSpentInCurrentQueueWaiting = 0;
        p->state = MLFQ_READY;
        mlfq_add(&sched, p);
    }

    mlfq_simulate(&sched);
    mlfq_print_metrics(&sched);
    mlfq_cleanup(&sched);
    printf(GREEN "\n[MLFQ Simulation Complete]\n" RESET);
}

/* ============================================================
   MAIN MENU
   ============================================================ */
int main(void) {
    int choice;

    while (1) {
        printf(BOLD CYAN "\n╔══════════════════════════════════════════════╗\n");
        printf(          "║      OS PROJECT 2 — Scheduling Algorithms    ║\n");
        printf(          "╠══════════════════════════════════════════════╣\n");
        printf(          "║  1. Lottery Scheduler                        ║\n");
        printf(          "║  2. EDF (Earliest Deadline First)            ║\n");
        printf(          "║  3. MLFQ (Multi-Level Feedback Queue)        ║\n");
        printf(          "║  0. Exit                                     ║\n");
        printf(          "╚══════════════════════════════════════════════╝\n" RESET);
        printf(YELLOW "Select algorithm: " RESET);
        scanf("%d", &choice);

        switch (choice) {
            case 1: run_lottery_scheduler(); break;
            case 2: run_edf_scheduler();     break;
            case 3: run_mlfq_scheduler();    break;
            case 0:
                printf(GREEN "\nGoodbye!\n" RESET);
                return 0;
            default:
                printf(RED "Invalid choice. Try again.\n" RESET);
        }
    }
}
