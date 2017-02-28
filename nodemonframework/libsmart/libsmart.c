#include <errno.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <scsi/sg.h>

#include <stdlib.h>
#include <strings.h>

#include <libsmart.h>


enum {
	SMART_IDENTIFY,
	SMART_STATUS,
	SMART_VALUES,
	SMART_THRESHOLDS,
	SMART_HEALTH
};


// cdb[0]: ATA PASS THROUGH (16) SCSI command opcode byte (0x85)
// cdb[1]: multiple_count, protocol + extend
// cdb[2]: offline, ck_cond, t_dir, byte_block + t_length
// cdb[3]: features (15:8)
// cdb[4]: features (7:0)
// cdb[5]: sector_count (15:8)
// cdb[6]: sector_count (7:0)
// cdb[7]: lba_low (15:8)
// cdb[8]: lba_low (7:0)
// cdb[9]: lba_mid (15:8)
// cdb[10]: lba_mid (7:0)
// cdb[11]: lba_high (15:8)
// cdb[12]: lba_high (7:0)
// cdb[13]: device
// cdb[14]: (ata) command
// cdb[15]: control (SCSI, leave as zero)
//
// 24 bit lba (from MSB): cdb[12] cdb[10] cdb[8]
// 48 bit lba (from MSB): cdb[11] cdb[9] cdb[7] cdb[12] cdb[10] cdb[8]
//
//
// cdb[0]: ATA PASS THROUGH (12) SCSI command opcode byte (0xa1)
// cdb[1]: multiple_count, protocol + extend
// cdb[2]: offline, ck_cond, t_dir, byte_block + t_length
// cdb[3]: features (7:0)
// cdb[4]: sector_count (7:0)
// cdb[5]: lba_low (7:0)
// cdb[6]: lba_mid (7:0)
// cdb[7]: lba_high (7:0)
// cdb[8]: device
// cdb[9]: (ata) command
// cdb[10]: reserved
// cdb[11]: control (SCSI, leave as zero)
//
//
// ATA Return Descriptor (component of descriptor sense data)
// des[0]: descriptor code (0x9)
// des[1]: additional descriptor length (0xc)
// des[2]: extend (bit 0)
// des[3]: error
// des[4]: sector_count (15:8)
// des[5]: sector_count (7:0)
// des[6]: lba_low (15:8)
// des[7]: lba_low (7:0)
// des[8]: lba_mid (15:8)
// des[9]: lba_mid (7:0)
// des[10]: lba_high (15:8)
// des[11]: lba_high (7:0)
// des[12]: device
// des[13]: status


static long smart_cmd(int fd, unsigned int cmd, void *data){
	long res;
	struct sg_io_hdr sg;
	unsigned char scmd[16] = {0x85, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	unsigned char sbp[32];
	
	unsigned char feat = 0;
	unsigned char ata_cmd = ATA_SMART_CMD;
	unsigned char proto = 4;
	int dir = SG_DXFER_FROM_DEV;
	unsigned int xfer_len = 512;
	int t_len = 2;
	int sec_cnt = 1;
	unsigned char lba_lo = 0;
	unsigned char lba_mid = 0x4f;
	unsigned char lba_hi = 0xc2;
	unsigned long ck_cond = 0;
	
	switch (cmd){
	case SMART_IDENTIFY:
		lba_mid = 0;
		lba_hi = 0;
		ata_cmd = ATA_IDENTIFY_DEVICE;
		break;
	case SMART_STATUS:
	case SMART_HEALTH:
		feat = ATA_SMART_STATUS;
		proto = 3;
		ck_cond = 1;
		t_len = 0;
		xfer_len = 0;
		sec_cnt = 0;
		dir = SG_DXFER_NONE;
		break;
	case SMART_VALUES:
		feat = ATA_SMART_READ_VALUES;
		break;
	case SMART_THRESHOLDS:
		feat = ATA_SMART_READ_THRESHOLDS;
		lba_lo = 1;
		break;
	default:
		return -1;
	}
	
	scmd[1] = proto << 1;
	scmd[2] = (ck_cond << 5) | (t_len) | 0x0c;
	scmd[4] = feat;
	scmd[6] = sec_cnt;
	scmd[8] = lba_lo;
	scmd[10]= lba_mid;
	scmd[12]= lba_hi;
	scmd[14] = ata_cmd;
	
	bzero(sbp, 32);
	bzero(&sg, sizeof(struct sg_io_hdr));
	sg.interface_id = 'S';
	sg.dxfer_direction = dir;
	sg.dxfer_len = xfer_len;
	sg.cmd_len = 16;
	sg.mx_sb_len = 32;
	sg.iovec_count = 0;
	sg.timeout = 6000;
	sg.flags = 0;
	sg.dxferp = data;
	sg.cmdp = scmd;
	sg.sbp = sbp;
	
	res =  ioctl(fd, SG_IO, &sg);
	
	if (sg.status == 0x02){
		if (cmd == SMART_HEALTH){
			if (sbp[17]==0x4f && sbp[19] == 0xc2)
				res = 0; /* HEALTH OK*/
			else res = -1;
		}
	} else if (sg.status != 0)
		res = -1;
	
	return res;
}


int smart_identify(int fd, struct ata_identify_device *ident){
	return smart_cmd(fd, SMART_IDENTIFY, ident);
}


int smart_support(int fd){
	return smart_cmd(fd, SMART_STATUS, NULL);
}


/* returns 0 if smart enbled, otherwise -1 */
int smart_enabled(int fd){
	struct ata_identify_device ident;

	int res = smart_cmd(fd, SMART_IDENTIFY, &ident);
	
	if (!res && ((ident.csf_default >> 14) == 0x01) && (ident.cfs_enable_1 & 0x01)){
		return 0;
	}
	
	return -1;
}


int smart_health(int fd){
	return smart_cmd(fd, SMART_HEALTH, NULL);
}


int smart_values(int fd, struct ata_smart_values *vals){
	return smart_cmd(fd, SMART_VALUES, vals);
}


int smart_thresholds(int fd, struct ata_smart_thresholds_pvt *thrsh){
	return smart_cmd(fd, SMART_THRESHOLDS, thrsh);
}



