/* Host test for keymap_rev.c: saved keymap edits are deleted exactly when the layout
 * revision changes, and the new revision is recorded only after a complete clean-up.
 * Run with run_host_tests.sh. */
#include <stdio.h>
#include <string.h>

#define CONFIG_CRUSH80_KEYMAP_REV 3
#include "../src/keymap_rev.c"
#include "mock/mock.h"

static int discards;
static int discard_result;

int zmk_keymap_discard_changes(void) {
    discards++;
    return discard_result;
}

static int failures;

#define CHECK(cond, ...)                                                                           \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            printf("FAIL line %d: ", __LINE__);                                                    \
            printf(__VA_ARGS__);                                                                   \
            printf("\n");                                                                          \
            failures++;                                                                            \
        }                                                                                          \
    } while (0)

static ssize_t read_int(void *cb_arg, void *data, size_t len) {
    memcpy(data, cb_arg, len);
    return len;
}

static void put(const char *name) {
    uint8_t v[8] = {1};

    mock_store_put(name, v, sizeof(v));
}

static void boot(int stored) { /* what main() does: load the revision, then commit */
    if (stored >= 0) {
        int rev = stored;

        mock_store_put(REV_KEY, &rev, sizeof(rev));
        rev_set("keymap_rev", sizeof(rev), read_int, &rev);
    } else {
        stored_rev = -1;
    }
    mock_lock_held = 1; /* the commit handler runs under the settings lock ... */
    crush80_commit_handler();
    mock_advance(100); /* ... and the system work queue runs at once meanwhile */
    mock_lock_held = 0;
    mock_advance(10000);
}

static void fresh(void) {
    mock_reset_clock();
    mock_settings_reset();
    stored_rev = -1;
    discards = 0;
    discard_result = 0;
}

static int saved_rev(void) {
    int rev = -1;

    for (int i = 0; i < mock_store_count; i++) {
        if (strcmp(mock_store[i].name, REV_KEY) == 0) {
            memcpy(&rev, mock_store[i].value, sizeof(rev));
        }
    }
    return rev;
}

static void populate_old_layout(void) {
    put("keymap/l/0/0");
    put("keymap/l/0/87");
    put("keymap/l/7/12");
    put("keymap/l/31/127");
    put("keymap/l_n/2");
    put("keymap/layer_order");
    put("via_macro/buf"); /* must survive */
    put("rgb/state");     /* must survive */
}

static void test_clean_migration(void) {
    fresh();
    populate_old_layout();
    boot(2);
    CHECK(mock_deadlocks == 0, "migration touched settings while the commit handler held the lock");
    CHECK(mock_deleted_count == 6, "deleted %d, want 6", mock_deleted_count);
    CHECK(!mock_store_has("keymap/l/7/12") && !mock_store_has("keymap/layer_order"), "edits left");
    CHECK(mock_store_has("via_macro/buf") && mock_store_has("rgb/state"), "other data touched");
    CHECK(saved_rev() == 3, "revision saved as %d", saved_rev());
    CHECK(discards == 1, "keymap reloaded %d times", discards);
}

static void test_first_boot(void) {
    fresh();
    populate_old_layout();
    boot(-1); /* no revision stored yet: an old firmware's edits are cleared */
    CHECK(mock_deleted_count == 6 && saved_rev() == 3, "first boot: deleted %d rev %d",
          mock_deleted_count, saved_rev());
}

static void test_same_revision_keeps_edits(void) {
    fresh();
    populate_old_layout();
    boot(3);
    CHECK(mock_deleted_count == 0 && discards == 0, "a normal update must keep live edits");
}

static void test_runs_once(void) {
    fresh();
    populate_old_layout();
    boot(2);
    put("keymap/l/1/1"); /* a new edit after the migration */
    crush80_commit_handler(); /* a later settings commit */
    mock_advance(10);
    CHECK(mock_store_has("keymap/l/1/1"), "a second commit deleted new edits");
}

static void test_failed_delete_retries(void) {
    fresh();
    populate_old_layout();
    mock_delete_fail_count = 1;
    boot(2);
    CHECK(saved_rev() == 2, "the revision was saved over a partial clean-up (%d)", saved_rev());
    /* next boot: the rest is still there, stored revision is still the old one */
    mock_deleted_count = 0;
    stored_rev = -1;
    boot(2);
    CHECK(saved_rev() == 3, "second attempt should finish (rev %d)", saved_rev());
}

static void test_out_of_range_names(void) {
    fresh();
    put("keymap/l/32/0");   /* layer past MAX_LAYERS: ignored, not a crash */
    put("keymap/l/0/128");  /* position past MAX_POS */
    put("keymap/l/x/y");
    put("keymap/lx/0/0");
    put("keymap/l_n");
    boot(2);
    CHECK(saved_rev() == 3, "unparseable names must not block the migration");
}

static void test_reload_failure(void) {
    fresh();
    populate_old_layout();
    discard_result = -22;
    boot(2);
    CHECK(discards == 1, "reload attempted");
}

int main(void) {
    test_clean_migration();
    test_first_boot();
    test_same_revision_keeps_edits();
    test_runs_once();
    test_failed_delete_retries();
    test_out_of_range_names();
    test_reload_failure();
    if (failures) {
        printf("%d check(s) failed\n", failures);
        return 1;
    }
    printf("keymap_rev: all checks passed\n");
    return 0;
}
