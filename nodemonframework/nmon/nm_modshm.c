#define _XOPEN_SOURCE
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <stdlib.h>

#include <nm_modshm.h>


static void buf_dt(void *buf, struct nm_module_bufdesc_t *mdesc){
	if (!buf)
		return;

	if (!mdesc){
		/* Standalone */
		free(buf);
		return;
	}

	/* Module */
	shmdt(buf);
	mdesc->buf_shmid = -1;
	mdesc->buf_size = 0;
}


void nm_mod_buf_dt(void *buf, struct nm_module_bufdesc_t *mdesc){
	buf_dt(buf, mdesc);
	if (mdesc)
		mdesc->buf_flags |= NM_BUF_SHMID_CHANGED;
}


void *nm_mod_buf_at(size_t len, struct nm_module_bufdesc_t *mdesc){
	int shmid;
	void *res;

	if (!mdesc)
		/* Standalone */
		return malloc(len);

	/* Module */
	shmid = shmget(IPC_PRIVATE, len,
				IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
	if (shmid < 0)
		return NULL;

	res = shmat(shmid, NULL, 0);
	shmctl(shmid, IPC_RMID, NULL);
	if (res == (void *)-1)
		return NULL;

	mdesc->buf_size = len;
	mdesc->buf_shmid = shmid;
	mdesc->buf_flags |= NM_BUF_SHMID_CHANGED;

	return res;
}


void *nm_mod_buf_reat(size_t len, void *old_buf, struct nm_module_bufdesc_t *mdesc){

	if (mdesc->buf_flags & NM_BUF_SHMID_CHANGED)
		/* Cannot change new buffer until nmond connects
		   the old one. */
		return NULL;

	buf_dt(old_buf, mdesc);

	return nm_mod_buf_at(len, mdesc);
}


void nm_mod_bufdesc_dt(struct nm_module_bufdesc_t *bufdesc){
	if (bufdesc)
		shmdt(bufdesc);
}


struct nm_module_bufdesc_t *nm_mod_bufdesc_at(int shmkey){
	void *res;

	res = shmat(shmkey, NULL, 0);
	if (res == (void *) -1)
		return NULL;

	return (struct nm_module_bufdesc_t *)res;
}

