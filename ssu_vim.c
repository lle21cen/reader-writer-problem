#include "header.h"
void mode_w(char *filename);
void get_pid_by_name(char *name);

int mode_rw; // mode_w()안에서 rw모드인지 구별하기 위한 변수
int ofm_pid, sigusr1;
int ot=0, os=0, od=0;
time_t beforeModTime; // 접근한 파일에서 수정을 했는지 여부를 검사하기 위해 수정 전 시간을 저장한다.
off_t beforeSize; // 접근하기 전의 파일 크기를 저장한다.
char *sharedfile;
char tempFileName[MIN] = "/tmp/tmpfile0j8505"; // -d옵션에서 달라진 곳을 찾기 위해 임시파일을 만들어 그곳에다가 원래 파일을 복사하고 원래 파일을 수정 후 임시파일과 비교한다.
void sighandler1(int signo)
{
	sigusr1 = 1; // SIGUSR1 시그널을 받으면 sigusr1 값을 0에서 1로 바꾸어준다.

	// vim 에디터에 들어가기 직전에 -d -s 옵션을 위한 파일 복사와 수정 전 파일 크기 저장
	if (od) {
		// d 옵션이 설정되어있는 경우, 임시파일을 열고 원래있던 파일의 내용을 복사한다.
		sprintf(tempFileName, "%s%d", tempFileName, getpid()); // 유일한 파일명을 만들기 위해 임시파일명에 pid를 붙인다.
		int tempfd = open(tempFileName, O_RDWR | O_CREAT | O_TRUNC, 0666); // 임시파일을 연다
		int fd = open(sharedfile, O_RDONLY); // 공유파일을 연다.
		char buf[MAX];
		int len;
		if (tempfd < 0){
			fprintf(stderr, "temporary file open error\n");
			exit(1);
		}

		lseek(fd, 0, 0);
		while ((len = read(fd, buf, MAX)) > 0)
			write(tempfd, buf, len); // 나중에 파일을 비교하기 위해 원래 파일을 복사한다.
	}

	if (os) {
		struct stat st;
		stat(sharedfile, &st);
		beforeSize = st.st_size; // s가 설정되어있는 경우 전 사이즈를 저장한다.
	}
}
char *timeToString(struct tm *t);
void printCurrentTime();
void printLastModTime(char *filename);

void option_d(char *filename)
{
	// -d옵션이 설정되어있을 경우 자식프로세스를 생성하고 그 자식프로세스가 execl()을 이용해 diff 명령어를 실행한다. 위에서 복사해 두었던 임시파일과 수정이 끝나고 난 원래 파일을 비교 인자로 설정한다.
	int pid = fork();
	int status;


	if (pid == 0) {
		// diff command
		printf("##[Compare with Previous File]##\n");
		if (execl("/usr/bin/diff", "diff", filename, tempFileName, (char *)0) < 0)
			perror("execl"); 

		exit(0);
	}

	else
		wait(&status);
}

void sigusr2_handler (int signo)
{
	/* 
	   요청한 파일이름이 ssu_ofm의 shared file 이름과 다른경우 SIGUSR2 시그널을 받게되고, 
	   ssu_ofm에게 프로세스의 실행이 끝난다는 SIGUSR2 시그널을 보내고 종료한다.
	 */
	fprintf(stderr, "공유 파일이 아닙니다.\n");
	kill(ofm_pid, SIGUSR2);
	exit(0);
}

int main(int argc, char *argv[])
{
	int fd, fileDes, repet_check=0;
	int len;
	char buf[MAX];
	struct stat st;

	if (argc < 3) {
		fprintf(stderr, "usage: ssu_vim <FILENAME> <-r|-w|-rw> [OPTION]");
		exit(1);
	}
	stat(argv[1], &st);
	beforeModTime = st.st_mtime; // 파일을 수정하기 전에 마지막 수정시간을 저장한다.

	if ((fileDes = open(argv[1], O_RDWR)) < 0) {
		// 요청 할 파일을 연다. 파일이 존재하지 않거나, open에러시 메시지를 출력하고 종료한다.
		perror(argv[1]);
		exit(1);
	}

	for (int i=1; i < argc; i++) {
		if (!strcmp(argv[i], "-r") || !strcmp(argv[i], "-w") || !strcmp(argv[i], "-rw"))
		{
			if (repet_check)
			{
				// 만약 -r, -w, -rw 모드가 두 번이상 나오면 오류를 출력하고 종료한다.
				fprintf(stderr, "Not allowed repetition.\n");
				exit(0);
			}
			repet_check = 1;
		}
		// Option setting.
		if (!strcmp(argv[i], "-t")) {
			// t옵션이 설정되어 있을 경우, 최종 수정시간, 현재시간 등의 메시지를 출력하고 1초를 sleep()한 후 다시 실행한다.
			ot=1;
			printf("##[Modification Time]##\n");
			printLastModTime(argv[1]);
			printf("Current Time: ");
			printCurrentTime();
			sleep(1);
		}

		else if (!strcmp(argv[i], "-s")) {
			os=1;
		}
		else if (!strcmp(argv[i], "-d")) {
			od=1;
		}

		else if (i > 2) {
			// 유효하지 않은 옵션이 들어 온 경우 에러 메시지를 출력하고 종료한다.
			fprintf(stderr, "Invalid option %s\n", argv[i]);
			exit(1);
		}
	}

	if (repet_check == 0) {
		// -r, -w, -rw 중 하나도 들어오지 않은 경우 메시지를 출력하고 종료한다.
		fprintf(stderr, "You must input -r or -w or -rw\n");
		close(fileDes);
		exit(1);
	}
	sharedfile = argv[1];

	signal(SIGUSR1, sighandler1); // SIGUSR1 시그널에 대한 핸들러를 설정한다.
	signal(SIGUSR2, sigusr2_handler); // SIGUSR2 시그널에 대한 핸들러를 설정한다.

	if (access(FIFO, F_OK) != 0 )
		mkfifo(FIFO, 0666); // FIFO파일이 존재하지 않을 경우 파일을 만든다.

	if ((fd = open(FIFO, O_RDWR)) < 0) {
		// FIFO파일을 연다.
		fprintf(stderr, "open error for %s\n", FIFO);
		close(fileDes);
		exit(1);
	}

	if (!strcmp(argv[2], "-r")) {
		// -r 모드는 파일을 읽어 출력만 하고 바로 종료한다.
		lseek(fileDes, 0, 0);
		while ((len = read(fileDes, buf, MAX)) > 0) {
			write(1, buf, len);
		}
		close(fileDes);
		exit(0);
	}

	if ( write(fd, argv[1], strlen(argv[1])) < 0 ) {
		// FIFO 파일에 요청 할 파일이름을 적는다.
		fprintf(stderr, "write error\n");
		close(fileDes);
		exit(1);
	}

	// Send signal to ssu_ofm
	get_pid_by_name("ssu_ofm"); // "ssu_ofm"이라는 이름의 프로세스 아이디를 찾는다.
	if (ofm_pid == 0){ 
		// ssu_ofm 이라는 프로세스를 찾지 못했을 경우 메시지를 출력하고 종료한다.
		printf("where is ssu_ofm?\nssu_vim error\n");
		exit(1);
	}

	kill(ofm_pid, SIGUSR1); // ssu_ofm에 SIGUSR1 시그널을 보낸다.

	// 만약 -w 모드이면 mode_w() 함수를 실행한다.
	if (!strcmp(argv[2], "-w"))
		mode_w(argv[1]);

	else if (!strcmp(argv[2], "-rw")) {
		// 만약 -rw모드이면 파일 내용을 한번 출력 한 후에  mode_w()를 실행한다.
		lseek(fileDes, 0, 0);
		while ((len = read(fileDes, buf, MAX)) > 0) {
			write(1, buf, len);
		}
		mode_rw = 1;
		mode_w(argv[1]);
	}

	exit(0);
}

void mode_w(char *filename)
{
	int fd;
	int pid;
	int status;
	char user_respond[10]; // rw모드인 경우 사용자가 입력한 yes or no를 받는다.
	if ((fd = open(filename, O_RDWR)) < 0 ) {
		fprintf(stderr, "open error for %s\n", filename);
		exit(1);
	}

	while(1) {
		// ssu_ofm으로부터 SIGUSR1 시그널이 올 때까지 대기
		if (sigusr1)
			break; // SIGUSR1 시그널이 오면 sigusr1값이 1로 바뀌어 루프를 나가게 된다.
		sleep(1);
		if (ot)
		{
			printf("Waiting for Token...%s", filename);
			printCurrentTime(); // t옵션이 설정되어있을 경우 현재 시간도 같이 출력한다.
		}
		else
			printf("Waiting for Token...%s\n", filename);
	}

	if ((pid = fork()) < 0) { 
		// 자식 프로세스 생성
		fprintf(stderr, "fork() error\n");
		exit(1);
	}

	if (pid == 0) {
		if (mode_rw == 1)
			while(1) {
				// rw모드가 설정되어있을 경우 수정여부를 묻는 메시지를 출력
				printf("Would you like to modify '%s'? (yes/no) : ", filename);
				scanf("%s", user_respond);
				// 수정할 경우 무한루프를 나간다.
				if (!strcmp(user_respond, "yes"))
					break;
				else if (!strcmp(user_respond, "no")) {
					// 수정하지 않을 경우 vim이 끝났음을 알리는 시그널 전송 후 종료
					kill(ofm_pid, SIGUSR2); // 시그널 전송
					exit(1);
				}
			}
		// vim 에디터를 execl()을 이용하여 실행시킨다.
		if (execl("/usr/bin/vim", "vim", filename, (char *)0) < 0)
			perror("vim:");
	}
	else {
		wait(&status); // 부모프로세스는 기다림

		// 부모프로세스의 기다림이 끝났다는 것은 vim 에디터가 종료되었다는 것을 의미한다.
		struct stat st;
		stat(filename, &st); // 요청했던 파일의 stat정보를 얻는다.

		if (ot) {
			// t가 설정된 경우 파일이 수정된 경우와 그렇지 않은 경우에 맞는 메시지를 출력한다.
			printf("##[Modification Time]##\n");
			if (beforeModTime != st.st_mtime) 
				printf("There was modification.\n");
			else
				printf("There was no modification.\n");
		}
		if (os && beforeModTime != st.st_mtime) {
			// 옵션 s가 설정되었고 수정이 일어났다면 수정된 파일 크기를 출력한다.
			printf("##[File Size]##\n");
			printf("- - Before modification : %ld(bytes)\n", beforeSize);
			printf("- - After modification : %ld(bytes)\n", st.st_size);
		}
		if (od && beforeModTime != st.st_mtime) {
			/* 옵션 d가 설정되었고 수정이 일어났다면 
			   수정 전후의 차이를 출력하는 option_d 함수를 호출하고 임시파일을 삭제한다.
			 */
			option_d(filename);
			remove(tempFileName); // remove temporary file.
		}

		if (kill(ofm_pid, SIGUSR2) < 0) {
			// ssu_vim의 수행이 모두 완료되었다는 SIGUSR2 시그널을 전송한다.
			// 만약 실행 도중 ssu_ofm이 종료되어 시그널을 전송할 수 없게 되면 메시지를 출력한다.
			fprintf(stderr, "where is ofm?\n");
		}
	}

	exit(0);
}

void get_pid_by_name(char *name)
{
	// kill()을 통해 원하는 프로세스에 시그널을 보내기위해 name에 해당하는 pid를 알아낸다.	
	DIR *dp;
	struct dirent *d;
	FILE *fp;
	char buf[500];

	dp = opendir("/proc");
	while ((d = readdir(dp)) != NULL) {
		// proc 디렉터리 내의 모든 파일들을 읽으면서 [PID] 형태로 된 모든 디렉터리를 탐색한다.
		char path[50];
		if (!strcmp(d->d_name, ".") || !strcmp(d->d_name, ".."))
			continue;
		sprintf(path, "/proc/%s/status", d->d_name); // status 경로를 만든다.
		if ((fp = fopen(path, "r")) == NULL) {
			continue; // 위에서 만든 경로를 open하는데, 존재하지 않으면 다음으로 넘어간다.
		}

		if( fgets(buf, sizeof(buf), fp) == NULL) // open 한 파일의 첫번째 줄을 읽는다.
			perror(buf);
		char *token = strtok(buf, " ,\t,\n");
		token = strtok(NULL, " ,\t,\n"); // 첫 번째 줄의 두 번째 토큰을 읽는다.
		if (!strcmp(token, name)) { // 그 토큰을 인자로 받은 name과 비교한다.
			ofm_pid = atoi(d->d_name); 
			// 일치하면 해당 pid를 정수로 변환하여 ofm_pid에 저장한다.
		}
	}
}
