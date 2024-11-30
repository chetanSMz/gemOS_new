#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <sys/wait.h>

int main(int argc, char* argv[])
{
	char *end;

	if(argc <= 1 && argc > 17){
		printf("Unable to execute\n");
		exit(-1);
	}

	if(argv[argc-1][0] == '-' && argv[argc-1][1] != '0'){
		printf("Unable to execute\n");
		exit(-1);
	}

	unsigned long num = strtoul(argv[argc-1], &end, 10);


	unsigned long square_num = num * num;

	printf("testing- square_num = %lu\n", square_num);


	if(argc == 2){
		printf("%lu\n", square_num);
	}

	else{
		pid_t pid;
		pid = fork();
		if(pid < 0){
			printf("Unable to execute\n");
			exit(-1);
		}

		if(!pid){
			char **args = (char **)malloc((argc) * sizeof(char *));
			args[argc-1] = NULL;

			if (args == NULL) {
				printf("Unable to execute\n");
				exit(-1);
			}

			char *str = (char *)malloc(20 + 1);
			sprintf(str, "%lu", square_num);

			for (int i = 0; i < argc-1; i++) {
				if(i == argc-2){
					args[i] = (char *)malloc(strlen(str) + 1);
				}

				else if(i == 0){
					args[i] = (char *)malloc(2 + strlen(argv[1]) + 1);	// 2 is for ./ and 1 is for NULL
				}
				else{
					args[i] = (char *)malloc(strlen(argv[i+1]) + 1);
				}

				if (args[i] == NULL) {
					printf("Unable to execute\n");
					for (int j = 0; j < i; j++) {
						free(args[j]);
					}
					free(args);
					exit(-1);
				}

				if(i == argc-2){
					strcpy(args[i], str);
					free(str);
				}

				else if(i == 0){
					strcpy(args[i], "./");
					strcat(args[i], argv[i+1]);
				}

				else{
					strcpy(args[i], argv[i+1]);
				}
			}

			char *first_parameter = (char *)malloc(2 + strlen(args[0]) + 1);

			if(first_parameter == NULL){
				printf("Unable to execute\n");

				for(int i = 0; i < argc; i++){
					free(args[i]);
				}
				free(args);
				exit(-1);
			}

			strcpy(first_parameter, "./");
			strcat(first_parameter, args[0]);

			if(execv(first_parameter, args)){
				printf("Unable to execute\n");
				exit(-1);
			}
		}

		else{
			int status;
            wait(&status);
            
            if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
                printf("Unable to execute\n");
                exit(-1);
            }

		}
	}

}

