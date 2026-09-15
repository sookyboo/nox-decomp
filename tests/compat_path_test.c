/* Regression for 1247869: installation paths must resolve on case-sensitive
 * filesystems when the game supplies Windows-style casing and separators. */
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int compat_open(const char *filename, int oflag, ...);
FILE *compat_fopen(const char *path, const char *mode);
int external_compat_casepath(const char *path, char *out, size_t outsz);

const char *progname = "compat_path_test";

static int read_file(FILE *file)
{
    char contents[32] = {0};
    return fgets(contents, sizeof(contents), file) != NULL &&
           strcmp(contents, "fixture\n") == 0;
}

int main(void)
{
    char cwd[PATH_MAX];
    char root_template[] = "/tmp/nox-compat-path-XXXXXX";
    char *root;
    char path[PATH_MAX];
    char resolved[PATH_MAX];
    FILE *file = NULL;
    int fd = -1;
    int result = 1;

    if (!getcwd(cwd, sizeof(cwd)))
        return 1;

    root = mkdtemp(root_template);
    if (!root)
        return 1;

    snprintf(path, sizeof(path), "%s/PrimaryInstall", root);
    if (mkdir(path, 0700) != 0)
        goto cleanup;
    snprintf(path, sizeof(path), "%s/PrimaryInstall/DataFiles", root);
    if (mkdir(path, 0700) != 0)
        goto cleanup;
    snprintf(path, sizeof(path), "%s/PrimaryInstall/DataFiles/Mixed.DAT", root);
    file = fopen(path, "wb");
    if (!file)
        goto cleanup;
    if (fputs("fixture\n", file) == EOF || fclose(file) != 0) {
        file = NULL;
        goto cleanup;
    }
    file = NULL;

    if (chdir(root) != 0)
        goto cleanup;
    /* Verify the public normalizer resolves both separators and casing. */
    if (!external_compat_casepath("primaryinstall\\datafiles\\mixed.dat",
                                  resolved, sizeof(resolved)))
        goto cleanup;
    if (strstr(resolved, "PrimaryInstall/DataFiles/Mixed.DAT") == NULL)
        goto cleanup;

    /* This is the path used by ordinary C stdio callers in the game. */
    file = compat_fopen("PRIMARYINSTALL/DATAFILES/MIXED.DAT", "rb");
    if (!file || !read_file(file) || fclose(file) != 0) {
        file = NULL;
        goto cleanup;
    }
    file = NULL;

    /* This covers lower-level asset reads that use the open wrapper. */
    fd = compat_open("primaryinstall\\datafiles\\mixed.dat", O_RDONLY);
    if (fd < 0)
        goto cleanup;
    {
        char contents[32] = {0};
        ssize_t count = read(fd, contents, sizeof(contents) - 1);
        if (count < 0 || strcmp(contents, "fixture\n") != 0)
            goto cleanup;
    }

    result = 0;

cleanup:
    if (file)
        fclose(file);
    if (fd >= 0)
        close(fd);
    chdir(cwd);
    snprintf(path, sizeof(path), "%s/PrimaryInstall/DataFiles/Mixed.DAT", root);
    unlink(path);
    snprintf(path, sizeof(path), "%s/PrimaryInstall/DataFiles", root);
    rmdir(path);
    snprintf(path, sizeof(path), "%s/PrimaryInstall", root);
    rmdir(path);
    rmdir(root);
    return result;
}
