#define _GNU_SOURCE

#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <sys/poll.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>


#define DEFAULT_SENSOR_NUMBER	25
#define DEFAULT_SOCKET_PATH	"/var/run/nmhopsad.socket"


typedef uint32_t msg_len_t;

struct msg_hdr {
	uint16_t        type;
}__attribute__((packed));
typedef struct msg_hdr msg_hdr_t;

#define MSG_TYPE_SENSORS_DATA   1
#define MSG_TYPE_CONN_REQ	2


static int active = 1;


void deactivate(int sig){
	active = 0;
}

static inline uint64_t htonll(uint64_t val){
#ifndef __BIG_ENDIAN__
	uint64_t res;
	res = (((uint64_t)htonl(val >> 32)) & 0xFFFFFFFF) |
	    (((uint64_t)(htonl(val & 0xFFFFFFFF))) << 32);
	return res;
#else
	return val;
#endif
}


int send_message(int fd, uint8_t *buf, int size){
	int wrt;
	int total;
	struct pollfd pfd;

	pfd.fd = fd;
	pfd.events = 0;

	if (poll(&pfd, 1, 0) < 0){
		perror("poll");
		return -1;
	}
	if (pfd.revents & POLLHUP){
		fprintf(stderr, "Connection closed by server!\n");
		return -2;
	}

	total = 0;
	while (total < size){
		wrt = write(fd, buf + total, size - total);
		if (wrt < 0){
			perror("write");
			return -3;
		}
		total += wrt;
	}

	return 0;
}

int get_sensors(int sens_num, uint64_t *sensors){
	FILE *fmem = NULL;
	unsigned long sensor;
	int i;
	int rval = 0;

	if (!(fmem = fopen("/proc/meminfo", "r"))){
		perror("open");
		return -1;
	}

	for (i = 0; i < sens_num; i++){
		if (fscanf(fmem, "%*s %lu %*[^\n]\n", &sensor) == 1){
			sensors[i] = htonll((uint64_t)sensor);
		} else {
			perror("fscanf");
			rval = -2;
			break;
		}
	}

	fclose(fmem);
	return rval;
}


int connect_to_server(char *client_path, char *server_path){
	int fd, err, len, rval;
	struct sockaddr_un un;

	if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0){
		perror("socket");
		return -1;
	}

	bzero(&un, sizeof(struct sockaddr_un));
	un.sun_family = AF_UNIX;
	strcpy(un.sun_path, client_path);
	len = offsetof(struct sockaddr_un, sun_path) + strlen(client_path);

	if (bind(fd, (struct sockaddr *)&un, len) < 0){
		perror("bind");
		rval = -2;
		goto err_xit;
	}

	bzero(&un, sizeof(struct sockaddr_un));
	un.sun_family = AF_UNIX;
	strcpy(un.sun_path, server_path);
	len = offsetof(struct sockaddr_un, sun_path) + strlen(server_path);

	if (connect(fd, (struct sockaddr *)&un, len) < 0){
		perror("connect");
		rval = -2;
		goto err_xit;
	}
	return fd;
err_xit:
	err = errno;
	close(fd);
	errno = err;
	return rval;
}


void usage(FILE *s, char *appname){
        fprintf(s, "Usage: %s %s", appname, " [options]\n"
            "\t-h               : show this message\n"
            "\t-n number        : maximum number of sensors\n"
            "\t-j number        : job identifier\n"
            "\t-r number        : MPI rank\n"
            "\t-s path          : path to server socket\n"
            "\t-d               : run in foreground\n\n");

}


int main(int argc, char *argv[]){
	int serverfd, rval, opt;
	int buf_size;
	int total_len;
	struct sigaction sa;
	int sens_num = DEFAULT_SENSOR_NUMBER;
	int daemonize = 1;
	unsigned long jid = 0;
	unsigned long rank = 0;
	char *client_path = NULL;
	char *server_path = NULL;
	uint8_t *buf = NULL;
	uint8_t *mdata;
	msg_len_t *mlen;
	msg_hdr_t *mhdr;

        while((opt = getopt(argc, argv, "hn:j:r:s:d")) != -1){
                switch (opt){
                case 'h':
                        usage(stdout, argv[0]);
                        _exit(0);
                case 'n':
                        sens_num = atoi(optarg);
                        break;
                case 'j':
                        jid = atol(optarg);
                        break;
                case 'r':
                        rank = atol(optarg);
                        break;
                case 's':
                        server_path = strdup(optarg);
                        break;
                case 'd':
                        daemonize = 0;
                        break;
		default:
			usage(stderr, argv[0]);
			_exit(8);
		}
	}


	if (!server_path)
		server_path = DEFAULT_SOCKET_PATH;

	if (sens_num < 0 || sens_num > 25){
		fprintf(stderr, "Invalid number of sensors.\n");
		_exit(8);
	}

	buf_size = sens_num * sizeof(uint64_t) + sizeof(msg_hdr_t) + sizeof(msg_len_t);
	if (!(buf = malloc(buf_size))){
		perror("malloc");
		_exit(errno);
	}
	mlen = (msg_len_t *)buf;
	mhdr = (msg_hdr_t *)(buf + sizeof(msg_len_t));
	mdata = buf + sizeof(msg_len_t) + sizeof(msg_hdr_t);

	if (asprintf(&client_path, "/tmp/lwm2-%d.socket", getpid()) < 0){
		perror("asprintf");
		_exit(errno);
	}

	if ((serverfd = connect_to_server(client_path, server_path)) < 0){
		fprintf(stderr, "Cannot connect to server!\n");
		_exit(errno);
	}

	if (daemonize)
		daemon(0, 0);

        signal(SIGHUP, SIG_IGN);
        signal(SIGINT, SIG_IGN);
        bzero(&sa, sizeof(struct sigaction));
        sa.sa_handler = deactivate;
        sigemptyset(&sa.sa_mask);
        sigaction(SIGTERM, &sa, NULL);

	total_len = sizeof(msg_hdr_t) + sizeof(uint64_t);
	*mlen = htonl(total_len);
	total_len += sizeof(msg_len_t);
	mhdr->type = htons(MSG_TYPE_CONN_REQ);
	*((uint64_t *)mdata) = htonll(((uint64_t)jid<<32)|rank);

	if (send_message(serverfd, buf, total_len)){
		fprintf(stderr, "Connection failed!\n");
		rval = -1;
		goto err_xit;
	}

	total_len = sizeof(msg_hdr_t) + sizeof(uint64_t) * sens_num;
	*mlen = htonl(total_len);
	total_len += sizeof(msg_len_t);
	mhdr->type = htons(MSG_TYPE_SENSORS_DATA);

	while (active){
		if (get_sensors(sens_num, (uint64_t *)mdata)){
			fprintf(stderr, "Get sensors failed!\n");
		} else {
			if (send_message(serverfd, buf, total_len)){
				fprintf(stderr, "Connection failed!\n");
				rval = -2;
				goto err_xit;
			}
		}
		sleep(10);
	}
err_xit:
	close(serverfd);
	unlink(client_path);
	free(buf);
	return rval;
}
