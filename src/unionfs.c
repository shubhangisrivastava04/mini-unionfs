#define FUSE_USE_VERSION 31

#include <fuse3/fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <libgen.h>
#include <sys/stat.h>
#include <unistd.h>

#include "unionfs.h"

#define UNIONFS_DATA \
    ((struct mini_unionfs_state *)fuse_get_context()->private_data)

int resolve_path(const char *path, char *out)
{
    struct mini_unionfs_state *state = UNIONFS_DATA;

    char tmp[PATH_MAX];
    snprintf(tmp, PATH_MAX, "%s%s", state->upper_dir, path);

    char dir_part[PATH_MAX], base_part[PATH_MAX];
    strncpy(dir_part,  tmp, PATH_MAX - 1);
    strncpy(base_part, tmp, PATH_MAX - 1);
    char *dname = dirname(dir_part);
    char *bname = basename(base_part);

    char wh_path[PATH_MAX];
    snprintf(wh_path, PATH_MAX, "%s/.wh.%s", dname, bname);

    if (access(wh_path, F_OK) == 0) {
        
        return -ENOENT;
    }

    char upper_full[PATH_MAX];
    snprintf(upper_full, PATH_MAX, "%s%s", state->upper_dir, path);
    if (access(upper_full, F_OK) == 0) {
        strncpy(out, upper_full, PATH_MAX - 1);
        out[PATH_MAX - 1] = '\0';
        return 0;
    }

    char lower_full[PATH_MAX];
    snprintf(lower_full, PATH_MAX, "%s%s", state->lower_dir, path);
    if (access(lower_full, F_OK) == 0) {
        strncpy(out, lower_full, PATH_MAX - 1);
        out[PATH_MAX - 1] = '\0';
        return 0;
    }

    return -ENOENT;
}

void build_whiteout_path(const char *path, char *out)
{
    struct mini_unionfs_state *state = UNIONFS_DATA;

    char tmp[PATH_MAX];
    snprintf(tmp, PATH_MAX, "%s%s", state->upper_dir, path);

    char dir_part[PATH_MAX], base_part[PATH_MAX];
    strncpy(dir_part,  tmp, PATH_MAX - 1);
    strncpy(base_part, tmp, PATH_MAX - 1);

    char *dname = dirname(dir_part);
    char *bname = basename(base_part);

    snprintf(out, PATH_MAX, "%s/.wh.%s", dname, bname);
}

extern int unionfs_getattr(const char *, struct stat *, struct fuse_file_info *);
extern int unionfs_readdir(const char *, void *, fuse_fill_dir_t, off_t,
                           struct fuse_file_info *, enum fuse_readdir_flags);
extern int unionfs_open   (const char *, struct fuse_file_info *);
extern int unionfs_read   (const char *, char *, size_t, off_t,
                           struct fuse_file_info *);

extern int unionfs_write  (const char *, const char *, size_t, off_t,
                           struct fuse_file_info *);
extern int unionfs_create (const char *, mode_t, struct fuse_file_info *);
extern int unionfs_mkdir  (const char *, mode_t);

extern int unionfs_unlink (const char *);
extern int unionfs_rmdir  (const char *);

static struct fuse_operations unionfs_oper = {
    .getattr  = unionfs_getattr,
    .readdir  = unionfs_readdir,
    .open     = unionfs_open,
    .read     = unionfs_read,
    .write    = unionfs_write,
    .create   = unionfs_create,
    .mkdir    = unionfs_mkdir,
    .unlink   = unionfs_unlink,
    .rmdir    = unionfs_rmdir,
};

int main(int argc, char *argv[])
{
    if (argc < 4) {
        fprintf(stderr,
                "Usage: %s <lower_dir> <upper_dir> <mount_dir> [fuse_opts]\n",
                argv[0]);
        return EXIT_FAILURE;
    }

    struct mini_unionfs_state *state =
        calloc(1, sizeof(struct mini_unionfs_state));
    if (!state) {
        perror("calloc");
        return EXIT_FAILURE;
    }

    state->lower_dir = realpath(argv[1], NULL);
    state->upper_dir = realpath(argv[2], NULL);

    if (!state->lower_dir || !state->upper_dir) {
        fprintf(stderr, "Error: could not resolve lower_dir or upper_dir.\n");
        free(state);
        return EXIT_FAILURE;
    }

    fprintf(stderr, "[mini-unionfs] lower : %s\n", state->lower_dir);
    fprintf(stderr, "[mini-unionfs] upper : %s\n", state->upper_dir);
    fprintf(stderr, "[mini-unionfs] mount : %s\n", argv[3]);

    int fuse_argc = 3;   // program + -f + mountpoint
    char **fuse_argv = malloc(sizeof(char *) * 4);

    fuse_argv[0] = argv[0];
    fuse_argv[1] = "-f";          
    fuse_argv[2] = argv[3];       // mountpoint
    fuse_argv[3] = NULL;
    
    if (!fuse_argv) {
        perror("malloc");
        free(state->lower_dir);
        free(state->upper_dir);
        free(state);
        return EXIT_FAILURE;
    }

    int ret = fuse_main(fuse_argc, fuse_argv, &unionfs_oper, state);

    free(fuse_argv);
    free(state->lower_dir);
    free(state->upper_dir);
    free(state);
    return ret;
}
