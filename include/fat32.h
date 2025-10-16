#ifndef FAT32_H
#define FAT32_H

#include <stdint.h>
#include <stdio.h>

/**
 * @file fat32.h
 * @brief FAT32 filesystem structures, constants, and API for the FAT32 emulator.
 *
 * This header defines the data structures, constants, and function prototypes
 * necessary for working with a FAT32 filesystem in a file-backed emulator.
 */

#define SECTOR_SIZE 512
#define CLUSTER_SIZE 4096  /**< 8 sectors per cluster */
#define TOTAL_SECTORS 40960 /**< 20 MB disk size */
#define RESERVED_SECTORS 32
#define FAT_COUNT 2
#define ROOT_CLUSTER 2

/**
 * @brief FAT32 Boot Sector structure.
 *
 * Represents the on-disk boot sector of a FAT32 filesystem.
 * Packed to match the exact layout on disk.
 */
typedef struct {
    uint8_t jump[3];
    char oem[8];
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t fat_count;
    uint16_t root_entries;
    uint16_t total_sectors_16;
    uint8_t media_type;
    uint16_t fat_size_16;
    uint16_t sectors_per_track;
    uint16_t head_count;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    uint32_t fat_size_32;
    uint16_t ext_flags;
    uint16_t fs_version;
    uint32_t root_cluster;
    uint16_t fs_info;
    uint16_t backup_boot;
    uint8_t reserved[12];
    uint8_t drive_number;
    uint8_t reserved1;
    uint8_t boot_signature;
    uint32_t volume_id;
    char volume_label[11];
    char fs_type[8];
    uint8_t boot_code[420];
    uint16_t signature;
} __attribute__((packed)) Fat32BootSector;

/**
 * @brief FAT32 directory entry structure.
 *
 * Represents a single file or directory entry in a FAT32 directory.
 */
typedef struct {
    char name[11];
    uint8_t attr;
    uint8_t nt_res;
    uint8_t crt_time_tenth;
    uint16_t crt_time;
    uint16_t crt_date;
    uint16_t lst_acc_date;
    uint16_t cluster_high;
    uint16_t wrt_time;
    uint16_t wrt_date;
    uint16_t cluster_low;
    uint32_t file_size;
} __attribute__((packed)) DirEntry;

/** File attribute flags for DirEntry */
#define ATTR_READ_ONLY 0x01
#define ATTR_HIDDEN 0x02
#define ATTR_SYSTEM 0x04
#define ATTR_VOLUME_ID 0x08
#define ATTR_DIRECTORY 0x10
#define ATTR_ARCHIVE 0x20
#define ATTR_LONG_NAME 0x0F

/**
 * @brief FAT32 emulator context structure.
 *
 * Stores all relevant information about the current state of the
 * emulated FAT32 filesystem.
 */
typedef struct {
    FILE* disk_file;         /**< File pointer to the disk image */
    char* disk_path;         /**< Path to the disk image file */
    uint32_t fat_start;      /**< Starting sector of the FAT */
    uint32_t data_start;     /**< Starting sector of the data region */
    uint32_t fat_size;       /**< Number of sectors in a FAT */
    uint32_t total_clusters; /**< Total number of clusters */
    char current_path[256];  /**< Current working directory path */
    uint32_t current_cluster; /**< Cluster number of the current directory */
} Fat32Context;

/** @name FAT32 Core Functions */
//@{
int fat32_init(Fat32Context* ctx, const char* disk_path);
int fat32_format(Fat32Context* ctx);
int fat32_mkdir(Fat32Context* ctx, const char* name);
int fat32_touch(Fat32Context* ctx, const char* name);
int fat32_cd(Fat32Context* ctx, const char* path);
int fat32_ls(Fat32Context* ctx, const char* path);
void fat32_cleanup(Fat32Context* ctx);
int fat32_is_valid(Fat32Context* ctx);
//@}

/** @name FAT32 Utility Functions */
//@{
uint32_t fat32_get_cluster_from_entry(const DirEntry* entry);
void fat32_set_cluster_to_entry(DirEntry* entry, uint32_t cluster);
void fat32_format_name(const char* name, char* formatted_name);
int fat32_read_sector(Fat32Context* ctx, uint32_t sector, void* buffer);
int fat32_write_sector(Fat32Context* ctx, uint32_t sector, const void* buffer);
uint32_t fat32_get_fat_entry(Fat32Context* ctx, uint32_t cluster);
int fat32_set_fat_entry(Fat32Context* ctx, uint32_t cluster, uint32_t value);
uint32_t fat32_find_free_cluster(Fat32Context* ctx);
int fat32_clear_cluster(Fat32Context* ctx, uint32_t cluster);
int fat32_read_cluster(Fat32Context* ctx, uint32_t cluster, void* buffer);
int fat32_write_cluster(Fat32Context* ctx, uint32_t cluster, const void* buffer);
//@}

#endif // FAT32_H
