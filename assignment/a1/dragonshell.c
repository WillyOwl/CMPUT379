#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
	
// Import all libraries that are necessary

#define LINE_LENGTH 100
#define MAX_ARGS 5
#define MAX_LENGTH 20
// Define constants suggested by the requirement file

pid_t background_pid = 0;

/**
 * @brief Tokenize a C string 
 * 
 * @param str - The C string to tokenize 
 * @param delim - The C string containing delimiter character(s) 
 * @param argv - A char* array that will contain the tokenized strings
 * Make sure that you allocate enough space for the array.
 */

void tokenize(char* str, const char* delim, char ** argv, int* pipe_flag) {
	char* token;
	token = strtok(str, delim);
	for(size_t i = 0; token != NULL; ++i){
		if (strcmp(token, "|") == 0){
			*pipe_flag = 1;
			break;
		}

    	argv[i] = token;
  		token = strtok(NULL, delim);
  	}
}

// Built-in Commands Implementation

void pwd_command(){
	char cwd[LINE_LENGTH];

	if (getcwd(cwd, sizeof(cwd)) != NULL) printf("%s\n", cwd);

	else perror("getcwd");
}

void cd_command(char* path){
	if (path == NULL || chdir(path) != 0)
		fprintf(stderr, "dragonshell: Expected argument to \"cd\"\n");
}

// Run external programs

void run_external_program(char* command, char** args, int background){
	int input_redirect = 0, output_redirect = 0;
	char* input_file = NULL, *output_file = NULL;

	// Check for output redirection

	for (int i = 0; args[i] != NULL; ++i)
		if (strcmp(args[i], ">") == 0 && args[i + 1] != NULL){
			output_redirect = 1;
			output_file = args[i + 1];
			args[i] = NULL; 
			/* Since '>' is just an identifier, but not a part of the file. 
			We need to set it to NULL after the system specifies it and finishes redirection*/

			break;
		}

	// Check for input redirection

	for (int i = 0; args[i] != NULL; ++i)
		if (strcmp(args[i], "<") == 0 && args[i + 1] != NULL){
			input_redirect = 1;
			input_file = args[i + 1];
			args[i] = NULL; // Same logic as above one

			break;
		}

	pid_t pid = fork();

	if (pid == 0){
		// Handle redirection in child process

		if (output_redirect){
			int output_fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);

			if (output_fd == -1){
				perror("Error opening output file.");
				exit(1);
			}

			dup2(output_fd, STDOUT_FILENO);
			close(output_fd);
		}

		if (input_redirect){
			int input_fd = open(input_file, O_RDONLY);

			if (input_fd == -1){
				perror("Error opening input file.");
				exit(1);
			}

			dup2(input_fd, STDIN_FILENO);
			close(input_fd);
		}


		execvp(command, args);
		perror("dragonshell: Command not found.\n");
		exit(1);
	}

	else if (pid > 0){
		// Parent process: wait for child to complete

		if (background){
			background_pid = pid;

			printf("PID %d is sent to background\n", background_pid);
		}

		else waitpid(pid, NULL, 0); // Foreground behavior, need to wait
	}

	else {
		perror("fork failed!");
		exit(1);
	}
}

void execute_pipe(char* args[], char* second_args[]){
	int fd[2];

	if (pipe(fd) < 0){
		perror("pipe failed");
		exit(1);
	}

	pid_t pid1 = fork();

	if (pid1 < 0){
		perror("fork error");
		exit(1);
	}

	else if (pid1 == 0){
		// First child process
		// First command (writes to the pipe)

		dup2(fd[1], STDOUT_FILENO);
		close(fd[0]); // CLose read end of the pipe in child
		close(fd[1]); // Close write end after duplicating
		
		execvp(args[0], args);
		perror("execvp failed");
		exit(1);
	}

	else {
		// Parent process - fork again for second command

		pid_t pid2 = fork();

		if (pid2 < 0){
			perror("fork error");
			exit(1);
		}

		else if (pid2 == 0){
			// Second child process
			// Second command (reads from the pipe)

			dup2(fd[0], STDIN_FILENO);
			close(fd[1]); // Close write end of the pipe in child
			close(fd[0]); // Close read end after duplicating
			
			execvp(second_args[0], second_args);
			perror("execvp failed");
			exit(1);
		}

		else {
			// Second parent process

			close(fd[0]);
			close(fd[1]);
			waitpid(pid1, NULL, 0);
			waitpid(pid2, NULL, 0);
		}
	}
}

int main(int argc, char **argv) {

	char line[LINE_LENGTH];
	char* args[MAX_ARGS + 1] = {NULL};
	char* command;
	int pipe_flag = 0; // Flag to indicate pipe

	printf("Welcome to Dragon Shell!\n");

	while (1){
		if (background_pid > 0){
			int status;

			pid_t result = waitpid(background_pid, &status, WNOHANG);

			if (result > 0){
				printf("Background process finished\n");

				background_pid = 0;
			}
		}

		printf("dragonshell> ");
		fflush(stdout); // Make sure prompt appears immediately

		if (fgets(line, LINE_LENGTH, stdin) == NULL) break;
		line[strcspn(line, "\n")] = '\0'; // Remove the newline character

		if (strlen(line) == 0) continue; // Empty line

		for (int i = 0; i < MAX_ARGS; ++i)
			args[i] = NULL;

		tokenize(line, " ", args, &pipe_flag); // To tokenize the input

		if (pipe_flag){
			char* second_args[MAX_ARGS + 1] = {NULL};
			
			tokenize(strchr(line, '|'), " ", second_args, &pipe_flag);

			execute_pipe(args, second_args);
			pipe_flag = 0;
			continue; 
		}

		int background = 0; // For background processes (indicator)
		int arg_count = 0; // To define an additional arg_count to record how many arguments the user typed in the shell prompt

		/*
		The argc parameter in main() tells you how many command-line arguments were passed to your program

		If you run ./dragonshell argc = 1; ./dragonshell -v argc = 2
		*/

		while (args[arg_count] != NULL)
			arg_count++;

		if (arg_count > 0 && strcmp(args[arg_count - 1], "&") == 0){
			background = 1;
			args[arg_count - 1] = NULL;
		}

		if (strcmp(args[0], "exit") == 0) break; // Check for exit command

		/* Why strcmp() instead of ==
		
		'==' compares memory address of strings, not their contents; strcmp() compares the actual characters
		
		in the strings; for example: "exit" == "exit" might be false even if they look the same;*/

		command = args[0];

		// Check for built-in commands

		if (strcmp(command, "pwd") == 0) pwd_command();
		else if (strcmp(command, "cd") == 0) cd_command(args[1]);

		else run_external_program(command, args, background);
	}

	return 0;
}