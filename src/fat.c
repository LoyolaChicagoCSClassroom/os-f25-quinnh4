#include "fat.h"
#include "ide.h"
#include "rprintf.h"
#include "io.h"

#define FAT_PARTITION_OFFSET 2048


// Global variables for FAT filesystem state
static struct boot_sector *bs;
static char bootSector[SECTOR_SIZE];
static char fat_table[8 * SECTOR_SIZE];
static unsigned int root_sector;
static unsigned int data_region_start;

// Wrapper function to use IDE driver with our interface
static int sd_readblock(uint32_t sector, char *buffer, uint32_t nsectors) {
    return ata_lba_read(sector, (unsigned char *)buffer, nsectors);
}

// Helper function: fixed string compare
static int str_compare(const char *s1, const char *s2, int len) {
    for (int i = 0; i < len; i++) {
        if (s1[i] != s2[i]) return 0;
    }
    return 1;
}

// Helper function to convert char to uppercase
static char to_upper(char c) {
    if (c >= 'a' && c <= 'z') {
        return c - 'a' + 'A';
    }
    return c;
}

// Helper function to extract filename from RDE
static void extract_filename(struct root_directory_entry *rde, char *fname) {
    int k = 0;
    while (k < 8 && rde->file_name[k] != ' ') {
        fname[k] = rde->file_name[k];
        k++;
    }

    if (rde->file_extension[0] != ' ') {
        fname[k++] = '.';
        int n = 0;
        while (n < 3 && rde->file_extension[n] != ' ') {
            fname[k++] = rde->file_extension[n];
            n++;
        }
    }

    fname[k] = '\0';
}

// Helper function to get next cluster from FAT table
static uint16_t get_next_cluster(uint16_t current_cluster) {
    // Check if FAT16 (based on fs_type in boot sector)
    if (bs->fs_type[3] == '1' && bs->fs_type[4] == '6') {
        uint16_t *fat16 = (uint16_t *)fat_table;
        return fat16[current_cluster];
    } else {
        // FAT12: 1.5 bytes per entry
        uint32_t fat_offset = current_cluster + (current_cluster / 2);
        uint16_t table_value = *(uint16_t *)&fat_table[fat_offset];
        if (current_cluster & 1) {
            table_value >>= 4;
        } else {
            table_value &= 0x0FFF;
        }
        return table_value;
    }
}

/**
 * fatInit - Initialize the FAT filesystem driver
 */
int fatInit(void) {
    extern int putc(int);

    // Read boot sector from FAT partition start
    if (sd_readblock(FAT_PARTITION_OFFSET, bootSector, 1) != 0) {
        esp_printf((func_ptr)putc, "ERROR: Failed to read boot sector\n");
        return -1;
    }

    bs = (struct boot_sector *)bootSector;

    // Validate boot signature
    if (bs->boot_signature != 0xAA55) {
        esp_printf((func_ptr)putc, "ERROR: Invalid boot signature: 0x%x\n", bs->boot_signature);
        return -1;
    }

    // Validate filesystem type
    if (!str_compare(bs->fs_type, "FAT12", 5) && !str_compare(bs->fs_type, "FAT16", 5)) {
        esp_printf((func_ptr)putc, "ERROR: Invalid FS type: %.8s\n", bs->fs_type);
        return -1;
    }

    esp_printf((func_ptr)putc, "FAT Filesystem initialized:\n");
    esp_printf((func_ptr)putc, "  FS Type: %.8s\n", bs->fs_type);
    esp_printf((func_ptr)putc, "  Bytes per sector: %d\n", bs->bytes_per_sector);
    esp_printf((func_ptr)putc, "  Sectors per cluster: %d\n", bs->num_sectors_per_cluster);
    esp_printf((func_ptr)putc, "  Reserved sectors: %d\n", bs->num_reserved_sectors);
    esp_printf((func_ptr)putc, "  FAT tables: %d\n", bs->num_fat_tables);
    esp_printf((func_ptr)putc, "  Root dir entries: %d\n", bs->num_root_dir_entries);

    // Read FAT table into memory
    uint32_t fat_start = FAT_PARTITION_OFFSET + bs->num_reserved_sectors;
    uint32_t fat_sectors = bs->num_sectors_per_fat;

    if (fat_sectors > 8) fat_sectors = 8;

    if (sd_readblock(fat_start, fat_table, fat_sectors) != 0) {
        esp_printf((func_ptr)putc, "ERROR: Failed to read FAT table\n");
        return -1;
    }

    esp_printf((func_ptr)putc, "  FAT table loaded (%d sectors)\n", fat_sectors);

    // Calculate root directory and data region start
    root_sector = FAT_PARTITION_OFFSET + bs->num_reserved_sectors +
                  (bs->num_fat_tables * bs->num_sectors_per_fat);
    uint32_t root_dir_sectors =
        ((bs->num_root_dir_entries * 32) + (bs->bytes_per_sector - 1)) / bs->bytes_per_sector;
    data_region_start = root_sector + root_dir_sectors;

    esp_printf((func_ptr)putc, "  Root directory at sector: %d\n", root_sector);
    esp_printf((func_ptr)putc, "  Data region starts at sector: %d\n", data_region_start);

    return 0;
}

/**
 * fatOpen - Open a file in the FAT filesystem
 */
struct file* fatOpen(const char *filename) {
    extern int putc(int);
    static struct file open_file;
    static char rde_buffer[SECTOR_SIZE * 32];

    uint32_t root_dir_sectors =
        ((bs->num_root_dir_entries * 32) + (bs->bytes_per_sector - 1)) / bs->bytes_per_sector;

    if (sd_readblock(root_sector, rde_buffer, root_dir_sectors) != 0) {
        esp_printf((func_ptr)putc, "ERROR: Failed to read root directory\n");
        return 0;
    }

    struct root_directory_entry *rde_table = (struct root_directory_entry *)rde_buffer;

    // Prepare search name
    char search_name[9] = {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', '\0'};
    char search_ext[4] = {' ', ' ', ' ', '\0'};

    int i = 0, j = 0;
    while (filename[i] != '\0' && filename[i] != '.' && j < 8) {
        search_name[j++] = to_upper(filename[i++]);
    }
    if (filename[i] == '.') {
        i++;
        j = 0;
        while (filename[i] != '\0' && j < 3) {
            search_ext[j++] = to_upper(filename[i++]);
        }
    }

    esp_printf((func_ptr)putc, "Searching for file: %.8s.%.3s\n", search_name, search_ext);

    // Search entries
    for (int k = 0; k < bs->num_root_dir_entries; k++) {
        struct root_directory_entry *rde = &rde_table[k];

        if (rde->file_name[0] == 0x00) break;
        if (rde->file_name[0] == 0xE5) continue;
        if (rde->attribute & FILE_ATTRIBUTE_SUBDIRECTORY) continue;

        if (str_compare(rde->file_name, search_name, 8) &&
            str_compare(rde->file_extension, search_ext, 3)) {
            char found_name[13];
            extract_filename(rde, found_name);
            esp_printf((func_ptr)putc, "Found file: %s (cluster: %d, size: %d bytes)\n",
                       found_name, rde->cluster, rde->file_size);

            open_file.rde = *rde;
            open_file.start_cluster = rde->cluster;
            open_file.current_position = 0;
            open_file.next = 0;
            open_file.prev = 0;
            return &open_file;
        }
    }

    esp_printf((func_ptr)putc, "ERROR: File not found: %s\n", filename);
    return 0;
}

/**
 * fatRead - Read data from an open file
 */
int fatRead(struct file *f, char *buffer, unsigned int size) {
    extern int putc(int);
    if (!f) {
        esp_printf((func_ptr)putc, "ERROR: Invalid file pointer\n");
        return -1;
    }

    uint32_t bytes_to_read = size;
    if (bytes_to_read > f->rde.file_size)
        bytes_to_read = f->rde.file_size;

    uint32_t bytes_read = 0;
    uint16_t current_cluster = f->start_cluster;
    char cluster_buffer[SECTOR_SIZE * 8];

    uint16_t eof_marker = (bs->fs_type[3] == '1' && bs->fs_type[4] == '6') ? 0xFFF8 : 0xFF8;

    while (bytes_read < bytes_to_read && current_cluster < eof_marker) {
        uint32_t cluster_sector = data_region_start +
                                  ((current_cluster - 2) * bs->num_sectors_per_cluster);

        if (sd_readblock(cluster_sector, cluster_buffer, bs->num_sectors_per_cluster) != 0) {
            esp_printf((func_ptr)putc, "ERROR: Failed to read cluster %d\n", current_cluster);
            return -1;
        }

        uint32_t bytes_in_cluster = bs->num_sectors_per_cluster * bs->bytes_per_sector;
        uint32_t bytes_to_copy = bytes_to_read - bytes_read;
        if (bytes_to_copy > bytes_in_cluster) bytes_to_copy = bytes_in_cluster;

        for (uint32_t i = 0; i < bytes_to_copy; i++) {
            buffer[bytes_read++] = cluster_buffer[i];
        }

        current_cluster = get_next_cluster(current_cluster);
    }

    esp_printf((func_ptr)putc, "Read %d bytes successfully\n", bytes_read);
    return bytes_read;
}

