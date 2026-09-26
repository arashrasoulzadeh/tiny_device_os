#include "host_stack.h"

#include <stdint.h>
#include <stdlib.h>

#if defined(_WIN32)

/* Windows uses Fibers in scheduler.c — this stub must not be called. */
void host_call_on_stack(void* stack_top, void (*fn)(void*), void* arg) {
    (void)stack_top;
    (void)fn;
    (void)arg;
    abort();
}

#elif defined(__aarch64__)

void host_call_on_stack(void* stack_top, void (*fn)(void*), void* arg) {
    __asm__ __volatile__(
        "mov sp, %[stk]\n\t"
        "mov x0, %[a]\n\t"
        "blr %[f]\n\t"
        :
        : [stk] "r"(stack_top), [f] "r"(fn), [a] "r"(arg)
        /* x18 is reserved on Darwin — do not clobber */
        : "x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7", "x8", "x9", "x10", "x11",
          "x12", "x13", "x14", "x15", "x16", "x17", "memory");
    abort();
}

#elif defined(__x86_64__)

void host_call_on_stack(void* stack_top, void (*fn)(void*), void* arg) {
    __asm__ __volatile__(
        "mov %[stk], %%rsp\n\t"
        "mov %[a], %%rdi\n\t"
        "call *%[f]\n\t"
        :
        : [stk] "r"(stack_top), [f] "r"(fn), [a] "r"(arg)
        : "rsp", "rdi", "rsi", "rax", "rcx", "rdx", "r8", "r9", "r10", "r11", "memory");
    abort();
}

#else
#error "host_call_on_stack: unsupported host architecture"
#endif
