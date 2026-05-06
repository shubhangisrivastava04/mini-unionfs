#ifndef UNIONFS_H
#define UNIONFS_H

#define FUSE_USE_VERSION 31

#include <fuse3/fuse.h>
#include <limits.h>
#include <sys/stat.h>

struct mini_unionfs_state {
    char *lower_dir;   /* read-only base layer (Docker "image layer") */
    char *upper_dir;   /* read-write container layer                  */
};

#define UNIONFS_DATA \
    ((struct mini_unionfs_state *)fuse_get_context()->private_data)


int resolve_path(const char *path, char *out);

void build_whiteout_path(const char *path, char *out);

#endif /* UNIONFS_H */
