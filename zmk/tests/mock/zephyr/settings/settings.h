/* Host stand-in for Zephyr settings: a flat name -> value store with failure injection. */
#pragma once

#include <stddef.h>
#include <sys/types.h>

typedef ssize_t (*settings_read_cb)(void *cb_arg, void *data, size_t len);

#define SETTINGS_STATIC_HANDLER_DEFINE(name, subtree, get, set, commit, export)                    \
    const void *name##_set_handler __attribute__((unused)) = (const void *)(set);                  \
    int (*name##_commit_handler)(void) __attribute__((unused)) = (commit)

int settings_save_one(const char *name, const void *value, size_t len);
int settings_delete(const char *name);
int settings_load_subtree_direct(const char *subtree,
                                 int (*cb)(const char *key, size_t len, settings_read_cb read_cb,
                                           void *cb_arg, void *param),
                                 void *param);
int settings_name_steq(const char *name, const char *key, const char **next);
