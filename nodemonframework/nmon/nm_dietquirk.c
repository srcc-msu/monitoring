#include <sys/types.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>


#define NULL_FILE	"/dev/null"


int daemon(int nochdir, int noclose){
	int fd;
	pid_t pid;
	
	signal(SIGHUP, SIG_IGN);
	
	pid = fork();
	
	switch (pid){
	case -1:
		return -1;
	case 0:
		break;
	default:
		_exit(0);
	}
	
	setsid();
	
	signal(SIGHUP, SIG_DFL);
	
	if (!nochdir)
		chdir("/");
		
	if (!noclose){
		if ((fd = open(NULL_FILE, O_RDWR)) != -1){
			dup2(fd, STDIN_FILENO);
			dup2(fd, STDOUT_FILENO);
			dup2(fd, STDERR_FILENO);
			
			if (fd > STDERR_FILENO)
				close(fd);
		}
	}
	
	return 0;
}


