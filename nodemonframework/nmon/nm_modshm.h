#ifndef _NM_MODSHM_H_
#define _NM_MODSHM_H_

#include <nm_module.h>

void nm_mod_buf_dt(void *buf, struct nm_module_bufdesc_t *mdesc);
void *nm_mod_buf_at(size_t len, struct nm_module_bufdesc_t *mdesc);
void *nm_mod_buf_reat(size_t len, void *old_buf, struct nm_module_bufdesc_t *mdesc);

struct nm_module_bufdesc_t *nm_mod_bufdesc_at(int shmkey);
void nm_mod_bufdesc_dt(struct nm_module_bufdesc_t *bufdesc);

#endif /* _NM_MODSHM_H_ */

