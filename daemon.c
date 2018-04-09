#include "header.h"
int my_daemon_init()
{
	
	int fd, maxfd;
	pid_t pid;

	if (( pid = fork()) < 0)
		exit(0);

	// 부모프로세스를 종료한다. 
	else if(pid != 0)
		exit(0);

	pid = getpid();
	setsid();
	signal(SIGTTIN, SIG_IGN);
	signal(SIGTTOU, SIG_IGN);
	signal(SIGTSTP, SIG_IGN);
	maxfd = getdtablesize();
	for (fd = 0; fd < maxfd; fd++)
		close(fd);
	umask(0);
	chdir("/");
	fd = open("/dev/null", O_RDWR);
	dup(0);
	dup(0);
	return 0;
}
