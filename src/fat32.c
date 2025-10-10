#include "fat32.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>  // Add this for ftruncate and fileno

int fat32_init(Fat32Context* ctx, const char* disk_path) {
    if (!ctx || !disk_path) return -1;
    
    memset(ctx, 0, sizeof(Fat32Context));
    ctx->disk_path = malloc(strlen(disk_path) + 1);
    if (!ctx->disk_path) return -1;
    strcpy(ctx->disk_path, disk_path);
    strcpy(ctx->current_path, "/");
    ctx->current_cluster = ROOT_CLUSTER;
    
    // Try to open existing file
    ctx->disk_file = fopen(disk_path, "r+b");
    if (ctx->disk_file) {
        // Check if it's a valid FAT32 filesystem
        if (fat32_is_valid(ctx) == 0) {
            return 0;  // Valid filesystem
        }
        fclose(ctx->disk_file);
    }
    
    // Create new file
    ctx->disk_file = fopen(disk_path, "w+b");
    if (!ctx->disk_file) {
        free(ctx->disk_path);
        return -1;
    }
    
    // Create 20MB file by writing zeros
    uint8_t zero_sector[SECTOR_SIZE] = {0};
    for (uint32_t i = 0; i < TOTAL_SECTORS; i++) {
        if (fwrite(zero_sector, SECTOR_SIZE, 1, ctx->disk_file) != 1) {
            fclose(ctx->disk_file);
            free(ctx->disk_path);
            return -1;
        }
    }
    fflush(ctx->disk_file);
    
    return 0;
}

void fat32_cleanup(Fat32Context* ctx) {
    if (ctx) {
        if (ctx->disk_file) {
            fclose(ctx->disk_file);
        }
        free(ctx->disk_path);
    }
}

int fat32_is_valid(Fat32Context* ctx) {
    if (!ctx || !ctx->disk_file) return -1;
    
    Fat32BootSector bs;
    if (fat32_read_sector(ctx, 0, &bs) != 0) {
        return -1;
    }
    
    // Check signature
    if (bs.signature != 0xAA55) {
        return -1;
    }
    
    // Check FS type
    if (strncmp(bs.fs_type, "FAT32", 5) != 0) {
        return -1;
    }
    
    // Calculate FAT32 parameters
    ctx->fat_size = bs.fat_size_32;
    ctx->fat_start = bs.reserved_sectors;
    ctx->data_start = bs.reserved_sectors + (bs.fat_count * bs.fat_size_32);
    ctx->total_clusters = (TOTAL_SECTORS - ctx->data_start) / bs.sectors_per_cluster;
    
    return 0;
}

int fat32_format(Fat32Context* ctx) {
    if (!ctx || !ctx->disk_file) return -1;
    
    // Create boot sector
    Fat32BootSector bs;
    memset(&bs, 0, sizeof(bs));
    
    // Basic boot sector setup
    bs.jump[0] = 0xEB;
    bs.jump[1] = 0x58;
    bs.jump[2] = 0x90;
    memcpy(bs.oem, "MSWIN4.1", 8);
    bs.bytes_per_sector = SECTOR_SIZE;
    bs.sectors_per_cluster = CLUSTER_SIZE / SECTOR_SIZE;
    bs.reserved_sectors = RESERVED_SECTORS;
    bs.fat_count = FAT_COUNT;
    bs.root_entries = 0;  // FAT32 has root in data area
    bs.total_sectors_16 = 0;
    bs.media_type = 0xF8;
    bs.fat_size_16 = 0;
    bs.sectors_per_track = 32;
    bs.head_count = 64;
    bs.hidden_sectors = 0;
    bs.total_sectors_32 = TOTAL_SECTORS;
    bs.fat_size_32 = 256;  // Calculated for 20MB
    bs.ext_flags = 0;
    bs.fs_version = 0;
    bs.root_cluster = ROOT_CLUSTER;
    bs.fs_info = 1;
    bs.backup_boot = 6;
    bs.drive_number = 0x80;
    bs.boot_signature = 0x29;
    bs.volume_id = 0x12345678;
    memcpy(bs.volume_label, "NO NAME    ", 11);
    memcpy(bs.fs_type, "FAT32   ", 8);
    bs.signature = 0xAA55;
    
    // Write boot sector
    if (fat32_write_sector(ctx, 0, &bs) != 0) {
        return -1;
    }
    
    // Calculate parameters
    ctx->fat_size = bs.fat_size_32;
    ctx->fat_start = bs.reserved_sectors;
    ctx->data_start = bs.reserved_sectors + (bs.fat_count * bs.fat_size_32);
    ctx->total_clusters = (TOTAL_SECTORS - ctx->data_start) / bs.sectors_per_cluster;
    
    // Initialize FAT tables
    uint8_t fat_sector[SECTOR_SIZE] = {0};
    
    // Mark first two FAT entries
    uint32_t* fat_entries = (uint32_t*)fat_sector;
    fat_entries[0] = 0x0FFFFFF8;  // Media type
    fat_entries[1] = 0x0FFFFFFF;  // EOF
    
    // Write first sector of both FATs
    for (int i = 0; i < FAT_COUNT; i++) {
        if (fat32_write_sector(ctx, ctx->fat_start + i * ctx->fat_size, fat_sector) != 0) {
            return -1;
        }
    }
    
    // Clear remaining FAT sectors
    memset(fat_sector, 0, SECTOR_SIZE);
    for (int fat_copy = 0; fat_copy < FAT_COUNT; fat_copy++) {
        for (uint32_t sector = 1; sector < ctx->fat_size; sector++) {
            if (fat32_write_sector(ctx, ctx->fat_start + fat_copy * ctx->fat_size + sector, fat_sector) != 0) {
                return -1;
            }
        }
    }
    
    // Create root directory
    uint8_t root_cluster[CLUSTER_SIZE] = {0};
    DirEntry* entries = (DirEntry*)root_cluster;
    
    // Create "." entry
    memcpy(entries[0].name, ".          ", 11);
    entries[0].attr = ATTR_DIRECTORY;
    fat32_set_cluster_to_entry(&entries[0], ROOT_CLUSTER);
    
    // Create ".." entry
    memcpy(entries[1].name, "..         ", 11);
    entries[1].attr = ATTR_DIRECTORY;
    fat32_set_cluster_to_entry(&entries[1], 0);  // Root's parent is root
    
    if (fat32_write_cluster(ctx, ROOT_CLUSTER, root_cluster) != 0) {
        return -1;
    }
    
    // Mark root cluster as used
    if (fat32_set_fat_entry(ctx, ROOT_CLUSTER, 0x0FFFFFFF) != 0) {
        return -1;
    }
    
    return 0;
}

void fat32_format_name(const char* name, char* formatted_name) {
    memset(formatted_name, ' ', 11);
    
    // Handle special cases for "." and ".."
    if (strcmp(name, ".") == 0) {
        memcpy(formatted_name, ".          ", 11);
        return;
    }
    if (strcmp(name, "..") == 0) {
        memcpy(formatted_name, "..         ", 11);
        return;
    }
    
    const char* dot = strchr(name, '.');
    if (dot) {
        // File with extension
        int name_len = dot - name;
        if (name_len > 8) name_len = 8;
        int ext_len = strlen(dot + 1);
        if (ext_len > 3) ext_len = 3;
        
        memcpy(formatted_name, name, name_len);
        memcpy(formatted_name + 8, dot + 1, ext_len);
    } else {
        // Directory or file without extension
        int name_len = strlen(name);
        if (name_len > 11) name_len = 11;
        memcpy(formatted_name, name, name_len);
    }
    
    // Remove conversion to uppercase to preserve case
    // for (int i = 0; i < 11; i++) {
    //     formatted_name[i] = toupper(formatted_name[i]);
    // }
}

uint32_t fat32_get_cluster_from_entry(const DirEntry* entry) {
    return ((uint32_t)entry->cluster_high << 16) | entry->cluster_low;
}

void fat32_set_cluster_to_entry(DirEntry* entry, uint32_t cluster) {
    entry->cluster_high = (cluster >> 16) & 0xFFFF;
    entry->cluster_low = cluster & 0xFFFF;
}

int fat32_mkdir(Fat32Context* ctx, const char* name) {
    if (!ctx || !name || strlen(name) == 0) return -1;
    
    // Read current directory
    uint8_t cluster[CLUSTER_SIZE];
    if (fat32_read_cluster(ctx, ctx->current_cluster, cluster) != 0) {
        return -1;
    }
    
    DirEntry* entries = (DirEntry*)cluster;
    int entry_count = CLUSTER_SIZE / sizeof(DirEntry);
    
    // Check if name already exists
    char formatted_name[11];
    fat32_format_name(name, formatted_name);
    
    for (int i = 0; i < entry_count; i++) {
        if (entries[i].name[0] == 0x00) break;  // End of directory
        if ((uint8_t)entries[i].name[0] == 0xE5) continue;  // Deleted entry
        
        if (memcmp(entries[i].name, formatted_name, 11) == 0) {
            return -1;  // Name exists
        }
    }
    
    // Find free entry
    int free_entry = -1;
    for (int i = 0; i < entry_count; i++) {
        if (entries[i].name[0] == 0x00 || (uint8_t)entries[i].name[0] == 0xE5) {
            free_entry = i;
            break;
        }
    }
    
    if (free_entry == -1) {
        return -1;  // No space in directory
    }
    
    // Allocate new cluster for the directory
    uint32_t new_cluster = fat32_find_free_cluster(ctx);
    if (new_cluster == 0) return -1;
    
    // Initialize new directory cluster
    uint8_t new_dir[CLUSTER_SIZE] = {0};
    DirEntry* new_entries = (DirEntry*)new_dir;
    
    // Create "." entry
    memcpy(new_entries[0].name, ".          ", 11);
    new_entries[0].attr = ATTR_DIRECTORY;
    fat32_set_cluster_to_entry(&new_entries[0], new_cluster);
    
    // Create ".." entry
    memcpy(new_entries[1].name, "..         ", 11);
    new_entries[1].attr = ATTR_DIRECTORY;
    fat32_set_cluster_to_entry(&new_entries[1], ctx->current_cluster);
    
    if (fat32_write_cluster(ctx, new_cluster, new_dir) != 0) {
        return -1;
    }
    
    // Mark cluster as used (EOF)
    if (fat32_set_fat_entry(ctx, new_cluster, 0x0FFFFFFF) != 0) {
        return -1;
    }
    
    // Create directory entry in parent
    memset(&entries[free_entry], 0, sizeof(DirEntry));
    memcpy(entries[free_entry].name, formatted_name, 11);
    entries[free_entry].attr = ATTR_DIRECTORY;
    fat32_set_cluster_to_entry(&entries[free_entry], new_cluster);
    
    if (fat32_write_cluster(ctx, ctx->current_cluster, cluster) != 0) {
        return -1;
    }
    
    return 0;
}

int fat32_touch(Fat32Context* ctx, const char* name) {
    if (!ctx || !name || strlen(name) == 0) {
        printf("Error: Invalid parameters\n");
        return -1;
    }
    
    printf("Debug: touch called with name '%s'\n", name);
    
    // Read current directory
    uint8_t cluster[CLUSTER_SIZE];
    if (fat32_read_cluster(ctx, ctx->current_cluster, cluster) != 0) {
        printf("Error: Cannot read current directory cluster\n");
        return -1;
    }
    
    DirEntry* entries = (DirEntry*)cluster;
    int entry_count = CLUSTER_SIZE / sizeof(DirEntry);
    
    // Check if name already exists
    char formatted_name[11];
    fat32_format_name(name, formatted_name);
    
    printf("Debug: Formatted name: '");
    for (int i = 0; i < 11; i++) {
        printf("%c", formatted_name[i] == ' ' ? '.' : formatted_name[i]);
    }
    printf("'\n");
    
    for (int i = 0; i < entry_count; i++) {
        if (entries[i].name[0] == 0x00) break;
        if ((uint8_t)entries[i].name[0] == 0xE5) continue;
        
        printf("Debug: Existing entry %d: '", i);
        for (int j = 0; j < 11; j++) {
            printf("%c", entries[i].name[j] == ' ' ? '.' : entries[i].name[j]);
        }
        printf("'\n");
        
        if (memcmp(entries[i].name, formatted_name, 11) == 0) {
            printf("Error: Name already exists\n");
            return -1;  // Name exists
        }
    }
    
    // Find free entry
    int free_entry = -1;
    for (int i = 0; i < entry_count; i++) {
        if (entries[i].name[0] == 0x00 || (uint8_t)entries[i].name[0] == 0xE5) {
            free_entry = i;
            printf("Debug: Found free entry at position %d\n", i);
            break;
        }
    }
    
    if (free_entry == -1) {
        printf("Error: No free directory entries\n");
        return -1;  // No space in directory
    }
    
    // Create file entry
    memset(&entries[free_entry], 0, sizeof(DirEntry));
    memcpy(entries[free_entry].name, formatted_name, 11);
    entries[free_entry].attr = ATTR_ARCHIVE;
    entries[free_entry].file_size = 0;
    fat32_set_cluster_to_entry(&entries[free_entry], 0);  // Empty file
    
    printf("Debug: Creating file entry with name '");
    for (int i = 0; i < 11; i++) {
        printf("%c", entries[free_entry].name[i] == ' ' ? '.' : entries[free_entry].name[i]);
    }
    printf("'\n");
    
    if (fat32_write_cluster(ctx, ctx->current_cluster, cluster) != 0) {
        printf("Error: Cannot write directory cluster\n");
        return -1;
    }
    
    printf("Debug: File created successfully\n");
    return 0;
}

int fat32_cd(Fat32Context* ctx, const char* path) {
    if (!ctx || !path) return -1;
    
    // Only absolute paths are allowed
    if (path[0] != '/') {
        return -1;
    }
    
    if (strcmp(path, "/") == 0) {
        ctx->current_cluster = ROOT_CLUSTER;
        strcpy(ctx->current_path, "/");
        return 0;
    }
    
    // Handle special cases for "." and ".."
    const char* dir_name = path + 1;  // Skip leading '/'
    
    if (strcmp(dir_name, ".") == 0) {
        // Stay in current directory
        return 0;
    }
    
    if (strcmp(dir_name, "..") == 0) {
        // Go to parent directory
        if (ctx->current_cluster == ROOT_CLUSTER) {
            // Already at root, stay here
            return 0;
        }
        
        // Read current directory to find ".." entry
        uint8_t cluster[CLUSTER_SIZE];
        if (fat32_read_cluster(ctx, ctx->current_cluster, cluster) != 0) {
            return -1;
        }
        
        DirEntry* entries = (DirEntry*)cluster;
        int entry_count = CLUSTER_SIZE / sizeof(DirEntry);
        
        // Find ".." entry to get parent cluster
        for (int i = 0; i < entry_count; i++) {
            if (entries[i].name[0] == 0x00) break;
            if ((uint8_t)entries[i].name[0] == 0xE5) continue;
            
            if (strncmp(entries[i].name, "..         ", 11) == 0) {
                uint32_t parent_cluster = fat32_get_cluster_from_entry(&entries[i]);
                ctx->current_cluster = parent_cluster;
                
                // Update current path - go up one level
                char* last_slash = strrchr(ctx->current_path, '/');
                if (last_slash && last_slash != ctx->current_path) {
                    *last_slash = '\0';
                } else {
                    strcpy(ctx->current_path, "/");
                }
                return 0;
            }
        }
        return -1;
    }
    
    // For now, keep simple implementation - only handles immediate subdirectories
    if (strchr(dir_name, '/') != NULL) {
        printf("Multi-level paths not supported in this version\n");
        return -1;
    }
    
    // Read current directory
    uint8_t cluster[CLUSTER_SIZE];
    if (fat32_read_cluster(ctx, ctx->current_cluster, cluster) != 0) {
        return -1;
    }
    
    DirEntry* entries = (DirEntry*)cluster;
    int entry_count = CLUSTER_SIZE / sizeof(DirEntry);
    char formatted_name[11];
    fat32_format_name(dir_name, formatted_name);
    
    // Search for directory
    for (int i = 0; i < entry_count; i++) {
        if (entries[i].name[0] == 0x00) break;
        if ((uint8_t)entries[i].name[0] == 0xE5) continue;
        
        if ((entries[i].attr & ATTR_DIRECTORY) && 
            memcmp(entries[i].name, formatted_name, 11) == 0) {
            
            uint32_t new_cluster = fat32_get_cluster_from_entry(&entries[i]);
            ctx->current_cluster = new_cluster;
            snprintf(ctx->current_path, sizeof(ctx->current_path), "/%s", dir_name);
            return 0;
        }
    }
    
    return -1;  // Directory not found
}

int fat32_ls(Fat32Context* ctx, const char* path) {
    uint32_t target_cluster = ctx->current_cluster;
    
    if (path) {
        if (strcmp(path, "/") == 0) {
            target_cluster = ROOT_CLUSTER;
        } else {
            // Simple path resolution
            if (path[0] == '/') {
                const char* dir_name = path + 1;
                char formatted_name[11];
                fat32_format_name(dir_name, formatted_name);
                
                // Read root directory
                uint8_t cluster[CLUSTER_SIZE];
                if (fat32_read_cluster(ctx, ROOT_CLUSTER, cluster) != 0) {
                    return -1;
                }
                
                DirEntry* entries = (DirEntry*)cluster;
                int entry_count = CLUSTER_SIZE / sizeof(DirEntry);
                for (int i = 0; i < entry_count; i++) {
                    if (entries[i].name[0] == 0x00) break;
                    if ((uint8_t)entries[i].name[0] == 0xE5) continue;
                    
                    if ((entries[i].attr & ATTR_DIRECTORY) && 
                        memcmp(entries[i].name, formatted_name, 11) == 0) {
                        target_cluster = fat32_get_cluster_from_entry(&entries[i]);
                        break;
                    }
                }
            }
        }
    }
    
    // Read directory
    uint8_t cluster[CLUSTER_SIZE];
    if (fat32_read_cluster(ctx, target_cluster, cluster) != 0) {
        return -1;
    }
    
    DirEntry* entries = (DirEntry*)cluster;
    int entry_count = CLUSTER_SIZE / sizeof(DirEntry);
    
    // List entries
    for (int i = 0; i < entry_count; i++) {
        if (entries[i].name[0] == 0x00) break;  // End of directory
        if ((uint8_t)entries[i].name[0] == 0xE5) continue;  // Deleted entry
        
        // Convert 8.3 name to readable format
        char name[13];
        memcpy(name, entries[i].name, 8);
        name[8] = '\0';
        
        // Trim trailing spaces from name
        int name_len = 8;
        while (name_len > 0 && name[name_len - 1] == ' ') {
            name_len--;
        }
        name[name_len] = '\0';
        
        // Add extension if present
        if (entries[i].name[8] != ' ') {
            strcat(name, ".");
            strncat(name, entries[i].name + 8, 3);
            
            // Trim trailing spaces from extension
            int ext_len = 3;
            while (ext_len > 0 && name[strlen(name) - 1] == ' ') {
                name[strlen(name) - 1] = '\0';
                ext_len--;
            }
        }
        
        // Show entry without trailing slash for directories
        printf("%s\n", name);
    }
    
    return 0;
}