#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <syslog.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <wait.h>
#include <time.h>
#include <ctype.h>
#include <pwd.h>
#include <dirent.h>

#define MAX 1024
#define MIN 256
#define FIFO "/tmp/fifo"
#define LOGFILE "ssu_log.txt"
