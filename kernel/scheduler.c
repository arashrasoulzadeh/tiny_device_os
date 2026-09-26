#include "scheduler.h"
#include "host_stack.h"
#include "hal_power.h"
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#include <stdio.h>

#if defined(_WIN32)
#include <windows.h>
#endif

static scheduler_t g_scheduler = {0};

static task_tcb_t* g_main_task = NULL;
static task_tcb_t* g_last_switched_task = NULL;
static jmp_buf g_create_jmp;

#if defined(_WIN32)
static void* g_main_fiber = NULL;
#endif

static void scheduler_update_next_wake_tick(void);
static void task_trampoline(void);

static void task_list_add(task_tcb_t** head, task_tcb_t* task) {
    task->next = *head;
    task->prev = NULL;
    if (*head) {
        (*head)->prev = task;
    }
    *head = task;
}

static void task_list_remove(task_tcb_t** head, task_tcb_t* task) {
    if (task->prev) {
        task->prev->next = task->next;
    } else {
        *head = task->next;
    }
    if (task->next) {
        task->next->prev = task->prev;
    }
    task->next = task->prev = NULL;
}

static void task_add_to_ready(task_tcb_t* task) {
    task_list_add(&g_scheduler.ready_list[task->priority], task);
    task->state = TASK_STATE_READY;
}

static task_tcb_t* task_get_highest_ready(void) {
    for (int prio = TASK_PRIO_CRITICAL; prio >= TASK_PRIO_IDLE; prio--) {
        if (g_scheduler.ready_list[prio]) {
            return g_scheduler.ready_list[prio];
        }
    }
    return NULL;
}

static void switch_to_main(void) {
    g_scheduler.current = g_main_task;
    g_main_task->state = TASK_STATE_RUNNING;
    g_last_switched_task = NULL;
    if (g_scheduler.caller_context_valid) {
        longjmp(g_scheduler.caller_context, 1);
    }
}

static void context_switch(task_tcb_t* next) {
    if (next == g_main_task) {
        switch_to_main();
        return;
    }

    task_list_remove(&g_scheduler.ready_list[next->priority], next);
    g_scheduler.current = next;
    next->state = TASK_STATE_RUNNING;
    longjmp(next->context, 1);
}

/* Runs on the task's private stack during task_create bootstrap. */
static void host_stack_boot(void* arg) {
    task_tcb_t* task = (task_tcb_t*)arg;
    if (setjmp(task->context) == 0) {
        task->context_valid = true;
        longjmp(g_create_jmp, 1);
    }
    task_trampoline();
}

#if defined(_WIN32)
static void WINAPI win_fiber_boot(void* arg) {
    host_stack_boot(arg);
}
#endif

int scheduler_init(void) {
    memset(&g_scheduler, 0, sizeof(scheduler_t));

    for (int i = 0; i < MAX_TASKS; i++) {
        g_scheduler.tasks[i].id = (uint8_t)i;
        task_list_add(&g_scheduler.free_list, &g_scheduler.tasks[i]);
    }

    g_scheduler.next_task_id = 1;
    g_scheduler.tick_count = 0;
    g_scheduler.scheduler_started = false;
    g_scheduler.in_isr = false;
    g_scheduler.nesting_level = 0;

    g_scheduler.tickless_idle_enabled = true;
    g_scheduler.next_wake_tick = UINT32_MAX;
    g_scheduler.idle_tick_count = 0;
    g_scheduler.power_mode = POWER_MODE_ACTIVE;
    g_scheduler.deep_sleep_min_ticks = 10000;

#if defined(_WIN32)
    if (!g_main_fiber) {
        g_main_fiber = ConvertThreadToFiber(NULL);
        if (!g_main_fiber) {
            return -1;
        }
    }
#endif

    task_tcb_t* idle_task_tcb;
    int ret = task_create("idle", idle_task, NULL, TASK_PRIO_IDLE, HOST_STACK_MIN_BYTES, &idle_task_tcb);
    if (ret != 0) {
        return ret;
    }

    if (!g_scheduler.free_list) {
        return -1;
    }
    g_main_task = g_scheduler.free_list;
    task_list_remove(&g_scheduler.free_list, g_main_task);

    strncpy(g_main_task->name, "main", TASK_NAME_LEN - 1);
    g_main_task->name[TASK_NAME_LEN - 1] = '\0';
    g_main_task->entry = NULL;
    g_main_task->arg = NULL;
    g_main_task->priority = TASK_PRIO_NORMAL;
    g_main_task->state = TASK_STATE_RUNNING;
    g_main_task->wake_time = 0;
    g_main_task->sleep_ticks = 0;
    g_main_task->context_valid = false;
    g_main_task->host_fiber = NULL;
    g_main_task->stack_size = 0;
    g_main_task->stack_base = NULL;
    g_main_task->stack_ptr = NULL;

    g_scheduler.current = g_main_task;

    return 0;
}

int scheduler_start(void) {
    if (g_scheduler.scheduler_started) {
        return -1;
    }

    g_scheduler.scheduler_started = true;

    if (setjmp(g_main_task->context) == 0) {
        g_main_task->context_valid = true;
        return scheduler_step();
    }

    return 0;
}

int scheduler_step(void) {
    if (!g_scheduler.scheduler_started) {
        return -1;
    }

    task_tcb_t* next = task_get_highest_ready();
    if (!next || next->priority == TASK_PRIO_IDLE) {
        return 0;
    }

    if (next == g_last_switched_task) {
        return 0;
    }

    jmp_buf caller_ctx;
    if (setjmp(caller_ctx) == 0) {
        g_scheduler.caller_context_valid = true;
        memcpy(&g_scheduler.caller_context, &caller_ctx, sizeof(jmp_buf));
        context_switch(next);
        /* unreachable when switch succeeds */
        g_last_switched_task = next;
    }
    return 0;
}

static void task_trampoline(void) {
    task_tcb_t* task = g_scheduler.current;
    if (task && task->entry) {
        task->entry(task->arg);
    }
    if (task) {
        task->state = TASK_STATE_TERMINATED;
    }
    switch_to_main();
}

int task_create(const char* name, task_entry_t entry, void* arg, task_priority_t priority,
                size_t stack_size, task_tcb_t** out_task) {
    if (!g_scheduler.free_list || priority > TASK_PRIO_CRITICAL) {
        return -1;
    }

    if (stack_size < HOST_STACK_MIN_BYTES) {
        stack_size = HOST_STACK_MIN_BYTES;
    }

    task_tcb_t* task = g_scheduler.free_list;
    task_list_remove(&g_scheduler.free_list, task);

    strncpy(task->name, name, TASK_NAME_LEN - 1);
    task->name[TASK_NAME_LEN - 1] = '\0';
    task->entry = entry;
    task->arg = arg;
    task->priority = priority;
    task->state = TASK_STATE_READY;
    task->wake_time = 0;
    task->sleep_ticks = 0;
    task->context_valid = false;
    task->host_fiber = NULL;

    task->stack_size = stack_size;
    task->stack_base = (uint32_t*)malloc(stack_size);
    if (!task->stack_base) {
        task_list_add(&g_scheduler.free_list, task);
        return -1;
    }
    memset(task->stack_base, 0, stack_size);

    uintptr_t top = (uintptr_t)task->stack_base + stack_size;
    top &= ~(uintptr_t)15u; /* 16-byte align */
    task->stack_ptr = (uintptr_t*)top;

    /* Bootstrap: run on the private stack, setjmp a durable context, return here. */
    if (setjmp(g_create_jmp) == 0) {
#if defined(_WIN32)
        task->host_fiber = CreateFiber(stack_size, win_fiber_boot, task);
        if (!task->host_fiber) {
            free(task->stack_base);
            task->stack_base = NULL;
            task_list_add(&g_scheduler.free_list, task);
            return -1;
        }
        SwitchToFiber(task->host_fiber);
#else
        host_call_on_stack((void*)top, host_stack_boot, task);
#endif
    }

    if (!task->context_valid) {
        free(task->stack_base);
        task->stack_base = NULL;
#if defined(_WIN32)
        if (task->host_fiber) {
            DeleteFiber(task->host_fiber);
            task->host_fiber = NULL;
        }
#endif
        task_list_add(&g_scheduler.free_list, task);
        return -1;
    }

    task_add_to_ready(task);

    if (out_task) {
        *out_task = task;
    }

    return 0;
}

int task_delete(task_tcb_t* task) {
    if (!task || task == g_scheduler.current) {
        return -1;
    }

    switch (task->state) {
        case TASK_STATE_READY:
            task_list_remove(&g_scheduler.ready_list[task->priority], task);
            break;
        case TASK_STATE_BLOCKED:
            task_list_remove(&g_scheduler.blocked_list, task);
            break;
        case TASK_STATE_SUSPENDED:
            break;
        case TASK_STATE_TERMINATED:
            break;
        default:
            return -1;
    }

#if defined(_WIN32)
    if (task->host_fiber) {
        DeleteFiber(task->host_fiber);
        task->host_fiber = NULL;
    }
#endif

    free(task->stack_base);
    task->stack_base = NULL;
    task->state = TASK_STATE_TERMINATED;
    task_list_add(&g_scheduler.free_list, task);

    return 0;
}

void task_yield(void) {
    scheduler_lock();

    task_tcb_t* current = g_scheduler.current;

    if (current == g_main_task) {
        scheduler_unlock();
        scheduler_step();
        return;
    }

    if (current && current->state == TASK_STATE_RUNNING) {
        if (setjmp(current->context) == 0) {
            current->context_valid = true;
            current->state = TASK_STATE_READY;

            /* Pick the next task before re-queueing current (round-robin). */
            task_tcb_t* next = task_get_highest_ready();
            if (next && next->priority == TASK_PRIO_IDLE) {
                next = NULL;
            }

            task_add_to_ready(current);

            if (next) {
                context_switch(next);
            }
            switch_to_main();
        }
    }

    scheduler_unlock();
}

void task_sleep(uint32_t ticks) {
    task_tcb_t* current = g_scheduler.current;
    if (!current) {
        return;
    }

    /* Sleeping the host main/test thread: advance time cooperatively. */
    if (current == g_main_task) {
        uint32_t until = g_scheduler.tick_count + ticks;
        while (g_scheduler.tick_count < until) {
            scheduler_tick();
            scheduler_step();
        }
        return;
    }

    current->sleep_ticks = ticks;
    current->wake_time = g_scheduler.tick_count + ticks;
    current->state = TASK_STATE_BLOCKED;
    /* Running tasks are already off the ready list (dequeued in context_switch). */
    task_list_add(&g_scheduler.blocked_list, current);

    if (setjmp(current->context) == 0) {
        current->context_valid = true;
        switch_to_main();
    }

    current->state = TASK_STATE_RUNNING;
}

void task_suspend(task_tcb_t* task) {
    scheduler_lock();

    if (!task) {
        scheduler_unlock();
        return;
    }

    switch (task->state) {
        case TASK_STATE_READY:
            task_list_remove(&g_scheduler.ready_list[task->priority], task);
            break;
        case TASK_STATE_BLOCKED:
            /* Sleeping apps must leave the blocked list or they keep waking
             * and stealing scheduler_step slots from the foreground app. */
            task_list_remove(&g_scheduler.blocked_list, task);
            task->wake_time = 0;
            task->sleep_ticks = 0;
            break;
        case TASK_STATE_RUNNING:
            /* Current task: fall through to mark suspended and switch away. */
            break;
        case TASK_STATE_SUSPENDED:
            scheduler_unlock();
            return;
        default:
            scheduler_unlock();
            return;
    }

    task->state = TASK_STATE_SUSPENDED;

    if (task == g_scheduler.current) {
        task_tcb_t* next = task_get_highest_ready();
        if (next && next->priority != TASK_PRIO_IDLE) {
            context_switch(next);
        } else {
            switch_to_main();
        }
    }

    scheduler_unlock();
}

void task_resume(task_tcb_t* task) {
    scheduler_lock();

    if (!task || task->state != TASK_STATE_SUSPENDED) {
        scheduler_unlock();
        return;
    }

    task->state = TASK_STATE_READY;
    task_add_to_ready(task);

    scheduler_unlock();
}

task_tcb_t* task_get_current(void) {
    return g_scheduler.current;
}

const char* task_get_name(task_tcb_t* task) {
    return task ? task->name : NULL;
}

task_state_t task_get_state(task_tcb_t* task) {
    return task ? task->state : TASK_STATE_TERMINATED;
}

task_priority_t task_get_priority(task_tcb_t* task) {
    return task ? task->priority : TASK_PRIO_IDLE;
}

uint32_t scheduler_get_tick_count(void) {
    return g_scheduler.tick_count;
}

void scheduler_tick(void) {
    scheduler_lock();

    g_scheduler.tick_count++;

    task_tcb_t* task = g_scheduler.blocked_list;
    task_tcb_t* next_task;
    while (task) {
        next_task = task->next;
        if (g_scheduler.tick_count >= task->wake_time) {
            task_list_remove(&g_scheduler.blocked_list, task);
            task->state = TASK_STATE_READY;
            task_add_to_ready(task);
        }
        task = next_task;
    }

    scheduler_update_next_wake_tick();

    scheduler_unlock();
}

void scheduler_lock(void) {
    if (!g_scheduler.in_isr) {
        g_scheduler.nesting_level++;
    }
}

void scheduler_unlock(void) {
    if (!g_scheduler.in_isr && g_scheduler.nesting_level > 0) {
        g_scheduler.nesting_level--;
    }
}

bool scheduler_is_in_isr(void) {
    return g_scheduler.in_isr;
}

void scheduler_enter_isr(void) {
    g_scheduler.in_isr = true;
    g_scheduler.nesting_level++;
}

void scheduler_exit_isr(void) {
    if (g_scheduler.nesting_level > 0) {
        g_scheduler.nesting_level--;
    }
    if (g_scheduler.nesting_level == 0) {
        g_scheduler.in_isr = false;
    }
}

void idle_task(void* arg) {
    (void)arg;
    while (1) {
        if (g_scheduler.tickless_idle_enabled) {
            scheduler_enter_idle();
        } else {
#if (defined(__arm__) || defined(__aarch64__)) && \
    (defined(ARDUBOT_TARGET_ESP32) || defined(ARDUBOT_TARGET_ESP8266) || defined(ARDUBOT_TARGET_AVR) || \
     defined(ARDUBOT_TARGET_RP2040))
            __asm__ volatile("wfi" ::: "memory");
#else
            task_yield();
#endif
        }
    }
}

static void scheduler_update_next_wake_tick(void) {
    g_scheduler.next_wake_tick = UINT32_MAX;

    task_tcb_t* task = g_scheduler.blocked_list;
    while (task) {
        if (task->wake_time < g_scheduler.next_wake_tick) {
            g_scheduler.next_wake_tick = task->wake_time;
        }
        task = task->next;
    }
}

void scheduler_enable_tickless_idle(bool enable) {
    g_scheduler.tickless_idle_enabled = enable;
}

uint32_t scheduler_get_next_wake_tick(void) {
    return g_scheduler.next_wake_tick;
}

void scheduler_enter_idle(void) {
    g_scheduler.power_mode = POWER_MODE_IDLE;
    g_scheduler.idle_tick_count++;

    uint32_t next_wake = g_scheduler.next_wake_tick;
    uint32_t now = g_scheduler.tick_count;
    uint32_t sleep_ticks = (next_wake == UINT32_MAX) ? 0 : (next_wake - now);

    if (sleep_ticks > 0) {
        if (sleep_ticks >= g_scheduler.deep_sleep_min_ticks) {
            g_scheduler.power_mode = POWER_MODE_LIGHT_SLEEP;
            g_scheduler.tick_count += sleep_ticks;
        } else {
            g_scheduler.power_mode = POWER_MODE_LIGHT_SLEEP;
        }
        g_scheduler.power_mode = POWER_MODE_ACTIVE;
    } else {
        g_scheduler.power_mode = POWER_MODE_DEEP_SLEEP;
        task_yield();
    }
}

void scheduler_tickless_idle(void) {
    if (!g_scheduler.tickless_idle_enabled) {
        return;
    }

    uint32_t next_wake = g_scheduler.next_wake_tick;
    uint32_t now = g_scheduler.tick_count;

    if (next_wake == UINT32_MAX) {
        g_scheduler.power_mode = POWER_MODE_DEEP_SLEEP;
        return;
    }

    uint32_t sleep_ticks = next_wake - now;

    if (sleep_ticks == 0) {
        return;
    }

    if (sleep_ticks >= g_scheduler.deep_sleep_min_ticks) {
        g_scheduler.power_mode = POWER_MODE_LIGHT_SLEEP;
        g_scheduler.tick_count = next_wake;
    } else {
        g_scheduler.power_mode = POWER_MODE_LIGHT_SLEEP;
        while (g_scheduler.tick_count < next_wake) {
        }
    }
    g_scheduler.power_mode = POWER_MODE_ACTIVE;
}

int scheduler_enter_deep_sleep(uint32_t timeout_ticks) {
    if (!g_scheduler.tickless_idle_enabled) {
        return -1;
    }

    g_scheduler.power_mode = POWER_MODE_DEEP_SLEEP;

    if (timeout_ticks > 0) {
        g_scheduler.tick_count += timeout_ticks;
    }
    g_scheduler.tickless_idle_enabled = false;

    return 0;
}

power_mode_t scheduler_get_power_mode(void) {
    return g_scheduler.power_mode;
}

void scheduler_set_deep_sleep_min_ticks(uint32_t ticks) {
    g_scheduler.deep_sleep_min_ticks = ticks;
}
