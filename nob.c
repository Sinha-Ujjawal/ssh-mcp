#define NOB_IMPLEMENTATION
#include "./thirdparty/nob.h"

#define BUILD_DIR "build/"
#define CC        "cc"

Cmd cmd = {0};
Procs procs = {0};

#ifdef needs_rebuild
#undef needs_rebuild
#endif
#define needs_rebuild(source, ...) \
    nob_needs_rebuild((source), (char const*[]){ __VA_ARGS__ }, (sizeof((const char*[]){ __VA_ARGS__ }) / sizeof(char const*)))

#define THIRDPARTY_LIBS              \
    "thirdparty/jim.h"               \
    , "thirdparty/jimp.h"            \
    , "thirdparty/nob.h"             \
    , "thirdparty/nob_br.h"          \
    , "thirdparty/nob_channels.h"    \
    , "thirdparty/nob_fixed_deque.h" \
    , "thirdparty/nob_hash.h"        \
    , "thirdparty/nob_ht.h"          \
    , "thirdparty/nob_jsonrpc.h"     \
    , "thirdparty/nob_mcp.h"

int main(int argc, char **argv) {
    int result = 1;
    GO_REBUILD_URSELF(argc, argv);
    if (!mkdir_if_not_exists(BUILD_DIR)) return_defer(1);

    if (needs_rebuild(BUILD_DIR"ssh-mcp", "src/main.c", THIRDPARTY_LIBS)) {
        cmd_append(&cmd, CC, "-o", BUILD_DIR"ssh-mcp", "src/main.c");
        cmd_append(&cmd, "-Wall", "-Wextra", "-Werror", "-Wswitch-enum", "-ggdb");
        cmd_append(&cmd, "-Wno-unused-but-set-variable");
        cmd_append(&cmd, "-Wno-deprecated-declarations");
        cmd_append(&cmd, "-Ithirdparty/");
        cmd_append(&cmd, "-lcrypto");
        if (!cmd_run(&cmd, .async=&procs)) return_defer(1);
    }

    if (needs_rebuild(BUILD_DIR"test", "test.c")) {
        cmd_append(&cmd, CC, "-o", BUILD_DIR"test", "test.c", THIRDPARTY_LIBS);
        cmd_append(&cmd, "-Wall", "-Wextra", "-Werror", "-Wswitch-enum", "-ggdb");
        cmd_append(&cmd, "-Wno-unused-but-set-variable");
        cmd_append(&cmd, "-Wno-deprecated-declarations");
        cmd_append(&cmd, "-Ithirdparty/");
        cmd_append(&cmd, "-lcrypto");
        if (!cmd_run(&cmd, .async=&procs)) return_defer(1);
    }

    if (!procs_wait_and_reset(&procs)) return_defer(1);

    result = 0;
defer:
    free(cmd.items);
    free(procs.items);
    return result;
}
