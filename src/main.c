#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define NOB_IMPLEMENTATION
#define JIM_IMPLEMENTATION
#define JIMP_IMPLEMENTATION
#include "nob.h"
#include "jim.h"
#include "jimp.h"

#include "config.h"
#define NOB_MCP_IMPLEMENTATION
#include "nob_mcp.h"

bool handle_session(
    MCP *mcp, const char *fdin_label, int fdin, const char *fdout_label, int fdout) {
    bool result = false;
    MCP_Session session = mcp_create_sesssion(mcp, fdin_label, fdin, fdout_label, fdout);
    for (;;) {
        MCP_Request req = {0};
        if (!mcp_parse_request(&session, &req)) return_defer(false);
        nob_log(INFO, "Got Request[%s]", mcp_method_kind_to_cstr(req.method));

        switch(req.method) {
            case MCP_METHOD_INITIALIZE: {
                if (!mcp_handle_initialize(&session, &req)) return_defer(false);
            } break;
            case MCP_METHOD_NOTIFS_INITIALIZED: {
                if (!mcp_handle_notifs_initialized(&session, &req)) return_defer(false);
            } break;
            case __count_Nob_MCP_Method_Kind:
            default: {
                nob_log(ERROR, "Could not handle unknown request method");
                return_defer(false);
            }
        }
    }

    result = true;
defer:
    free_mcp_session(&session);
    return result;
}

int main(void) {
    MCP mcp = {0};
    mcp.pinfo.jsonrpc_ver = JSONRPC_VER;
    mcp.pinfo.protocol_ver = MCP_PROTOCOL_VER;
    mcp.sinfo.name = SERVER_INFO_NAME;
    mcp.sinfo.ver = SERVER_INFO_VER;
    if (!handle_session(&mcp, "stdin", STDIN_FILENO, "stdout", STDOUT_FILENO)) return 1;
    return 0;
}
