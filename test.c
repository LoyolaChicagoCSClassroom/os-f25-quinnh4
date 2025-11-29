#include <stdio.h>
#include <string.h>


void main(int argc, char **argv) {
	int len = strlen(argv[1]);
	
	printf("arg is %d bytes long\n", len);

	char *upper_str = malloc(len);

	return 0;
}

