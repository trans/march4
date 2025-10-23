/*
 * March Language - Compiler CLI
 * Main entry point for the C compiler
 */

#define _POSIX_C_SOURCE 200809L

#include "compiler.h"
#include "loader.h"
#include "runner.h"
#include "database.h"
#include "dictionary.h"
#include "debug.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <ctype.h>

static void print_usage(const char* prog) {
    printf("March Language Compiler (C version)\n\n");
    printf("Usage: %s [options] <input.march>\n\n", prog);
    printf("Options:\n");
    printf("  -o <db>       Output database file (default: march.db)\n");
    printf("  -v            Verbose output\n");
    printf("  -d <cats>     Enable debug output (comma-separated: compiler,dict,types,cid,loader,db,all)\n");
    printf("  -r <word>     Run word after compilation\n");
    printf("  -s            Show stack after execution\n");
    printf("  -h            Show this help\n\n");
    printf("Examples:\n");
    printf("  %s hello.march                    # Compile to march.db\n", prog);
    printf("  %s -v -o my.db hello.march        # Verbose, custom DB\n", prog);
    printf("  %s -d dict,types hello.march      # Debug dictionary and types\n", prog);
    printf("  %s -d all hello.march             # Debug all categories\n", prog);
    printf("  %s -r main hello.march            # Compile and run 'main'\n", prog);
    printf("  %s -r main -s hello.march         # Run and show stack\n", prog);
}

int main(int argc, char** argv) {
    const char* output_db = "march.db";
    const char* run_word = NULL;
    bool verbose = false;
    bool show_stack = false;
    int opt;

    /* Initialize debug system from environment */
    debug_init();

    /* Parse options */
    while ((opt = getopt(argc, argv, "o:r:d:vsh")) != -1) {
        switch (opt) {
            case 'o':
                output_db = optarg;
                break;
            case 'r':
                run_word = optarg;
                break;
            case 'd':
                {
                    /* Parse comma-separated debug categories */
                    char* str = strdup(optarg);
                    char* token = strtok(str, ",");
                    while (token) {
                        /* Trim whitespace */
                        while (isspace(*token)) token++;
                        char* end = token + strlen(token) - 1;
                        while (end > token && isspace(*end)) *end-- = '\0';

                        /* Enable category */
                        if (strcmp(token, "all") == 0) {
                            debug_enable(DEBUG_ALL);
                        } else if (strcmp(token, "compiler") == 0) {
                            debug_enable(DEBUG_COMPILER);
                        } else if (strcmp(token, "dict") == 0) {
                            debug_enable(DEBUG_DICT);
                        } else if (strcmp(token, "types") == 0) {
                            debug_enable(DEBUG_TYPES);
                        } else if (strcmp(token, "cid") == 0) {
                            debug_enable(DEBUG_CID);
                        } else if (strcmp(token, "loader") == 0) {
                            debug_enable(DEBUG_LOADER);
                        } else if (strcmp(token, "runtime") == 0) {
                            debug_enable(DEBUG_RUNTIME);
                        } else if (strcmp(token, "db") == 0) {
                            debug_enable(DEBUG_DB);
                        } else {
                            fprintf(stderr, "Warning: Unknown debug category '%s'\n", token);
                        }
                        token = strtok(NULL, ",");
                    }
                    free(str);
                }
                break;
            case 'v':
                verbose = true;
                break;
            case 's':
                show_stack = true;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    /* Check for input file */
    if (optind >= argc) {
        fprintf(stderr, "Error: No input file specified\n\n");
        print_usage(argv[0]);
        return 1;
    }

    const char* input_file = argv[optind];

    /* Open/create database */
    march_db_t* db = db_open(output_db);
    if (!db) {
        fprintf(stderr, "Error: Cannot open database: %s\n", output_db);
        return 1;
    }

    /* Initialize schema if new database */
    if (!db_init_schema(db, "schema.sql")) {
        /* Schema might already exist, that's okay */
    }

    /* Create dictionary and compiler */
    dictionary_t* dict = dict_create();
    if (!dict) {
        fprintf(stderr, "Error: Cannot create dictionary\n");
        db_close(db);
        return 1;
    }

    compiler_t* comp = compiler_create(dict, db);
    if (!comp) {
        fprintf(stderr, "Error: Cannot create compiler\n");
        dict_free(dict);
        db_close(db);
        return 1;
    }

    comp->verbose = verbose;

    /* Register primitives */
    compiler_register_primitives(comp);

    if (verbose) {
        printf("Compiling: %s → %s\n", input_file, output_db);
    }

    /* Compile file */
    if (!compiler_compile_file(comp, input_file)) {
        fprintf(stderr, "Compilation failed\n");
        compiler_free(comp);
        dict_free(dict);
        db_close(db);
        return 1;
    }

    if (verbose) {
        printf("✓ Compilation successful\n");
    }

    /* Run word if requested */
    if (run_word) {
        if (verbose) {
            printf("\nExecuting: %s\n", run_word);
        }

        loader_t* loader = loader_create(db, dict);
        if (!loader) {
            fprintf(stderr, "Error: Cannot create loader\n");
            compiler_free(comp);
            dict_free(dict);
            db_close(db);
            return 1;
        }

        runner_t* runner = runner_create(loader);
        if (!runner) {
            fprintf(stderr, "Error: Cannot create runner\n");
            loader_free(loader);
            compiler_free(comp);
            dict_free(dict);
            db_close(db);
            return 1;
        }

        if (!runner_execute(runner, run_word)) {
            fprintf(stderr, "Execution failed\n");
            runner_free(runner);
            loader_free(loader);
            compiler_free(comp);
            dict_free(dict);
            db_close(db);
            return 1;
        }

        if (show_stack) {
            runner_print_stack(runner);
        }

        runner_free(runner);
        loader_free(loader);
    }

    /* Clean up */
    compiler_free(comp);
    dict_free(dict);
    db_close(db);

    return 0;
}
