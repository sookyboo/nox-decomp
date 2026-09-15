/* Regression for 850ade5: save files must be writable when map/save path
 * components differ only in case on a case-sensitive filesystem. */
#define _GNU_SOURCE
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

char *mkdtemp(char *template);

int compat_open(const char *filename, int oflag, ...);
const char *progname = "save_casepath_test";

int main(void)
{
    char cwd[PATH_MAX];
    char root_template[] = "/tmp/nox-save-case-XXXXXX";
    char *root;
    char path[PATH_MAX];
    char contents[32] = {0};
    int fd = -1;
    int result = 1;

    if (!getcwd(cwd, sizeof(cwd)))
        return 1;
    root = mkdtemp(root_template);
    if (!root)
        return 1;

    snprintf(path, sizeof(path), "%s/SaveData", root);
    if (mkdir(path, 0700) != 0)
        goto cleanup;
    snprintf(path, sizeof(path), "%s/SaveData/ChapterOne", root);
    if (mkdir(path, 0700) != 0)
        goto cleanup;
    if (chdir(root) != 0)
        goto cleanup;

    /* This is the save write path: the file is new, but its parent
     * directories already exist with different casing. */
    fd = compat_open("savedata\\chapterone\\Player.plr",
                     O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0)
        goto cleanup;
    if (write(fd, "map=ChapterOne\n", 15) != 15)
        goto cleanup;
    close(fd);
    fd = -1;

    fd = compat_open("SAVEDATA/CHAPTERONE/PLAYER.PLR", O_RDONLY);
    if (fd < 0)
        goto cleanup;
    if (read(fd, contents, sizeof(contents) - 1) != 15 ||
        strcmp(contents, "map=ChapterOne\n") != 0)
        goto cleanup;

    result = 0;

cleanup:
    if (fd >= 0)
        close(fd);
    chdir(cwd);
    snprintf(path, sizeof(path), "%s/SaveData/ChapterOne/Player.plr", root);
    unlink(path);
    snprintf(path, sizeof(path), "%s/SaveData/ChapterOne", root);
    rmdir(path);
    snprintf(path, sizeof(path), "%s/SaveData", root);
    rmdir(path);
    rmdir(root);
    return result;
}
