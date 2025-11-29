#ifndef __IDE_H__
#define __IDE_H__

/**
 * ata_lba_read - Read sectors from IDE disk using LBA mode
 * @lba: Logical Block Address of sector
 * @buffer: Buffer to store read data
 * @numsectors: Number of sectors to read
 * 
 * Returns: 0 on success, -1 on failure
 * 
 * Note: This function is implemented in assembly (ide.s)
 */
int ata_lba_read(unsigned int lba, unsigned char *buffer, unsigned int numsectors);

#endif
