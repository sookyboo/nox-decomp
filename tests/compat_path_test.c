/* Regression for 1247869: installation paths must resolve on case-sensitive
 * filesystems when the game supplies Windows-style casing and separators. */
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "../src/windows.h"

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

static int find_handle_width_test(void)
{
    WIN32_FIND_DATAA find_data;
    intptr_t handle;

    handle = FindFirstFileA("N00.plr", &find_data);
    if (handle == (intptr_t)-1)
        return 0;
#if UINTPTR_MAX > UINT32_MAX
    if ((uintptr_t)handle <= UINT32_MAX) {
        FindClose(handle);
        return 0;
    }
#endif
    if (strcmp(find_data.cFileName, "N00.plr") != 0) {
        FindClose(handle);
        return 0;
    }
    if (FindNextFileA(handle, &find_data)) {
        FindClose(handle);
        return 0;
    }
    return FindClose(handle);
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

    snprintf(path, sizeof(path), "%s/PrimaryInstall/Save", root);
    if (mkdir(path, 0700) != 0)
        goto cleanup;
    snprintf(path, sizeof(path), "%s/PrimaryInstall/Save/N00.plr", root);
    file = fopen(path, "wb");
    if (!file)
        goto cleanup;
    if (fputs("profile fixture\n", file) == EOF || fclose(file) != 0) {
        file = NULL;
        goto cleanup;
    }
    file = NULL;

    /* A number of game assets live directly in the install root.  The final
     * component must be case-folded without being treated as a directory. */
    snprintf(path, sizeof(path), "%s/soundset.bin", root);
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

    file = compat_fopen("SoundSet.bin", "rb");
    if (!file || !read_file(file) || fclose(file) != 0) {
        file = NULL;
        goto cleanup;
    }
    file = NULL;

    if (chdir("PrimaryInstall/Save") != 0 || !find_handle_width_test())
        goto cleanup;
    if (chdir(root) != 0)
        goto cleanup;

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
    snprintf(path, sizeof(path), "%s/soundset.bin", root);
    unlink(path);
    snprintf(path, sizeof(path), "%s/PrimaryInstall/Save/N00.plr", root);
    unlink(path);
    snprintf(path, sizeof(path), "%s/PrimaryInstall/Save", root);
    rmdir(path);
    snprintf(path, sizeof(path), "%s/PrimaryInstall/DataFiles", root);
    rmdir(path);
    snprintf(path, sizeof(path), "%s/PrimaryInstall", root);
    rmdir(path);
    rmdir(root);
    return result;
}
