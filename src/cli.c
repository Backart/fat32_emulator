/**
 * @file cli.c
 * @brief Command-line interface (CLI) for the FAT32 emulator.
 *
 * This module implements a simple interactive CLI for performing
 * filesystem operations on the FAT32 emulator.
 */

#include "fat32.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/**
 * @brief Prints the CLI prompt showing the current directory.
 *
 * @param ctx Pointer to the FAT32 context containing the current path.
 */
void print_prompt(Fat32Context* ctx) {
    printf("%s>", ctx->current_path);
    fflush(stdout);
}

/**
 * @brief Processes a single user command in the CLI.
 *
 * Supported commands:
 * - format : formats the disk image with FAT32
 * - ls [path] : lists directory contents
 * - mkdir <name> : creates a new directory
 * - touch <name> : creates a new empty file
 * - cd <path> : changes the current working directory
 * - exit / quit : exits the CLI
 *
 * @param ctx Pointer to the FAT32 context.
 * @param command Null-terminated string containing the user command.
 * @return 0 on success, -1 on exit or error.
 */
int process_command(Fat32Context* ctx, const char* command) {
    char cmd[256];
    char arg1[256] = {0};
    char arg2[256] = {0};
    
    int parsed = sscanf(command, "%255s %255s %255s", cmd, arg1, arg2);
    
    if (parsed == 0) {
        return 0; /**< Empty command */
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
            /**< Successfully changed directory, no message needed */
        } else {
            printf("cd failed\n");
        }
    }
    else if (strcmp(cmd, "exit") == 0 || strcmp(cmd, "quit") == 0) {
        return -1; /**< Signal to exit CLI */
    }
    else {
        printf("Unknown command: %s\n", cmd);
    }
    
    return 0; /**< Command processed successfully */
}
