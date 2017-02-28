#define _BSD_SOURCE
#define _XOPEN_SOURCE

#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/poll.h>
#include <arpa/inet.h>
#include <signal.h>
#include <fcntl.h>
#include <grp.h>
#include <errno.h>

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

#include <nm_module.h>
#include <nm_modshm.h>
#include <nm_syslog.inc.c>


#define HOPSA_MAX_SENS  25
#define HOPSA_MAX_CONN  64
#define HOPSA_ERR_VAL   0
#define HOPSA_FRST_SENS 28000

typedef uint64_t hopsa_sens_t;
#define HOPSA_ELEM_LEN  sizeof(hopsa_sens_t)


#define HOPSA_CONN_ACTIVE	1
#define HOPSA_CONN_LEN		2
#define HOPSA_CONN_ID		4

struct hopsa_conn {
	uint8_t		flags;
	uint64_t	id;
	uint32_t	msg_len;
	size_t		buf_offset;
	int		buf_size;
	int		sens_elem;
	struct timeval	tv;
	uint8_t		*buf;
	struct pollfd   *pfd;
};
typedef struct hopsa_conn hopsa_conn_t;


struct hopsa_conf {
	int num_sens;
	int max_conn;
	int timeout;
	int shmkey;
	char *sock_path;
	char *group;
};
typedef struct hopsa_conf hopsa_conf_t;

static hopsa_conf_t conf;


struct hopsa_data {
	uint8_t         num_sens;
	uint8_t         num_elem;
	uint16_t        dynlen;
	uint8_t         *dynval;
};
typedef struct hopsa_data hopsa_data_t;

static hopsa_data_t data;


struct msg_hdr {
	uint16_t	type;
}__attribute__((packed));
typedef struct msg_hdr msg_hdr_t;

#define	MSG_TYPE_SENSORS_DATA	1
#define	MSG_TYPE_CONN_REQUEST	2


static int hopsa_active = 1;


void hopsa_deactivate(int sig){
	hopsa_active = 0;
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


static inline uint64_t ntohll(uint64_t val){
#ifndef __BIG_ENDIAN__
	uint64_t res;
	res = (((uint64_t)ntohl(val >> 32)) & 0xFFFFFFFF) |
	    (((uint64_t)(ntohl(val & 0xFFFFFFFF))) << 32);
	return res;
#else
	return val;
#endif
}


static int tv_cmp(struct timeval *tv0, struct timeval *tv1){
        if (tv0->tv_sec < tv1->tv_sec)
                return -1;
        if (tv0->tv_sec > tv1->tv_sec)
                return 1;
        if (tv0->tv_usec < tv1->tv_usec)
                return -1;
        if (tv0->tv_usec > tv1->tv_usec)
                return 1;
        return 0;
}


static void tv_fix(struct timeval *tv){
        if ((int) tv->tv_usec < 0){
                tv->tv_sec--;
                tv->tv_usec += 1000000;
        }

        if ((int) tv->tv_usec >= 1000000){
                tv->tv_sec++;
                tv->tv_usec -= 1000000;
        }
}


static void tv_sub(struct timeval *tv0, struct timeval *tv1){
        tv0->tv_sec -= tv1->tv_sec;
        tv0->tv_usec-= tv1->tv_usec;
        tv_fix(tv0);
}


inline static void clear_conn_data(int elem_cnt){
	int i;

	for (i = 0; i < data.num_sens; i++){
		NM_VECTADDR(data.dynval, HOPSA_ELEM_LEN, data.num_elem, i, hopsa_sens_t)[elem_cnt] =
		    htonll((uint64_t)NM_CNT_NONE);
	}
}


static void hopsa_release(struct nm_module_bufdesc_t *mdesc){
	data.dynlen = 0;

	nm_mod_buf_dt(data.dynval, mdesc);
	data.dynval = NULL;
}


static int hopsa_init(struct nm_module_bufdesc_t *mdesc){
	int i;

	data.dynlen = NM_DYNELEMLEN(HOPSA_ELEM_LEN, data.num_elem) * data.num_sens;

	data.dynval = nm_mod_buf_at(data.dynlen, mdesc);
	if (!data.dynval)
		goto err_xit;

	for (i = 0; i < data.num_sens; i++){
		*(struct nm_tlv_hdr_t *)NM_GROUPADDR(data.dynval, HOPSA_ELEM_LEN, data.num_elem, i) =
		    NM_MONTYPE(HOPSA_FRST_SENS + i, data.num_elem * HOPSA_ELEM_LEN);
	}

	for (i = 0; i < data.num_elem; i++){
		clear_conn_data(i);
	}

	return 0;

err_xit:
	hopsa_release(mdesc);
	return -1;
}


static void hopsa_get_sensors(hopsa_conn_t *conn){
	int i;
	hopsa_sens_t *sens;

	sens = (hopsa_sens_t *)(conn->buf + sizeof(msg_hdr_t));

	NM_VECTADDR(data.dynval, HOPSA_ELEM_LEN, data.num_elem, 0, hopsa_sens_t)[conn->sens_elem] = conn->id;
	for (i = 1; i < data.num_sens; i++){
		NM_VECTADDR(data.dynval, HOPSA_ELEM_LEN, data.num_elem, i, hopsa_sens_t)[conn->sens_elem] = sens[i - 1];
	}
}


inline static int hopsa_read(int fd, uint8_t *buf, size_t size, size_t *offset){
	ssize_t rd;

	while (*offset < size){
		rd = read(fd, buf + *offset, size - *offset);
		if (rd < 0){
			if (errno != EAGAIN){
				nm_syslog(LOG_ERR, "read: %s", strerror(errno));
				return -1;
			}
			return 0;
		}
		*offset += rd;
	}
	return 0;
}


static int hopsa_read_message(hopsa_conn_t *conn){
	int msg_data_len;
	msg_hdr_t *hdr;

	if (!(conn->flags & HOPSA_CONN_LEN)){
		if (hopsa_read(conn->pfd->fd,
				(uint8_t *)&conn->msg_len,
				sizeof(uint32_t),
				&conn->buf_offset)){

			return -1;
		}

		if (conn->buf_offset != sizeof(uint32_t))
			return 0;

		conn->msg_len = ntohl(conn->msg_len);
		conn->buf_offset = 0;
		conn->flags |= HOPSA_CONN_LEN;

		if (conn->msg_len < 0 || conn->msg_len > conn->buf_size){
			nm_syslog(LOG_ERR, "%s", "Incorrect message len.");
			return -1;
		}
	}

	if (hopsa_read(conn->pfd->fd,
			conn->buf,
			conn->msg_len,
			&conn->buf_offset)){

		return -1;
	}

	if (conn->buf_offset != conn->msg_len)
		return 0;

	conn->buf_offset = 0;

	hdr = (msg_hdr_t *)conn->buf;
	hdr->type = ntohs(hdr->type);

	if (hdr->type == MSG_TYPE_SENSORS_DATA){
		if (!(conn->flags & HOPSA_CONN_ID)){
			nm_syslog(LOG_ERR, "%s", "Client without ID.");
			return -1;
		}

		msg_data_len = conn->msg_len - sizeof(msg_hdr_t);
		if (msg_data_len != sizeof(hopsa_sens_t) * (conf.num_sens)){
			nm_syslog(LOG_ERR, "%s", "Icorrect sensors data.");
			return -1;
		}

		conn->flags &= ~HOPSA_CONN_LEN;
		hopsa_get_sensors(conn);
		gettimeofday(&conn->tv, NULL);

	} else if (hdr->type == MSG_TYPE_CONN_REQUEST){
		if (conn->flags & HOPSA_CONN_ID){
			nm_syslog(LOG_ERR, "%s", "Second connection request.");
			return -1;
		}

		msg_data_len = conn->msg_len - sizeof(msg_hdr_t);
		if (msg_data_len != sizeof(uint64_t)){
			nm_syslog(LOG_ERR, "%s", "Icorrect client ID size.");
			return -1;
		}

		conn->id = *((uint64_t *)(conn->buf + sizeof(msg_hdr_t)));
		conn->flags |= HOPSA_CONN_ID;
		conn->flags &= ~HOPSA_CONN_LEN;
	} else {
		nm_syslog(LOG_ERR, "%s", "Unknown message type.");
		return -1;
	}

	return 0;
}


static int hopsa_listen_sock(){
	int fd, addr_len;
	struct sockaddr_un un;
	struct group *grp;

	if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0){
		nm_syslog(LOG_ERR, "socket: %s", strerror(errno));
		return -1;
	}

        if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0){
		nm_syslog(LOG_ERR, "fcntl: %s", strerror(errno));
		goto err_xit;
	}

	unlink(conf.sock_path);

	bzero(&un, sizeof(struct sockaddr_un));
	un.sun_family = AF_UNIX;
	strcpy(un.sun_path, conf.sock_path);

	addr_len = offsetof(struct sockaddr_un, sun_path) + strlen(conf.sock_path);

	if (bind(fd, (struct sockaddr *)&un, addr_len) < 0){
		nm_syslog(LOG_ERR, "bind: %s", strerror(errno));
		goto err_xit;
	}

	if (conf.group){
		if (!(grp = getgrnam(conf.group))){
			nm_syslog(LOG_ERR, "getgrnam: %s", strerror(errno));
			goto err_xit;
		}

		if (chown(un.sun_path, -1, grp->gr_gid) < 0){
			nm_syslog(LOG_ERR, "chown: %s", strerror(errno));
			goto err_xit;
		}
	}

	if (chmod(un.sun_path, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP) < 0){
		nm_syslog(LOG_ERR, "chmod: %s", strerror(errno));
		goto err_xit;
	}

	if (listen(fd, conf.max_conn) < 0){
		nm_syslog(LOG_ERR, "listen: %s", strerror(errno));
		goto err_xit;
	}

	return fd;
err_xit:
	close(fd);
	return -1;
}


inline static int add_connection(int newfd, hopsa_conn_t *connections){
	hopsa_conn_t *conn;

	if (fcntl(newfd, F_SETFL, O_NONBLOCK) < 0){
		nm_syslog(LOG_ERR, "fcntl: %s", strerror(errno));
		return -1;
	}

	for (conn = connections; conn->buf; conn++){
		if (!(conn->flags & HOPSA_CONN_ACTIVE)){
			conn->pfd->fd = newfd;
			conn->flags = HOPSA_CONN_ACTIVE;
			conn->buf_offset = 0;
			gettimeofday(&conn->tv, NULL);
			return 0;
		}
	}

	nm_syslog(LOG_ERR, "%s", "Connection limit reached.");
	return -2;
}


inline static void del_connection(hopsa_conn_t *conn){
	close(conn->pfd->fd);
	conn->pfd->fd = -1;
	conn->flags = 0;
	clear_conn_data(conn->sens_elem);
}


static int hopsa_server(int listenfd){
	int newfd, pfd_num, buf_size, poll_rval, i;
	int rval = 0;
	struct timeval tv_delay, tv_timeout, tv_curr, tv_tmp;
	struct pollfd *p;
	struct pollfd *pfd = NULL;
	hopsa_conn_t *conn;
	hopsa_conn_t *connections = NULL;

	connections = malloc(sizeof(hopsa_conn_t) * (conf.max_conn + 1));

	if (!connections){
		nm_syslog(LOG_ERR, "malloc: %s", strerror(errno));
		rval = -1;
		goto err_xit;
	}

	for (i = 0; i <= conf.max_conn; i++){
		connections[i].buf = NULL;
		connections[i].sens_elem = i;
	}

	buf_size = sizeof(hopsa_sens_t) * conf.num_sens + sizeof(msg_hdr_t);
	for (i = 0; i < conf.max_conn; i++){
		if (!(connections[i].buf = malloc(buf_size))){
			nm_syslog(LOG_ERR, "malloc: %s", strerror(errno));
			rval = -1;
			goto err_xit;
		}
		connections[i].buf_size = buf_size;
	}

	pfd_num = conf.max_conn + 1;
	if(!(pfd = malloc(sizeof(struct pollfd) * pfd_num))){
		nm_syslog(LOG_ERR, "malloc: %s", strerror(errno));
		rval = -1;
		goto err_xit;
	}

	p = pfd;
	p->fd = listenfd;
	p->events = POLLIN;
	p++;

	for (conn = connections; conn->buf; conn++, p++){
		p->fd = -1;
		p->events = POLLIN;
		conn->pfd = p;
	}

	tv_timeout.tv_sec = conf.timeout;
	tv_timeout.tv_usec = 0;
	tv_delay = tv_timeout;

	while(hopsa_active){
		poll_rval = poll(pfd, pfd_num, tv_delay.tv_sec * 1000 + tv_delay.tv_usec / 1000);
		if (poll_rval < 0){
			if (errno != EINTR){
				nm_syslog(LOG_ERR, "poll: %s", strerror(errno));
				break;
			}
			continue;
		}

		if (poll_rval == 0)
			goto skip_revents_check;


		if (pfd[0].revents & POLLIN){
			if ((newfd = accept(listenfd, NULL, NULL)) < 0){
				nm_syslog(LOG_ERR, "accept: %s", strerror(errno));
			} else {
				if(add_connection(newfd, connections)){
					nm_syslog(LOG_WARNING, "%s",
					    "Cannot create new connection.");
					close(newfd);
				}
			}
		}

		for (conn = connections; conn->buf; conn++){
			if (!(conn->flags & HOPSA_CONN_ACTIVE))
				continue;

			if (conn->pfd->revents & POLLHUP){
				del_connection(conn);
				continue;
			}

			if (conn->pfd->revents & POLLIN)
				if (hopsa_read_message(conn))
					del_connection(conn);
		}
skip_revents_check:
		tv_delay = tv_timeout;
		gettimeofday(&tv_curr, NULL);

		for (conn = connections; conn->buf; conn++){
			if (!(conn->flags & HOPSA_CONN_ACTIVE))
				continue;

			tv_tmp = tv_curr;
			tv_sub(&tv_tmp, &conn->tv);
			if (tv_cmp(&tv_tmp, &tv_timeout) > 0)
				del_connection(conn);
			else
				if (tv_cmp(&tv_tmp, &tv_delay) < 0)
					tv_delay = tv_tmp;
		}
	}

	for (conn = connections; conn->buf; conn++){
		if (conn->flags & HOPSA_CONN_ACTIVE)
			close(conn->pfd->fd);
	}
err_xit:
	if (connections){
		for (conn = connections; conn->buf; conn++){
			if (conn->buf)
				free(conn->buf);
		}
		free(connections);
	}

	if (pfd)
		free(pfd);
	return rval;
}


static void usage(FILE *s){
	fprintf(s, APPNAME " [options]\n"
	    "\t-h               : show this message\n"
	    "\t-s key           : module SHM key\n"
	    "\t-c config        : config file\n"
	    "\t-m number        : maximum number of client connections\n"
	    "\t-n number        : number of sensors\n"
	    "\t-t number        : client timeout in seconds\n"
	    "\t-u path          : unix socket path\n"
	    "\t-g group         : name of group socket file owner\n\n");
}


static void hopsa_get_conf(int ac, char *av[]){
	int opt;
	FILE *conf_file;
	char *conf_path = NULL;
	char *conf_arg = NULL;
	char *conf_val = NULL;
	char bad_cnf_str[] = "Bad configuration:";
	char max_conn_str[] = "max_connections";
	char num_sens_str[] = "num_sensors";
	char timeout_str[] = "timeout";
	char sock_path_str[] = "socket_path";
	char group_str[] = "group";

	conf.shmkey = -1;
	conf.max_conn = -1;
	conf.num_sens = -1;
	conf.timeout = -1;
	conf.sock_path = NULL;
	conf.group = NULL;

	while((opt = getopt(ac, av, "hs:c:m:n:t:u:g:")) != -1){
		switch (opt){
		case 'h':/* show help */
			usage(stdout);
			_exit(0);
		case 's':/* module SHM key */
			conf.shmkey = atoi(optarg);
			break;
		case 'c':/* config file path */
			conf_path = strdup(optarg);
			break;
		case 'm':/* maximum number of client connections */
			conf.max_conn = atoi(optarg);
			break;
		case 'n':/* number of sensors */
			conf.num_sens = atoi(optarg);
			break;
		case 't':/* client timeout */
			conf.timeout = atoi(optarg);
			break;
		case 'u':/* unix socket path */
			conf.sock_path = strdup(optarg);
			break;
		case 'g':/* group socket owner */
			conf.group = strdup(optarg);
			break;
		default:
			usage(stderr);
			nm_syslog(LOG_ERR, "%s unknown option -%c",
			    bad_cnf_str, optopt);
			_exit(8);
		}
	}

	if (conf.max_conn < 0 || conf.num_sens < 0 || conf.timeout < 0){
		if (!conf_path)
			conf_path = CONFIGFILE;

		if (!(conf_arg = malloc(BUFSIZE))){
			nm_syslog(LOG_ERR, "malloc: %s", strerror(errno));
			_exit(4);
		}

		if (!(conf_val = malloc(BUFSIZE))){
			nm_syslog(LOG_ERR, "malloc: %s", strerror(errno));
			_exit(4);
		}

		if (!(conf_file = fopen(conf_path, "r"))){
			nm_syslog(LOG_ERR, "Cannot open file %s: %s", conf_path, strerror(errno));
			_exit(8);
		}

		while(!feof(conf_file)){
			fscanf(conf_file, "%s %s", conf_arg, conf_val);

			if (!strcmp(conf_arg, "")){
				/* skip empty string */
			} else if (!strcmp(conf_arg, max_conn_str)){
				if (conf.max_conn < 0)
					conf.max_conn = atoi(conf_val);
			} else if (!strcmp(conf_arg, num_sens_str)){
				if (conf.num_sens < 0)
					conf.num_sens = atoi(conf_val);
			} else if (!strcmp(conf_arg, timeout_str)){
				if (conf.timeout < 0)
					conf.timeout = atoi(conf_val);
			} else if (!strcmp(conf_arg, sock_path_str)){
				if (!conf.sock_path)
				conf.sock_path = strdup(conf_val);
			} else if (!strcmp(conf_arg, group_str)){
				if (!conf.group)
				conf.group = strdup(conf_val);
			} else {
				nm_syslog(LOG_ERR, "%s unknown parameter %s",
				    bad_cnf_str, conf_arg);
				_exit(8);
			}
		}
		fclose(conf_file);
	}

	if (conf.max_conn <= 0 || conf.max_conn > HOPSA_MAX_CONN){
		nm_syslog(LOG_ERR, "%s invalid value of parameter %s",
		    bad_cnf_str, max_conn_str);
		_exit(8);
	}

	if (conf.num_sens <= 0 || conf.num_sens > HOPSA_MAX_SENS){
		nm_syslog(LOG_ERR, "%s invalid value of parameter %s",
		    bad_cnf_str, num_sens_str);
		_exit(8);
	}

	if (conf.timeout <= 0){
		nm_syslog(LOG_ERR, "%s invalid value of parameter %s",
		    bad_cnf_str, timeout_str);
		_exit(8);
	}

	if (!conf.sock_path){
		nm_syslog(LOG_ERR, "%s invalid value of parameter %s",
		    bad_cnf_str, sock_path_str);
		_exit(8);
	}

	free(conf_arg);
	free(conf_val);
}


int main(int argc, char *argv[]){
	int sockfd;
	struct nm_module_bufdesc_t *bufdesc = NULL;
	struct sigaction sigact;

	hopsa_get_conf(argc, argv);

	data.num_sens = conf.num_sens + 1;
	data.num_elem = conf.max_conn;

	if (conf.shmkey != -1){
		bufdesc = nm_mod_bufdesc_at(conf.shmkey);
		if (!bufdesc){
			nm_syslog(LOG_ERR, "%s",
			    "cannot commuticate with master process!");
			_exit(4);
		}
	}

	if (hopsa_init(bufdesc)){
		nm_syslog(LOG_ERR, "%s", "initialization module failed.");
		return -1;
	}

	if ((sockfd = hopsa_listen_sock()) < 0){
		nm_syslog(LOG_ERR, "%s", "cannot create listen socket.");
		return -1;
	}

	nm_syslog(LOG_NOTICE, "%s", "starting");

	signal(SIGHUP, SIG_IGN);
	signal(SIGINT, SIG_IGN);
	bzero(&sigact, sizeof(struct sigaction));
	sigact.sa_handler = hopsa_deactivate;
	sigemptyset(&sigact.sa_mask);
	sigaction(SIGTERM, &sigact, NULL);

	hopsa_server(sockfd);

	nm_syslog(LOG_NOTICE, "%s", "stopping");

	close(sockfd);
	unlink(conf.sock_path);
	free(conf.sock_path);

	hopsa_release(bufdesc);
	nm_mod_bufdesc_dt(bufdesc);

	return 0;
}
