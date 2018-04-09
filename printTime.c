#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>

char* timeToString(struct tm *t) {
	static char s[20];

	sprintf(s, "%04d-%02d-%02d %02d:%02d:%02d",
			t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
			t->tm_hour, t->tm_min, t->tm_sec);

	return s;
}

void printCurrentTime()
{
	time_t rawtime;
	struct tm *timeinfo;

	time(&rawtime);
	timeinfo = localtime(&rawtime);

	printf("[%s]\n", timeToString(timeinfo));
}

void printLastModTime(char *filename)
{
	struct stat st;

	if (stat(filename, &st) < 0) {
		fprintf(stderr, "stat error for %s\n", filename);
		exit(1);
	}

	printf("Last Modification time of '%s' : [%s]\n", filename, timeToString(localtime(&st.st_mtime)));
}

