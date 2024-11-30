#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <wait.h>


int main(int argc, char* argv[])
{
	char *end;
	long long num = strtoul(argv[argc-1], &end, 10);

	printf("argc = %d\n", argc);

	if(num < 0){
		printf("Unable to execute\n");
		exit(-1);
	}

	unsigned long sqrt_num = (unsigned long)round(sqrt(num));


	if(argc == 2){
		printf("%lu\n", sqrt_num);
	}

	else{
		pid_t pid;
		pid = fork();
		// if(pid < 0){
		// 	printf("Unable to execute\n");
		// 	exit(-1);
		// }

		if(!pid){
			char **args = (char **)malloc((argc-1) * sizeof(char *));

			// if (args == NULL) {
			// 	printf("Unable to execute\n");
			// 	exit(-1);
			// }

			char str[20];
			sprintf(str, "%lu", sqrt_num);

			for (int i = 0; i < argc-1; i++) {
				if(i == argc-2){
					args[i] = (char *)malloc(strlen(str) + 1);
				}

				else{
					args[i] = (char *)malloc(strlen(argv[i+1]) + 1);
				}

				// if (args[i] == NULL) {
				// 	printf("Unable to execute\n");
				// 	for (int j = 0; j < i; j++) {
				// 		free(args[j]);
				// 	}
				// 	free(args);
				// 	exit(-1);
				// }

				if(i == argc-2){
					strcpy(args[i], str);
				}

				else{
					strcpy(args[i], argv[i+1]);
				}
			}

			char *first = (char *)malloc(3 + strlen(args[1]));
			strcpy(first, "./");
			strcat(first, args[1]);
			
			execvp(first, args);

			// if(execvp(first, args)){
			// 	printf("Unable to execute\n");
			// 	exit(-1);
			// }
		}
	}

	return 0;
}

