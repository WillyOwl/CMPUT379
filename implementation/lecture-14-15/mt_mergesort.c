#define _POSIX_C_SOURCE 200809L
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <unistd.h>

typedef struct {
    int* a; // array
    int* tmp; // shared temporary buffer (size = n)
    int left; // inclusive start index
    int right; // exclusive end index
    int depth; // current recursion depth
    int max_depth; // max depth where we still spawn threads

} Task;

static void* mergesort_worker(void* arg);

static void merge(int* a, int* tmp, int left, int mid, int right) {
    // merge [left, mid) and [mid, right) into tmp, then copy back to a

    int i = left, j = mid, k = left;

    while (i < mid && j < right) {
        // Keeps elements in the left side always <= right side
        if (a[i] <= a[j]) tmp[k++] = a[i++];
        
        else tmp[k++] = a[j++];
    }

    while (i < mid)
        tmp[k++] = a[i++];

    while (j < right)
        tmp[k++] = a[j++];

    // Copy back

    for (int t = left; t < right; ++t)
        a[t] = tmp[t];
}

void mergesort_run(Task* t) {
    int left = t->left, right = t->right;
    if (right - left <= 1) return;

    int mid = left + (right - left) / 2;

    if (t->depth < t->max_depth) {
        // Spawn two threads

        Task left_task = {t->a, t->tmp, left, mid, t->depth + 1, t->max_depth};
        Task right_task = {t->a, t->tmp, mid, right, t->depth + 1, t->max_depth};

        pthread_t thread_left, thread_right;

        int rc;

        rc = pthread_create(&thread_left, NULL, mergesort_worker, &left_task);

        if (rc != 0) {
            fprintf(stderr, "pthread_create: %s\n", strerror(rc));
            exit(1);
        }

        rc = pthread_create(&thread_right, NULL, mergesort_worker, &right_task);

        if (rc != 0) {
            fprintf(stderr, "pthread_create: %s\n", strerror(rc));
            exit(1);
        }

        rc = pthread_join(thread_left, NULL);

        if (rc != 0) {
            fprintf(stderr, "pthread_join: %s\n", strerror(rc));
            exit(1);
        }

        rc = pthread_join(thread_right, NULL);

        if (rc != 0) {
            fprintf(stderr, "pthread_join: %s\n", strerror(rc));
            exit(1);
        }

        /*Use pthread_join to wait is because that merge
        can't be occurred until both threads finish
        
        without it, merge() could read half-sorted data, causing wrong result*/  

    }

    else {
        Task left_task = {t->a, t->tmp, left, right, t->depth + 1, t->max_depth};
        Task right_task = {t->a, t->tmp, left, right, t->depth + 1, t->max_depth};

        mergesort_run(&left_task);
        mergesort_run(&right_task);
    }

    merge(t->a, t->tmp, left, mid, right);
}

static void* mergesort_worker(void* arg) {
    /*In POSIX threads, every new thread must start executing from a function
    with void* (start_routine)(void*);
    */

    Task local = *(Task*)arg;

    /*Parent reuses/overwrites that stack slot before the worker reads it.
Even if the parent hasn’t returned, compilers may reuse stack space for later locals; 
or code might mutate left. If the worker hasn’t copied yet, it could read partially stale/overwritten data → UB or wrong work.*/

    mergesort_run(&local);

    return NULL;
}

int parse_threads_from_args(int argc, char* argv[], int default_threads) {
    for (int i = 1; i < argc; ++i) {
        if (strncmp(argv[i], "--threads=", 10) == 0) {
            int t = atoi(argv[i] + 10);

            if (t >= 1) return t;
            fprintf(stderr, "Ignoring invalid --threads value: %s\n", argv[i]+10);
        }
    }

    return default_threads;
}

int suggest_default_threads() {
#if defined(_SC_NPROCESSORS_ONLN)
    long n = sysconf(_SC_NPROCESSORS_ONLN);

    if (n > 0 && n < 4096) return (int)n;
#endif

    return 4;
}

int main(int argc, char* argv[]) {
    // Input format:
    // First line: n
    // Next line(s): n integers

    int n;
    if (scanf("%d", &n) != 1 || n < 0) {
        fprintf(stderr, "Expected a non-negative n on stdin.\n");
        return 1;
    }

    int* a = (int*)malloc(n * sizeof(int));
    int* tmp = (int*)malloc(n * sizeof(int));

    if (!a || !tmp) {
        fprintf(stderr, "malloc failed\n");
        return 1;
    }

    for (int i = 0; i < n; ++i) 
        if (scanf("%d", &a[i]) != 1) {
            fprintf(stderr, "Expected %d integers after n.\n", n);
            free(a); free(tmp);
            return 1;
        }

    int default_threads = suggest_default_threads();
    int max_threads = parse_threads_from_args(argc, argv, default_threads);

    if (max_threads < 1) max_threads = 1;

    int max_depth = 0;

    while ((1 << (max_depth + 1)) <= max_threads) 
        max_depth++;

    Task root = {a, tmp, 0, n, 0, max_depth};

    mergesort_run(&root);

    // Output the sorted array (one line)

    for (int i = 0; i < n; ++i) {
        if (i) putchar(' ');
        printf("%d", a[i]);
    }

    putchar('\n');

    free(a); free(tmp);

    return 0;
}