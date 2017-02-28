#include <sys/types.h>
#include <unistd.h>

#include <stdint.h>
#include <stdlib.h>


static uint32_t nms_crc32(int fd){
	uint8_t buf;
	uint32_t crc;
	uint32_t i, j;
	uint32_t *crc_tbl;
	off_t lim_off;

	if (!(crc_tbl = malloc(256 * sizeof(uint32_t))))
		return 0xFFFFFFFF;
	
	/* building CRC table */
	for (i=0; i<256; i++){
		crc = i;
		
		for (j=0; j<8; j++)
			crc = crc & 1 ? (crc >> 1) ^ 0xEDB88320 : crc >> 1;
		
		crc_tbl[i] = crc;
	}
	
	lim_off = lseek(fd, -sizeof(uint32_t), SEEK_END);
	lseek(fd, 0, SEEK_SET);
	
	//Calculating CRC
	crc = 0xFFFFFFFF;
	while (read(fd, &buf, sizeof(uint8_t)) > 0){
		crc = crc_tbl[(crc ^ buf) & 0xFF] ^ (crc >> 8);
		
		if (lim_off == lseek(fd, 0, SEEK_CUR))
			break;
	}
	
	crc ^= 0xFFFFFFFF;
	
	free(crc_tbl);
	return crc;
}


int nms_valid_crc32(int fd){
	int unused;
	uint32_t r_crc;
	uint32_t c_crc = nms_crc32(fd);
	
	lseek(fd, -sizeof(uint32_t), SEEK_END);
	unused = read(fd, &r_crc, sizeof(uint32_t));
	
	return r_crc == c_crc;
}


#ifdef NMS_CRC32_APP
static int nms_write_crc32(int fd){
	uint32_t crc = nms_crc32(fd);
	
	lseek(fd, -sizeof(uint32_t), SEEK_END);
	
	return write(fd, &crc, sizeof(uint32_t));
}


#include <fcntl.h>

#include <stdio.h>
#include <string.h>


static void print_usage(){
		fprintf(stderr, APPNAME " <option> <file>\n"
				"\t-v --validate\t: validate file checksum\n"
				"\t-s --sign\t: sign file with checksum\n\n"
			);
}


int main(int ac, char *av[]){
	int fd;

	if (ac != 3){
		print_usage();
		return 1;
	}
	
	if (!strcmp(av[1], "-v") || !strcmp(av[1], "--validate")){
		if ((fd = open(av[2], O_RDONLY)) < 0)
			return 2;
		
		if (nms_valid_crc32(fd)){
			printf("File %s has valid CRC32\n", av[2]);
		} else
			printf("File %s has invalid CRC32\n", av[2]);
		
	} else if (!strcmp(av[1], "-s") || !strcmp(av[1], "--sign")){
		if ((fd = open(av[2], O_RDWR)) < 0)
			return 2;
		
		if (nms_write_crc32(fd) > 0)
			printf("File %s successfuly signed with CRC32\n", av[2]);
		else 
			printf("Error signing file %s\n", av[2]);
	} else {
		print_usage();
		return 1;
	}
	
	close(fd);
	
	return 0;
}

#endif

