#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static int file_is_nonempty(const char *path)
{
    struct stat info;

    return stat(path, &info) == 0 && info.st_size > 0;
}

static int source_contains_all(const char *path)
{
    static const char *const required[] = {
        "UnitToPtr(", "GetMemory(", "SetMemory(", "CreateObjectAt(",
        "SetCallback(", "SetUnitCallbackOnCollide(",
        "SetUnitCallbackOnUseItem(", "Effect(", "MagicMissile",
        "LinearOrbMove(", "SetUnitMaxHealth(", "VoiceList(",
        "5483536", "0x53a720", "amount[27]",
    };
    char *source;
    long size;
    FILE *file;
    size_t read_size;
    unsigned int i;

    file = fopen(path, "rb");
    if (!file || fseek(file, 0, SEEK_END) != 0)
    {
        if (file)
            fclose(file);
        return 0;
    }
    size = ftell(file);
    if (size <= 0 || fseek(file, 0, SEEK_SET) != 0)
    {
        fclose(file);
        return 0;
    }
    source = (char *)malloc((size_t)size + 1);
    if (!source)
    {
        fclose(file);
        return 0;
    }
    read_size = fread(source, 1, (size_t)size, file);
    fclose(file);
    source[read_size] = 0;
    for (i = 0; i < sizeof(required) / sizeof(required[0]); ++i)
    {
        if (!strstr(source, required[i]))
        {
            free(source);
            return 0;
        }
    }
    free(source);
    return 1;
}

int main(int argc, char **argv)
{
    static const char *const gamefiles[] = {"NOX.EXE", "thing.bin", "maps"};
    unsigned int i;
    char path[4096];

    if (argc != 4)
    {
        fprintf(stderr, "usage: %s Monster.map monster.c gamefiles/app\n", argv[0]);
        return 2;
    }
    if (!file_is_nonempty(argv[1]))
    {
        fprintf(stderr, "missing or empty Monster map: %s\n", argv[1]);
        return 1;
    }
    if (!source_contains_all(argv[2]))
    {
        fprintf(stderr, "Monster source lacks expected EUD operations: %s\n", argv[2]);
        return 1;
    }
    for (i = 0; i < sizeof(gamefiles) / sizeof(gamefiles[0]); ++i)
    {
        if (snprintf(path, sizeof(path), "%s/%s", argv[3], gamefiles[i])
                >= (int)sizeof(path)
            || !file_is_nonempty(path))
        {
            fprintf(stderr, "missing Nox gamefile: %s/%s\n", argv[3], gamefiles[i]);
            return 1;
        }
    }
    puts("Monster map/source and Nox gamefiles found; gameplay execution remains "
         "display/package dependent.");
    return 0;
}
