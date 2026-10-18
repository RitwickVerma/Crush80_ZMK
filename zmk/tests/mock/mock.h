#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MOCK_MAX_SETTINGS 256

struct mock_setting {
    char name[48];
    uint8_t value[1024];
    size_t len;
};

extern struct mock_setting mock_store[MOCK_MAX_SETTINGS];
extern int mock_store_count;
extern int mock_save_error;
extern int mock_delete_fail_count;
extern char mock_deleted[MOCK_MAX_SETTINGS][48];
extern int mock_deleted_count;
extern int mock_saved_count;
/* Zephyr runs settings commit handlers while holding the settings lock. A settings call
 * made from another thread while it is held would block until the handler returns. */
extern int mock_lock_held;
extern int mock_deadlocks;
extern void (*mock_on_save)(const char *name, const void *value, size_t len);

void mock_store_put(const char *name, const void *value, size_t len);
bool mock_store_has(const char *name);
void mock_settings_reset(void);
