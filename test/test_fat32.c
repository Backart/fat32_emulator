/**
 * @file test_fat32.c
 * @brief Unit tests for FAT32 emulator CLI and filesystem operations.
 *
 * This file contains automated tests for the FAT32 emulator, verifying:
 * - Disk creation and size
 * - Format operations
 * - Directory creation (mkdir)
 * - File creation (touch)
 * - Navigation (cd)
 * - Listing directory contents (ls)
 * - Handling of unknown commands
 *
 * Tests are implemented using assertions.
 */

#define _POSIX_C_SOURCE 200809L
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "fat32.h"
#include "cli.h"

/// Path to temporary test disk image
#define TEST_DISK "test_fat32.img"

/**
 * @brief Remove test disk file
 */
void cleanup() {
    remove(TEST_DISK);
}

/**
 * @brief Get the size of a file in bytes
 * @param path Path to the file
 * @return File size in bytes
 */
long get_file_size(const char* path) {
    FILE* f = fopen(path, "rb");
    assert(f);
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fclose(f);
    return size;
}

/**
 * @brief Execute a CLI command and capture its stdout output
 * @param ctx FAT32 context
 * @param cmd Command string
 * @param output Buffer to store stdout output
 * @param out_size Size of the output buffer
 * @return Return value of process_command()
 */
int run_command(Fat32Context* ctx, const char* cmd, char* output, size_t out_size) {
    FILE* tmp = tmpfile();
    assert(tmp);
    int stdout_backup = dup(fileno(stdout));
    fflush(stdout);
    dup2(fileno(tmp), fileno(stdout));

    int ret = process_command(ctx, cmd);

    fflush(stdout);
    fseek(tmp, 0, SEEK_SET);
    size_t n = fread(output, 1, out_size - 1, tmp);
    output[n] = '\0';

    dup2(stdout_backup, fileno(stdout));
    close(stdout_backup);
    fclose(tmp);
    return ret;
}

/**
 * @brief Main test function
 *
 * Executes a sequence of tests for FAT32 emulator:
 * 1. Verify disk file size
 * 2. Attempt `ls` before formatting
 * 3. Format the disk
 * 4. `ls` after formatting
 * 5. Create directory `ttt`
 * 6. Verify directory listing
 * 7. Navigate into `/ttt`
 * 8. List subdirectory
 * 9. Return to root
 * 10. Create file `file1.txt`
 * 11. Verify file creation
 * 12. Test unknown command handling
 */
int main() {
    cleanup();

    Fat32Context ctx;
    assert(fat32_init(&ctx, TEST_DISK) == 0);

    // === 1. Verify file size is 20MB ===
    long size = get_file_size(TEST_DISK);
    assert(size == 20 * 1024 * 1024);

    // === 2. ls before formatting ===
    char out[1024];
    int ret = run_command(&ctx, "ls", out, sizeof(out));
    assert(strstr(out, "Unknown disk format") != NULL);

    // === 3. format ===
    ret = run_command(&ctx, "format", out, sizeof(out));
    assert(strstr(out, "Ok") != NULL);
    assert(fat32_is_valid(&ctx) == 0);

    // === 4. ls after formatting ===
    ret = run_command(&ctx, "ls", out, sizeof(out));
    assert(strstr(out, ".") != NULL && strstr(out, "..") != NULL);

    // === 5. mkdir ttt ===
    ret = run_command(&ctx, "mkdir ttt", out, sizeof(out));
    assert(strstr(out, "Ok") != NULL);

    // === 6. ls confirms ttt ===
    ret = run_command(&ctx, "ls", out, sizeof(out));
    assert(strstr(out, "ttt") != NULL);

    // === 7. cd /ttt ===
    ret = run_command(&ctx, "cd /ttt", out, sizeof(out));
    assert(ctx.current_cluster != ROOT_CLUSTER);
    assert(strcmp(ctx.current_path, "/ttt") == 0);

    // === 8. ls in subdirectory ===
    ret = run_command(&ctx, "ls", out, sizeof(out));
    assert(strstr(out, ".") != NULL && strstr(out, "..") != NULL);

    // === 9. cd / ===
    ret = run_command(&ctx, "cd /", out, sizeof(out));
    assert(ctx.current_cluster == ROOT_CLUSTER);
    assert(strcmp(ctx.current_path, "/") == 0);

    // === 10. touch file1.txt ===
    ret = run_command(&ctx, "touch file1.txt", out, sizeof(out));
    assert(strstr(out, "Ok") != NULL);

    // === 11. ls confirms file1.txt ===
    ret = run_command(&ctx, "ls", out, sizeof(out));
    assert(strstr(out, "file1.txt") != NULL);

    // === 12. Unknown command ===
    ret = run_command(&ctx, "unknowncmd", out, sizeof(out));
    assert(strstr(out, "Unknown command") != NULL);

    fat32_cleanup(&ctx);
    cleanup();

    printf("All Task #3 tests passed!\n");
    return 0;
}
