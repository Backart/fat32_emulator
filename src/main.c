/**
 * @file main.c
 * @brief Entry point for the FAT32 Emulator CLI.
 *
 * Implements a simple command-line interface to interact with the
 * FAT32 filesystem emulator. Supports commands like format, ls,
 * mkdir, touch, cd, exit, etc.
 */

#include "fat32.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

extern int process_command(Fat32Context* ctx, const char* command);
extern void print_prompt(Fat32Context* ctx);

/**
 * @brief Main entry point of the FAT32 Emulator.
 *
 * Initializes the FAT32 context with the given disk image file,
 * then enters a command loop reading user input and executing commands.
 *
 * @param argc Argument count. Must be 2.
 * @param argv Argument vector. argv[1] should be path to disk image.
 * @return 0 on normal exit, 1 on error.
 */

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Usage: %s <disk_file>\n", argv[0]);
        return 1;
    }
    
    Fat32Context ctx;
    if (fat32_init(&ctx, argv[1]) != 0) {
        printf("Failed to initialize FAT32 emulator\n");
        return 1;
    }
    
    printf("FAT32 Emulator started. Type 'exit' or 'quit' to exit.\n");
    
    char command[256];
    while (1) {
        print_prompt(&ctx);
        
        if (fgets(command, sizeof(command), stdin) == NULL) {
            break;
        }
        
        // Remove newline
        command[strcspn(command, "\n")] = '\0';
        
        if (process_command(&ctx, command) == -1) {
            break;
        }
    }
    
    fat32_cleanup(&ctx);
    printf("Goodbye!\n");
    return 0;
}
