#define _POSIX_C_SOURCE 200809L   /* for mkdtemp */
/* test_errors: every dialog has real text, the queue behaves, the log is written. */
#include "../src/errors.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails;
#define CHECK(c, msg) do { if (!(c)) { printf("FAIL: %s\n", msg); fails++; } } while (0)

int main(void)
{
    for (int c = 1; c < ERR_COUNT; c++) {
        const ErrInfo *e = err_info((ErrCode)c);
        char m[160];
        snprintf(m, sizeof m, "code %d has title", c);       CHECK(e->title && e->title[0], m);
        snprintf(m, sizeof m, "code %d has explanation", c); CHECK(e->what && strlen(e->what) > 20, m);
        snprintf(m, sizeof m, "code %d has a tip", c);       CHECK(e->tips[0] != NULL, m);
        snprintf(m, sizeof m, "code %d has a button", c);    CHECK(e->button[0] != NULL, m);
        snprintf(m, sizeof m, "code %d matches its slot", c); CHECK(e->code == (ErrCode)c, m);
        /* the dialog box draws 44 characters per line; no tip may be absurdly long */
        for (int i = 0; i < ERR_MAX_TIPS && e->tips[i]; i++) {
            snprintf(m, sizeof m, "code %d tip %d under 100 chars", c, i);
            CHECK(strlen(e->tips[i]) < 100, m);
        }
    }

    char dir[64], logpath[100];
    snprintf(dir, sizeof dir, "/tmp/shooter-upper-test-XXXXXX");
    if (!mkdtemp(dir)) { printf("FAIL: could not make a temporary folder\n"); return 1; }
    snprintf(logpath, sizeof logpath, "%s/log.txt", dir);
    err_log_open(dir);
    CHECK(!err_pending(), "queue starts empty");
    err_report(ERR_AUDIO_DEVICE, "no device found");
    err_report(ERR_AUDIO_DEVICE, "again");                 /* same code, within 30 s: not queued twice */
    err_report(ERR_SLOW, NULL);
    CHECK(err_pending() && err_current() == ERR_AUDIO_DEVICE, "first reported shows first");
    CHECK(!strcmp(err_current_detail(), "no device found"), "detail kept");
    err_dismiss();
    CHECK(err_current() == ERR_SLOW, "second follows");
    err_dismiss();
    CHECK(!err_pending(), "queue empty again");
    err_report(ERR_AUDIO_DEVICE, "later");
    CHECK(!err_pending(), "repeat inside 30 s is suppressed");
    for (int i = 0; i < 60 * 31; i++) err_tick();
    err_report(ERR_AUDIO_DEVICE, "much later");
    CHECK(err_pending(), "allowed again after 30 s");

    char buf[800];
    err_format_plain(ERR_MUSIC_FILE, "/home/x/.config/shooter-upper/theme.mp3", buf, sizeof buf);
    CHECK(strstr(buf, "theme.mp3") && strstr(buf, "What you can try"), "plain text has detail and tips");

    FILE *f = fopen(logpath, "r");
    CHECK(f != NULL, "log written");
    if (f) { char l[400]; int lines = 0; while (fgets(l, sizeof l, f)) lines++; fclose(f); CHECK(lines >= 4, "every report logged, even suppressed ones"); }

    { char cmd[100]; snprintf(cmd, sizeof cmd, "rm -rf '%s'", dir); (void)!system(cmd); }
    printf(fails ? "%d FAILED\n" : "all error checks passed\n", fails);
    return fails ? 1 : 0;
}
