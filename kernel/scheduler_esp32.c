/* ArdubotOS scheduler backend for real ESP32/FreeRTOS hardware
 * (RISCV_TODO.md Phase 2).
 *
 * kernel/scheduler.c implements ArdubotOS's cooperative "fiber" scheduler:
 * each task gets its own stack via a raw sp-switch (host_stack.c) plus
 * setjmp/longjmp for later resumes. That model works fine on the host/sim
 * build (no real OS underneath to object), but ESP32-C6 (and presumably
 * other ESP32 RISC-V targets) runs real FreeRTOS underneath Arduino, and
 * FreeRTOS's hardware "assist_debug" stack-pointer monitor panics the
 * instant sp leaves the bounds registered for the currently running real
 * FreeRTOS task. Re-arming those bounds correctly on every raw switch
 * turned out to require bracketing interrupt-disable around every single
 * setjmp/longjmp resume point in scheduler.c - error prone, with a worse
 * failure mode (silent watchdog hang) than the loud panic it replaces if
 * any site were missed. See git history / RISCV_TODO.md for the dead end.
 *
 * This file implements the exact same scheduler.h API using REAL FreeRTOS
 * tasks (xTaskCreate) instead of fighting the hardware guard - each
 * ArdubotOS task is a genuine FreeRTOS task with its own FreeRTOS-managed
 * stack, so the guard's assumptions hold and nothing needs disabling.
 *
 * Scope: task_create/delete/yield/sleep/suspend/resume, scheduler_init/
 * start/tick, scheduler_lock/unlock (-> vTaskSuspendAll/xTaskResumeAll),
 * and task introspection all work. Tickless idle, deep sleep, and ISR
 * nesting tracking are NOT implemented here (no app uses them yet per a
 * repo-wide grep at the time this was written) - they return harmless
 * no-ops/failure codes rather than silently pretending to work; implement
 * for real if/when an app on this target actually needs them.
 */
#include "scheduler.h"
#include "power.h"
#include "host_stack.h" /* HOST_STACK_MIN_BYTES, unused here otherwise */
#include <string.h>
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static scheduler_t g_scheduler = {0};
static bool g_started = false;

static int find_free_slot(void) {
    for (int i = 0; i < MAX_TASKS; i++) {
        if (g_scheduler.tasks[i].name[0] == '\0') {
            return i;
        }
    }
    return -1;
}

/* FreeRTOS task entry trampoline: runs the ArdubotOS entry, then cleans
 * up and deletes itself - FreeRTOS tasks must never return. */
static void esp32_task_trampoline(void* arg) {
    task_tcb_t* task = (task_tcb_t*)arg;
    task->state = TASK_STATE_RUNNING;
    if (task->entry) {
        task->entry(task->arg);
    }
    task->state = TASK_STATE_TERMINATED;
    vTaskDelete(NULL);
}

int scheduler_init(void) {
    memset(&g_scheduler, 0, sizeof(scheduler_t));
    g_started = false;
    power_governor_reset();
    return 0;
}

int scheduler_start(void) {
    if (g_started) {
        return -1;
    }
    g_started = true;
    return 0;
}

static UBaseType_t to_freertos_priority(task_priority_t priority) {
    /* tskIDLE_PRIORITY is 0 on this port; ArdubotOS's own TASK_PRIO_IDLE
     * is also 0, so the enum maps directly without renormalizing. */
    return (UBaseType_t)priority;
}

int task_create(const char* name, task_entry_t entry, void* arg, task_priority_t priority,
                size_t stack_size, task_tcb_t** out_task) {
    if (priority > TASK_PRIO_CRITICAL) {
        return -1;
    }

    int slot = find_free_slot();
    if (slot < 0) {
        return -1;
    }

    /* Several stdapps (e.g. pomodoro_app.c's worker task) pass 0 here,
     * relying on the caller enforcing a floor the way kernel/scheduler.c's
     * task_create() already does - this file never did, so a 0-byte
     * request produced a 0-word FreeRTOS stack depth and xTaskCreate()
     * silently failed (confirmed on hardware, RISCV_TODO.md Phase 4/5:
     * pomodoro's countdown worker never started). */
    if (stack_size < HOST_STACK_MIN_BYTES) {
        stack_size = HOST_STACK_MIN_BYTES;
    }

    task_tcb_t* task = &g_scheduler.tasks[slot];
    memset(task, 0, sizeof(task_tcb_t));
    task->id = (uint8_t)slot;
    strncpy(task->name, name, TASK_NAME_LEN - 1);
    task->name[TASK_NAME_LEN - 1] = '\0';
    task->entry = entry;
    task->arg = arg;
    task->priority = priority;
    task->state = TASK_STATE_READY;
    task->stack_size = stack_size;

    /* FreeRTOS wants the stack depth in words, not bytes. */
    TaskHandle_t handle = NULL;
    BaseType_t ok = xTaskCreate(esp32_task_trampoline, name,
                                (uint32_t)(stack_size / sizeof(StackType_t)), task,
                                to_freertos_priority(priority), &handle);
    if (ok != pdPASS || !handle) {
        task->name[0] = '\0';
        return -1;
    }

    /* Repurpose host_fiber (sim/unit-test-only on other platforms, per
     * scheduler.h's own comment) to hold the real FreeRTOS handle here. */
    task->host_fiber = (void*)handle;

    if (out_task) {
        *out_task = task;
    }
    return 0;
}

int task_delete(task_tcb_t* task) {
    if (!task || !task->host_fiber) {
        return -1;
    }
    vTaskDelete((TaskHandle_t)task->host_fiber);
    task->name[0] = '\0';
    task->host_fiber = NULL;
    task->state = TASK_STATE_TERMINATED;
    return 0;
}

void task_yield(void) {
    taskYIELD();
}

void task_sleep(uint32_t ticks) {
    /* 1 ArdubotOS tick == 1ms, same convention the sim/host build uses
     * (see sim/sim_main.c's tick-pump loop and kernel_boot.cpp's loop()). */
    vTaskDelay(pdMS_TO_TICKS(ticks));
}

void task_suspend(task_tcb_t* task) {
    if (!task || !task->host_fiber) {
        return;
    }
    task->state = TASK_STATE_SUSPENDED;
    vTaskSuspend((TaskHandle_t)task->host_fiber);
}

void task_resume(task_tcb_t* task) {
    if (!task || !task->host_fiber) {
        return;
    }
    task->state = TASK_STATE_READY;
    vTaskResume((TaskHandle_t)task->host_fiber);
}

task_tcb_t* task_get_current(void) {
    TaskHandle_t current = xTaskGetCurrentTaskHandle();
    for (int i = 0; i < MAX_TASKS; i++) {
        if (g_scheduler.tasks[i].host_fiber == (void*)current) {
            return &g_scheduler.tasks[i];
        }
    }
    return NULL;
}

const char* task_get_name(task_tcb_t* task) {
    return task ? task->name : "";
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

uint32_t scheduler_get_idle_tick_count(void) {
    return g_scheduler.idle_tick_count;
}

static bool esp32_any_runnable(void) {
    for (int i = 0; i < MAX_TASKS; i++) {
        task_tcb_t* task = &g_scheduler.tasks[i];
        if (task->name[0] == '\0' || task->priority == TASK_PRIO_IDLE) {
            continue;
        }
        if (task->state == TASK_STATE_SUSPENDED || task->state == TASK_STATE_TERMINATED) {
            continue;
        }
        return true;
    }
    return false;
}

void scheduler_tick(void) {
    /* Real task scheduling is FreeRTOS's own job now (preemptive, driven
     * by its own hardware tick) - this just keeps the tick_count counter
     * available for any code that reads scheduler_get_tick_count(), and
     * services ArdubotOS's own (non-FreeRTOS) os_timer_t list, same as
     * the fiber backend's caller (sim_main.c / kernel_boot.cpp) expects. */
    g_scheduler.tick_count++;
    /* Next-wake is not tracked on this backend, so the gap stays unknown
     * and the governor will not recommend light sleep from an infinite wait. */
    power_governor_tick(g_scheduler.tick_count, UINT32_MAX, esp32_any_runnable());
}

void scheduler_lock(void) {
    vTaskSuspendAll();
}

void scheduler_unlock(void) {
    xTaskResumeAll();
}

bool scheduler_is_in_isr(void) {
    return false; /* not tracked on this backend - no caller needs it yet */
}

void scheduler_enter_isr(void) {
}

void scheduler_exit_isr(void) {
}

void idle_task(void* arg) {
    (void)arg; /* unused: FreeRTOS supplies its own idle task on this backend */
}

void scheduler_enable_tickless_idle(bool enable) {
    (void)enable; /* not implemented on this backend - see file header */
}

uint32_t scheduler_get_next_wake_tick(void) {
    return UINT32_MAX;
}

void scheduler_enter_idle(void) {
}

void scheduler_exit_idle(void) {
}

void scheduler_tickless_idle(void) {
}

power_mode_t scheduler_get_power_mode(void) {
    return POWER_MODE_ACTIVE;
}

void scheduler_set_deep_sleep_min_ticks(uint32_t ticks) {
    (void)ticks;
}

int scheduler_enter_deep_sleep(uint32_t timeout_ticks) {
    (void)timeout_ticks;
    return -1; /* not implemented on this backend - see file header */
}

int scheduler_step(void) {
    /* FreeRTOS preempts on its own. One sample per call lets the governor
     * see that an app task exists (busy) or that the board is idle. */
    if (esp32_any_runnable()) {
        power_governor_note_busy(task_get_current());
    } else {
        power_governor_note_idle();
    }
    return 0;
}

const task_tcb_t* scheduler_get_task_slots(int* out_count) {
    if (out_count) {
        *out_count = MAX_TASKS;
    }
    return g_scheduler.tasks;
}
