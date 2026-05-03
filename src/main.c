#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define NOB_IMPLEMENTATION
#define JIM_IMPLEMENTATION
#define JIMP_IMPLEMENTATION
#define NOB_BR_IMPLEMENTATION
#include "nob.h"
#include "jim.h"
#include "jimp.h"
#include "nob_br.h"

#include "config.h"
#define NOB_MCP_IMPLEMENTATION
#include "nob_mcp.h"

bool handle_session(
    MCP *mcp, const char *fdin_label, int fdin, const char *fdout_label, int fdout) {
    bool result = false;
    MCP_Session session = mcp_create_sesssion(mcp, fdin_label, fdin, fdout_label, fdout);
    Jimp jimp = {0};
    for (;;) {
        MCP_Request req = {0};
        if (!mcp_parse_request(&session, &req)) return_defer(false);
        nob_log(INFO, "Got Request[%s], Params: |"SV_Fmt"|", mcp_method_kind_to_cstr(req.method), SV_Arg(req.params));

        switch(req.method) {
            case MCP_METHOD_INITIALIZE: {
                if (!mcp_handle_initialize(&session, req)) return_defer(false);
            } break;
            case MCP_METHOD_NOTIFS_INITIALIZED: {
                // Do Nothing.
            } break;
            case MCP_METHOD_TOOLS_LIST: {
                mcp_begin_tools_list(&session, req); {
                    mcp_begin_tool(&session, "get_echo", .desc="Echoes back the input text"); {
                        mcp_add_param(
                            &session, "text", MCP_PARAM_TYPE_STRING, .desc="input text");

                        // mcp_add_array_param(
                        //     &session, "complex1", MCP_PARAM_TYPE_STRING, .desc="complex list");

                        // mcp_begin_object_param(&session, "metadata1"); {
                        // } mcp_end_object_param(&session);

                        // mcp_begin_array_param(&session, "complex2"); {
                        //     mcp_begin_object_param(&session, "metadata2"); {
                        //     } mcp_end_object_param(&session);
                        // } mcp_end_array_param(&session);

                        // mcp_begin_array_param(&session, "matrix", .desc="A list containing other lists"); {
                        //     // mcp_add_array_param(
                        //     //     &session, "integers", MCP_PARAM_TYPE_INTEGER, .desc="A row of numbers");
                        //     mcp_begin_array(&session, .desc="An array of integers"); {
                        //         mcp_set_array_type(&session, MCP_PARAM_TYPE_INTEGER, .desc="An Integer");
                        //     } mcp_end_array(&session);
                        // } mcp_end_array_param(&session);

                        // mcp_begin_object_param(&session, "metadata3"); {
                        //     mcp_begin_object_param(&session, "metadata3.xyz"); {
                        //         mcp_add_param(
                        //             &session, "text", MCP_PARAM_TYPE_STRING, .desc="input text");
                        //         mcp_begin_array_param(&session, "metadata3.complex2"); {
                        //             mcp_begin_array_param(&session, "metadata3.complex2.123"); {
                        //                 mcp_begin_object_param(&session, "metadata3.complex2.1234"); {
                        //                     mcp_add_array_param(
                        //                         &session, "complex1", MCP_PARAM_TYPE_STRING, .desc="complex list");
                        //                 } mcp_end_object_param(&session);
                        //             } mcp_end_array_param(&session);
                        //         } mcp_end_array_param(&session);
                        //     } mcp_end_object_param(&session);
                        // } mcp_end_object_param(&session);
                    } mcp_end_tool(&session);
                } mcp_end_tools_list(&session);
                if (!mcp_flush_tools_list(&session)) return_defer(false);
            } break;
            case NOB_MCP_METHOD_RESOURCES_LIST: {
                if (!mcp_handle_resources_list(&session, req)) return_defer(false);
            } break;
            case NOB_MCP_METHOD_PROMPTS_LIST: {
                if (!mcp_handle_prompts_list(&session, req)) return_defer(false);
            } break;
            case NOB_MCP_METHOD_TOOL_CALL: {
                nob_log(INFO, "Tool Name: |"SV_Fmt"|", SV_Arg(req.tool_name));
                nob_log(INFO, "Tool Args: |"SV_Fmt"|", SV_Arg(req.tool_args));
                if (sv_eq(req.tool_name, sv_from_cstr("get_echo"))) {
                    size_t saved = temp_save();
                    jimp_begin(&jimp, temp_sprintf("request_id_%zu", req.id), req.tool_args.data, req.tool_args.count);
                    assert(jimp_object_begin(&jimp));
                    const char *text = NULL;
                    while (jimp_object_member(&jimp)) {
                        if (strcmp(jimp.string, "text") == 0) {
                            text = jimp.string;
                        } else {
                            jimp_skip_member(&jimp);
                        }
                    }
                    assert(text != NULL);

                    assert(jimp_object_end(&jimp));
                    temp_rewind(saved);
                    TODO("NOB_MCP_METHOD_TOOL_CALL: get_echo");
                }
                TODO("NOB_MCP_METHOD_TOOL_CALL");
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
    free(jimp.string);
    return result;
}

int main(void) {
    MCP mcp = {0};
    mcp.pinfo.jsonrpc_ver = "2.0";
    mcp.pinfo.protocol_ver = MCP_PROTOCOL_VER;
    mcp.sinfo.name = SERVER_INFO_NAME;
    mcp.sinfo.ver = SERVER_INFO_VER;
    if (!handle_session(&mcp, "stdin", STDIN_FILENO, "stdout", STDOUT_FILENO)) return 1;
    return 0;
}
