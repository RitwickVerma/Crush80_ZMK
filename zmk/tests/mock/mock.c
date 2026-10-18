/* Host implementation of the mock kernel and settings APIs (see the headers beside it). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/settings/settings.h>

#include "mock.h"

/* ---- virtual clock and work ---------------------------------------------------------- */

#define MAX_WORKS 16

static int64_t now;
static struct k_work_delayable *works[MAX_WORKS];
static int work_count;

int64_t k_uptime_get(void) {
    return now;
}

void mock_reset_clock(void) {
    now = 0;
    for (int i = 0; i < work_count; i++) {
        works[i]->pending = false;
    }
}

static void arm(struct k_work_delayable *w, k_timeout_t delay) {
    if (!w->registered) {
        if (work_count == MAX_WORKS) {
            abort();
        }
        works[work_count++] = w;
        w->registered = true;
    }
    w->due = now + (delay.ms < 0 ? 0 : delay.ms);
    w->pending = true;
}

int k_work_reschedule(struct k_work_delayable *w, k_timeout_t delay) {
    arm(w, delay);
    return 0;
}

int k_work_schedule(struct k_work_delayable *w, k_timeout_t delay) {
    if (!w->pending) {
        arm(w, delay);
    }
    return 0;
}

int k_work_submit(struct k_work_delayable *w) {
    return k_work_schedule(w, K_NO_WAIT);
}

int k_work_cancel_delayable(struct k_work_delayable *w) {
    w->pending = false;
    return 0;
}

void mock_advance(int64_t ms) {
    int64_t end = now + ms;

    for (;;) {
        struct k_work_delayable *next = NULL;

        for (int i = 0; i < work_count; i++) {
            if (works[i]->pending && works[i]->due <= end && (!next || works[i]->due < next->due)) {
                next = works[i];
            }
        }
        if (!next) {
            break;
        }
        if (next->due > now) {
            now = next->due;
        }
        next->pending = false;
        next->handler(&next->work);
    }
    now = end;
}

/* ---- settings -------------------------------------------------------------------------- */

struct mock_setting mock_store[MOCK_MAX_SETTINGS];
int mock_store_count;
int mock_save_error;
int mock_delete_fail_count; /* fail this many settings_delete() calls, then recover */
char mock_deleted[MOCK_MAX_SETTINGS][48];
int mock_deleted_count;
int mock_saved_count;
int mock_lock_held;
int mock_deadlocks;
void (*mock_on_save)(const char *name, const void *value, size_t len);

static struct mock_setting *find(const char *name) {
    for (int i = 0; i < mock_store_count; i++) {
        if (strcmp(mock_store[i].name, name) == 0) {
            return &mock_store[i];
        }
    }
    return NULL;
}

void mock_store_put(const char *name, const void *value, size_t len) {
    struct mock_setting *s = find(name);

    if (!s) {
        if (mock_store_count == MOCK_MAX_SETTINGS) {
            abort();
        }
        s = &mock_store[mock_store_count++];
        snprintf(s->name, sizeof(s->name), "%s", name);
    }
    s->len = len;
    memcpy(s->value, value, len);
}

bool mock_store_has(const char *name) {
    return find(name) != NULL;
}

void mock_settings_reset(void) {
    mock_store_count = 0;
    mock_save_error = 0;
    mock_delete_fail_count = 0;
    mock_deleted_count = 0;
    mock_saved_count = 0;
    mock_lock_held = 0;
    mock_deadlocks = 0;
    mock_on_save = NULL;
}

int settings_save_one(const char *name, const void *value, size_t len) {
    if (mock_lock_held) {
        mock_deadlocks++;
        return -35;
    }
    if (mock_on_save) {
        mock_on_save(name, value, len);
    }
    mock_saved_count++;
    if (mock_save_error) {
        return mock_save_error;
    }
    mock_store_put(name, value, len);
    return 0;
}

int settings_delete(const char *name) {
    if (mock_lock_held) {
        mock_deadlocks++;
        return -35;
    }
    if (mock_delete_fail_count > 0) {
        mock_delete_fail_count--;
        return -5;
    }
    for (int i = 0; i < mock_store_count; i++) {
        if (strcmp(mock_store[i].name, name) == 0) {
            snprintf(mock_deleted[mock_deleted_count++], sizeof(mock_deleted[0]), "%s", name);
            mock_store[i] = mock_store[--mock_store_count];
            return 0;
        }
    }
    return 0; /* Zephyr treats deleting a missing key as success */
}

struct read_ctx {
    const struct mock_setting *s;
};

static ssize_t read_cb(void *cb_arg, void *data, size_t len) {
    const struct read_ctx *c = cb_arg;
    size_t n = len < c->s->len ? len : c->s->len;

    memcpy(data, c->s->value, n);
    return n;
}

int settings_load_subtree_direct(const char *subtree,
                                 int (*cb)(const char *key, size_t len, settings_read_cb rcb,
                                           void *cb_arg, void *param),
                                 void *param) {
    size_t n = strlen(subtree);

    if (mock_lock_held) {
        mock_deadlocks++;
        return -35;
    }
    /* Copy: the callback may delete entries. */
    struct mock_setting snap[MOCK_MAX_SETTINGS];
    int count = mock_store_count;

    memcpy(snap, mock_store, sizeof(snap[0]) * count);
    for (int i = 0; i < count; i++) {
        if (strncmp(snap[i].name, subtree, n) == 0 && snap[i].name[n] == '/') {
            struct read_ctx ctx = {&snap[i]};

            cb(snap[i].name + n + 1, snap[i].len, read_cb, &ctx, param);
        }
    }
    return 0;
}

/* Same contract as Zephyr's: true if name starts with key as a whole path element;
 * *next then points past the '/' (or is NULL at the end). */
int settings_name_steq(const char *name, const char *key, const char **next) {
    size_t n = strlen(key);

    if (next) {
        *next = NULL;
    }
    if (strncmp(name, key, n) != 0) {
        return 0;
    }
    if (name[n] == '\0') {
        return 1;
    }
    if (name[n] == '/') {
        if (next) {
            *next = name + n + 1;
        }
        return 1;
    }
    return 0;
}
