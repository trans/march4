/*
 * March Language - VM Runner
 * Execute compiled words on the VM
 */

#ifndef MARCH_RUNNER_H
#define MARCH_RUNNER_H

#include "types.h"
#include "loader.h"
#include "compiler.h"
#include <stddef.h>
#include <stdint.h>

/* VM interface - implemented in kernel/x86-64/vm.asm */
extern void vm_init(void);
extern void vm_run(uint64_t* code);
extern uint64_t* vm_get_dsp(void);
extern uint64_t data_stack_base[1024];  /* BSS array, not pointer */

/* Runner context */
typedef struct {
    loader_t* loader;
    compiler_t* comp;  /* For on-demand compilation of token-based words (Phase 5) */
} runner_t;

/* Create/free runner */
runner_t* runner_create(loader_t* loader, compiler_t* comp);
void runner_free(runner_t* runner);

/* Execute a word by name */
bool runner_execute(runner_t* runner, const char* name);

/* Get stack contents after execution */
int runner_get_stack(runner_t* runner, int64_t* stack, int max_depth);

/* Print stack (for debugging) */
void runner_print_stack(runner_t* runner);

#endif /* MARCH_RUNNER_H */
