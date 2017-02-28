#ifndef _HM_PROTO_ERR_H_
#define _HM_PROTO_ERR_H_


#define HM_PROTO_ERR_OK		200
#define HM_PROTO_ERR_PARSE	400
#define HM_PROTO_ERR_SERV	500
#define HM_PROTO_ERR_METHOD	501
#define HM_PROTO_ERR_RESOURCE	503


const char *hmp_proto_errstr(int);


#endif
