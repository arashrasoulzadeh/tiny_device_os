#include "sim_net.h"
#include <stdlib.h>

static sim_net_callback_t g_callback = NULL;
static void* g_callback_arg = NULL;
static bool g_connected = false;

int sim_net_init(void) {
    g_connected = true;
    return 0;
}

void sim_net_cleanup(void) {
    g_connected = false;
}

int sim_net_send(const uint8_t* data, size_t len) {
    (void)data; (void)len;
    return 0;
}

void sim_net_set_callback(sim_net_callback_t cb, void* arg) {
    g_callback = cb;
    g_callback_arg = arg;
}

bool sim_net_is_connected(void) {
    return g_connected;
}