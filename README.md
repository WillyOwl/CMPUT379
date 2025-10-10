# CMPUT 379 Repository Overview

This repository consolidates course material, assignments, and reference implementations for CMPUT 379 (Operating Systems Concepts). It captures both completed coursework and lecture examples in C that demonstrate systems programming patterns such as process management, interprocess communication, networking, and RPC.

## Repository Structure

| Path | Description |
| --- | --- |
| `assignment/a1/` | Source, report, and supporting files for the Dragon Shell assignment, including `dragonshell.c`, build scripts, and marking rubric. |
| `ch2/prog-problems/` | Practice programs from Chapter 2 of the course text, currently featuring a POSIX file copy utility (`FileCopy.c`). |
| `implementation/lecture-8/` | Example producer/consumer pipeline that demonstrates interprocess communication via UNIX pipes (`pc_pipe.c`). |
| `implementation/lecture-10/` | TCP and UDP networking samples, including iterative and concurrent servers plus companion clients. |
| `implementation/lecture-11/` | RPC client/server pair illustrating ONC RPC usage in C. |
| `implementation/lecture-12/` | CPU scheduling simulator implementing FCFS, SJF (non-preemptive), and Round Robin algorithms (`cpu_sched.c`). |

## Highlight: Dragon Shell Assignment

Dragon Shell is a custom UNIX shell written in C that supports built-in commands, external program execution, job control, signal handling, and standard shell conveniences such as I/O redirection, pipes, and quoting. The assignment includes detailed testing notes and references for further reading. Use the provided `Makefile` in `assignment/a1` to build the shell locally:

```bash
cd assignment/a1
make            # Compile dragonshell
valgrind --tool=memcheck --leak-check=yes ./dragonshell
make clean      # Remove build artifacts
```

## Lecture Implementations

The `implementation` directory collects code snippets from lectures that complement the assignment material:

- **Pipes and Processes (Lecture 8):** Demonstrates parent/child communication using anonymous pipes to move data between producer and consumer processes.
- **Networking (Lecture 10):** Provides iterative and concurrent TCP servers, a UDP server, and matching clients to experiment with socket programming patterns.
- **Remote Procedure Calls (Lecture 11):** Shows how to define, implement, and invoke RPC services using the ONC RPC toolchain (`rpcgen`).
- **CPU Scheduling (Lecture 12):** Interactive simulator that compares FCFS, SJF (non-preemptive), and Round Robin scheduling algorithms by computing average waiting and turnaround times for user-defined process sets.

## Getting Started

1. Clone the repository and review the directory-specific README or comments for prerequisites (e.g., `rpcbind`, `valgrind`).
2. Compile programs using the supplied `Makefile` targets when available or `gcc` directly for standalone sources.
3. Use the lecture examples as reference implementations to experiment with core operating systems concepts such as concurrency, IPC, networking, and RPC.

## License

The repository is distributed under the MIT License. Refer to the `LICENSE` file for the full text.
