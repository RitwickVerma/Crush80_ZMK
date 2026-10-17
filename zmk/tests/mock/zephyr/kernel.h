/* Host stand-in for the parts of Zephyr's kernel API the firmware sources under test use.
 * Delayable work runs on a virtual clock driven by mock_advance(). */
#pragma once

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <zephyr/sys/util.h> /* the real kernel.h pulls this in too */

typedef struct {
    int64_t ms;
} k_timeout_t;

#define K_MSEC(x) ((k_timeout_t){(x)})
#define K_SECONDS(x) K_MSEC((int64_t)(x) * 1000)
#define K_NO_WAIT K_MSEC(0)
#define K_FOREVER K_MSEC(-1)

struct k_work {
    int unused;
};

struct k_work_delayable {
    void (*handler)(struct k_work *);
    struct k_work work;
    int64_t due;
    bool pending;
    bool registered;
};

#define K_WORK_DELAYABLE_DEFINE(name, fn) struct k_work_delayable name = {.handler = (fn)}
#define K_WORK_DEFINE(name, fn) struct k_work_delayable name = {.handler = (fn)}

struct k_mutex {
    int depth;
};

#define K_MUTEX_DEFINE(name) struct k_mutex name

int64_t k_uptime_get(void);
int k_work_reschedule(struct k_work_delayable *w, k_timeout_t delay);
int k_work_schedule(struct k_work_delayable *w, k_timeout_t delay);
int k_work_submit(struct k_work_delayable *w);
int k_work_cancel_delayable(struct k_work_delayable *w);

static inline int k_mutex_lock(struct k_mutex *m, k_timeout_t t) {
    (void)t;
    m->depth++;
    return 0;
}

static inline int k_mutex_unlock(struct k_mutex *m) {
    m->depth--;
    return 0;
}

/* Test side: run due work in order until the virtual clock reaches now + ms. */
void mock_advance(int64_t ms);
void mock_reset_clock(void);
