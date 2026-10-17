#pragma once
#include <stdint.h>

struct zmk_behavior_binding {
    uint32_t param1, param2;
};

struct zmk_behavior_binding_event {
    int unused;
};

struct behavior_driver_api {
    int (*binding_pressed)(struct zmk_behavior_binding *, struct zmk_behavior_binding_event);
    int (*binding_released)(struct zmk_behavior_binding *, struct zmk_behavior_binding_event);
};

#define DT_HAS_COMPAT_STATUS_OKAY(x) 1
#define POST_KERNEL 0
#define CONFIG_KERNEL_INIT_PRIORITY_DEFAULT 0
#define BEHAVIOR_DT_INST_DEFINE(n, init, pm, data, config, level, prio, api)                       \
    const struct behavior_driver_api *mock_behavior_api = (api)
