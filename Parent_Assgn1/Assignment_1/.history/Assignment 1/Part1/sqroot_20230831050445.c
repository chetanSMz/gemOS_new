#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

int main(int argc, char* argv[])
{
	// if(argc != 2){
	// 	printf("Unable to execute\n");
	// 	exit(1);
	// }

	char *end;
	long long num = strtoul(argv[argc-1], &end, 10);


	// printf("%lu\n", num);

	// for(int i = 0; i < argc; i++){
	// 	printf("%s\n", argv[i]);
	// }

	// char *args[]

	if(num < 0){
		printf("Unable to execute\n");
		exit(1);
	}

	unsigned long sqrt_num = (unsigned long)round(sqrt(num));


	if(argc == 2){
		printf("%lu\n", sqrt_num);

	}

	return 0;
}

