#define NOB_IMPLEMENTATION
#include "./thirdparty/nob.h"

#define BUILD_DIR "build/"
#define CC        "cc"

Cmd cmd = {0};
Procs procs = {0};

int main(int argc, char **argv) {
    int result = 1;
    GO_REBUILD_URSELF(argc, argv);
    if (!mkdir_if_not_exists(BUILD_DIR)) return_defer(1);

    cmd_append(&cmd, CC, "-o", BUILD_DIR"ssh-mcp", "src/main.c");
    cmd_append(&cmd, "-Wall", "-Wextra", "-Werror", "-Wswitch-enum", "-ggdb");
    cmd_append(&cmd, "-Wno-unused-but-set-variable");
    cmd_append(&cmd, "-Ithirdparty/");
    if (!cmd_run(&cmd, .async=&procs)) return_defer(1);

    if (!procs_wait_and_reset(&procs)) return_defer(1);

    result = 0;
defer:
    free(cmd.items);
    free(procs.items);
    return result;
}
