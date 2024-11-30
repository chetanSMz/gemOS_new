#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/wait.h>

#define MAX_PATH_LENGTH 4096

void calculate_directory_size(const char *path, int pipe_fd) {
    struct dirent *entry;
    struct stat info;
    DIR *dir = opendir(path);
    if (dir == NULL) {
        perror("opendir");
        exit(EXIT_FAILURE);
    }

    unsigned long total_size = 0;

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char child_path[MAX_PATH_LENGTH];
        snprintf(child_path, sizeof(child_path), "%s/%s", path, entry->d_name);

        if (lstat(child_path, &info) == -1) {
            perror("lstat");
            exit(EXIT_FAILURE);
        }

        if (S_ISLNK(info.st_mode)) {
            char target_path[MAX_PATH_LENGTH];
            ssize_t len = readlink(child_path, target_path, sizeof(target_path) - 1);
            if (len == -1) {
                perror("readlink");
                exit(EXIT_FAILURE);
            }
            target_path[len] = '\0';

            struct stat target_info;
            if (lstat(target_path, &target_info) == -1) {
                perror("lstat (target)");
                exit(EXIT_FAILURE);
            }

            total_size += target_info.st_size;
        } else if (S_ISDIR(info.st_mode)) {
            int pipe_fds[2];
            if (pipe(pipe_fds) == -1) {
                perror("pipe");
                exit(EXIT_FAILURE);
            }

            pid_t child_pid = fork();
            if (child_pid == -1) {
                perror("fork");
                exit(EXIT_FAILURE);
            }

            if (child_pid == 0) { // Child process
                close(pipe_fds[0]); // Close read end of the pipe
                calculate_directory_size(child_path, pipe_fds[1]);
                close(pipe_fds[1]); // Close write end of the pipe
                exit(EXIT_SUCCESS);
            } else { // Parent process
                close(pipe_fds[1]); // Close write end of the pipe
                unsigned long child_size;
                if (read(pipe_fds[0], &child_size, sizeof(child_size)) == -1) {
                    perror("read from pipe");
                    exit(EXIT_FAILURE);
                }
                close(pipe_fds[0]); // Close read end of the pipe
                total_size += child_size;
                wait(NULL); // Wait for the child process to finish
            }
        } else if (S_ISREG(info.st_mode)) {
            total_size += info.st_size;
        }
    }

    closedir(dir);

    if (write(pipe_fd, &total_size, sizeof(total_size)) == -1) {
        perror("write to pipe");
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <relative path to a directory>\n", argv[0]);
        return EXIT_FAILURE;
    }

    struct stat root_info;
    if (lstat(argv[1], &root_info) == -1) {
        perror("lstat (root)");
        return EXIT_FAILURE;
    }

    if (!S_ISDIR(root_info.st_mode)) {
        fprintf(stderr, "Provided path is not a directory.\n");
        return EXIT_FAILURE;
    }

    unsigned long root_size = root_info.st_size;
    int pipe_fds[2];

    if (pipe(pipe_fds) == -1) {
        perror("pipe");
        return EXIT_FAILURE;
    }

    pid_t child_pid = fork();
    if (child_pid == -1) {
        perror("fork");
        return EXIT_FAILURE;
    }

    if (child_pid == 0) { // Child process
        close(pipe_fds[0]); // Close read end of the pipe
        calculate_directory_size(argv[1], pipe_fds[1]);
        close(pipe_fds[1]); // Close write end of the pipe
        exit(EXIT_SUCCESS);
    } else { // Parent process
        close(pipe_fds[1]); // Close write end of the pipe
        unsigned long child_size;
        if (read(pipe_fds[0], &child_size, sizeof(child_size)) == -1) {
            perror("read from pipe");
            return EXIT_FAILURE;
        }
        close(pipe_fds[0]); // Close read end of the pipe
        root_size += child_size;
        wait(NULL); // Wait for the child process to finish
    }

    printf("Total space used by directory: %lu bytes\n", root_size);

    return EXIT_SUCCESS;
}

