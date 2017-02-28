#define _BSD_SOURCE
#define _GNU_SOURCE

#include <sys/types.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <netinet/in.h>

#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "plugins_api.h"
#include "plugin_ctl.h"
#include "plugin_err.h"
#include "proto_err.h"
#include "versions.h"


#define DEFPENDING		5
#define CLIENTBUFSIZE		65536


static void report_version(int fd){
	int unused;
	char *buf;
	unused = asprintf(&buf, "%s\r\n", HM_PROTO_VERSION_STR);
	unused = write(fd, buf, strlen(buf));
	free(buf);
}


static void hm_method(char *buf, int fd){
	int readed, method;
	char *currs, *par;
	char *currp = buf;

	enum {
		METH_UNDEF = -1,
		METH_GET = 0,
		METH_GETVERSION
	};

	const char *hm_methods[] = {
		"GET",
		"GETVERSION",
		NULL
	};

	method = METH_UNDEF;
	if (!(currs = malloc(BUFSIZE))){
		hmp_write_error(HMP_SRC_ERR_PROTO, HM_PROTO_ERR_RESOURCE, fd);
		return;
	}

	while ((readed = sscanf(currp, "%s\r\n", currs))){
		if (readed <= 0){
			hmp_write_error(HMP_SRC_ERR_PROTO, HM_PROTO_ERR_PARSE, fd);
			goto xit;
		}
		
		currp += strlen(currs);
		if (strncmp("\r\n", currp, 2) && strcmp(".", currs)){
			hmp_write_error(HMP_SRC_ERR_PROTO, HM_PROTO_ERR_METHOD, fd);
			goto xit;
		}
		
		currp += + 2;
		
		if (method == METH_UNDEF){
			
			int i = 0;
			while (hm_methods[i]){
				if (!strcmp(currs, hm_methods[i])){
					//Method found
					hmp_write_error(HMP_SRC_ERR_PROTO, HM_PROTO_ERR_OK, fd);
					method = i;
					goto method_found;
				}
				
				i++;
			}
			//Unnknown method
			hmp_write_error(HMP_SRC_ERR_PROTO, HM_PROTO_ERR_METHOD, fd);
			goto xit;
		}

method_found:
		if (method == METH_GETVERSION){
			report_version(fd);
			goto xit;
		}
		
		if (method == METH_GET){
			if (strcmp(currs, hm_methods[method])){
				if ((par = strchr(currs, '.'))){
					par[0] = 0;
					par++;
				}
				hm_request_plugin(currs, par, fd);
			}
		}
		
		if (!strcmp(currp, ".\r\n"))
			goto xit;
	}
xit:
	free(currs);
}


static int serv_alive = 1;
static int is_module = 0;


void kill_serv(int sig){
	if (!is_module){
		killpg(getpid(), sig);
	}

	serv_alive = 0;
	//To prevent infinite loop
	signal(SIGTERM, SIG_IGN);
}


static void hm_client(int fd){
	char *buf, *tbuf;
	ssize_t len, unused;
	int currbufsize;

	if (!(tbuf = malloc(BUFSIZE)))
		return;

	bzero(tbuf, BUFSIZE);
	
	if (!(buf = malloc(CLIENTBUFSIZE))){
		hmp_write_error(HMP_SRC_ERR_PROTO, HM_PROTO_ERR_RESOURCE, fd);
		goto xit;
	}
	bzero(buf, CLIENTBUFSIZE);

	currbufsize = 0;
	while((len = read(fd, tbuf, BUFSIZE)) > 0 && serv_alive){
		if ((currbufsize += len) > CLIENTBUFSIZE){
			hmp_write_error(HMP_SRC_ERR_PROTO, HM_PROTO_ERR_SERV, fd);
			break;
		}
		strcat(buf, tbuf);
		
		if (strstr(buf, "\r\n.\r\n")){
			hm_method(buf, fd);
			unused = write(fd, ".\r\n", 3);
			currbufsize = 0;
			bzero(buf, CLIENTBUFSIZE);
		}
		
		if (!strcmp(buf, "\r\n")){
			currbufsize = 0;
			bzero(buf, CLIENTBUFSIZE);
		}
		
		if (!strcmp(buf, ".\r\n")){
			hmp_write_error(HMP_SRC_ERR_PROTO, HM_PROTO_ERR_PARSE, fd);
			currbufsize = 0;
			bzero(buf, CLIENTBUFSIZE);
		}
		
		bzero(tbuf, BUFSIZE);
	}
	
	free(buf);
xit:
	free(tbuf);
}


static void hm_client_handle(int fd){
	pid_t pid;
	
	pid = fork();
	switch (pid){
	case 0:
		hm_client(fd);
		exit(0);
	case -1:
		hmp_write_error(HMP_SRC_ERR_PROTO, HM_PROTO_ERR_RESOURCE, fd);
		close(fd);
	default:
		close(fd);
		waitpid(pid, NULL, WNOHANG);
	}
}


static int hm_server(int sock_fd, int pending_cnt){
	int fd;

	if (listen(sock_fd, pending_cnt) < 0)
		return 1;

	hm_init_plugins();

	while (serv_alive){
		fd = accept(sock_fd, NULL, NULL);
		if (fd >= 0){
			hm_client_handle(fd);
		}
	}

	hm_cleanup_plugins();
	return 0;
}


static void hm_usage(FILE *stream){
	fprintf(stream, "hybmond [options]\n"
	    "\t-a addr	: address to use (default - all)\n"
	    "\t-p port  : port bind to\n"
	    "\t-h	: show this message\n"
	    "\t-c conf	: config file (default - " CONFIGFILE ")\n"
	    "\t-n pend	: number of pending connections (default - %d)\n"
	    "\t-s skey	: running as part of monitoring framework (do not fork)\n\n", 
	    DEFPENDING);
}


int main(int ac, char *av[]){
	int unused;
	FILE *f;
	char *arg, *val;
	char *conf = NULL;
	int sock, err = 0;
	int opt, port = 0;
	int numpending = DEFPENDING;
	struct sockaddr_in bindaddr;
	struct sigaction sigact;
	struct addrinfo hints, *addr = NULL;

	bzero(&hints, sizeof(struct addrinfo));
	bzero(&bindaddr, sizeof(struct sockaddr_in));
	hints.ai_family = AF_INET;
	bindaddr.sin_family = AF_INET;

	//process command line options
	if (ac > 1){
		while ((opt = getopt(ac,av, "s:p:a:n:c:h")) != -1){
			switch (opt){
			case 'p':/* port number to use */
				port = atoi(optarg);
				break;
			case 'a':/* address to use */
				if (getaddrinfo(optarg, NULL, &hints, &addr)){
					fprintf(stderr, "Bad node name: %s\n", optarg);
					_exit(8);
				}
				memcpy(&bindaddr.sin_addr, &(((struct sockaddr_in *)(addr->ai_addr))->sin_addr), sizeof(struct in_addr));
				break;
			case 'h':/* help */
				hm_usage(stdout);
				_exit(0);
			case 'n':/* max number of pending connections */
				numpending = atoi(optarg);
				break;
			case 'c':/* config file */
				conf = strdup(optarg);
				break;
			case 's':/* running as part of framework */
				is_module = 1;
				break;
			default:
				fprintf(stderr, "Unknown key: %s\n\n", av[optind]);
				hm_usage(stderr);
				_exit(8);
			}
		}
	} else conf = CONFIGFILE;

	//process config file
	//options in command line redefine options in config file
	if (conf){
		arg = malloc(BUFSIZE);
		val = malloc(BUFSIZE);
		
		if (!(val && arg))
			_exit(4);
		
		if (!(f = fopen(conf, "r"))){
			fprintf(stderr, "Config file not found\n");
			goto bad_file;
		}
		
		while (!feof(f)){
			bzero(arg, BUFSIZE);
			bzero(val, BUFSIZE);
			
			unused = fscanf(f, "%s %s", arg, val);
			
			if (!strcmp(arg, "address")){
				if (!addr){
					if (getaddrinfo(val, NULL, &hints, &addr)){
						fprintf(stderr, "Bad config node name: %s\n", optarg);
						goto bad_param;
					}
					memcpy(&bindaddr.sin_addr, &(((struct sockaddr_in *)(addr->ai_addr))->sin_addr), sizeof(struct in_addr));
				}
			} else if (!strcmp(arg, "port")){
				if (!port){
					port = atoi(val);
				}
			} else if (!strcmp(arg, "pending")){
				if (numpending == DEFPENDING){
					numpending = atoi(val);
				}
			} else if (!strcmp(arg, "")){
				//Just do nothing - empty string
			} else {
				fprintf(stderr, "Bad config file key: %s\n", arg);
bad_param:
				fclose(f);
bad_file:
				free(val);
				free(arg);
				if (addr)
					free(addr);
				if (conf != (char *) CONFIGFILE)
					free(conf);
				return 8;
			}
		}
		
		fclose(f);
		
		free(val);
		free(arg);
		if (conf != (char *) CONFIGFILE)
			free(conf);
	}

	if (addr)
		free(addr);
	
	if (port < 1 || port > 65535){
		fprintf(stderr, "Bad config port value: %d\n", port);
		return 8;
	}
	bindaddr.sin_port = htons(port);

	if (!is_module){
		if (daemon(0, 0) < 0){
			return 1;
		}
		
		setpgid(getpid(), getpid());
		if ((f = fopen(PIDFILE, "w"))){
			fprintf(f, "%d\n", getpid());
			fclose(f);
		}
	}
	
	signal(SIGHUP, SIG_IGN);
	signal(SIGINT, SIG_IGN);
	
	bzero(&sigact, sizeof(struct sigaction));
	sigact.sa_handler = kill_serv;
	sigemptyset(&sigact.sa_mask);
	sigaction(SIGTERM, &sigact, NULL);
	
	bzero(&sigact, sizeof(struct sigaction));
	sigact.sa_flags = SA_NOCLDWAIT | SA_NOCLDSTOP;
	sigemptyset(&sigact.sa_mask);
	sigaction(SIGCHLD, &sigact, NULL);
	
	sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock < 0)
		return 2;
	
	if (bind(sock, (struct sockaddr *) &bindaddr, sizeof(struct sockaddr_in)) < 0){
		err = 3;
		goto err_xit;
	}
	
	hm_server(sock , numpending);
	
	if (!is_module){
		unlink(PIDFILE);
	}
	
err_xit:
	close(sock);
	return err;
}


