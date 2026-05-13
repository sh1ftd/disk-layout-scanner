#include "core/disk_info.h"

#ifndef _WIN32
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#define REPORT_PATH_MAX 4096
#else
#define REPORT_PATH_MAX MAX_PATH
#endif

static void print_usage(FILE* fp) {
    fprintf(fp,
        "disk-layout-scanner %s - cross-platform disk layout and identity report\n\n"
        "Usage: disk-layout-scanner [options]\n\n"
        "Options:\n"
        "  (default)      HTML report as report.html next to this program, open in browser\n"
        "  --text          Plain text to standard output\n"
        "  --json          JSON to standard output\n"
        "  --html PATH     Write HTML to PATH (does not open a browser)\n"
        "  --no-open       Do not open a browser (with default HTML output)\n"
        "  -h, --help      Show this help\n"
        "  -V, --version   Print version\n",
        DISK_LAYOUT_SCANNER_VERSION);
}

#ifndef _WIN32
static void try_open_html_unix(const char* path) {
    pid_t pid = fork();
    if (pid == -1)
        return;
    if (pid == 0) {
#if (defined(__GLIBC__) && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 34))) \
    || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__DragonFly__)
        closefrom(3);
#else
        {
            long mx = sysconf(_SC_OPEN_MAX);
            int lim = (mx > 0 && mx < 65536) ? (int)mx : 1024;
            for (int fd = 3; fd < lim; fd++) (void)close(fd);
        }
#endif
        execlp("xdg-open", "xdg-open", path, (char*)NULL);
        execlp("wslview", "wslview", path, (char*)NULL);
        execlp("open", "open", path, (char*)NULL);
        _exit(127);
    }
    int st = 0;
    waitpid(pid, &st, 0);
}
#endif

int main(int argc, char* argv[]) {
    int mode = 2;
    const char* html_file = NULL;
    int auto_open = 1;
    int seen_out = -1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(stdout);
            return 0;
        }
        if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-V") == 0) {
            printf("%s\n", DISK_LAYOUT_SCANNER_VERSION);
            return 0;
        }
        if (strcmp(argv[i], "--no-open") == 0) {
            auto_open = 0;
            continue;
        }
        if (strcmp(argv[i], "--text") == 0) {
            if (seen_out >= 0 && seen_out != 0) {
                fprintf(stderr, "disk-layout-scanner: use only one of --text, --json, or --html\n");
                return 2;
            }
            seen_out = 0;
            mode = 0;
            continue;
        }
        if (strcmp(argv[i], "--json") == 0) {
            if (seen_out >= 0 && seen_out != 1) {
                fprintf(stderr, "disk-layout-scanner: use only one of --text, --json, or --html\n");
                return 2;
            }
            seen_out = 1;
            mode = 1;
            continue;
        }
        if (strcmp(argv[i], "--html") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "disk-layout-scanner: --html requires a path\n");
                return 2;
            }
            if (seen_out >= 0 && seen_out != 2) {
                fprintf(stderr, "disk-layout-scanner: use only one of --text, --json, or --html\n");
                return 2;
            }
            seen_out = 2;
            mode = 2;
            html_file = argv[++i];
            auto_open = 0;
            continue;
        }
        fprintf(stderr, "disk-layout-scanner: unknown option '%s'\n", argv[i]);
        fprintf(stderr, "Try 'disk-layout-scanner --help'.\n");
        return 2;
    }

    char default_html[REPORT_PATH_MAX];
    memset(default_html, 0, sizeof(default_html));
    if (mode == 2 && !html_file) {
#ifdef _WIN32
        DWORD gmlen = GetModuleFileNameA(NULL, default_html, REPORT_PATH_MAX);
        if (gmlen == 0 || gmlen == REPORT_PATH_MAX) {
            snprintf(default_html, sizeof(default_html), "report.html");
        } else {
            char* last_sep = strrchr(default_html, '\\');
            if (last_sep) {
                *(last_sep + 1) = '\0';
                snprintf(last_sep + 1, sizeof(default_html) - (size_t)(last_sep + 1 - default_html),
                         "report.html");
            } else {
                snprintf(default_html, sizeof(default_html), "report.html");
            }
        }
#else
        ssize_t len = readlink("/proc/self/exe", default_html, sizeof(default_html) - 1);
        if (len > 0) {
            default_html[len] = '\0';
            char* last_sep = strrchr(default_html, '/');
            if (last_sep) {
                *(last_sep + 1) = '\0';
                snprintf(last_sep + 1, sizeof(default_html) - (size_t)(last_sep + 1 - default_html),
                         "report.html");
            } else {
                snprintf(default_html, sizeof(default_html), "report.html");
            }
        } else {
            snprintf(default_html, sizeof(default_html), "report.html");
        }
#endif
        html_file = default_html;
    }

    DiskInfo* disks = calloc((size_t)MAX_DRIVES, sizeof(DiskInfo));
    if (!disks) {
        fprintf(stderr, "disk-layout-scanner: out of memory\n");
        return 1;
    }

    int count = scan_disks(disks, MAX_DRIVES);
    int rc = 0;

    switch (mode) {
        case 0:
            output_text(disks, count);
            break;
        case 1:
            output_json(disks, count, stdout);
            if (fflush(stdout) != 0 || ferror(stdout)) {
                fprintf(stderr, "disk-layout-scanner: error writing JSON\n");
                rc = 1;
            }
            break;
        case 2:
            if (output_html(disks, count, html_file) != 0) {
                rc = 1;
                break;
            }
            printf("Report: %s\n", html_file);
            if (auto_open) {
#ifdef _WIN32
                INT_PTR sh = (INT_PTR)ShellExecuteA(NULL, "open", html_file, NULL, NULL, SW_SHOWNORMAL);
                if (sh <= 32)
                    fprintf(stderr, "disk-layout-scanner: could not open report in browser (code %lld)\n",
                            (long long)sh);
#else
                try_open_html_unix(html_file);
#endif
            }
            break;
    }

    free(disks);
    return rc;
}
