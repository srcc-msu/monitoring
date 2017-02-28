#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <plugins_api.h>

#include <libsmart.h>


#define DEV_LOCATION	"/dev"


static void process_disks(char *param, int fd){
	char *answ;
	DIR *devdir;
	struct dirent *dsk;

	answ = malloc(BUFSIZE << 1);
	bzero(answ, BUFSIZE << 1);

	if (!(devdir = opendir(DEV_LOCATION))){
		hmp_write_error(HMP_SRC_ERR_SYSTEM, errno, fd);
		return;
	}
	
	while ((dsk = readdir(devdir))){
		if (strstr(dsk->d_name, "sd") == dsk->d_name && !isdigit(dsk->d_name[strlen(dsk->d_name)-1])){
			if (answ[0])
				strcat(answ, " ");
			strcat(answ, dsk->d_name);
		}
	}
	
	hmp_write_local_answer(param, answ, fd);
	
	free(answ);
}


static void process_health(char *param, char *dskname, int fd){
	int fdd;
	char *dname;
	
	if (asprintf(&dname, DEV_LOCATION "/%s", dskname) < 0){
		hmp_write_error(HMP_SRC_ERR_SYSTEM, errno, fd);
		return;
	}
	
	if ((fdd = open(dname, O_RDONLY)) < 0){
		hmp_write_error(HMP_SRC_ERR_SYSTEM, errno, fd);
		goto err_xit;
	}
	
	if (smart_health(fdd) < 0)
		hmp_write_local_answer(param, "SICK", fd);
	else
		hmp_write_local_answer(param, "HEALTHY", fd);
	
	close(fdd);
	
err_xit:
	free(dname);
}


static void process_values(char *param, char *dskname, int fd){
	int fdd, i;
	char *dname;
	struct ata_smart_values vals;
	struct ata_smart_thresholds_pvt thrsh;

	if (asprintf(&dname, DEV_LOCATION "/%s", dskname) < 0){
		hmp_write_error(HMP_SRC_ERR_SYSTEM, errno, fd);
		return;
	}
	
	if ((fdd = open(dname, O_RDONLY)) < 0){
		hmp_write_error(HMP_SRC_ERR_SYSTEM, errno, fd);
		goto err_xit;
	}

	if (smart_support(fdd) < 0){
		hmp_write_error(HMP_SRC_ERR_SYSTEM, ENOSYS, fd);
		goto sm_xit;
	}
	
	if (smart_enabled(fdd) < 0){
		hmp_write_error(HMP_SRC_ERR_SYSTEM, ENODATA, fd);
		goto sm_xit;
	}
	
	if (smart_values(fdd, &vals) < 0 || smart_thresholds(fdd, &thrsh) < 0){
		hmp_write_error(HMP_SRC_ERR_SYSTEM, EIO, fd);
		goto sm_xit;
	}
	
	dname = realloc(dname, BUFSIZE << 1);
	bzero(dname, BUFSIZE << 1);
	
	char *tmpbuf = malloc(BUFSIZE);
	for (i = 0; i<NUMBER_ATA_SMART_ATTRIBUTES; i++){
		bzero(tmpbuf, BUFSIZE);
		/* id value worst thresh raw */
		sprintf(tmpbuf, "%u %u %u %u %x%x%x%x%x%x;", vals.vendor_attributes[i].id, vals.vendor_attributes[i].current, 
		    vals.vendor_attributes[i].worst, thrsh.thres_entries[i].threshold, 
		    vals.vendor_attributes[i].raw[5], vals.vendor_attributes[i].raw[4], 
		    vals.vendor_attributes[i].raw[3], vals.vendor_attributes[i].raw[2], 
		    vals.vendor_attributes[i].raw[1], vals.vendor_attributes[i].raw[0]);
		
		strcat(dname, tmpbuf);
	}
	free(tmpbuf);
	
	hmp_write_local_answer(param, dname, fd);
	
sm_xit:
	close(fdd);
err_xit:
	free(dname);
}


void smart_process(char *param, int fd){
	char *dsk;

	if (!strcmp(param, "disks")){
		process_disks(param, fd);
		return;
	}
	
	dsk = strchr(param, '.') + 1;
	
	if (strstr(param, "health") == param){
		process_health(param, dsk, fd);
		return;
	}
	
	if (strstr(param, "values") == param){
		process_values(param, dsk, fd);
		return;
	}
	
	hmp_write_error(HMP_SRC_ERR_SYSTEM, ENOENT, fd);
}

