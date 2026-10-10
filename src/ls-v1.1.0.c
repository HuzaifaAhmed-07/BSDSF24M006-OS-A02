/*
 * Programming Assignment 02: ls-v1.1.0
 * Re-engineered ls utility (version 1.1.0)
 *
 * Usage:
 *       $ ./bin/ls
 *       $ ./bin/ls /home /etc
 *       $ ./bin/ls -l [directory...]
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


enum {
    MODE_DEFAULT,
    MODE_LONG
};

static int display_mode = MODE_DEFAULT;



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



static void do_ls(const char *dir)
{
    struct dirent *entry;
    DIR *dp = opendir(dir);
    if (dp == NULL) {
        fprintf(stderr, "Cannot open directory : %s\n", dir);
        return;
    }
    errno = 0;
    while ((entry = readdir(dp)) != NULL) {
        if (entry->d_name[0] == '.')
            continue;
        if (display_mode == MODE_LONG)
            print_long_entry(dir, entry->d_name);
        else
            printf("%s\n", entry->d_name);
    }
    if (errno != 0)
        perror("readdir failed");
    closedir(dp);
}

int main(int argc, char *argv[])
{
    int opt;
    while ((opt = getopt(argc, argv, "l")) != -1) {
        switch (opt) {
        case 'l':
            display_mode = MODE_LONG;
            break;
        default:
            fprintf(stderr, "Usage: %s [-l] [directory...]\n", argv[0]);
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
