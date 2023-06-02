#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>


int main() {
 pid_t	pid = fork();
	int fd[2];
	if (pid == 0) {
		char* cmd[] = {"ls"};
		close(fd[0]);
		dup2(fd[1], STDOUT_FILENO);
		execvp(cmd[0], cmd);
		perror("실행 안 됨");
	}
	else {
		char buff[4096];
		int n;

		close(fd[1]);
		FILE *fp = fopen("output.txt", "w");
		if (fp == NULL) {
			perror("파일 생성 안됨");
			exit(1);
		}
		wait(NULL);
		while (n = read(fd[0], buff, sizeof(buff) )) > 0) {
			fwrite(buff, 1, n, fp);
		}
		fclose(fp);
		printf("%s", buff);
	}
}
