/*Problem:  Write a program that copies the contents of one file 
to a destination file using the POSIX. Be sure to include all necessary error 
checking, including ensuring that the source file exists.*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#define BUFFER_SIZE 4096

typedef __ssize_t ssize_t;

int copy_file_posix(const char* source, const char* destination){
    int src_fd, dest_fd;
    ssize_t bytes_read, bytes_written;
    char buffer[BUFFER_SIZE];

    struct stat src_stat;
    int result = 0;

    // Check if source file exists and get its properties

    if (stat(source, &src_stat) == -1){
        fprintf(stderr, "Error: Source file '%s' does not exist or cannot be accessed: %s\n",
                source, strerror(errno));
        
        return -1;
    }

    // Check if source is a regular file

    if (!S_ISREG(src_stat.st_mode)){
        fprintf(stderr, "Error: Source '%s' is not a regular file. \n", source);

        return -1;
    }

    // Open source file for reading

    src_fd = open(source, O_RDONLY);
    if (src_fd == -1){
        fprintf(stderr, "Error: Cannot open source file '%s': %s\n", source, strerror(errno));

        return -1;
    }

    // Create (open) destination file for writing

    dest_fd = open(destination, O_WRONLY | O_CREAT | O_TRUNC, src_stat.st_mode & 0777);

    /*
    For example: Assume st_mode is:    100644 (octal) = regular file + rw-r--r--
    0777 mask:                    777 (octal) = rwxrwxrwx
    Result of AND:               644 (octal) = rw-r--r--

    The destination file gets the same read/write/execute permissions as the source

    */

    if (dest_fd == -1){
        fprintf(stderr, "Error: Cannot create destination file '%s': %s\n", destination, strerror(errno));
        close(src_fd);

        return -1;
    }

    while ((bytes_read = read(src_fd, buffer, BUFFER_SIZE)) > 0){
        char* write_ptr = buffer;
        ssize_t remaining = bytes_read;

        /*The write() system call cannot guarantee that it will write all the requested bytes in a single call, 
        even under normal conditions */

        // Handle partial writes

        while (remaining > 0){
            bytes_written = write(dest_fd, write_ptr, remaining);

            if (bytes_written == -1){
                fprintf(stderr, "Error: Failed to write to destination file '%s': %s\n", destination, strerror(errno));

                result = -1;

                goto cleanup;
            }

            remaining -= bytes_written;
            write_ptr += bytes_written;
        }
    }

    if (bytes_read == -1) {
        fprintf(stderr, "Error: Failed to read from source file '%s': %s\n", source, strerror(errno));

        result = -1;
        goto cleanup;
    }

    printf("File copied successfully from '%s' to '%s'\n", source, destination);

cleanup:
    // Clean up file descriptors

    close(src_fd);
    close(dest_fd);

    if (result == -1) unlink(destination);

    return result;

}

int main(int argc, char* argv[]){
    if (argc != 3){
        fprintf(stderr, "Usage: %s <source_file> <destination_file>", argv[0]);

        return 1;
    }

    const char* source = argv[1];
    const char* destination = argv[2];

    // Check for empty filenames

    if (strlen(source) == 0 || strlen(destination) == 0){
        fprintf(stderr, "Error: Source and destination filenames cannot be empty.\n");

        return 1;
    }

    // Check if source and destination are the same

    if (strcmp(source, destination) == 0){
        fprintf(stderr, "Error: Source and destination files cannot be the same.\n");

        return 1;
    }

    return copy_file_posix(source, destination);
}