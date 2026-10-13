/*
 * Programming Assignment 02: ls-v1.4.0
 * Re-engineered ls utility (version 1.4.0)
 *
 * Usage:
 *       $ ./bin/ls
 *       $ ./bin/ls /home /etc
 *       $ ./bin/ls -lx [directory...]
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <time.h>
#include <pwd.h>
#include <grp.h>
#include <sys/stat.h>
#include <sys/ioctl.h>


enum {
    MODE_DEFAULT,
    MODE_HORIZONTAL,
    MODE_LONG
};

static int display_mode = MODE_DEFAULT;

/* qsort comparator: names are char* elements of the array */
static int cmp_names(const void *a, const void *b)
{
    const char *x = *(const char *const *)a;
    const char *y = *(const char *const *)b;
    return strcmp(x, y);
}

/* Terminal width via ioctl(TIOCGWINSZ); falls back to 80 columns */
static int term_width(void)
{
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0)
        return ws.ws_col;
    return 80;
}

/* Build "dir/name" without producing a double slash */
static void build_path(char *buf, size_t size, const char *dir, const char *name)
{
    size_t len = strlen(dir);
    if (len > 0 && dir[len - 1] == '/')
        snprintf(buf, size, "%s%s", dir, name);
    else
        snprintf(buf, size, "%s/%s", dir, name);
}

/* Convert st_mode to a string like "drwxr-xr-x" using bitwise operators */
static void mode_string(mode_t m, char *s)
{
    if (S_ISDIR(m))       s[0] = 'd';
    else if (S_ISLNK(m))  s[0] = 'l';
    else if (S_ISCHR(m))  s[0] = 'c';
    else if (S_ISBLK(m))  s[0] = 'b';
    else if (S_ISFIFO(m)) s[0] = 'p';
    else if (S_ISSOCK(m)) s[0] = 's';
    else                  s[0] = '-';

    s[1] = (m & S_IRUSR) ? 'r' : '-';
    s[2] = (m & S_IWUSR) ? 'w' : '-';
    s[3] = (m & S_ISUID) ? ((m & S_IXUSR) ? 's' : 'S') : ((m & S_IXUSR) ? 'x' : '-');
    s[4] = (m & S_IRGRP) ? 'r' : '-';
    s[5] = (m & S_IWGRP) ? 'w' : '-';
    s[6] = (m & S_ISGID) ? ((m & S_IXGRP) ? 's' : 'S') : ((m & S_IXGRP) ? 'x' : '-');
    s[7] = (m & S_IROTH) ? 'r' : '-';
    s[8] = (m & S_IWOTH) ? 'w' : '-';
    s[9] = (m & S_ISVTX) ? ((m & S_IXOTH) ? 't' : 'T') : ((m & S_IXOTH) ? 'x' : '-');
    s[10] = '\0';
}


/* Print one file name (colored when enabled). Does NOT print padding. */
static void print_name(const char *dir, const char *name)
{
    (void)dir;
    fputs(name, stdout);
}

/* One line of the -l format */
static void print_long_entry(const char *dir, const char *name)
{
    char path[PATH_MAX];
    struct stat st;
    build_path(path, sizeof path, dir, name);
    if (lstat(path, &st) == -1) {
        perror(path);
        return;
    }

    char perm[11];
    mode_string(st.st_mode, perm);

    char owner[32], group[32];
    struct passwd *pw = getpwuid(st.st_uid);
    struct group  *gr = getgrgid(st.st_gid);
    if (pw) snprintf(owner, sizeof owner, "%s", pw->pw_name);
    else    snprintf(owner, sizeof owner, "%u", (unsigned)st.st_uid);
    if (gr) snprintf(group, sizeof group, "%s", gr->gr_name);
    else    snprintf(group, sizeof group, "%u", (unsigned)st.st_gid);

    char timebuf[32];
    struct tm *tm = localtime(&st.st_mtime);
    strftime(timebuf, sizeof timebuf, "%b %e %H:%M", tm);

    printf("%s %3lu %-8s %-8s %8lld %s ", perm, (unsigned long)st.st_nlink,
           owner, group, (long long)st.st_size, timebuf);
    print_name(dir, name);

    if (S_ISLNK(st.st_mode)) {
        char target[PATH_MAX];
        ssize_t n = readlink(path, target, sizeof target - 1);
        if (n != -1) {
            target[n] = '\0';
            printf(" -> %s", target);
        }
    }
    putchar('\n');
}

/* Read all visible entries into a dynamically growing array of strings.
 * Returns NULL on failure. Caller must call free_entries(). */
static char **read_entries(const char *dir, size_t *count, size_t *maxlen)
{
    DIR *dp = opendir(dir);
    if (dp == NULL) {
        fprintf(stderr, "Cannot open directory : %s\n", dir);
        return NULL;
    }

    size_t cap = 16, n = 0, ml = 0;
    char **names = malloc(cap * sizeof *names);
    if (names == NULL) {
        perror("malloc");
        closedir(dp);
        return NULL;
    }

    struct dirent *entry;
    errno = 0;
    while ((entry = readdir(dp)) != NULL) {
        if (entry->d_name[0] == '.')
            continue;
        if (n == cap) {
            cap *= 2;
            char **tmp = realloc(names, cap * sizeof *names);
            if (tmp == NULL) {
                perror("realloc");
                break;
            }
            names = tmp;
        }
        names[n] = strdup(entry->d_name);
        if (names[n] == NULL) {
            perror("strdup");
            break;
        }
        size_t len = strlen(entry->d_name);
        if (len > ml) ml = len;
        n++;
    }
    if (errno != 0)
        perror("readdir failed");

    closedir(dp);
    *count = n;
    *maxlen = ml;
    return names;
}

static void free_entries(char **names, size_t count)
{
    for (size_t i = 0; i < count; i++)
        free(names[i]);
    free(names);
}

/* "total N" line used by -l (blocks are 512 bytes, ls shows 1K blocks) */
static long long total_blocks(const char *dir, char **names, size_t count)
{
    long long total = 0;
    char path[PATH_MAX];
    struct stat st;
    for (size_t i = 0; i < count; i++) {
        build_path(path, sizeof path, dir, names[i]);
        if (lstat(path, &st) == 0)
            total += st.st_blocks;
    }
    return total / 2;
}

static void pad_spaces(int n)
{
    while (n-- > 0)
        putchar(' ');
}

/* Default layout: "down then across" */
static void print_columns(const char *dir, char **names, size_t count, size_t maxlen)
{
    if (count == 0)
        return;
    int width = term_width();
    int colw = (int)maxlen + 2;
    int cols = width / colw;
    if (cols < 1) cols = 1;
    size_t rows = (count + cols - 1) / cols;

    for (size_t r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            size_t idx = (size_t)c * rows + r;
            if (idx >= count)
                break;
            print_name(dir, names[idx]);
            if (idx + rows < count)               /* another column follows */
                pad_spaces(colw - (int)strlen(names[idx]));
        }
        putchar('\n');
    }
}

/* -x layout: left to right, wrap when the next column will not fit */
static void print_horizontal(const char *dir, char **names, size_t count, size_t maxlen)
{
    int width = term_width();
    int colw = (int)maxlen + 2;
    int pos = 0;

    for (size_t i = 0; i < count; i++) {
        if (pos > 0 && pos + colw > width) {
            putchar('\n');
            pos = 0;
        }
        print_name(dir, names[i]);
        pad_spaces(colw - (int)strlen(names[i]));
        pos += colw;
    }
    if (pos > 0)
        putchar('\n');
}

static void do_ls(const char *dir)
{
    size_t count = 0, maxlen = 0;
    char **names = read_entries(dir, &count, &maxlen);
    if (names == NULL)
        return;

    qsort(names, count, sizeof *names, cmp_names);

    if (display_mode == MODE_LONG) {
        printf("total %lld\n", total_blocks(dir, names, count));
        for (size_t i = 0; i < count; i++)
            print_long_entry(dir, names[i]);
    }
    else if (display_mode == MODE_HORIZONTAL)
        print_horizontal(dir, names, count, maxlen);
    else
        print_columns(dir, names, count, maxlen);


    free_entries(names, count);
}

int main(int argc, char *argv[])
{
    int opt;
    while ((opt = getopt(argc, argv, "lx")) != -1) {
        switch (opt) {
        case 'l':
            display_mode = MODE_LONG;
            break;
        case 'x':
            if (display_mode != MODE_LONG)
                display_mode = MODE_HORIZONTAL;
            break;
        default:
            fprintf(stderr, "Usage: %s [-lx] [directory...]\n", argv[0]);
            return 1;
        }
    }


    int ndirs = argc - optind;
    if (ndirs == 0) {
        do_ls(".");
    } else {
        for (int i = optind; i < argc; i++) {
            if (ndirs > 1)
                printf("%s:\n", argv[i]);
            do_ls(argv[i]);
            if (i < argc - 1)
                putchar('\n');
        }
    }
    return 0;
}
