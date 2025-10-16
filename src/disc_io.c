/**
 * @file disc_io.c
 * @brief Low-level disk I/O operations for the FAT32 emulator.
 *
 * This module provides functions to read and write sectors and clusters
 * on the FAT32 disk image, as well as functions to manipulate the FAT table.
 */

#include "fat32.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/**
 * @brief Reads a single 512-byte sector from the disk.
 *
 * @param ctx Pointer to FAT32 context.
 * @param sector Sector number to read.
 * @param buffer Pointer to a buffer of at least SECTOR_SIZE bytes.
 * @return 0 on success, -1 on failure.
 */
int fat32_read_sector(Fat32Context* ctx, uint32_t sector, void* buffer) {
    if (!ctx || !ctx->disk_file || !buffer) return -1;
    
    if (fseek(ctx->disk_file, sector * SECTOR_SIZE, SEEK_SET) != 0) {
        return -1;
    }
    
    return fread(buffer, SECTOR_SIZE, 1, ctx->disk_file) == 1 ? 0 : -1;
}

/**
 * @brief Writes a single 512-byte sector to the disk.
 *
 * @param ctx Pointer to FAT32 context.
 * @param sector Sector number to write.
 * @param buffer Pointer to the data buffer to write (SECTOR_SIZE bytes).
 * @return 0 on success, -1 on failure.
 */
int fat32_write_sector(Fat32Context* ctx, uint32_t sector, const void* buffer) {
    if (!ctx || !ctx->disk_file || !buffer) return -1;
    
    if (fseek(ctx->disk_file, sector * SECTOR_SIZE, SEEK_SET) != 0) {
        return -1;
    }
    
    int result = fwrite(buffer, SECTOR_SIZE, 1, ctx->disk_file);
    fflush(ctx->disk_file);
    return result == 1 ? 0 : -1;
}

/**
 * @brief Reads an entire cluster from the disk.
 *
 * @param ctx Pointer to FAT32 context.
 * @param cluster Cluster number to read (>=2).
 * @param buffer Pointer to a buffer of at least CLUSTER_SIZE bytes.
 * @return 0 on success, -1 on failure.
 */
int fat32_read_cluster(Fat32Context* ctx, uint32_t cluster, void* buffer) {
    if (cluster < 2) return -1;
    
    uint32_t sector = ctx->data_start + (cluster - 2) * (CLUSTER_SIZE / SECTOR_SIZE);
    uint8_t* buf = (uint8_t*)buffer;
    
    for (int i = 0; i < CLUSTER_SIZE / SECTOR_SIZE; i++) {
        if (fat32_read_sector(ctx, sector + i, buf + i * SECTOR_SIZE) != 0) {
            return -1;
        }
    }
    return 0;
}

/**
 * @brief Writes an entire cluster to the disk.
 *
 * @param ctx Pointer to FAT32 context.
 * @param cluster Cluster number to write (>=2).
 * @param buffer Pointer to a buffer containing CLUSTER_SIZE bytes of data.
 * @return 0 on success, -1 on failure.
 */
int fat32_write_cluster(Fat32Context* ctx, uint32_t cluster, const void* buffer) {
    if (cluster < 2) return -1;
    
    uint32_t sector = ctx->data_start + (cluster - 2) * (CLUSTER_SIZE / SECTOR_SIZE);
    const uint8_t* buf = (const uint8_t*)buffer;
    
    for (int i = 0; i < CLUSTER_SIZE / SECTOR_SIZE; i++) {
        if (fat32_write_sector(ctx, sector + i, buf + i * SECTOR_SIZE) != 0) {
            return -1;
        }
    }
    return 0;
}

/**
 * @brief Reads a FAT entry for a given cluster.
 *
 * @param ctx Pointer to FAT32 context.
 * @param cluster Cluster number.
 * @return FAT entry value (0x0FFFFFFF if invalid cluster).
 */
uint32_t fat32_get_fat_entry(Fat32Context* ctx, uint32_t cluster) {
    if (cluster >= ctx->total_clusters) return 0x0FFFFFFF;
    
    uint32_t fat_sector = ctx->fat_start + (cluster * 4) / SECTOR_SIZE;
    uint32_t fat_offset = (cluster * 4) % SECTOR_SIZE;
    
    uint8_t sector[SECTOR_SIZE];
    if (fat32_read_sector(ctx, fat_sector, sector) != 0) {
        return 0x0FFFFFFF;
    }
    
    uint32_t* fat_entry = (uint32_t*)(sector + fat_offset);
    return *fat_entry & 0x0FFFFFFF;
}

/**
 * @brief Updates a FAT entry for a given cluster.
 *
 * @param ctx Pointer to FAT32 context.
 * @param cluster Cluster number.
 * @param value New FAT entry value.
 * @return 0 on success, -1 on failure.
 */
int fat32_set_fat_entry(Fat32Context* ctx, uint32_t cluster, uint32_t value) {
    if (cluster >= ctx->total_clusters) return -1;
    
    value &= 0x0FFFFFFF;
    
    for (int fat_copy = 0; fat_copy < FAT_COUNT; fat_copy++) {
        uint32_t fat_sector = ctx->fat_start + (fat_copy * ctx->fat_size) + (cluster * 4) / SECTOR_SIZE;
        uint32_t fat_offset = (cluster * 4) % SECTOR_SIZE;
        
        uint8_t sector[SECTOR_SIZE];
        if (fat32_read_sector(ctx, fat_sector, sector) != 0) {
            return -1;
        }
        
        uint32_t* fat_entry = (uint32_t*)(sector + fat_offset);
        *fat_entry = (*fat_entry & 0xF0000000) | value;
        
        if (fat32_write_sector(ctx, fat_sector, sector) != 0) {
            return -1;
        }
    }
    return 0;
}

/**
 * @brief Finds the first free cluster in the FAT.
 *
 * @param ctx Pointer to FAT32 context.
 * @return Cluster number of the first free cluster, or 0 if none found.
 */
uint32_t fat32_find_free_cluster(Fat32Context* ctx) {
    for (uint32_t cluster = 2; cluster < ctx->total_clusters; cluster++) {
        uint32_t fat_entry = fat32_get_fat_entry(ctx, cluster);
        if (fat_entry == 0) {  /**< Free cluster found */
            return cluster;
        }
    }
    return 0;  /**< No free clusters */
}

/**
 * @brief Clears all data in a cluster by writing zeros.
 *
 * @param ctx Pointer to FAT32 context.
 * @param cluster Cluster number to clear.
 * @return 0 on success, -1 on failure.
 */
int fat32_clear_cluster(Fat32Context* ctx, uint32_t cluster) {
    uint8_t zero_buffer[CLUSTER_SIZE] = {0};
    return fat32_write_cluster(ctx, cluster, zero_buffer);
}
