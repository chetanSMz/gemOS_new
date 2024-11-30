#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

int main(int argc, char* argv[])
{
	char *end;
	long long num = strtoul(argv[argc-1], &end, 10);


	// printf("%lu\n", num);

	// for(int i = 0; i < argc; i++){
	// 	printf("%s\n", argv[i]);
	// }

	// char *args[]

	if(num < 0){
		printf("Unable to execute\n");
		exit(-1);
	}

	unsigned long sqrt_num = (unsigned long)round(sqrt(num));


	if(argc == 2){
		printf("%lu\n", sqrt_num);
	}

	else{
		
		char **args = (char **)malloc((argc-1) * sizeof(char *)); // Allocate memory for args

		if (args == NULL) {
			printf("Unable to execute\n");
			exit(-1);
		}

		for (int i = 0; i < argc-2; i++) {
			args[i] = (char *)malloc(strlen(argv[i+1]) + 1); // +1 for the null terminator
			if (args[i] == NULL) {
				printf("Unable to execute\n");
				// Free previously allocated memory before returning
				for (int j = 0; j < i; j++) {
					free(args[j]);
				}
				free(args);
				exit(-1);
			}
			strcpy(args[i], argv[i]); // Copy each string using strcpy
		}

		char str[20];
		sprintf(str, "%lu", sqrt_num);
		args[argc-2] = (char *)malloc(strlen(str) + 1)
		
	}

	return 0;
}

