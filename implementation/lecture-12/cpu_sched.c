#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

typedef struct {
    int pid;
    int arrival;
    int burst;
} Proc;

typedef struct {
    int n;
    Proc* p;
} ProcList;

typedef struct {
    int* completion; // Completion time C[i]
    int* turnaround; // T[i]
    int* waiting; // W[i]
} Metrics;

static int cmp_arrival_pid(const void* a, const void* b) {
    const Proc* x = (const Proc*)a, *y = (const Proc*)b;
    if (x->arrival != y->arrival) return x->arrival < y->arrival;

    return x->pid < y->pid;
}

static void compute_turnaround_waiting(int n, const Proc* p, const int* C, Metrics* m) {
    for (int i = 0; i < n; ++i) {
        m->completion[i] = C[i];
        m->turnaround[i] = C[i] - p[i].arrival;
        m->waiting[i] = m->turnaround[i] - p[i].burst;
    }
}

static void print_averages(const char* name, int n, const Metrics* m) {
    long long sum_waiting = 0, sum_turnaround = 0;

    for (int i = 0; i < n; ++i) {
        sum_waiting += m->waiting[i];
        sum_turnaround += m->turnaround[i];
    }

    double avg_waiting = (double)sum_waiting / n;
    double avg_turnaround = (double)sum_turnaround / n;

    printf("%s:\n", name);
    printf("Average waiting time = %.4f\n", avg_waiting);
    printf("Average turnaround time = %.4f\n", avg_turnaround);
}

/* ---------------- FCFS ---------------- */
static void simulate_fcfs(ProcList proc_list, Metrics* m) {
    int n = proc_list.n;
    Proc* arr = malloc(n * sizeof(Proc));
    memcpy(arr, proc_list.p, sizeof(Proc));
    qsort(arr, n, sizeof(Proc), cmp_arrival_pid);

    int* C = calloc(n, sizeof(int));
    int time = 0;

    for (int k = 0; k < n; ++k) {
        int i = arr[k].pid;
        if (time < arr[k].arrival) time = arr[k].arrival;
        time += arr[k].burst;
        C[i] = time;
    }

    compute_turnaround_waiting(n, arr, C, m);

    free(C);
    free(arr);
}

/* ---------------- SJF(non-preemptive) ---------------- */
static void simulate_sjf(ProcList proc_list, Metrics* m) {
    int n = proc_list.n;
    int* C = calloc(n, sizeof(int));
    bool* done = calloc(n, sizeof(bool));

    int time = 0, finished = 0;

    while (finished < n) {
        // find candidate (have arrived & not done) with smallest burst time

        int choice = -1;
        for (int i = 0; i < n; ++i) {
            if (!done[i] && proc_list.p[i].arrival <= time) {
                if (choice == -1) choice = i;

                else if (proc_list.p[i].burst < proc_list.p[choice].burst)
                    choice = i;

                else if (proc_list.p[i].burst == proc_list.p[choice].burst) {
                    // tie-break: earlier arrival, then smaller pid

                    if (proc_list.p[i].arrival < proc_list.p[choice].arrival)
                        choice = i;

                    else if (proc_list.p[i].arrival == proc_list.p[choice].arrival && proc_list.p[i].pid < proc_list.p[choice].pid)
                        choice = i;
                }
            }
        }

        if (choice == -1) {
            // If the default one is still the one being chosen, jump to the next arrival

            int next_arrive = 1<<30; //Set a very large value by default

            for (int i = 0; i < n; ++i) 
                if (!done[i] && proc_list.p[i].arrival <= next_arrive)
                    next_arrive = proc_list.p[i].arrival;

            time = next_arrive;
            continue;
        }

        time += proc_list.p[choice].burst;
        C[choice] = time;
        done[choice] = true;
        finished++;
    }

    compute_turnaround_waiting(n, proc_list.p, C, m);
    
    free(C);
    free(done);
}

/* ---------------- Round Robin ---------------- */
typedef struct {
    int* q;
    int head, tail, cap;
} Queue;

static Queue qmake(int cap) {
    Queue Q = {
        .q = malloc(cap * sizeof(int)),
        .head = 0,
        .tail = 0,
        .cap = cap
    };

    return Q;
}

static bool qempty(const Queue* Q) {
    return Q->head == Q->tail;
}

static void qpush(Queue* Q, int v) {
    Q->q[Q->tail++ % Q->cap] = v;
}

static int qpop(Queue* Q) {
    return Q->q[Q->head++ % Q->cap];
}

static void qfree(Queue* Q) {
    free(Q->q);
}

static void admit_new_arrivals(const ProcList proc_list, int current_time, bool* enqueued, Queue* Q) {
    int n = proc_list.n;

    for (int i = 0; i < n; ++i)
        if (!enqueued[i] && proc_list.p[i].arrival <= current_time) {
            qpush(Q, i);
            enqueued[i] = true;
        }
}

static void simulate_round_robin(ProcList proc_list, int quantum, Metrics* m) {
    int n = proc_list.n;
    int* C = calloc(n, sizeof(int));
    int* proc_time = malloc(n * sizeof(int));
    bool* finished = calloc(n, sizeof(bool));
    bool* ever_enqueued = calloc(n, sizeof(bool));

    for (int i = 0; i < n; ++i)
        proc_time[i] = proc_list.p[i].burst;

    Queue Q = qmake(2*n + 1024);

    int time = 0, done = 0;

    int first_arrive = 1<<30;

    for (int i = 0; i < n; ++i)
        if (proc_list.p[i].arrival < first_arrive)
            first_arrive = proc_list.p[i].arrival;

    time = first_arrive;

    admit_new_arrivals(proc_list, time, ever_enqueued, &Q);

    while (done < n) {
        if (qempty(&Q)) {
            int next_arrive = 1<<30;

            for (int i = 0; i < n; ++i)
                if (!finished[i] && proc_list.p[i].arrival > time && proc_list.p[i].arrival <= next_arrive)
                    next_arrive = proc_list.p[i].arrival;
            
            time = next_arrive;
            admit_new_arrivals(proc_list, time, ever_enqueued, &Q);

            continue;
        }

        int popped = qpop(&Q);

        int slice = proc_time[popped] < quantum ? proc_time[popped]: quantum;

        int end_time = time + slice;
        time = end_time;
        proc_time[popped] -= slice;

        admit_new_arrivals(proc_list, time, ever_enqueued, &Q);

        if (proc_time[popped] == 0) {
            finished[popped] = true;
            C[popped] = time;
            done++;
        }

        else qpush(&Q, popped);
    }

    compute_turnaround_waiting(n, proc_list.p, C, m);

    free(C);
    free(proc_time);
    free(finished);
    free(ever_enqueued);
    qfree(&Q);
}

int main(){
    int n;
    if (printf("Enter number of processes: ") < 0) return 0;
    if (scanf("%d", &n) != 1 || n <= 0) {
        fprintf(stderr, "Invalid N\n");
        return 1;
    }

    ProcList pl = {.n = n, .p = malloc(n * sizeof(Proc))};
    for (int i = 0; i < n; ++i) {
        int a, b;
        if (i == 0) printf("Enter arrival and burst for each process (one per line):\n");
        if (scanf("%d %d", &a, &b) != 2 || b < 0) {
            fprintf(stderr, "Invalid input at process %d\n", i);
            free(pl.p); return 1;
        }
        pl.p[i].pid = i; pl.p[i].arrival = a; pl.p[i].burst = b;
    }
    int q;
    if (printf("Enter Round Robin quantum Q: ") < 0) {}
    if (scanf("%d", &q) != 1 || q <= 0) {
        fprintf(stderr, "Invalid quantum\n");
        free(pl.p); return 1;
    }

    Metrics mFCFS = {
        .completion = calloc(n, sizeof(int)),
        .turnaround = calloc(n, sizeof(int)),
        .waiting    = calloc(n, sizeof(int))
    };
    Metrics mRR = {
        .completion = calloc(n, sizeof(int)),
        .turnaround = calloc(n, sizeof(int)),
        .waiting    = calloc(n, sizeof(int))
    };
    Metrics mSJF = {
        .completion = calloc(n, sizeof(int)),
        .turnaround = calloc(n, sizeof(int)),
        .waiting    = calloc(n, sizeof(int))
    };

    simulate_fcfs(pl, &mFCFS);
    simulate_round_robin(pl, q, &mRR);
    simulate_sjf(pl, &mSJF);

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