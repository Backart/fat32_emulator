#include "fat32.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int fat32_read_sector(Fat32Context* ctx, uint32_t sector, void* buffer) {
    if (!ctx || !ctx->disk_file || !buffer) return -1;
    
    if (fseek(ctx->disk_file, sector * SECTOR_SIZE, SEEK_SET) != 0) {
        return -1;
    }
    
    return fread(buffer, SECTOR_SIZE, 1, ctx->disk_file) == 1 ? 0 : -1;
}

int fat32_write_sector(Fat32Context* ctx, uint32_t sector, const void* buffer) {
    if (!ctx || !ctx->disk_file || !buffer) return -1;
    
    if (fseek(ctx->disk_file, sector * SECTOR_SIZE, SEEK_SET) != 0) {
        return -1;
    }
    
    int result = fwrite(buffer, SECTOR_SIZE, 1, ctx->disk_file);
    fflush(ctx->disk_file);
    return result == 1 ? 0 : -1;
}

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

int fat32_set_fat_entry(Fat32Context* ctx, uint32_t cluster, uint32_t value) {
    if (cluster >= ctx->total_clusters) return -1;
    
    value &= 0x0FFFFFFF;
    
    // Update both FAT copies
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

uint32_t fat32_find_free_cluster(Fat32Context* ctx) {
    // Start from cluster 2 (first data cluster)
    for (uint32_t cluster = 2; cluster < ctx->total_clusters; cluster++) {
        uint32_t fat_entry = fat32_get_fat_entry(ctx, cluster);
        printf("Debug: Cluster %d FAT entry: 0x%08X\n", cluster, fat_entry);
        if (fat_entry == 0) {  // Free cluster
            printf("Debug: Found free cluster: %d\n", cluster);
            return cluster;
        }
    }
    printf("Debug: No free clusters found\n");
    return 0;  // No free clusters
}

int fat32_clear_cluster(Fat32Context* ctx, uint32_t cluster) {
    uint8_t zero_buffer[CLUSTER_SIZE] = {0};
    return fat32_write_cluster(ctx, cluster, zero_buffer);
}