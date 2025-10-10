#include "fat32.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

extern int process_command(Fat32Context* ctx, const char* command);
extern void print_prompt(Fat32Context* ctx);

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