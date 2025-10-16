#ifndef CLI_H
#define CLI_H

#include "fat32.h"

/**
 * @file cli.h
 * @brief Command-line interface (CLI) utilities for the FAT32 emulator.
 *
 * This module provides functions to display the prompt and process user commands
 * within the emulator's interactive CLI. It interacts with the Fat32Context to
 * execute filesystem operations such as format, ls, mkdir, cd, and touch.
 */

/**
 * @brief Display the CLI prompt with the current working path.
 *
 * @param ctx Pointer to the Fat32Context containing the current path and state.
 *
 * @note This function only prints to stdout and flushes the output.
 */
void print_prompt(Fat32Context* ctx);

/**
 * @brief Process a single command entered by the user in the CLI.
 *
 * Parses the given command string and executes the corresponding filesystem
 * operation in the provided Fat32Context.
 *
 * Supported commands include:
 * - format
 * - ls
 * - mkdir <name>
 * - touch <name>
 * - cd <path>
 * - exit / quit
 *
 * @param ctx Pointer to the Fat32Context representing the current filesystem state.
 * @param command Null-terminated string containing the user command.
 * 
 * @return int Returns 0 on successful execution of a command.
 *             Returns -1 if the command is 'exit', 'quit', or an error occurs.
 *
 * @note The function prints command results or error messages directly to stdout.
 * @see print_prompt()
 */
int process_command(Fat32Context* ctx, const char* command);

#endif // CLI_H
