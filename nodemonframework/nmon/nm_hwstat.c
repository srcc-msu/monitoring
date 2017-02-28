#define _GNU_SOURCE
#define _BSD_SOURCE

#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <syslog.h>
#include <errno.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <nm_dumb.h>
#include <nm_module.h>
#include <nm_syslog.h>

#define NM_HWCHECK_PROG        "hwcheckp"
#define NM_HW_CODE_FAIL	255
#define NM_HW_MSG_MAX_LEN	80


void nm_cleanup_hwstat(struct nm_hw_status_t *stat){
	if (stat->dynval)
		free(stat->dynval);
	
	if (stat->hw_msg)
		free(stat->hw_msg);
	
	stat->dynlen = stat->code = NM_HW_CODE_FAIL;
}


static int nm_run_hwcheck(char **hwargs, struct nm_hw_status_t *stat){
	int rc;
	unsigned int msg_len;
	FILE *f;
	pid_t pid;
	int fd[2];
	const char hwstatus_str[] = "-hwstatus=";
	const char hwmessage_str[] = "-hwmessage=";
	int hw_code = -1;
	char *hw_msg = NULL;
	char *buf = NULL;
	char *format = NULL;

	stat->code = NM_HW_CODE_FAIL;

	if (!(buf = malloc(BUFSIZE))){
		rc = -1;
		goto err_xit;
	}

	if (!(hw_msg = malloc(NM_HW_MSG_MAX_LEN))){
		rc = -2;
		goto err_xit;
	}

	if (asprintf(&format, "%s%%d %s%%%d[^\r\n]", hwstatus_str, hwmessage_str, NM_HW_MSG_MAX_LEN) < 0){
		rc = -3;
		goto err_xit;
	}

	if (pipe(fd)){
		rc = -4;
		goto err_xit;
	}

	if (!(f = fdopen(fd[0], "r"))){
		close(fd[0]);
		close(fd[1]);
		rc = -5;
		goto err_xit;
	}

	if ((pid = fork()) < 0){
		fclose(f);
		close(fd[1]);
		rc = -6;
		goto err_xit;
	} else if (!pid){
		// child
		fclose(f);

		if (fd[1] != STDOUT_FILENO){
			if (dup2(fd[1], STDOUT_FILENO) != STDOUT_FILENO){
				return -1;
			}
			close(fd[1]);
		}

		execvp(hwargs[0], hwargs);
		nm_syslog(LOG_ERR, "Failed to exec %s: %s", NM_HWCHECK_PROG, strerror(errno));
		_exit(1);
	}
	// parent
	close(fd[1]);
	while (fgets(buf, BUFSIZE, f)){
		if (!strncmp(buf, hwstatus_str, strlen(hwstatus_str))){
			if (sscanf(buf, format, &hw_code, hw_msg) < 2)
				hw_msg[0] = 0;
			break;
		}
	}
	fclose(f);

	if (hw_code < 0 || hw_code > 2){
		rc = -7;
		goto wp_err_xit;
	}

	msg_len = strlen(hw_msg);
	if (!(hw_code && msg_len))
		stat->hw_msg = NULL;
	else {
		if (!(stat->hw_msg = malloc(msg_len))){
			rc = -8;
			goto wp_err_xit;
		}
		strncpy(stat->hw_msg, hw_msg, msg_len);
	}
	rc = hw_code;
	stat->code = (uint8_t)hw_code;

wp_err_xit:
	waitpid(pid, NULL, 0);
err_xit:
	if (format)
		free(format);

	if (hw_msg)
		free(hw_msg);

	if (buf)
		free(buf);

	return rc;
}


int nm_init_hwstat(struct nm_hw_opt_t *opt, struct nm_hw_status_t *stat){
	int hw_rc;
	int retr;
	uint8_t *addr;
	char hwcheck_prog[] = NM_HWCHECK_PROG;
	char **hwargs = NULL;

	hwargs = malloc((opt->ac + 2) * sizeof(char *));
	if (!hwargs)
		return -1;

	if (opt->ac)
		memcpy(hwargs + 1, opt->av, opt->ac * sizeof(char *));

	hwargs[0] = hwcheck_prog;
	hwargs[opt->ac + 1] = NULL;

	retr = 0;
	hw_rc = nm_run_hwcheck(hwargs, stat);
	while(hw_rc < 0 && retr <= opt->retr_max){
		retr++;
		sleep(opt->retr_interval);
		hw_rc = nm_run_hwcheck(hwargs, stat);
	}
	free(hwargs);

	if (hw_rc < 0)
		return hw_rc;

	stat->dynlen = sizeof(struct nm_tlv_hdr_t) + sizeof(uint8_t);
	
	if (stat->hw_msg)
		stat->dynlen += strlen(stat->hw_msg) + sizeof(struct nm_tlv_hdr_t);
	else
		stat->dynlen += sizeof(struct nm_tlv_hdr_t);
	
	if (!(stat->dynval = malloc(stat->dynlen))){
		nm_cleanup_hwstat(stat);
		return -1;
	}
	
	bzero(stat->dynval, stat->dynlen);
	
	addr = stat->dynval;
	*(struct nm_tlv_hdr_t *)addr = NM_MONTYPE(MON_BAD_CNF, sizeof(uint8_t));
	addr += sizeof(struct nm_tlv_hdr_t);
	
	*addr = stat->code;
	addr += sizeof(uint8_t);
	
	*(struct nm_tlv_hdr_t *)addr = NM_MONTYPE(BAD_CNF_INFO, (stat->hw_msg ? strlen(stat->hw_msg) : 0));
	if (stat->hw_msg){
		addr += sizeof(struct nm_tlv_hdr_t);
		strncpy((char *)addr, stat->hw_msg, strlen(stat->hw_msg));
	}
	
	return stat->code;
}


/* for next releases - maybe some fixes needed */
#if 0
int nm_hwstat_file(char *fn, struct nm_hw_status_t *stat){
	int fd;
	uint16_t len = 0;
	uint8_t *addr;
	
	stat->code = 0;
	
	if ((fd = open(fn, O_RDONLY)) >= 0){
		if (read(fd, &(stat->code), sizeof(uint8_t)) > 0){
			if (stat->code)
				read(fd, &len, sizeof(uint16_t));
		}
	}
	
	if (stat->dynlen != sizeof(struct nm_tlv_hdr_t) + sizeof(uint8_t) + len){
		stat->dynlen = sizeof(struct nm_tlv_hdr_t) + sizeof(uint8_t) +len;
		addr = realloc(stat->dynval, stat->dynlen);
		if (!addr){
			if (fd >= 0)
				close(fd);
			nm_cleanup_hwstat(stat);
			return -1;
		}
		
		stat->dynval = addr;
		bzero(addr, stat->dynlen);
		
		*(struct nm_tlv_hdr_t *)addr = NM_MONTYPE(MON_BAD_CNF, sizeof(uint8_t));
	} else {
		addr = stat->dynval;
	}
	
	addr += sizeof(struct nm_tlv_hdr_t);
	
	*addr = stat->code;
	addr += sizeof(uint8_t);
	
	if (len){
		*(struct nm_tlv_hdr_t *)addr = NM_MONTYPE(BAD_CNF_INFO, len);
		addr += sizeof(struct nm_tlv_hdr_t);
		read(fd, addr, len);
	}
	
	if (fd >= 0)
		close(fd);
	
	return stat->code;
}
#endif


