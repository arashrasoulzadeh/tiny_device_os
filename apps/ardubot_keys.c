#include "ardubot_keys.h"

sim_key_class_t sim_key_get_class(sim_key_t key) {
    switch (key) {
        case SIM_KEY_SYS_NEXT_APP:
        case SIM_KEY_SYS_ESCAPE:
        case SIM_KEY_SYS_MENU:
            return SIM_KEY_CLASS_SYSTEM;
        default:
            return SIM_KEY_CLASS_APP;
    }
}

bool sim_key_is_system(sim_key_t key) {
    return sim_key_get_class(key) == SIM_KEY_CLASS_SYSTEM;
}
