#ifndef NOB_BR_H_
#define NOB_BR_H_

#ifndef NOB_BR_DEFAULT_CAPACITY
// 8KBs
#define NOB_BR_DEFAULT_CAPACITY (8 * 1024)
#endif // NOB_BR_DEFAULT_CAPACITY

#ifndef NOB_BR_DEFAULT_POLL
// 10ms
#define NOB_BR_DEFAULT_POLL 10
#endif // NOB_BR_DEFAULT_POLL

// Fixed size Buffer
typedef struct {
    int fdin;
    size_t poll_timeout;
    size_t capacity;

    // Below should ideally not be touched by the users
    char *items;
    size_t count;
    size_t cursor;
} Nob_Buffered_Reader;

Nob_Buffered_Reader nob_create_br(int fdin);
bool nob_br_fill(Nob_Buffered_Reader *reader);
bool nob_br_read_while(Nob_Buffered_Reader *reader, int (*predicate)(int c), void (*clb)(void *ctx, char b), void *ctx);
bool nob_br_read_while_to_sb(Nob_Buffered_Reader *reader, int (*predicate)(int c), Nob_String_Builder *sb);
bool nob_br_read_line(Nob_Buffered_Reader *reader, void (*clb)(void *ctx, char b), void *ctx);
bool nob_br_read_line_to_sb(Nob_Buffered_Reader *reader, Nob_String_Builder *sb);

#endif // NOB_BR_H_

#ifdef NOB_BR_IMPLEMENTATION
#ifndef NOB_BR_IMPLEMENTATION_GAURD_
#define NOB_BR_IMPLEMENTATION_GAURD_

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <io.h>
    typedef SSIZE_T ssize_t;
    #define read _read
#else
    #include <poll.h>
#endif

#include <string.h>
#include <errno.h>

Nob_Buffered_Reader nob_create_br(int fdin) {
    Nob_Buffered_Reader ret = {0};
    ret.fdin = fdin;
    ret.poll_timeout = NOB_BR_DEFAULT_POLL;
    ret.capacity = NOB_BR_DEFAULT_CAPACITY;
    return ret;
}

bool nob_br_fill(Nob_Buffered_Reader *reader) {
    bool result = false;

    if (reader->items == NULL) {
        reader->items = malloc(reader->capacity);
        assert(reader->items != NULL && "Buy more Ram LOL!");
    }

    // Shift and Update
    assert(reader->count >= reader->cursor);
    size_t n = reader->count - reader->cursor;
    if (n > 0 && reader->cursor > 0) {
        memmove(reader->items, reader->items + reader->cursor, n);
    }
    reader->cursor = 0;
    reader->count = n;

    size_t available_space = reader->capacity - reader->count;
    if (available_space == 0) nob_return_defer(true); // No available space in buffer

    // Poll and Fill rest
    bool data_available = false;
#ifdef _WIN32
    // Windows logic: Check if it's a pipe (common for MCP) or console
    DWORD bytes_avail = 0;
    HANDLE h = (HANDLE)_get_osfhandle(reader->fdin);
    if (PeekNamedPipe(h, NULL, 0, NULL, &bytes_avail, NULL)) {
        data_available = (bytes_avail > 0);
    } else {
        // Fallback for console or other types
        data_available = true; // Just attempt the read
    }
#else
    struct pollfd pfd = {
        .fd = reader->fdin,
        .events = POLLIN
    };

    int poll_res = poll(&pfd, 1, (int)reader->poll_timeout);
    // Error while polling
    if (poll_res < 0) nob_return_defer(false);
    // Timeout
    if (poll_res == 0) nob_return_defer(true);
    data_available = pfd.revents & POLLIN;
#endif

    if (data_available) {
        ssize_t bytes_read = read(reader->fdin, reader->items + reader->count, available_space);
        if (bytes_read < 0) nob_return_defer(false);
        reader->count += (size_t)bytes_read;
    }

    result = true;
defer:
    if (!result && errno != 0) {
        nob_log(ERROR, "Error Occurred while filling the buffer from fdin(%d): %s", reader->fdin, strerror(errno));
    }
    return result;
}

bool nob_br_read_while(Nob_Buffered_Reader *reader, int (*predicate)(int c), void (*clb)(void *ctx, char b), void *ctx) {
    for (;;) {
        if (reader->cursor >= reader->count) {
            if(!nob_br_fill(reader)) return false;
            if (reader->count == 0) return true;
        }

        while(reader->cursor < reader->count) {
            char c = reader->items[reader->cursor];
            if (!predicate(c)) {
                return true;
            }
            clb(ctx, c);
            reader->cursor++;
        }
    }
}

void nob_br__sb_append_clb(void *ctx, char b) {
    Nob_String_Builder *sb = (Nob_String_Builder *)ctx;
    nob_sb_append_buf(sb, &b, 1);
}

bool nob_br_read_while_to_sb(Nob_Buffered_Reader *reader, int (*predicate)(int c), Nob_String_Builder *sb) {
    return nob_br_read_while(reader, predicate, nob_br__sb_append_clb, sb);
}

int nob_br__is_not_a_new_line_predicate(int c) {
    return c != '\n';
}

bool nob_br_read_line(Nob_Buffered_Reader *reader, void (*clb)(void *ctx, char b), void *ctx) {
    if (!nob_br_read_while(reader, nob_br__is_not_a_new_line_predicate, clb, ctx)) {
        return false;
    }
    // Consume \n
    if (reader->cursor < reader->count && reader->items[reader->cursor] == '\n') {
        reader->cursor++;
    }
    return true;
}

bool nob_br_read_line_to_sb(Nob_Buffered_Reader *reader, Nob_String_Builder *sb) {
    if (!nob_br_read_line(reader, nob_br__sb_append_clb, sb)) return false;
    return true;
}

#endif // NOB_BR_IMPLEMENTATION_GAURD_
#endif // NOB_BR_IMPLEMENTATION

#ifndef NOB_BR_STRIP_PREFIX_GUARD_
#define NOB_BR_STRIP_PREFIX_GUARD_
    #ifndef NOB_UNSTRIP_PREFIX
        #define Buffered_Reader     Nob_Buffered_Reader
        #define create_br           nob_create_br
        #define br_fill             nob_br_fill
        #define br_read_while       nob_br_read_while
        #define br_read_while_to_sb nob_br_read_while_to_sb
        #define br_read_line        nob_br_read_line
        #define br_read_line_to_sb  nob_br_read_line_to_sb
    #endif // NOB_UNSTRIP_PREFIX
#endif // NOB_BR_STRIP_PREFIX_GUARD_
