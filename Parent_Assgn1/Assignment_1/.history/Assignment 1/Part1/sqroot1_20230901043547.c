#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <sys/wait.h>

int main(int argc, char* argv[]) {
    char *end;
    long long num = strtoul(argv[argc - 1], &end, 10);

    printf("argc = %d\n", argc);

    if (num < 0) {
        printf("Unable to execute\n");
        exit(-1);
    }

    unsigned long sqrt_num = (unsigned long) round(sqrt(num));

    if (argc == 2) {
        printf("%lu\n", sqrt_num);
    } else {
        pid_t pid;
        pid = fork();

        if (pid < 0) {
            printf("Unable to execute\n");
            exit(-1);
        }

        if (!pid) {
            char str[20];
            sprintf(str, "%lu", sqrt_num);

            // Prepare the new argv array for execvp
            char *new_argv[argc + 1]; // +1 for NULL termination
            new_argv[0] = strdup("./sqroot"); // Assuming the executable is named "sqroot"
            new_argv[argc - 1] = strdup(str);
            new_argv[argc] = NULL;

            // Copy the rest of the arguments
            for (int i = 2; i < argc - 1; i++) {
                new_argv[i - 1] = strdup(argv[i]);
            }

            execvp(new_argv[0], new_argv);

            // If execvp fails, print error and exit
            printf("Unable to execute\n");
            exit(-1);
        } else {
            int status;
            wait(&status); // Wait for the child process to finish

            if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
                printf("Unable to execute\n");
                exit(-1);
            }
        }
    }

    return 0;
}
