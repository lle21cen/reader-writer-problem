#include "header.h"

int my_daemon_init(void);
void process_in_usr1();

char filename[MIN]; // 요청으로 들어온 파일이름을 파일 관리 큐에서 꺼내 저장하는 변수이다.
char currentDirectory[MIN]; // 디몬 프로세스에 의해 작업디렉터리가 변하므로 현재디렉터리를 저장한다.
char sharedfile[MIN]; // 인자로 들어온 <FILENAME>을 저장한다.
int *sig_que; // 시그널을 보낸 pid를 저장할 큐이다. 메인함수에서 동적할당 한다.
char fname_que[MIN][MIN]; // 요청으로 들어온 파일이름도 큐로 관리한다.
int top=0, f_top=0; // 각각 시그널 관리 큐와 파일이름 관리 큐의 맨 처음에 들어온 인자의 위치를 나타낸다.
int sigusr1 = 0; // ssu_vim으로부터 SIGUSR1이 들어오면 1이 된다.
int fifo_fd, log_fd; // 각각 FIFO파일과 로그파일을 관리하기 위한 파일디스크립터이다.

// 각 옵션을 나타내는 번호들이다. 첫 글자 o는 option을 나타내고 다음 글자가 옵션 이름이다.
int ol=0, ot=0, on=0, op=0, oid=0; // 해당하는 옵션이 들어오면 1로 셋팅된다.

char* timeToString() {
	// 현재 시간을 정해진 포멧에 맞추어 리턴해주는 함수이다.
	time_t rawtime;
	struct tm *t;
	time(&rawtime);
	t = localtime(&rawtime); // 초 단위 형태의 시간을 지역시간 형태로 바꾼다. 

	static char s[20];
	// tm구조체에 저장된 각 정보를 가지고 형식에 맞는 시간을 만들어 s에 저장한다.
	sprintf(s, "%04d-%02d-%02d %02d:%02d:%02d",
			t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
			t->tm_hour, t->tm_min, t->tm_sec);

	return s; // s를 리턴해준다. 
}

void push(int pid)
{
	/* 
	   시그널을 보낸 pid를 push하는 함수이다. 처음에 들어있던 모든 원소들을 뒤로 밀고,
	   새로 들어온 pid를 큐의 인덱스 0번째에 저장한다.
	 */
	for (int i = top; i > 0; i--) {
		sig_que[i] = sig_que[i-1];
	}
	top++;
	sig_que[0] = pid;
}

pid_t pop()
{
	/* 
	   큐에서 가장 위에 있는 pid를 pop하는 함수이다. push() 함수에서 pid를 넣을 때 마다 top값을 증가시켰으므로 top-1 값이 가장 먼저들어 온 원소의 인덱스이다. 그 인덱스를 c_pid에 저장하고 top값을 줄인 후 c_pid를 리턴한다.
	 */
	pid_t c_pid = sig_que[top-1];
	if (top > 0)
		sig_que[--top] = 0;
	return c_pid;
}

void fname_push(char* fname)
{
	/* 
	   파일 이름을 관리하는 큐이다. pid가 큐에 의해 관리되므로 해당 pid가 요청한 파일이름을 관리하기 위해 똑같이 큐를 이용하였다.
	 */
	for (int i = f_top; i > 0; i--) {
		/* 
		   f_top은 파일관리 큐에서 맨 마지막에 있는 원소를 가리키는 역할을 한다. 그러므로 f_top의 맨 뒤에서	부터 원소들을 하나씩 뒤로 미루고 맨 앞의 인덱스(0)에 인자로 들어온 fname을 입력한다.
		 */
		strcpy(fname_que[i], fname_que[i-1]);
		fname_que[i][strlen(fname_que[i-1])]=0;
	}
	f_top++; // f_top을 1 늘려준다.
	strcpy(fname_que[0], fname);
}

void fname_pop(void)
{
	// 파일이름을 저장한 큐에서 맨 처음에 들어온 파일이름을 pop하는 함수이다.
	memset(filename, 0, strlen(filename));
	strcpy(filename, fname_que[f_top-1]);
	if (f_top > 0)
		memset(fname_que[--f_top], 0, strlen(fname_que[f_top]));
}

static void sighandler1(int signo, siginfo_t *info, void *uarg)
{
	// SIGUSR1시그널에 대한 시그널 핸들러
	/* 
	   파일을 요청한 프로세스에 대한 시그널을 저장
	   FIFO파일을 큐 형태로 관리함
	 */

	pid_t m_pid = info->si_pid; // 시그널을 보낸 pid를 저장한다.

	char fname[MIN];
	char buf[MIN];

	// FIFO파일을 연다.
	if ((fifo_fd = open(FIFO, O_RDWR)) < 0)
		exit(1);

	// FIFO파일에 저장된 파일이름을 읽어 fname에 저장한다.
	int len;
	if ((len = read(fifo_fd, fname, MIN)) < 0)  
		exit(1);
	fname[len] = '\0';

	/* 
	   옵션 t가 설정되어있는지 여부에 따라 로그파일에 출력 메시지가 달라진다. 설정되어있다면, 
	   timeToString() 함수를 통해 현재 시간도 같이 출력한다.
	 */
	if (!ot)
		sprintf(buf, "Requested Process ID : %d, Requested Filename : %s\n", m_pid, fname);
	if (ot)
		sprintf(buf, "[%s] Requested Process ID : %d, Requested Filename : %s\n", timeToString(), m_pid, fname);

	write(log_fd, buf, strlen(buf)); // 위에서 저장한 출력메시지를 로그 파일에 출력한다.

	if (oid) {
		// 만약 id옵션이 설정되어 있다면, 출력 메시지를 만들어 저장하고 로그파일에 출력한다.
		uid_t uid = info->si_uid; // 시그널을 보낸 프로세스의 uid를 얻는다.
		struct passwd *pw; // uid를 이용해 사용자 이름과 gid를 알아내기 위한 구조체이다.
		pw = getpwuid(uid); 
		char *user_name = pw -> pw_name; // uid를 이용해서 사용자 이름을 알아낸다.
		gid_t gid = pw->pw_gid; // uid를 이용해서 그룹 아이디도 알아낸다.

		char opt_id_buf[MIN];
		sprintf(opt_id_buf, "User : %s, UID : %d, GID : %d\n", user_name, uid, gid);
		// 알아낸 정보를 바탕으로 출력메시지를 만들고 buf에 저장한 후 로그 파일에 출력한다.
		write(log_fd, opt_id_buf, strlen(opt_id_buf));
	}

	push(m_pid); // 시그널을 보낸 pid를 큐에 넣는다.
	fname_push(fname); // 요청된 파일이름을 파일 관리 큐에 넣는다.
	process_in_usr1(); // 각각의 큐에서 pop을 해서 정보를 얻고 파일이름을 비교하는 함수이다.
}

void process_in_usr1() {
	pid_t c_pid; 
	if (!sigusr1)  { 
		/*
		   sigusr1이 0이면, 전역변수로 초기값은 0이기 때문에 처음 들어온 프로세스는 바로 실행이 되고, 요청 한 프로세스에 SIGUSR1 시그널을 보내면 sigusr1 값은 1이 되며, 그 프로세스가 SIGUSR2 시그널을 보내야 sigusr1값이 다시 0값이 되어 실행이 가능해지게 된다.
		 */
		// pid 관리 큐에서 pop한 값을 c_pid (current pid)에 저장하는데 만약 0이면 프로세스가 없으므로 종료한다.
		if ((c_pid = pop()) == 0) 
			return;
		fname_pop(); // 마찬가지로 파일이름도 pop한다. 전역변수 filename에 저장된다.

		// 읽은 파일 이름이 sharedfile과 같으면 SIGUSR1 시그널을 ssu_vim에 전송한다.
		if (!strcmp(filename, sharedfile)) {
			if (kill(c_pid, SIGUSR1) < 0) {
				/* 
				   시그널을 전송하는데 실패하면 sigusr1값이 0이 되어 다음 시그널이 들어왔을 때 바로 실행하게 하지만, 시그널이 성공적으로 전달되면 1이되어 다음 프로세스는 대기상태가 된다.
				 */
				sigusr1 = 0; 
			}
			else
				sigusr1 = 1;
		}
		else {
			// 만약 sharedfile과 요청한 파일의 이름이 같지 않으면, SIGUSR2시그널을 전송한다.
			kill(c_pid, SIGUSR2);
		}
		filename[0]=0; // filename이 다른 프로세스에서 재사용되는 것을 막는다.
	}
	return;
}

static void sighandler2 (int signo, siginfo_t *info, void *uarg)
{
	/* 
	   USR2시그널에 대한 시그널 핸들러. sigusr1 값을 0으로 바꾸고 
	   process_in_usr1()함수를 실행시킨다.
	 */
	char buf[50];
	sigusr1 = 0;
	process_in_usr1();
	/* 
	   buf는 로그파일에 출력할 문자열을 담는 변수이다. 
	   옵션 t가 설정되어 있지 않은경우 시간변수를 출력하지 않고, 설정되어 있는 경우는 시간도 출력한다.
	 */
	if (!ot)
		sprintf(buf, "Finished Process ID : %d\n", info->si_pid);	
	else
		sprintf(buf, "[%s] Finished Process ID : %d\n", timeToString(), info->si_pid);	
	write(log_fd, buf, strlen(buf)); // buf에 저장된 문자열을 로그 파일에 출력

	if (ol) {
		// 옵션 l이 설정되어있는 경우에 실행된다.
		int time_fd, len;
		time_t time_val;
		char buf[MIN];

		/* 
		   fname_time은 해당 디렉터리경로에 시간 문자열을 붙여 저장하는 변수이다. 
		   이 문자열이 파일 이름이 된다.
		 */
		char fname_time[MIN];
		sprintf(fname_time, "%s/[%s]", currentDirectory, timeToString());

		/* 
		   위에서 만든 파일이름으로 새로운 파일을 연다.
		   시간정보가 똑같은 파일이름은 하나밖에 없으므로 파일을 생성해야한다.
		 */
		if ((time_fd = open(fname_time, O_RDWR | O_CREAT, 0666)) < 0)
			perror(fname_time);

		lseek(log_fd, 0, SEEK_SET);

		while ((len = read(log_fd, buf, MIN)) > 0) {
			write(time_fd, buf, len); // 로그 파일의 내용을 새로만든 파일에 복사한다.
		}

		close(time_fd);
	}
}

int main(int argc, char *argv[])
{
	pid_t pid; 
	char logpath[MIN], buf[MIN], que_init_str[50];
	char *newPath;
	int que_size = 16;

	if (argc < 2) {
		fprintf(stderr, "usage: ssu_ofm <FILENAME> [OPTION]\n");
		exit(1);
	}

	for (int i = 2; i < argc; i++) {
		// 옵션의 유효성을 검사하는 루프이다.
		if (!strcmp(argv[i], "-l")) {
			ol = 1;
		}
		else if (!strcmp(argv[i], "-t")) {
			ot = 1;
		}
		else if (!strcmp(argv[i], "-n")) {
			/* 
			   옵션 n이 설정되어있으면, 옵션 다음에 아무 문자도 없으면 오류메시지 출력 후 종료.
			   다른문자가 있으면 그 문자를 정수형으로 변환하여 que_size에 저장한다.
			 */
			if (argc <= i + 1) {
				fprintf(stderr, "usage: -n <NUMBER>\n");
				exit(1);
			}
			on = 1;
			que_size = atoi(argv[++i]);
			if (que_size <= 0) {
				fprintf(stderr, "Queue size error : %d\n", que_size);
				exit(1);
			}
			sig_que = malloc(que_size); // 큐의 사이즈를 동적할당한다.
			sprintf(que_init_str, "Initialized Queue Size : %d\n", que_size);
		}
		else if (!strcmp(argv[i], "-p")) {
			// 옵션 p가 설정되었을 경우, 옵션 다음에 경로명이 있는지 검사하고 없으면 오류 출력 후 종료.
			if (argc <= i + 1) {
				fprintf(stderr, "usage: -p <DIRECTORY>\n");
				exit(1);
			}
			newPath = argv[++i]; // 디렉터리를 newPath에 저장.
			mkdir(newPath, 0777); // 그 디렉터리를 만든다.
			op = 1;
		}
		else if (!strcmp(argv[i], "-id")) {
			oid = 1;
		}

		else {
			// 정해진 옵션 외에 다른 옵션이 있을 경우 메시지 출력 후 종료한다.
			fprintf(stderr, "Invalid option %s\n", argv[i]);
			exit(1);
		}
	}

	if (on == 0) {
		// 옵션 n이 없을 경우 que_size인 16이 큐의 사이즈로 할당된다.
		sig_que = malloc(que_size);
		sprintf(que_init_str, "Initialized with Default Value : %d\n", que_size);
	}

	getcwd(currentDirectory, MIN); // 디몬을 실행하기 전에 현재 디렉터리의 경로를 저장한다.
	if (access(argv[1], F_OK) != 0) { // 공유파일이 존재하지 않는 경우 오류 출력 후 종료한다.
		fprintf(stderr, "%s: No such file or directory\n", argv[1]);
		exit(0);
	}
	strcpy(sharedfile, argv[1]); // 공유파일을 sharedfile에 저장한다.

	// 옵션 p가 있을 경우 작업디렉터리를 -p 옵션의 인자로 들어온 디렉터리로 바꾸어준다.
	if (op) sprintf(currentDirectory, "%s/%s", currentDirectory, newPath);
	printf("Daemon Process Initialization.\n");
	if (my_daemon_init() < 0) { // 디몬 프로세스를 실행시킨다.
		fprintf(stderr, "ssu_daemon_init faild\n");
		exit(1);
	}

	// 현재 디렉터리(currentDirectory)에 로그 파일을 연다.
	sprintf(logpath, "%s/%s", currentDirectory, LOGFILE);	
	if ((log_fd = open(logpath, O_RDWR | O_CREAT | O_TRUNC, 0666)) < 0) {
		perror(logpath);
		exit(1);
	}

	pid = getpid();

	// 옵션 t의 설정 여부에 따라 출력할 문자열의 맨 앞에 시간 정보 출력 여부를 결정한다.
	if (ot)
		sprintf(buf, "[%s] <<Deamon Process Initialized with pid : %d>>\n", timeToString(), pid);

	else
		sprintf(buf, "<<Deamon Process Initialized with pid : %d>>\n", pid);

	// 로그파일에 디몬 초기화 메시지와 큐 초기화 메시지를 각각 출력한다.
	write(log_fd, buf, strlen(buf));
	write(log_fd, que_init_str, strlen(que_init_str));

	// 시그널을 보낸 프로스의 pid를 얻기위해 sigaction을 설정한다.
	struct sigaction act, usr2_act; // act는 SIGUSR1, usr2_act는 SIGUSR2.
	act.sa_sigaction = sighandler1; // 시그널 핸들러를 설정한다.
	act.sa_flags = SA_SIGINFO; // 시그널 정보를 얻기 위해 플래그를 설정한다.
	sigemptyset(&act.sa_mask); // 시그널 마스크의 값을 비운다.

	// 같은 일을 SIGUSR2를 위한 sigaction 핸들러를 설정한다.
	usr2_act.sa_sigaction = sighandler2;
	usr2_act.sa_flags = SA_SIGINFO;
	sigemptyset(&usr2_act.sa_mask);

	// 시그널 처리
	sigaction(SIGUSR1, &act, NULL); // act는 SIGUSR1 시그널을 처리.
	sigaction(SIGUSR2, &usr2_act, NULL); // usr2_act는 SIGUSR2 시그널을 처리.
	while(1);

	exit(0);
}
