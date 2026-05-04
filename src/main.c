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

bool tools_list_clb(MCP_Session *session) {
    mcp_begin_tool(session, "get_echo", .desc="Echoes back the input text"); {
        mcp_add_param(
            session, "text", MCP_PARAM_TYPE_STRING, .desc="input text");
    } mcp_end_tool(session);

    mcp_begin_tool(session, "sum", .desc="Sums a list of numbers"); {
        mcp_add_array_param(
            session, "nums", MCP_PARAM_TYPE_NUMBER, .desc="Input numbers");
    } mcp_end_tool(session);
    return true;
}

bool tools_call_clb(MCP_Session *session, String_View tool_name, Jimp *tool_args) {
    UNUSED(session);
    String_Builder *scratch_pad = session->ctx;
    nob_log(INFO, "Tool Name: |"SV_Fmt"|", SV_Arg(tool_name));
    nob_log(INFO, "Tool Args: |"SV_Fmt"|", (int) (tool_args->end - tool_args->start), tool_args->start);
    if (sv_eq(tool_name, sv_from_cstr("get_echo"))) {
        const char *text = NULL;
        if (!jimp_object_begin(tool_args)) return false;
        while (jimp_object_member(tool_args)) {
            if (strcmp(tool_args->string, "text") == 0) {
                if (!jimp_string(tool_args)) return false;
                text = tool_args->string;
                break;
            } else {
                if (!jimp_skip_member(tool_args)) return false;
            }
        }
        if (!jimp_object_end(tool_args)) return false;
        if (text == NULL) return false;
        scratch_pad->count = 0;
        sb_appendf(scratch_pad, "Echoed... '%s'", text);
        sb_append_null(scratch_pad);
        mcp_write_text_content(session, scratch_pad->items);
        return true;
    } else if (sv_eq(tool_name, sv_from_cstr("sum"))) {
        double sum = 0.0;
        if (!jimp_object_begin(tool_args)) return false;
        while (jimp_object_member(tool_args)) {
            if (strcmp(tool_args->string, "nums") == 0) {
                if (!jimp_array_begin(tool_args)) return false;
                while (jimp_array_item(tool_args)) {
                    if (!jimp_number(tool_args)) return false;
                    sum += tool_args->number;
                }
                if (!jimp_array_end(tool_args)) return false;
                break;
            } else {
                if (!jimp_skip_member(tool_args)) return false;
            }
        }
        if (!jimp_object_end(tool_args)) return false;
        scratch_pad->count = 0;
        sb_appendf(scratch_pad, "%f", sum);
        sb_append_null(scratch_pad);
        mcp_write_text_content(session, scratch_pad->items);
        return true;
    }
    mcp_write_text_content(session, "This tool is not implemented yet!");
    return false;
}

int main(void) {
    int result = 1;
    String_Builder scratch_pad = {0};
    MCP_Session session = create_mcp_session(
        STDIN_FILENO, "stdin",
        STDOUT_FILENO, "stdout",
        "ssh", "0.0.1",
        tools_list_clb,
        tools_call_clb,
        &scratch_pad,
        NULL);
    for (;;) {
        if (!mcp_handle_request(&session)) return_defer(false);
    }
    
    result = 0;
defer:
    free_mcp_session(&session);
    free(scratch_pad.items);
    return result;
}
