#include "fat.h"
#include "ide.h"
#include <stddef.h>

// Global variables
static char boot_sector[512];
static char root_directory_region[512 * 32];
static char fat_table[512 * 16];
static struct boot_sector *bs = 0;
static int root_dir_region_start = 0;
static int data_region_start = 0;

// FAT filesystem starts at sector 2048 (1MB offset) due to partition table
#define FAT_OFFSET 2048

/**
 * fatInit - Initialize FAT filesystem driver
 * Reads the boot sector and FAT table into memory
 * Returns: 0 on success, -1 on failure
 */
int fatInit(void) {
    int result;
    
    // Read boot sector from disk (FAT starts at sector 2048 due to partition table)
    result = ata_lba_read(FAT_OFFSET, (unsigned char*)boot_sector, 1);
    if (result != 0) {
        return -1;  // Disk read failed
    }
    
    bs = (struct boot_sector*)boot_sector;
    
    // Validate boot signature
    if (bs->boot_signature != 0xAA55) {
        return -2;  // Boot signature invalid
    }
    
    // Calculate root directory start sector (relative to FAT start)
    root_dir_region_start = FAT_OFFSET + bs->num_reserved_sectors + 
                           bs->num_fat_tables * bs->num_sectors_per_fat;
    
    // Calculate data region start sector
    int root_dir_sectors = ((bs->num_root_dir_entries * 32) + 511) / 512;
    data_region_start = root_dir_region_start + root_dir_sectors;
    
    // Read FAT table into memory
    result = ata_lba_read(FAT_OFFSET + bs->num_reserved_sectors, (unsigned char*)fat_table, 
                         bs->num_sectors_per_fat);
    if (result != 0) {
        return -3;  // FAT read failed
    }
    
    // Read root directory region into memory
    result = ata_lba_read(root_dir_region_start, (unsigned char*)root_directory_region, 
                         root_dir_sectors);
    if (result != 0) {
        return -4;  // Root directory read failed
    }
    
    return 0;
}

// Helper: Extract readable filename from RDE
static void extract_filename(struct root_directory_entry *rde, char *fname) {
    int k = 0;
    
    // Copy filename (stop at space)
    while (k < 8 && rde->file_name[k] != ' ') {
        fname[k] = rde->file_name[k];
        k++;
    }
    
    // Add extension if it exists
    if (rde->file_extension[0] != ' ') {
        fname[k++] = '.';
        int n = 0;
        while (n < 3 && rde->file_extension[n] != ' ') {
            fname[k++] = rde->file_extension[n++];
        }
    }
    
    fname[k] = '\0';
}

// Helper: Compare strings (case-insensitive)
static int strcmp_nocase(const char *s1, const char *s2) {
    while (*s1 && *s2) {
        char c1 = (*s1 >= 'a' && *s1 <= 'z') ? *s1 - 32 : *s1;
        char c2 = (*s2 >= 'a' && *s2 <= 'z') ? *s2 - 32 : *s2;
        if (c1 != c2) return c1 - c2;
        s1++;
        s2++;
    }
    return *s1 - *s2;
}

/**
 * fatOpen - Open a file in the FAT filesystem
 * Searches root directory for the file and returns file structure
 * Returns: pointer to file structure on success, NULL if not found
 */
struct file* fatOpen(const char *filename) {
    if (!bs) return 0;
    
    struct root_directory_entry *rde = (struct root_directory_entry*)root_directory_region;
    
    // Search through root directory entries
    for (int k = 0; k < bs->num_root_dir_entries; k++) {
        // Skip empty/deleted entries
        if (rde[k].file_name[0] == 0x00 || rde[k].file_name[0] == 0xE5) {
            continue;
        }
        
        // Skip directories and volume labels
        if (rde[k].attribute & (FILE_ATTRIBUTE_SUBDIRECTORY | 0x08)) {
            continue;
        }
        
        // Extract and compare filename
        char fname[16];
        extract_filename(&rde[k], fname);
        
        if (strcmp_nocase(fname, filename) == 0) {
            // Found the file!
            static struct file f;
            f.rde = rde[k];
            f.start_cluster = rde[k].cluster;
            f.current_position = 0;
            f.next = 0;
            f.prev = 0;
            return &f;
        }
    }
    
    return 0; // File not found
}

// Helper: Get next cluster from FAT
static uint16_t get_next_cluster(uint16_t cluster) {
    uint16_t *fat = (uint16_t*)fat_table;
    uint16_t next = fat[cluster];
    return (next >= 0xFFF8) ? 0 : next; // 0 means end of chain
}

/**
 * fatRead - Read data from a file into a buffer
 * Reads up to 'size' bytes from the file into buffer
 * Returns: number of bytes read, or -1 on error
 */
int fatRead(struct file *f, char *buffer, unsigned int size) {
    if (!f || !buffer || !bs) return -1;
    
    // Calculate how much we can read
    uint32_t remaining = f->rde.file_size - f->current_position;
    uint32_t to_read = (size < remaining) ? size : remaining;
    
    if (to_read == 0) return 0;
    
    uint32_t bytes_read = 0;
    uint16_t cluster = f->start_cluster;
    uint32_t cluster_size = bs->num_sectors_per_cluster * bs->bytes_per_sector;
    char cluster_buf[CLUSTER_SIZE];
    
    // Skip to current position in file
    uint32_t skip = f->current_position;
    while (skip >= cluster_size && cluster != 0) {
        cluster = get_next_cluster(cluster);
        skip -= cluster_size;
    }
    
    // Read clusters and copy data to buffer
    while (bytes_read < to_read && cluster != 0) {
        // Read cluster from disk (cluster 2 is first data cluster)
        uint32_t sector = data_region_start + (cluster - 2) * bs->num_sectors_per_cluster;
        
        if (ata_lba_read(sector, (unsigned char*)cluster_buf, bs->num_sectors_per_cluster) != 0) {
            return -1;
        }
        
        // Copy from cluster to buffer
        uint32_t offset = (bytes_read == 0) ? skip : 0;
        uint32_t available = cluster_size - offset;
        uint32_t copy = (to_read - bytes_read < available) ? (to_read - bytes_read) : available;
        
        for (uint32_t i = 0; i < copy; i++) {
            buffer[bytes_read++] = cluster_buf[offset + i];
        }
        
        f->current_position += copy;
        
        // Get next cluster if we need more data
        if (bytes_read < to_read) {
            cluster = get_next_cluster(cluster);
        }
    }
    
    return bytes_read;
}
