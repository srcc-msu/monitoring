#ifndef _LIBSMART_H_
#define _LIBSMART_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


/*********** Data from atacmds.h (smartmontools) **************/

#define ASSERT_SIZEOF_STRUCT(s, n) \
  typedef char assert_sizeof_struct_##s[(sizeof(struct s) == (n)) ? 1 : -1]

// Add __attribute__((packed)) if compiler supports it
// because some gcc versions (at least ARM) lack support of #pragma pack()
#ifdef HAVE_ATTR_PACKED
#define ATTR_PACKED __attribute__((packed))
#else
#define ATTR_PACKED
#endif

// ATA Specification Command Register Values (Commands)
#define ATA_IDENTIFY_DEVICE             0xec
#define ATA_IDENTIFY_PACKET_DEVICE      0xa1
#define ATA_SMART_CMD                   0xb0
#define ATA_CHECK_POWER_MODE            0xe5

// ATA Specification Feature Register Values (SMART Subcommands).
// Note that some are obsolete as of ATA-7.
#define ATA_SMART_READ_VALUES           0xd0
#define ATA_SMART_READ_THRESHOLDS       0xd1
#define ATA_SMART_AUTOSAVE              0xd2
#define ATA_SMART_SAVE                  0xd3
#define ATA_SMART_IMMEDIATE_OFFLINE     0xd4
#define ATA_SMART_READ_LOG_SECTOR       0xd5
#define ATA_SMART_WRITE_LOG_SECTOR      0xd6
#define ATA_SMART_WRITE_THRESHOLDS      0xd7
#define ATA_SMART_ENABLE                0xd8
#define ATA_SMART_DISABLE               0xd9
#define ATA_SMART_STATUS                0xda
// SFF 8035i Revision 2 Specification Feature Register Value (SMART
// Subcommand)
#define ATA_SMART_AUTO_OFFLINE          0xdb

// Sector Number values for ATA_SMART_IMMEDIATE_OFFLINE Subcommand
#define OFFLINE_FULL_SCAN               0
#define SHORT_SELF_TEST                 1
#define EXTEND_SELF_TEST                2
#define CONVEYANCE_SELF_TEST            3
#define SELECTIVE_SELF_TEST             4
#define ABORT_SELF_TEST                 127
#define SHORT_CAPTIVE_SELF_TEST         129
#define EXTEND_CAPTIVE_SELF_TEST        130
#define CONVEYANCE_CAPTIVE_SELF_TEST    131
#define SELECTIVE_CAPTIVE_SELF_TEST     132
#define CAPTIVE_MASK                    (0x01<<7)

// Maximum allowed number of SMART Attributes
#define NUMBER_ATA_SMART_ATTRIBUTES     30

// Needed parts of the ATA DRIVE IDENTIFY Structure. Those labeled
// word* are NOT used.
#pragma pack(1)
struct ata_identify_device {
	uint16_t words000_009[10];
	uint8_t  serial_no[20];
	uint16_t words020_022[3];
	uint8_t  fw_rev[8];
	uint8_t  model[40];
	uint16_t words047_079[33];
	uint16_t major_rev_num;
	uint16_t minor_rev_num;
	uint16_t command_set_1;
	uint16_t command_set_2;
	uint16_t command_set_extension;
	uint16_t cfs_enable_1;
	uint16_t word086;
	uint16_t csf_default;
	uint16_t words088_255[168];
} ATTR_PACKED;
#pragma pack()
ASSERT_SIZEOF_STRUCT(ata_identify_device, 512);

/* ata_smart_attribute is the vendor specific in SFF-8035 spec */ 
#pragma pack(1)
struct ata_smart_attribute {
	uint8_t id;
	// meaning of flag bits: see MACROS just below
	// WARNING: MISALIGNED!
	uint16_t flags; 
	uint8_t current;
	uint8_t worst;
	uint8_t raw[6];
	uint8_t reserv;
} ATTR_PACKED;
#pragma pack()
ASSERT_SIZEOF_STRUCT(ata_smart_attribute, 12);

// MACROS to interpret the flags bits in the previous structure.
// These have not been implemented using bitflags and a union, to make
// it portable across bit/little endian and different platforms.

// 0: Prefailure bit

// From SFF 8035i Revision 2 page 19: Bit 0 (pre-failure/advisory bit)
// - If the value of this bit equals zero, an attribute value less
// than or equal to its corresponding attribute threshold indicates an
// advisory condition where the usage or age of the device has
// exceeded its intended design life period. If the value of this bit
// equals one, an attribute value less than or equal to its
// corresponding attribute threshold indicates a prefailure condition
// where imminent loss of data is being predicted.
#define ATTRIBUTE_FLAGS_PREFAILURE(x) (x & 0x01)

// 1: Online bit 

//  From SFF 8035i Revision 2 page 19: Bit 1 (on-line data collection
// bit) - If the value of this bit equals zero, then the attribute
// value is updated only during off-line data collection
// activities. If the value of this bit equals one, then the attribute
// value is updated during normal operation of the device or during
// both normal operation and off-line testing.
#define ATTRIBUTE_FLAGS_ONLINE(x) (x & 0x02)


// The following are (probably) IBM's, Maxtors and  Quantum's definitions for the
// vendor-specific bits:
// 2: Performance type bit
#define ATTRIBUTE_FLAGS_PERFORMANCE(x) (x & 0x04)

// 3: Errorrate type bit
#define ATTRIBUTE_FLAGS_ERRORRATE(x) (x & 0x08)

// 4: Eventcount bit
#define ATTRIBUTE_FLAGS_EVENTCOUNT(x) (x & 0x10)

// 5: Selfpereserving bit
#define ATTRIBUTE_FLAGS_SELFPRESERVING(x) (x & 0x20)


// Last ten bits are reserved for future use

/* ata_smart_values is format of the read drive Attribute command */
/* see Table 34 of T13/1321D Rev 1 spec (Device SMART data structure) for *some* info */
#pragma pack(1)
struct ata_smart_values {
	uint16_t revnumber;
	struct ata_smart_attribute vendor_attributes [NUMBER_ATA_SMART_ATTRIBUTES];
	uint8_t offline_data_collection_status;
	uint8_t self_test_exec_status;  //IBM # segments for offline collection
	uint16_t total_time_to_complete_off_line; // IBM different
	uint8_t vendor_specific_366; // Maxtor & IBM curent segment pointer
	uint8_t offline_data_collection_capability;
	uint16_t smart_capability;
	uint8_t errorlog_capability;
	uint8_t vendor_specific_371;  // Maxtor, IBM: self-test failure checkpoint see below!
	uint8_t short_test_completion_time;
	uint8_t extend_test_completion_time;
	uint8_t conveyance_test_completion_time;
	uint8_t reserved_375_385[11];
	uint8_t vendor_specific_386_510[125]; // Maxtor bytes 508-509 Attribute/Threshold Revision #
	uint8_t chksum;
} ATTR_PACKED;
#pragma pack()
ASSERT_SIZEOF_STRUCT(ata_smart_values, 512);

/* Maxtor, IBM: self-test failure checkpoint byte meaning:
 00 - write test
 01 - servo basic
 02 - servo random
 03 - G-list scan
 04 - Handling damage
 05 - Read scan
*/

/* Vendor attribute of SMART Threshold (compare to ata_smart_attribute above) */
#pragma pack(1)
struct ata_smart_threshold_entry {
	uint8_t id;
	uint8_t threshold;
	uint8_t reserved[10];
} ATTR_PACKED;
#pragma pack()
ASSERT_SIZEOF_STRUCT(ata_smart_threshold_entry, 12);

/* Format of Read SMART THreshold Command */
/* Compare to ata_smart_values above */
#pragma pack(1)
struct ata_smart_thresholds_pvt {
	uint16_t revnumber;
	struct ata_smart_threshold_entry thres_entries[NUMBER_ATA_SMART_ATTRIBUTES];
	uint8_t reserved[149];
	uint8_t chksum;
} ATTR_PACKED;
#pragma pack()
ASSERT_SIZEOF_STRUCT(ata_smart_thresholds_pvt, 512);


/*****************************************************************************/


/* returns 0 if smart enbled, otherwise -1 */
int smart_identify(int fd, struct ata_identify_device *ident);
int smart_support(int fd);
int smart_enabled(int fd);
int smart_health(int fd);
int smart_values(int fd, struct ata_smart_values *vals);
int smart_thresholds(int fd, struct ata_smart_thresholds_pvt *thrsh);


#ifdef __cplusplus
}
#endif


#endif

