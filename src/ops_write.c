#define FUSE_USE_VERSION 31

#include <fuse3/fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <libgen.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#include "unionfs.h"

static int ensure_upper_parent_dirs(const char *path)
{
    struct mini_unionfs_state *state = UNIONFS_DATA;

    char tmp[PATH_MAX];
    snprintf(tmp, PATH_MAX, "%s%s", state->upper_dir, path);

    char dir_part[PATH_MAX];
    strncpy(dir_part, tmp, PATH_MAX - 1);
    dir_part[PATH_MAX - 1] = '\0';
    char *parent = dirname(dir_part);

    char build[PATH_MAX];
    strncpy(build, parent, PATH_MAX - 1);
    build[PATH_MAX - 1] = '\0';

    size_t prefix_len = strlen(state->upper_dir);
    char *p = build + prefix_len;

    while (*p) {
        char *slash = strchr(p + 1, '/');
        if (slash) *slash = '\0';

        if (mkdir(build, 0755) < 0 && errno != EEXIST)
            return -errno;

        if (slash) {
            *slash = '/';
            p = slash;
        } else {
            break;
        }
    }

    if (mkdir(build, 0755) < 0 && errno != EEXIST)
        return -errno;

    return 0;
}

int cow_copy(const char *path)
{
    struct mini_unionfs_state *state = UNIONFS_DATA;

    char lower_path[PATH_MAX], upper_path[PATH_MAX];
    snprintf(lower_path, PATH_MAX, "%s%s", state->lower_dir, path);
    snprintf(upper_path, PATH_MAX, "%s%s", state->upper_dir, path);

    if (access(upper_path, F_OK) == 0)
        return 0;

    struct stat st;
    if (stat(lower_path, &st) < 0)
        return -errno;

    int res = ensure_upper_parent_dirs(path);
    if (res < 0)
        return res;

    int src_fd = open(lower_path, O_RDONLY);
    if (src_fd < 0)
        return -errno;

    int dst_fd = open(upper_path, O_CREAT | O_WRONLY | O_TRUNC, st.st_mode);
    if (dst_fd < 0) {
        int saved = errno;
        close(src_fd);
        return -saved;
    }

    char buf[65536];  
    ssize_t n;
    while ((n = read(src_fd, buf, sizeof(buf))) > 0) {
        ssize_t written = 0;
        while (written < n) {
            ssize_t w = write(dst_fd, buf + written, (size_t)(n - written));
            if (w < 0) {
                int saved = errno;
                close(src_fd);
                close(dst_fd);
                return -saved;
            }
            written += w;
        }
    }

    fchmod(dst_fd, st.st_mode);

    close(src_fd);
    close(dst_fd);

    fprintf(stderr, "[CoW] copied lower%s → upper%s\n", path, path);
    return 0;
}

int unionfs_write(const char *path, const char *buf, size_t size,
                  off_t offset, struct fuse_file_info *fi)
{
    (void)fi;

    struct mini_unionfs_state *state = UNIONFS_DATA;

    char upper_path[PATH_MAX];
    snprintf(upper_path, PATH_MAX, "%s%s", state->upper_dir, path);

    if (access(upper_path, F_OK) != 0) {
        int res = cow_copy(path);
        if (res < 0)
            return res;
    }

    if (offset == 0) {
        if (truncate(upper_path, 0) < 0)
            return -errno;
    }

    int fd = open(upper_path, O_WRONLY);
    if (fd < 0)
        return -errno;

    ssize_t written = pwrite(fd, buf, size, offset);
    if (written < 0)
        written = -errno;

    close(fd);
    return (int)written;
}

int unionfs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    struct mini_unionfs_state *state = UNIONFS_DATA;

    int res = ensure_upper_parent_dirs(path);
    if (res < 0)
        return res;

    char upper_path[PATH_MAX];
    snprintf(upper_path, PATH_MAX, "%s%s", state->upper_dir, path);

    int fd = open(upper_path, O_CREAT | O_WRONLY | O_TRUNC, mode);
    if (fd < 0)
        return -errno;

    fi->fh = (uint64_t)fd;
    return 0;
}

int unionfs_mkdir(const char *path, mode_t mode)
{
    struct mini_unionfs_state *state = UNIONFS_DATA;

    int res = ensure_upper_parent_dirs(path);
    if (res < 0)
        return res;

    char upper_path[PATH_MAX];
    snprintf(upper_path, PATH_MAX, "%s%s", state->upper_dir, path);

    if (mkdir(upper_path, mode) < 0)
        return -errno;

    return 0;
}
