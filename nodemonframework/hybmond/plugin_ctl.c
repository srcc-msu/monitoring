#define _GNU_SOURCE

#include <dlfcn.h>
#include <sys/queue.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "plugins_api.h"
#include "plugin_err.h"
#include "proto_err.h"


typedef void (*process_func_t)(char *, int);

struct hm_plugin_desc {
	LIST_ENTRY(hm_plugin_desc) plugin_desc;
	char *plugin_name;
	void *plugin_handler;
	process_func_t process_func;
};


LIST_HEAD(plugin_head_t, hm_plugin_desc) hm_plugin_list;

// ???
//static struct plugin_head_t *hm_plugin_head;


void hm_init_plugins(){
	LIST_INIT(&hm_plugin_list);
}


void hm_cleanup_plugins(){
	while(hm_plugin_list.lh_first){
		free(hm_plugin_list.lh_first->plugin_name);
		dlclose(hm_plugin_list.lh_first->plugin_handler);
		LIST_REMOVE(hm_plugin_list.lh_first, plugin_desc);
	}
}


static void hm_execute_plugin_req(struct hm_plugin_desc *hpd, char *param, int fd){
	hpd->process_func(param, fd);
}


static struct hm_plugin_desc *hm_load_plugin(char *name){
	char *so_name;
	struct hm_plugin_desc *pd;

	if (asprintf(&so_name, PLUGINS_LOCATION "lib" PLUGIN_PREFIX "%s.so", name) < 0)
		return NULL;

	if (!(pd = (struct hm_plugin_desc *) malloc(sizeof(struct hm_plugin_desc))))
		goto err_mem;

	if (!(pd->plugin_handler = dlopen(so_name, RTLD_LOCAL | RTLD_NOW))){
//		char *errc = dlerror();
//		write(1, errc, strlen(errc));
		goto err_dl;
	}

	free(so_name);
	if (asprintf(&so_name, "%s_process", name) < 0)
		goto err_close;

	if (!(pd->process_func = (process_func_t) dlsym(pd->plugin_handler, so_name)))
		goto err_close;

	if (!(pd->plugin_name = strdup(name)))
		goto err_close;

	LIST_INSERT_HEAD(&hm_plugin_list, pd, plugin_desc);

	return pd;

err_close:
	dlclose(pd->plugin_handler);
err_dl:
	free(pd);
err_mem:
	free(so_name);
	return NULL;
}


int hm_request_plugin(char *name, char *param, int fd){
	struct hm_plugin_desc *pd;

	if (!(name && param)){
		hmp_write_error(HMP_SRC_ERR_PLUGIN, HM_PLUGIN_ERR_INVALID, fd);
		return -1;
	}

	for (pd = hm_plugin_list.lh_first; pd; pd = pd->plugin_desc.le_next){
		if (!strcmp(name, pd->plugin_name)){
			hm_execute_plugin_req(pd, param, fd);
			return 0;
		}
	}

	if (!(pd = hm_load_plugin(name))){
		hmp_write_error(HMP_SRC_ERR_PLUGIN, HM_PLUGIN_ERR_NOT_FOUND, fd);
		return -1;
	}

	hm_execute_plugin_req(pd, param, fd);
	return 0;
}


