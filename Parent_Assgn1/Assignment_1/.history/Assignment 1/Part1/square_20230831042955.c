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
	long long num = strtoul(argv[1], &end, 10);

	if(num < 0){
		printf("Unable to execute\n");
		exit(1);
	}

	unsigned long square_num = num * num;

	printf("%lu\n", square_num);

	return 0;
}
