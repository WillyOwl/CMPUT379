// cpu_sched.c — FCFS, RR(Q), SJF(non-preemptive) without using PID for scheduling
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

typedef struct {
    int idx;      // input order index (stable tie-breaker only for mapping results)
    int arrival;  // arrival time
    int burst;    // burst time
} Proc;

typedef struct {
    int n;
    Proc *p;      // p[i].idx == i (input order)
} ProcList;

typedef struct {
    int *completion;  // completion time C[i]
    int *turnaround;  // T[i] = C - arrival
    int *waiting;     // W[i] = T - burst
} Metrics;

/* ----- Helpers ----- */
static void compute_TW(int n, const Proc *p, const int *C, Metrics *m) {
    for (int i = 0; i < n; ++i) {
        m->completion[i] = C[i];
        m->turnaround[i] = C[i] - p[i].arrival;
        m->waiting[i]    = m->turnaround[i] - p[i].burst;
    }
}

static void print_averages(const char *name, int n, const Metrics *m) {
    long long sumW = 0, sumT = 0;
    for (int i = 0; i < n; ++i) { sumW += m->waiting[i]; sumT += m->turnaround[i]; }
    printf("%s:\n", name);
    printf("  Average Waiting Time    = %.4f\n", (double)sumW / n);
    printf("  Average Turnaround Time = %.4f\n\n", (double)sumT / n);
}

/* ---------------- FCFS (ignore PID; stable on input order) ---------------- */
static int cmp_arrival_only(const void *a, const void *b) {
    const Proc *x = (const Proc*)a, *y = (const Proc*)b;
    if (x->arrival != y->arrival) return (x->arrival < y->arrival) ? -1 : 1;
    // Keep input order if arrivals tie: return 0 to remain stable if the sort is stable.
    // As qsort isn't stable, we encode stability by comparing original indices:
    return (x->idx < y->idx) ? -1 : (x->idx > y->idx);
}

static void simulate_fcfs(ProcList pl, Metrics *m) {
    int n = pl.n;
    Proc *arr = malloc(n * sizeof(Proc));
    memcpy(arr, pl.p, n * sizeof(Proc));
    qsort(arr, n, sizeof(Proc), cmp_arrival_only);

    int *C = calloc(n, sizeof(int));
    int time = 0;
    for (int k = 0; k < n; ++k) {
        const Proc *job = &arr[k];
        if (time < job->arrival) time = job->arrival;
        time += job->burst;
        C[job->idx] = time; // map back to input order index
    }
    compute_TW(n, pl.p, C, m);
    free(C);
    free(arr);
}

/* -------------- SJF (Non-preemptive; ignore PID) -------------- */
static void simulate_sjf_np(ProcList pl, Metrics *m) {
    int n = pl.n;
    int *C = calloc(n, sizeof(int));
    bool *done = calloc(n, sizeof(bool));

    int time = 0, finished = 0;

    while (finished < n) {
        int choice = -1;
        for (int i = 0; i < n; ++i) {
            if (!done[i] && pl.p[i].arrival <= time) {
                if (choice == -1) choice = i;
                else {
                    // Primary: shortest burst
                    if (pl.p[i].burst < pl.p[choice].burst) choice = i;
                    // Tie 1: earlier arrival
                    else if (pl.p[i].burst == pl.p[choice].burst &&
                             pl.p[i].arrival <  pl.p[choice].arrival) choice = i;
                    // Tie 2: input order (lower idx) — still doesn't use PID
                    else if (pl.p[i].burst == pl.p[choice].burst &&
                             pl.p[i].arrival == pl.p[choice].arrival &&
                             pl.p[i].idx < pl.p[choice].idx) choice = i;
                }
            }
        }
        if (choice == -1) {
            // No job ready: jump to next arrival
            int next_arr = 1<<30;
            for (int i = 0; i < n; ++i)
                if (!done[i] && pl.p[i].arrival < next_arr)
                    next_arr = pl.p[i].arrival;
            time = next_arr;
            continue;
        }
        time += pl.p[choice].burst; // run to completion
        C[choice] = time;
        done[choice] = true;
        ++finished;
    }

    compute_TW(n, pl.p, C, m);
    free(C);
    free(done);
}

/* ---------------- Round Robin (ignore PID) ---------------- */
typedef struct { int *q; int head, tail, cap; } Queue;
static Queue qmake(int cap){ Queue Q={.q=malloc(cap*sizeof(int)),.head=0,.tail=0,.cap=cap};return Q; }
static bool  qempty(const Queue *Q){ return Q->head==Q->tail; }
static void  qpush(Queue *Q,int v){ Q->q[Q->tail++ % Q->cap]=v; }
static int   qpop (Queue *Q){ return Q->q[Q->head++ % Q->cap]; }
static void  qfree(Queue *Q){ free(Q->q); }

static void admit_by_arrival_input_order(const ProcList pl, int cur_time, bool *enqueued, Queue *Q) {
    // Scan in input order to keep stability for simultaneous arrivals
    for (int i = 0; i < pl.n; ++i) {
        if (!enqueued[i] && pl.p[i].arrival <= cur_time) {
            qpush(Q, i);
            enqueued[i] = true;
        }
    }
}

static void simulate_rr(ProcList pl, int Qslice, Metrics *m) {
    int n = pl.n;
    int *C = calloc(n, sizeof(int));
    int *rem = malloc(n * sizeof(int));
    bool *finished = calloc(n, sizeof(bool));
    bool *ever_enqueued = calloc(n, sizeof(bool));

    for (int i = 0; i < n; ++i) rem[i] = pl.p[i].burst;

    Queue Q = qmake(2*n + 64);

    int time = 0, done = 0;

    // Start at first arrival
    int first_arr = 1<<30;
    for (int i = 0; i < n; ++i) if (pl.p[i].arrival < first_arr) first_arr = pl.p[i].arrival;
    time = first_arr;
    admit_by_arrival_input_order(pl, time, ever_enqueued, &Q);

    while (done < n) {
        if (qempty(&Q)) {
            // Jump to next arrival
            int next_arr = 1<<30;
            for (int i = 0; i < n; ++i)
                if (!finished[i] && pl.p[i].arrival > time && pl.p[i].arrival < next_arr)
                    next_arr = pl.p[i].arrival;
            time = next_arr;
            admit_by_arrival_input_order(pl, time, ever_enqueued, &Q);
            continue;
        }

        int i = qpop(&Q);
        int slice = rem[i] < Qslice ? rem[i] : Qslice;

        time += slice;
        rem[i] -= slice;

        // Admit all that have arrived by 'time' in input order
        admit_by_arrival_input_order(pl, time, ever_enqueued, &Q);

        if (rem[i] == 0) {
            finished[i] = true;
            C[i] = time;
            ++done;
        } else {
            // Not finished: re-enqueue to tail
            qpush(&Q, i);
        }
    }

    compute_TW(n, pl.p, C, m);
    free(C); free(rem); free(finished); free(ever_enqueued); qfree(&Q);
}

/* ---------------- Driver ---------------- */
int main(void) {
    int n;
    if (printf("Enter number of processes: ") < 0) return 0;
    if (scanf("%d", &n) != 1 || n <= 0) { fprintf(stderr, "Invalid N\n"); return 1; }

    ProcList pl = {.n = n, .p = malloc(n * sizeof(Proc))};
    for (int i = 0; i < n; ++i) {
        int a, b;
        if (i == 0) printf("Enter arrival and burst for each process (one per line):\n");
        if (scanf("%d %d", &a, &b) != 2 || b < 0) {
            fprintf(stderr, "Invalid input at process %d\n", i);
            free(pl.p); return 1;
        }
        pl.p[i].idx = i; pl.p[i].arrival = a; pl.p[i].burst = b;
    }
    int q;
    if (printf("Enter Round Robin quantum Q: ") < 0) {}
    if (scanf("%d", &q) != 1 || q <= 0) { fprintf(stderr, "Invalid quantum\n"); free(pl.p); return 1; }

    Metrics mFCFS = { .completion = calloc(n,sizeof(int)), .turnaround = calloc(n,sizeof(int)), .waiting = calloc(n,sizeof(int)) };
    Metrics mRR   = { .completion = calloc(n,sizeof(int)), .turnaround = calloc(n,sizeof(int)), .waiting = calloc(n,sizeof(int)) };
    Metrics mSJF  = { .completion = calloc(n,sizeof(int)), .turnaround = calloc(n,sizeof(int)), .waiting = calloc(n,sizeof(int)) };

    simulate_fcfs(pl, &mFCFS);
    simulate_rr  (pl, q, &mRR);
    simulate_sjf_np(pl, &mSJF);

    puts("");
    print_averages("FCFS", n, &mFCFS);
    print_averages("Round Robin", n, &mRR);
    print_averages("SJF (non-preemptive)", n, &mSJF);

    free(mFCFS.completion); free(mFCFS.turnaround); free(mFCFS.waiting);
    free(mRR.completion);   free(mRR.turnaround);   free(mRR.waiting);
    free(mSJF.completion);  free(mSJF.turnaround);  free(mSJF.waiting);
    free(pl.p);
    return 0;
}
