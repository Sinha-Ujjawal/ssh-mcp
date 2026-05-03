#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define NOB_IMPLEMENTATION
#define JIM_IMPLEMENTATION
#define JIMP_IMPLEMENTATION
#define NOB_BR_IMPLEMENTATION
#define NOB_JSONRPC_IMPLEMENTATION
#include "nob.h"
#include "jim.h"
#include "jimp.h"
#include "nob_br.h"
#include "nob_jsonrpc.h"

#include "config.h"
#define NOB_MCP_IMPLEMENTATION
#include "nob_mcp.h"

bool tools_list_clb(Nob_MCP_Session *session) {
    mcp_begin_tool(session, "get_echo", .desc="Echoes back the input text"); {
        mcp_add_param(
            session, "text", MCP_PARAM_TYPE_STRING, .desc="input text");
    } mcp_end_tool(session);
    return true;
}

int main(void) {
    int result = 1;
    MCP_Session session = create_mcp_session(
        STDIN_FILENO, "stdin",
        STDOUT_FILENO, "stdout",
        "ssh", "0.0.1",
        tools_list_clb,
        NULL);
    for (;;) {
        if (!mcp_handle_request(&session)) return_defer(false);
    }
    
    result = 0;
defer:
    free_mcp_session(&session);
    return result;
}
