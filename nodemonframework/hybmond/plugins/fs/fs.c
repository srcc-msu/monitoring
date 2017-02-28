#define _GNU_SOURCE

#include <errno.h>
#include <sys/vfs.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mntent.h>

#include <plugins_api.h>

#define MOUNTS		"/proc/self/mounts"

void fs_process(char *param, int fd){
	int unused;
	FILE *mounts;
	char *mpname, *answ;
	struct statfs statbuf;
	struct mntent* mountfield;
	
	if (!strcmp(param, "mounts")){
		//Show mountpoints
		
		mounts = setmntent(MOUNTS,"r");
		if (!mounts){
			hmp_write_error(HMP_SRC_ERR_SYSTEM, errno, fd);
		} else {
			answ = malloc(BUFSIZE);
			bzero(answ, BUFSIZE);
			
			mountfield = getmntent(mounts);
			strcpy(answ, mountfield->mnt_dir);
			
			while ((mountfield = getmntent(mounts))){
				strcat(answ, " ");
				strcat(answ, mountfield->mnt_dir);
			}
			
			fclose(mounts);
			
			hmp_write_local_answer(param, answ, fd);
			
			free(answ);
		}
		
		return;
	}
	
	if (strstr(param, "usage.") == param){
		mpname = strchr(param, '.');
		mpname++;
		//Report mountpoint
		if (statfs(mpname, &statbuf) < 0){
			hmp_write_error(HMP_SRC_ERR_SYSTEM, errno, fd);
		} else {
			if (statbuf.f_blocks && statbuf.f_files)
				unused = asprintf(&answ, "%d %d", (int)((statbuf.f_blocks - statbuf.f_bfree) * 100 / statbuf.f_blocks), (int)((statbuf.f_files - statbuf.f_ffree) * 100 / statbuf.f_files));
			else 
				unused = asprintf(&answ, "0 0");
			hmp_write_local_answer(param, answ, fd);
			free(answ);
		}
		
		return;
	}
	
	hmp_write_error(HMP_SRC_ERR_SYSTEM, ENOENT, fd);
}


