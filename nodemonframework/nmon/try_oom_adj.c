#include <stdio.h>
#include <stdlib.h>


#define OOM_ADJ_FILE	"/proc/self/oom_adj"


void try_oom_adj(const char *oom_var_name){
	char *oom_val;
	FILE *oom_file;
	
	if (!(oom_var_name && oom_var_name[0]))
		return;
	
	if (!(oom_val = getenv(oom_var_name)))
		return;
	
	if (!(oom_file = fopen(OOM_ADJ_FILE, "w")))
		return;
	
	fprintf(oom_file, "%s\n", oom_val);
	
	fclose(oom_file);
}


