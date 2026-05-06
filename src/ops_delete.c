#define FUSE_USE_VERSION 31

#include <fuse3/fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <libgen.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#include "unionfs.h"

int unionfs_unlink(const char *path)
{
    struct mini_unionfs_state *state = UNIONFS_DATA;

    char upper_path[PATH_MAX], lower_path[PATH_MAX];
    snprintf(upper_path, PATH_MAX, "%s%s", state->upper_dir, path);
    snprintf(lower_path, PATH_MAX, "%s%s", state->lower_dir, path);

    /* Case A: file in upper */
    if (access(upper_path, F_OK) == 0) {
        if (unlink(upper_path) < 0)
            return -errno;
        fprintf(stderr, "[unlink] removed upper%s\n", path);
        return 0;
    }

    /* Case B: file in lower → create whiteout */
    if (access(lower_path, F_OK) == 0) {
        char wh_path[PATH_MAX];
        build_whiteout_path(path, wh_path);

        char tmp[PATH_MAX];
        strncpy(tmp, wh_path, PATH_MAX - 1);
        tmp[PATH_MAX - 1] = '\0';

        char *parent = dirname(tmp);

        if (access(parent, F_OK) != 0) {
            if (mkdir(parent, 0755) < 0 && errno != EEXIST)
                return -errno;
        }

        int fd = open(wh_path, O_CREAT | O_WRONLY | O_TRUNC, 0000);
        if (fd < 0)
            return -errno;
        close(fd);

        fprintf(stderr, "[unlink] whiteout created: %s\n", wh_path);
        return 0;
    }

    return -ENOENT;
}

int unionfs_rmdir(const char *path)
{
    struct mini_unionfs_state *state = UNIONFS_DATA;

    char upper_path[PATH_MAX], lower_path[PATH_MAX];
    snprintf(upper_path, PATH_MAX, "%s%s", state->upper_dir, path);
    snprintf(lower_path, PATH_MAX, "%s%s", state->lower_dir, path);

    /* Case A: exists in upper → normal rmdir */
    if (access(upper_path, F_OK) == 0) {
        if (rmdir(upper_path) < 0)
            return -errno;
        fprintf(stderr, "[rmdir] removed upper%s\n", path);
        return 0;
    }

    /* Case B: exists in lower → ALWAYS allow whiteout */
    if (access(lower_path, F_OK) == 0) {

        char wh_path[PATH_MAX];
        build_whiteout_path(path, wh_path);

        char tmp[PATH_MAX];
        strncpy(tmp, wh_path, PATH_MAX - 1);
        tmp[PATH_MAX - 1] = '\0';

        char *parent = dirname(tmp);

        if (access(parent, F_OK) != 0) {
            if (mkdir(parent, 0755) < 0 && errno != EEXIST)
                return -errno;
        }

        int fd = open(wh_path, O_CREAT | O_WRONLY | O_TRUNC, 0000);
        if (fd < 0)
            return -errno;
        close(fd);

        fprintf(stderr, "[rmdir] whiteout created for dir: %s\n", wh_path);
        return 0;
    }

    return -ENOENT;
}
