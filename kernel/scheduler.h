#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_TASKS 16
#define TASK_NAME_LEN 16

typedef enum {
    TASK_STATE_READY = 0,
    TASK_STATE_RUNNING,
    TASK_STATE_BLOCKED,
    TASK_STATE_SUSPENDED,
    TASK_STATE_TERMINATED
} task_state_t;

typedef enum {
    TASK_PRIO_IDLE = 0,
    TASK_PRIO_LOW = 1,
    TASK_PRIO_NORMAL = 2,
    TASK_PRIO_HIGH = 3,
    TASK_PRIO_CRITICAL = 4
} task_priority_t;

typedef void (*task_entry_t)(void* arg);

typedef struct task_ctrl_block {
    uint8_t id;
    char name[TASK_NAME_LEN];
    task_state_t state;
    task_priority_t priority;
    task_entry_t entry;
    void* arg;
    
    uint32_t* stack_base;
    uintptr_t* stack_ptr;
    size_t stack_size;
    
    uint32_t wake_time;
    uint32_t sleep_ticks;
    
    struct task_ctrl_block* next;
    struct task_ctrl_block* prev;
} task_tcb_t;

typedef enum {
    POWER_MODE_ACTIVE = 0,
    POWER_MODE_IDLE,
    POWER_MODE_LIGHT_SLEEP,
    POWER_MODE_DEEP_SLEEP
} power_mode_t;

typedef struct {
    task_tcb_t tasks[MAX_TASKS];
    task_tcb_t* current;
    task_tcb_t* ready_list[TASK_PRIO_CRITICAL + 1];
    task_tcb_t* blocked_list;
    task_tcb_t* free_list;
    
    uint32_t tick_count;
    uint32_t next_task_id;
    bool scheduler_started;
    bool in_isr;
    uint8_t nesting_level;
    
    // Tickless idle
    bool tickless_idle_enabled;
    uint32_t next_wake_tick;
    uint32_t idle_tick_count;
    power_mode_t power_mode;
    uint32_t deep_sleep_min_ticks;
} scheduler_t;

int scheduler_init(void);
int scheduler_start(void);

int task_create(const char* name, task_entry_t entry, void* arg, 
                task_priority_t priority, size_t stack_size, task_tcb_t** out_task);
int task_delete(task_tcb_t* task);

void task_yield(void);
void task_sleep(uint32_t ticks);
void task_suspend(task_tcb_t* task);
void task_resume(task_tcb_t* task);

task_tcb_t* task_get_current(void);
const char* task_get_name(task_tcb_t* task);
task_state_t task_get_state(task_tcb_t* task);
task_priority_t task_get_priority(task_tcb_t* task);

uint32_t scheduler_get_tick_count(void);
void scheduler_tick(void);

void scheduler_lock(void);
void scheduler_unlock(void);

bool scheduler_is_in_isr(void);
void scheduler_enter_isr(void);
void scheduler_exit_isr(void);

void idle_task(void* arg);

// Tickless idle API
void scheduler_enable_tickless_idle(bool enable);
uint32_t scheduler_get_next_wake_tick(void);
void scheduler_enter_idle(void);
void scheduler_exit_idle(void);

// Deep sleep
int scheduler_enter_deep_sleep(uint32_t timeout_ticks);

#ifdef __cplusplus
}
#endif