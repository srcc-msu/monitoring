#include <stdlib.h>

#include "proto_err.h"


#define HM_PROTO_ERR_OK_STR		"OK"
#define HM_PROTO_ERR_PARSE_STR		"Parsing error"
#define HM_PROTO_ERR_SERV_STR		"Server error"
#define HM_PROTO_ERR_METHOD_STR		"Unknown method"
#define HM_PROTO_ERR_RESOURCE_STR	"Insufficient system resources"


const char *hmp_proto_errstr(int code){
	switch (code){
	case HM_PROTO_ERR_OK: return HM_PROTO_ERR_OK_STR;
	case HM_PROTO_ERR_PARSE: return HM_PROTO_ERR_PARSE_STR;
	case HM_PROTO_ERR_SERV: return HM_PROTO_ERR_SERV_STR;
	case HM_PROTO_ERR_METHOD: return HM_PROTO_ERR_METHOD_STR;
	case HM_PROTO_ERR_RESOURCE: return HM_PROTO_ERR_RESOURCE_STR;
	}

	return NULL;
}

