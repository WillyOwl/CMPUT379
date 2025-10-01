Name: Willy Zuo
SID: 1861800
CCID: zuo5

# Dragon Shell

A custom Unix shell implementation in C that provides essential shell functionalities including built-in commands, external program execution, I/O redirection, pipes, background processes, and signal handling.

## Features

- Built-in commands: `pwd`, `cd`, `jobs`, `exit`
- External program execution with path resolution
- I/O redirection (`<` for input, `>` for output)
- Pipe support (`|`) (only single pipe is supported so far)
- Background process execution (`&`)
- Signal handling (SIGINT, SIGTSTP, SIGCHLD)
- Job control and process management
- Quote processing for arguments
- Process cleanup on exit

## Design Choices

### 1. Process Management
- **Job List Structure**: Implemented as a linked list to track running processes with their PID, state ('R' for running, 'T' for terminated), and command string
- **Foreground/Background Tracking**: Used global variables `foreground_pid` and `background_pid` to track current processes for signal handling
- **State Management**: Jobs are added when processes start, updated when stopped/continued, and removed when terminated

### 2. Signal Handling
- **SIGINT Handler**: Forwards interrupt signal to foreground process only, preserving shell operation
- **SIGTSTP Handler**: Stops foreground process and updates job state to 'T' (terminated)
- **SIGCHLD Handler**: Asynchronously reaps child processes using `WNOHANG` to prevent blocking and updates job states

### 3. I/O Redirection
- **File Descriptor Management**: Uses `dup2()` to redirect stdin/stdout before `execve()`
- **Error Handling**: Validates file operations and provides appropriate error messages
- **Permission Setting**: Creates output files with 0644 permissions for security

### 4. Pipe Implementation
- **Two-Process Model**: Creates two child processes connected by a pipe
- **Synchronous Execution**: Parent waits for both processes to complete before continuing
- **File Descriptor Cleanup**: Properly closes pipe ends in parent and children

### 5. Memory Management
- **Dynamic Allocation**: Job structures are malloc'd and properly freed
- **Resource Cleanup**: Comprehensive cleanup function terminates all processes before exit

## System Calls Used

### Process Management
- **`fork()`**: Create child processes for external programs and pipe commands
- **`execve()`**: Execute external programs with environment inheritance
- **`waitpid()`**: Wait for specific child processes with options (WNOHANG, WUNTRACED, WCONTINUED)
- **`kill()`**: Send signals to processes (SIGTERM, SIGKILL, SIGINT, SIGTSTP)

### File Operations
- **`open()`**: Open files for I/O redirection with appropriate flags (O_RDONLY, O_WRONLY | O_CREAT | O_TRUNC)
- **`close()`**: Close file descriptors after use
- **`dup2()`**: Duplicate file descriptors for redirection

### Directory Operations
- **`getcwd()`**: Get current working directory for `pwd` command
- **`chdir()`**: Change working directory for `cd` command

### Signal Management
- **`sigaction()`**: Set up signal handlers with proper flags (SA_RESTART, SA_NOCLDSTOP)

### Utility Functions
- **`sleep()`**: Delay during process cleanup to allow graceful termination

## Testing

### 1. Built-in Commands Testing
```bash
# Test pwd command
dragonshell> pwd
/home/willy/dragonshell-f25-WillyOwl

# Test cd command
dragonshell> cd /tmp
dragonshell> pwd
/tmp
dragonshell> cd Assignment-12 (Assume that there is no Assignment-12 folder in your system)
dragonshell: No such file or directory

# Test jobs command (after running background processes)
dragonshell> /usr/bin/sleep 10 &
PID 1234 is sent to background
dragonshell> jobs
1234 R /usr/bin/sleep 10
```
**Valgrind Output:**
```
==111640== 
==111640== HEAP SUMMARY:
==111640==     in use at exit: 0 bytes in 0 blocks
==111640==   total heap usage: 7 allocs, 7 frees, 3,686 bytes allocated
==111640== 
==111640== All heap blocks were freed -- no leaks are possible
==111640== 
==111640== For lists of detected and suppressed errors, rerun with: -s
==111640== ERROR SUMMARY: 0 errors from 0 contexts (suppressed: 0 from 0)
```


### 2. External Program Execution Testing
```bash
# Test basic program execution
dragonshell> /usr/bin/ls
dragonshell.c  Makefile  README.md

# Test with arguments
dragonshell> /usr/bin/ls -la
total 32
drwxr-xr-x 3 willy willy 4096 Sep 30 10:00 .
...

# Test invalid command
dragonshell> invalidcommand (for example ls)
dragonshell: Command not found
```

**Valgrind Output:**
```
==107908== 
==107908== HEAP SUMMARY:
==107908==     in use at exit: 0 bytes in 0 blocks
==107908==   total heap usage: 5 allocs, 5 frees, 2,408 bytes allocated
==107908== 
==107908== All heap blocks were freed -- no leaks are possible
==107908== 
==107908== For lists of detected and suppressed errors, rerun with: -s
==107908== ERROR SUMMARY: 0 errors from 0 contexts (suppressed: 0 from 0)
```

### 3. I/O Redirection Testing
```bash
# Test output redirection
dragonshell> /usr/bin/echo "Hello World" > output.txt
dragonshell> /usr/bin/cat output.txt
Hello World

# Test input redirection
dragonshell> /usr/bin/wc -l < dragonshell.c
566

# Test combined redirection
dragonshell> /usr/bin/sort < input.txt > sorted.txt
```

**Valgrind Output:**
```
==113817== 
==113817== HEAP SUMMARY:
==113817==     in use at exit: 0 bytes in 0 blocks
==113817==   total heap usage: 7 allocs, 7 frees, 2,648 bytes allocated
==113817== 
==113817== All heap blocks were freed -- no leaks are possible
==113817== 
==113817== For lists of detected and suppressed errors, rerun with: -s
==113817== ERROR SUMMARY: 0 errors from 0 contexts (suppressed: 0 from 0)
```

### 4. Pipe Testing
```bash
# Test simple pipe
dragonshell> /usr/bin/ls | /usr/bin/wc -l
3

# Test pipe with arguments
dragonshell> /usr/bin/cat dragonshell.c | /usr/bin/grep "fork"
        pid_t pid = fork();
                perror("fork failed!");
        pid_t pid1 = fork();
                perror("fork error");
                // Parent process - fork again for second command
                pid_t pid2 = fork();
                        perror("fork error");
```

**Valgrind Output:**
```
==115578== 
==115578== HEAP SUMMARY:
==115578==     in use at exit: 0 bytes in 0 blocks
==115578==   total heap usage: 2 allocs, 2 frees, 2,048 bytes allocated
==115578== 
==115578== All heap blocks were freed -- no leaks are possible
==115578== 
==115578== For lists of detected and suppressed errors, rerun with: -s
==115578== ERROR SUMMARY: 0 errors from 0 contexts (suppressed: 0 from 0)
```

### 5. Background Process Testing
```bash
# Test background execution
dragonshell> /usr/bin/sleep 5 &
PID XXXX is sent to background
dragonshell> jobs
XXXX R /usr/bin/sleep 5
dragonshell>
# (after 5 seconds)
dragonshell> jobs
dragonshell>
```
**Valgrind Output:**
```
==116946== 
==116946== HEAP SUMMARY:
==116946==     in use at exit: 0 bytes in 0 blocks
==116946==   total heap usage: 3 allocs, 3 frees, 2,168 bytes allocated
==116946== 
==116946== All heap blocks were freed -- no leaks are possible
==116946== 
==116946== For lists of detected and suppressed errors, rerun with: -s
==116946== ERROR SUMMARY: 0 errors from 0 contexts (suppressed: 0 from 0)
```

### 6. Signal Handling Testing
```bash
# Test SIGINT (Ctrl+C) on foreground process
dragonshell> /usr/bin/sleep 10
^C
dragonshell>

# Test SIGTSTP (Ctrl+Z) on foreground process
dragonshell> /usr/bin/sleep 10
^Z
dragonshell> jobs
1234 T sleep 10
```
**Valgrind Output:**
```
==118330== 
==118330== HEAP SUMMARY:
==118330==     in use at exit: 0 bytes in 0 blocks
==118330==   total heap usage: 4 allocs, 4 frees, 2,288 bytes allocated
==118330== 
==118330== All heap blocks were freed -- no leaks are possible
==118330== 
==118330== For lists of detected and suppressed errors, rerun with: -s
==118330== ERROR SUMMARY: 0 errors from 0 contexts (suppressed: 0 from 0)
```


### 7. Quote Processing Testing
```bash
# Test quoted arguments
dragonshell> /usr/bin/echo "Hello World" > ../a.out
```
**Valgrind Output:**
```
==120108== 
==120108== HEAP SUMMARY:
==120108==     in use at exit: 0 bytes in 0 blocks
==120108==   total heap usage: 3 allocs, 3 frees, 2,168 bytes allocated
==120108== 
==120108== All heap blocks were freed -- no leaks are possible
==120108== 
==120108== For lists of detected and suppressed errors, rerun with: -s
==120108== ERROR SUMMARY: 0 errors from 0 contexts (suppressed: 0 from 0)
```


### 8. Edge Cases Testing
```bash
# Test empty input
dragonshell> 
dragonshell>

# Test exit command
dragonshell> exit
Terminating all running processes...
Dragon Shell exiting...

# Test EOF (Ctrl+D)
dragonshell> [Ctrl+D]
# Shell exits gracefully
```

**Valgrind Output:**
```
==121148== 
==121148== HEAP SUMMARY:
==121148==     in use at exit: 0 bytes in 0 blocks
==121148==   total heap usage: 2 allocs, 2 frees, 2,048 bytes allocated
==121148== 
==121148== All heap blocks were freed -- no leaks are possible
==121148== 
==121148== For lists of detected and suppressed errors, rerun with: -s
==121148== ERROR SUMMARY: 0 errors from 0 contexts (suppressed: 0 from 0)
```


## Notes

### Testing
- **Placeholder Output**: Some outputs of functionalities tested on my machine may vary from actual testing, and the outputs shown above regarding `pwd`, `pid`, `ls`, and pipe testing. (See Compilation and Usage)
- **Combined Redirections**: As for the example given for the combined redirections above, you may enter some numbers, separated by enter individually before using it to test this functionality.
- **Edge Case Coverage**: Tested boundary conditions like empty inputs, invalid commands, and malformed arguments.
- **Memory Leak Check**: Tested each module with memory leak check at the beginning to ensure the shell is intact in terms of the memory.



## Compilation and Usage

```bash
# Compile the shell
make

# Run the shell with memory leak check
valgrind --tool=memcheck --leak-check=yes ./dragonshell

# Clean compiled files
make clean
```

## References

<!-- Add your references here -->
- [Stack Overflow - Sigaction Incomplete Error](https://stackoverflow.com/questions/6491019/struct-sigaction-incomplete-error)
- [Stack Overflow - C fork and pipe multiple process](https://stackoverflow.com/questions/48066168/c-fork-and-pipe-multiple-process)
- [Linux Manual Pages - execve(2)](https://man7.org/linux/man-pages/man2/execve.2.html)
- [Linux Manual Pages - dup2(2)](https://man7.org/linux/man-pages/man2/dup2.2.html)
- [Linux Manual Page - open(2)](https://man7.org/linux/man-pages/man2/open.2.html)
- [Linux Manual Page - close(2)](https://man7.org/linux/man-pages/man2/close.2.html)
- [Linux Manual Page - chdir(2)](https://man7.org/linux/man-pages/man2/chdir.2.html)
- [Linux Manual Page - sigaction(2)](https://man7.org/linux/man-pages/man2/sigaction.2.html)
- [Lecture Slide - process-pipe]() (See slide on canvas)

[![Review Assignment Due Date](https://classroom.github.com/assets/deadline-readme-button-22041afd0340ce965d47ae6ef1cefeee28c7c493a6346c4f15d667ab976d596c.svg)](https://classroom.github.com/a/cC4vS2Rq)