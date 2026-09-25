#include "scheduler.h"
#include "time.h"
#include "hal_power.h"
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>

static scheduler_t g_scheduler = {0};

static void scheduler_update_next_wake_tick(void);

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

static void context_switch(task_tcb_t* next) {
    task_tcb_t* prev = g_scheduler.current;
    g_scheduler.current = next;
    next->state = TASK_STATE_RUNNING;
    (void)prev;
}

int scheduler_init(void) {
    memset(&g_scheduler, 0, sizeof(scheduler_t));
    
    for (int i = 0; i < MAX_TASKS; i++) {
        g_scheduler.tasks[i].id = i;
        task_list_add(&g_scheduler.free_list, &g_scheduler.tasks[i]);
    }
    
    g_scheduler.next_task_id = 1;
    g_scheduler.tick_count = 0;
    g_scheduler.scheduler_started = false;
    g_scheduler.in_isr = false;
    g_scheduler.nesting_level = 0;
    
    // Tickless idle defaults
    g_scheduler.tickless_idle_enabled = true;
    g_scheduler.next_wake_tick = UINT32_MAX;
    g_scheduler.idle_tick_count = 0;
    g_scheduler.power_mode = POWER_MODE_ACTIVE;
    g_scheduler.deep_sleep_min_ticks = 10000; // 10ms default
    
    task_tcb_t* idle_task_tcb;
    int ret = task_create("idle", idle_task, NULL, TASK_PRIO_IDLE, 256, &idle_task_tcb);
    if (ret != 0) {
        return ret;
    }
    
    return 0;
}

int scheduler_start(void) {
    if (g_scheduler.scheduler_started) {
        return -1;
    }
    
    g_scheduler.scheduler_started = true;
    
    task_tcb_t* next = task_get_highest_ready();
    if (!next) {
        return -1;
    }
    
    context_switch(next);
    
    return 0;
}

int task_create(const char* name, task_entry_t entry, void* arg,
                task_priority_t priority, size_t stack_size, task_tcb_t** out_task) {
    if (!g_scheduler.free_list || priority > TASK_PRIO_CRITICAL) {
        return -1;
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
    
    task->stack_size = stack_size;
    task->stack_base = (uint32_t*)malloc(stack_size);
    if (!task->stack_base) {
        task_list_add(&g_scheduler.free_list, task);
        return -1;
    }
    
    task->stack_ptr = (uintptr_t*)((uint8_t*)task->stack_base + stack_size - 16 * sizeof(uintptr_t));
    memset(task->stack_ptr, 0, 16 * sizeof(uintptr_t));
    task->stack_ptr[0] = (uintptr_t)entry;
    task->stack_ptr[1] = (uintptr_t)arg;
    task->stack_ptr[15] = 0x01000000;
    
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
        default:
            return -1;
    }
    
    free(task->stack_base);
    task->stack_base = NULL;
    task->state = TASK_STATE_TERMINATED;
    task_list_add(&g_scheduler.free_list, task);
    
    return 0;
}

void task_yield(void) {
    scheduler_lock();
    
    task_tcb_t* current = g_scheduler.current;
    if (current && current->state == TASK_STATE_RUNNING) {
        current->state = TASK_STATE_READY;
        task_add_to_ready(current);
    }
    
    task_tcb_t* next = task_get_highest_ready();
    if (next && next != current) {
        context_switch(next);
    }
    
    scheduler_unlock();
}

void task_sleep(uint32_t ticks) {
    scheduler_lock();
    
    task_tcb_t* current = g_scheduler.current;
    if (!current) {
        scheduler_unlock();
        return;
    }
    
    current->sleep_ticks = ticks;
    current->wake_time = g_scheduler.tick_count + ticks;
    current->state = TASK_STATE_BLOCKED;
    task_list_remove(&g_scheduler.ready_list[current->priority], current);
    task_list_add(&g_scheduler.blocked_list, current);
    
    task_tcb_t* next = task_get_highest_ready();
    if (next) {
        context_switch(next);
    }
    
    scheduler_unlock();
}

void task_suspend(task_tcb_t* task) {
    scheduler_lock();
    
    if (!task || task->state != TASK_STATE_READY) {
        scheduler_unlock();
        return;
    }
    
    task_list_remove(&g_scheduler.ready_list[task->priority], task);
    task->state = TASK_STATE_SUSPENDED;
    
    if (task == g_scheduler.current) {
        task_tcb_t* next = task_get_highest_ready();
        if (next) {
            context_switch(next);
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
    
    // Update next wake tick for tickless idle
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
#if defined(__arm__) || defined(__aarch64__)
            __asm__ volatile("wfi" ::: "memory");
#else
            // Simulator: yield to other tasks
            task_yield();
#endif
        }
    }
}

// Tickless idle implementation
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
        // Check if we can enter light sleep
        if (sleep_ticks >= g_scheduler.deep_sleep_min_ticks) {
            g_scheduler.power_mode = POWER_MODE_LIGHT_SLEEP;
            // In real hardware: call hal_power_light_sleep(sleep_ticks)
            // For simulator, just advance tick count
            g_scheduler.tick_count += sleep_ticks;
        } else {
            // Short sleep - just busy wait or light sleep
            g_scheduler.power_mode = POWER_MODE_LIGHT_SLEEP;
        }
        g_scheduler.power_mode = POWER_MODE_ACTIVE;
    } else {
        // No tasks to wake up - enter deep sleep
        g_scheduler.power_mode = POWER_MODE_DEEP_SLEEP;
        // In real hardware: hal_power_deep_sleep(MAX_DEEP_SLEEP)
        // For simulator, just yield
        task_yield();
    }
}

void scheduler_exit_idle(void) {
    g_scheduler.power_mode = POWER_MODE_ACTIVE;
}

int scheduler_enter_deep_sleep(uint32_t timeout_ticks) {
    if (!g_scheduler.tickless_idle_enabled) return -1;
    
    g_scheduler.power_mode = POWER_MODE_DEEP_SLEEP;
    
    // In real hardware: hal_power_deep_sleep(timeout_ticks)
    // For simulator:
    if (timeout_ticks > 0) {
        g_scheduler.tick_count += timeout_ticks;
    }
    
    return 0;
}

power_mode_t scheduler_get_power_mode(void) {
    return g_scheduler.power_mode;
}

void scheduler_set_deep_sleep_min_ticks(uint32_t ticks) {
    g_scheduler.deep_sleep_min_ticks = ticks;
}