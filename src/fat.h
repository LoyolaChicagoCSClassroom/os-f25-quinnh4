#ifndef __FAT_H__
#define __FAT_H__

#include <stdint.h>

#define SECTOR_SIZE 512
#define CLUSTER_SIZE 4096
#define SECTORS_PER_CLUSTER (CLUSTER_SIZE / SECTOR_SIZE)
#define FILE_ATTRIBUTE_SUBDIRECTORY 0x10

/*
 * Boot sector structure for FAT FS
 */
struct boot_sector {
    char code[3];
    char oem_name[8];
    uint16_t bytes_per_sector;
    uint8_t num_sectors_per_cluster;
    uint16_t num_reserved_sectors;
    uint8_t num_fat_tables;
    uint16_t num_root_dir_entries;
    uint16_t total_sectors;
    uint8_t media_descriptor;
    uint16_t num_sectors_per_fat;
    uint16_t num_sectors_per_track;
    uint16_t num_heads;
    uint32_t num_hidden_sectors;
    uint32_t total_sectors_in_fs;
    uint8_t logical_drive_num;
    uint8_t reserved;
    uint8_t extended_signature;
    uint32_t serial_number;
    char volume_label[11];
    char fs_type[8];
    char boot_code[448];
    uint16_t boot_signature;
} __attribute__((packed));

/*
 * Root directory entry structure (FAT12/FAT16)
 */
struct root_directory_entry {
    char file_name[8];
    char file_extension[3];
    uint8_t attribute;
    uint8_t reserved;
    uint8_t creation_time_tenths;
    uint16_t creation_time;
    uint16_t creation_date;
    uint16_t access_date;
    uint16_t cluster_high; // FAT32 only (unused for FAT16)
    uint16_t modified_time;
    uint16_t modified_date;
    uint16_t cluster;
    uint32_t file_size;
} __attribute__((packed));

/*
 * File structure for open files
 */
struct file {
    struct file *next;
    struct file *prev;
    struct root_directory_entry rde;
    uint32_t start_cluster;
    uint32_t current_position;
};

// Function prototypes
int fatInit(void);
struct file* fatOpen(const char *filename);
int fatRead(struct file *f, char *buffer, unsigned int size);

#endif

