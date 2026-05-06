#define FUSE_USE_VERSION 31

#include <fuse3/fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <libgen.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "unionfs.h"

extern int cow_copy(const char *path);

#define MAX_ENTRIES 4096

typedef struct {
    char names[MAX_ENTRIES][NAME_MAX + 1];
    int  count;
} NameSet;

static void nameset_init(NameSet *s)
{
    s->count = 0;
}

static int nameset_contains(const NameSet *s, const char *name)
{
    for (int i = 0; i < s->count; i++) {
        if (strcmp(s->names[i], name) == 0)
            return 1;
    }
    return 0;
}

static void nameset_add(NameSet *s, const char *name)
{
    if (s->count < MAX_ENTRIES) {
        strncpy(s->names[s->count], name, NAME_MAX);
        s->names[s->count][NAME_MAX] = '\0';
        s->count++;
    }
}

int unionfs_getattr(const char *path, struct stat *stbuf,
                    struct fuse_file_info *fi)
{
    (void)fi; 

    char real_path[PATH_MAX];
    int res = resolve_path(path, real_path);
    if (res < 0)
        return res;   

    if (lstat(real_path, stbuf) == -1)
        return -errno;

    return 0;
}

int unionfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                    off_t offset, struct fuse_file_info *fi,
                    enum fuse_readdir_flags flags)
{
    (void)offset;
    (void)fi;
    (void)flags;

    struct mini_unionfs_state *state = UNIONFS_DATA;

    NameSet seen;    
    NameSet hidden;  
    nameset_init(&seen);
    nameset_init(&hidden);

    filler(buf, ".",  NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);

    char upper_dir_path[PATH_MAX];
    snprintf(upper_dir_path, PATH_MAX, "%s%s", state->upper_dir, path);

    DIR *upper_dp = opendir(upper_dir_path);
    if (upper_dp) {
        struct dirent *de;
        while ((de = readdir(upper_dp)) != NULL) {
            const char *name = de->d_name;

            if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
                continue;

            if (strncmp(name, ".wh.", 4) == 0) {
                nameset_add(&hidden, name + 4);
                continue;  
            }

            /* Normal upper entry — emit and record */
            if (!nameset_contains(&seen, name)) {
                filler(buf, name, NULL, 0, 0);
                nameset_add(&seen, name);
            }
        }
        closedir(upper_dp);
    }

    char lower_dir_path[PATH_MAX];
    snprintf(lower_dir_path, PATH_MAX, "%s%s", state->lower_dir, path);

    DIR *lower_dp = opendir(lower_dir_path);
    if (lower_dp) {
        struct dirent *de;
        while ((de = readdir(lower_dp)) != NULL) {
            const char *name = de->d_name;

            if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
                continue;

            if (nameset_contains(&hidden, name))
                continue;

            if (nameset_contains(&seen, name))
                continue;

            filler(buf, name, NULL, 0, 0);
            nameset_add(&seen, name);
        }
        closedir(lower_dp);
    }

    return 0;
}

int unionfs_open(const char *path, struct fuse_file_info *fi)
{
    struct mini_unionfs_state *state = UNIONFS_DATA;

    char real_path[PATH_MAX];
    int res = resolve_path(path, real_path);
    if (res < 0)
        return res;

    int write_mode = (fi->flags & O_ACCMODE) == O_WRONLY ||
                     (fi->flags & O_ACCMODE) == O_RDWR;

    if (write_mode) {
        char upper_path[PATH_MAX];
        snprintf(upper_path, PATH_MAX, "%s%s", state->upper_dir, path);

        if (access(upper_path, F_OK) != 0) {
            res = cow_copy(path);
            if (res < 0)
                return res;
        }
    }

    return 0;
}

int unionfs_read(const char *path, char *buf, size_t size, off_t offset,
                 struct fuse_file_info *fi)
{
    (void)fi;

    char real_path[PATH_MAX];
    int res = resolve_path(path, real_path);
    if (res < 0)
        return res;

    int fd = open(real_path, O_RDONLY);
    if (fd < 0)
        return -errno;

    ssize_t bytes_read = pread(fd, buf, size, offset);
    if (bytes_read < 0)
        bytes_read = -errno;

    close(fd);
    return (int)bytes_read;
}
