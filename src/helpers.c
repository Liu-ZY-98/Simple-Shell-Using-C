// Your helper functions need to be here.
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include <sys/wait.h>

#include "shell_util.h"
#include "helpers.h"

int timeComparator(void *bpro1, void *bpro2)
{
	int diff = ((ProcessEntry_t *)bpro1)->seconds - ((ProcessEntry_t *)bpro2)->seconds;
	if (diff > 0)
		return 1;
	else
		return -1;
}
