#include "fat32.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void print_prompt(Fat32Context* ctx) {
    printf("%s>", ctx->current_path);
    fflush(stdout);
}

int process_command(Fat32Context* ctx, const char* command) {
    char cmd[256];
    char arg1[256] = {0};
    char arg2[256] = {0};
    
    int parsed = sscanf(command, "%255s %255s %255s", cmd, arg1, arg2);
    
    if (parsed == 0) {
        return 0;
    }
    
    if (strcmp(cmd, "format") == 0) {
        if (fat32_format(ctx) == 0) {
            printf("Ok\n");
        } else {
            printf("Format failed\n");
        }
    }
    else if (strcmp(cmd, "ls") == 0) {
        if (fat32_is_valid(ctx) != 0) {
            printf("Unknown disk format\n");
            return -1;
        }
        
        const char* path = NULL;
        if (arg1[0] != '\0') {
            path = arg1;
        }
        
        if (fat32_ls(ctx, path) != 0) {
            printf("ls failed\n");
        }
    }
    else if (strcmp(cmd, "mkdir") == 0) {
        if (fat32_is_valid(ctx) != 0) {
            printf("Unknown disk format\n");
            return -1;
        }
        
        if (arg1[0] == '\0') {
            printf("Usage: mkdir <name>\n");
        } else if (fat32_mkdir(ctx, arg1) == 0) {
            printf("Ok\n");
        } else {
            printf("mkdir failed\n");
        }
    }
    else if (strcmp(cmd, "touch") == 0) {
        if (fat32_is_valid(ctx) != 0) {
            printf("Unknown disk format\n");
            return -1;
        }
        
        if (arg1[0] == '\0') {
            printf("Usage: touch <name>\n");
        } else if (fat32_touch(ctx, arg1) == 0) {
            printf("Ok\n");
        } else {
            printf("touch failed\n");
        }
    }
    else if (strcmp(cmd, "cd") == 0) {
        if (fat32_is_valid(ctx) != 0) {
            printf("Unknown disk format\n");
            return -1;
        }
        
        if (arg1[0] == '\0') {
            printf("Usage: cd <path>\n");
        } else if (fat32_cd(ctx, arg1) == 0) {
        } else {
            printf("cd failed\n");
        }
    }
    else if (strcmp(cmd, "exit") == 0 || strcmp(cmd, "quit") == 0) {
        return -1;
    }
    else {
        printf("Unknown command: %s\n", cmd);
    }
    
    return 0;
}
