#ifndef NOB_MCP_H_
#define NOB_MCP_H_

// NOTE: Include below headers before including this
// #define NOB_IMPLEMENTATION
// #define JIM_IMPLEMENTATION
// #define JIMP_IMPLEMENTATION
// #define NOB_BR_IMPLEMENTATION
// #define NOB_JSONRPC_IMPLEMENTATION
// #include "nob.h"
// #include "jim.h"
// #include "jimp.h"
// #include "nob_br.h"
// #include "nob_jsonrpc.h"

#define NOB_MCP_DEFAULT_PROTOCOL_VER "2024-11-05"

typedef enum {
    NOB_MCP_SCOPE_TOOL               = 1 << 0,
    NOB_MCP_SCOPE_ARRAY              = 1 << 1,
    NOB_MCP_SCOPE_OBJECT             = 1 << 2,
    NOB_MCP_SCOPE_SET_ARRAY_TYPE     = 1 << 3,
    __count_Nob_MCP_Tools_List_Scope = 1 << 4,
} Nob_MCP_Tools_List_Scope;

typedef struct {
    Nob_MCP_Tools_List_Scope *items;
    size_t count;
    size_t capacity;
} Nob_MCP_Tools_List_Scopes;

typedef struct Nob_MCP_Session Nob_MCP_Session;

struct Nob_MCP_Session {
    Nob_JSONRPC_Session jsonrpc_session;
    const char *mcp_protocol_ver;
    const char *mcp_server_name;
    const char *mcp_server_ver;
    void *ctx;
    const char *instructions;

    bool (*tools_list_clb)(Nob_MCP_Session *session); // tools/list callback
    Nob_MCP_Tools_List_Scopes tools_list_scopes;

    Nob_String_View tool_name;
    Jimp tool_args;
    bool (*tools_call_clb)(Nob_MCP_Session *session, Nob_String_View tool_name, Jimp *tool_args); // tools/call callback
};

typedef struct {
    const char *name;
    const char *desc;
} Nob_MCP_Tool_Desc;

typedef enum {
    NOB_MCP_PARAM_TYPE_STRING,
    NOB_MCP_PARAM_TYPE_NUMBER,
    NOB_MCP_PARAM_TYPE_INTEGER,
    NOB_MCP_PARAM_TYPE_BOOL,
    __count_NOB_MCP_PARAM_TYPE_TYPE,
} NOB_MCP_PARAM_TYPE_TYPE;

typedef struct {
    const char *name;
    const char *desc;
} Nob_MCP_Tool_Param_Name_And_Desc;

typedef struct {
    const char *name;
    const char *desc;
    NOB_MCP_PARAM_TYPE_TYPE type;
    union {
        struct {
            const char *default_;
            // enum
            const char **enum_array;
            size_t enum_count;
        } string;
        struct {
            double default_;
            double minimum;
            double maximum;
        } number;
        struct {
            int default_;
            int minimum;
            int maximum;
        } integer;
        struct {
            bool default_;
        } boolean;
    } constraints;
} Nob_MCP_Tool_Param_Name_And_Desc_With_Constraints;

Nob_MCP_Session nob_create_mcp_session(
    int fdin, const char *fdin_label,
    int fdout, const char *fdout_label,
    const char *mcp_server_name, const char *mcp_server_ver,
    bool (*tools_list_clb)(Nob_MCP_Session *session), // tools/list callback
    bool (*tools_call_clb)(Nob_MCP_Session *session, Nob_String_View tool_name, Jimp *tool_args), // tools/call callback
    void *ctx,
    const char *instructions);
void nob_free_mcp_session(Nob_MCP_Session *session);
bool nob_mcp_handle_request(Nob_MCP_Session *session);

#define nob_mcp_begin_tool(session, tool_name, ...) \
    nob_mcp_begin_tool_impl((session), (Nob_MCP_Tool_Desc){.name=(tool_name), __VA_ARGS__}, __FILE__, __LINE__)
void nob_mcp_begin_tool_impl(Nob_MCP_Session *session, Nob_MCP_Tool_Desc tool_desc, const char *file, size_t line_no);
#define nob_mcp_end_tool(session) nob_mcp_end_tool_impl(session, __FILE__, __LINE__)
void nob_mcp_end_tool_impl(Nob_MCP_Session *session, const char *file, size_t line_no);

#define nob_mcp_add_param(session, param_name, param_type, ...) \
    nob_mcp_add_param_impl((session),                           \
        (Nob_MCP_Tool_Param_Name_And_Desc_With_Constraints){.name=(param_name), .type=(param_type), __VA_ARGS__}, __FILE__, __LINE__)
void nob_mcp_add_param_impl(Nob_MCP_Session *session, Nob_MCP_Tool_Param_Name_And_Desc_With_Constraints tool_param_desc, const char *file, size_t line_no);

#define nob_mcp_add_array_param(session, param_name, param_type, ...) \
    nob_mcp_add_array_param_impl((session),                           \
        (Nob_MCP_Tool_Param_Name_And_Desc_With_Constraints){.name=(param_name), .type=(param_type), __VA_ARGS__}, __FILE__, __LINE__)
void nob_mcp_add_array_param_impl(Nob_MCP_Session *session, Nob_MCP_Tool_Param_Name_And_Desc_With_Constraints tool_param_desc, const char *file, size_t line_no);

#define nob_mcp_begin_array(session, ...) \
    nob_mcp_begin_array_impl((session), (Nob_MCP_Tool_Param_Name_And_Desc){__VA_ARGS__}, __FILE__, __LINE__)
void nob_mcp_begin_array_impl(Nob_MCP_Session *session, Nob_MCP_Tool_Param_Name_And_Desc tool_param_desc, const char *file, size_t line_no);
#define nob_mcp_end_array(session) nob_mcp_end_array_impl(session, __FILE__, __LINE__)
void nob_mcp_end_array_impl(Nob_MCP_Session *session, const char *file, size_t line_no);

#define nob_mcp_begin_array_param(session, param_name, ...) \
    nob_mcp_begin_array_param_impl((session), (Nob_MCP_Tool_Param_Name_And_Desc){.name=(param_name), __VA_ARGS__}, __FILE__, __LINE__)
void nob_mcp_begin_array_param_impl(Nob_MCP_Session *session, Nob_MCP_Tool_Param_Name_And_Desc tool_param_desc, const char *file, size_t line_no);
#define nob_mcp_end_array_param(session) nob_mcp_end_array_param_impl(session, __FILE__, __LINE__)
void nob_mcp_end_array_param_impl(Nob_MCP_Session *session, const char *file, size_t line_no);

#define nob_mcp_set_array_type(session, param_type, ...) \
    nob_mcp_set_array_type_impl((session),               \
        (Nob_MCP_Tool_Param_Name_And_Desc_With_Constraints){.type=(param_type), __VA_ARGS__}, __FILE__, __LINE__)
void nob_mcp_set_array_type_impl(Nob_MCP_Session *session, Nob_MCP_Tool_Param_Name_And_Desc_With_Constraints tool_param_desc, const char *file, size_t line_no);

#define nob_mcp_begin_object_param(session, param_name, ...) \
    nob_mcp_begin_object_param_impl((session), (Nob_MCP_Tool_Param_Name_And_Desc){.name=(param_name), __VA_ARGS__}, __FILE__, __LINE__)
void nob_mcp_begin_object_param_impl(Nob_MCP_Session *session, Nob_MCP_Tool_Param_Name_And_Desc tool_param_desc, const char *file, size_t line_no);
#define nob_mcp_end_object_param(session) nob_mcp_end_object_param_impl(session, __FILE__, __LINE__)
void nob_mcp_end_object_param_impl(Nob_MCP_Session *session, const char *file, size_t line_no);

void nob_mcp_write_text_content(Nob_MCP_Session *session, const char *text);
void nob_mcp_write_text_content_sized(Nob_MCP_Session *session, const char *text, size_t text_len);

#endif // NOB_MCP_H_

#ifdef NOB_MCP_IMPLEMENTATION
#ifndef NOB_MCP_IMPLEMENTATION_GAURD_

bool nob_mcp__params_parser(void *mcp_session_ptr, Nob_String_View method, Jimp *jimp, void *params) {
    UNUSED(params);
    assert(mcp_session_ptr != NULL);
    Nob_MCP_Session *mcp_session = mcp_session_ptr;

    if (nob_sv_eq(method, sv_from_cstr("tools/call"))) {
        if (!jimp_object_begin(jimp)) return false;
        while (jimp_object_member(jimp)) {
            if (strcmp(jimp->string, "name") == 0) {
                // tool_name
                if (!jimp_string(jimp)) return false;
                mcp_session->tool_name = jimp_string_as_sv(jimp);
            } else if (strcmp(jimp->string, "arguments") == 0) {
                const char *tool_args_start = jimp->point;
                if (!jimp_skip_member(jimp)) return false;
                const char *tool_args_end = jimp->point;
                jimp_begin(&mcp_session->tool_args, mcp_session->jsonrpc_session.fdin_label, tool_args_start, tool_args_end - tool_args_start);
            } else {
                if (!jimp_skip_member(jimp)) return false;
            }
        }
        if (!jimp_object_end(jimp)) return false;
    }

    return true;
}

Nob_JSONRPC_Error_Code nob_mcp__method_handler(void *mcp_session_ptr, Nob_String_View method, void *params, Jim *success, Jim *failure, char **error_message) {
    UNUSED(params);
    UNUSED(failure);
    UNUSED(error_message);
    assert(mcp_session_ptr != NULL);
    Nob_MCP_Session *mcp_session = mcp_session_ptr;

    if (nob_sv_eq(method, sv_from_cstr("initialize"))) {
        jim_object_begin(success); {
            // protocolVersion
            jim_member_key(success, "protocolVersion");
            jim_string(success, mcp_session->mcp_protocol_ver);

            // TODO: Not currently providing the tools in the capabilities. This is just to conform with MCP Protocol
            // Providing tools description and handling tool calls has to be handled seperately
            // capabilities
            jim_member_key(success, "capabilities");
            jim_object_begin(success); {
                // tools
                jim_member_key(success, "tools");
                jim_object_begin(success); {
                } jim_object_end(success);
            } jim_object_end(success);

            // serverInfo
            jim_member_key(success, "serverInfo");
            jim_object_begin(success); {
                // name
                jim_member_key(success, "name");
                jim_string(success, mcp_session->mcp_server_name);

                // version
                jim_member_key(success, "version");
                jim_string(success, mcp_session->mcp_server_ver);
            } jim_object_end(success);

            // instructions
            if (mcp_session->instructions != NULL) {
                jim_member_key(success, "instructions");
                jim_string(success, mcp_session->instructions);
            }
        } jim_object_end(success);
        return NOB_JSONRPC_ERROR_CODE_SUCCESS;
    } else if (nob_sv_starts_with(method, sv_from_cstr("notification"))) { // No response in case of notification
        return NOB_JSONRPC_ERROR_CODE_NO_RESPONSE;
    } else if (nob_sv_eq(method, sv_from_cstr("tools/list"))) {
        jim_object_begin(success); {
            jim_member_key(success, "tools");
            jim_array_begin(success); {
                if (mcp_session->tools_list_clb != NULL) {
                    if (!mcp_session->tools_list_clb(mcp_session)) {
                        *error_message = "Error while generating tools/list. Please check if the tools/list is properly getting generated";
                        return NOB_JSONRPC_ERROR_CODE_INTERNAL_ERROR;
                    }
                }
            } jim_array_end(success);
        } jim_object_end(success);
        return NOB_JSONRPC_ERROR_CODE_SUCCESS;
    } else if (nob_sv_eq(method, sv_from_cstr("tools/call"))) {
        jim_object_begin(success); {
            bool is_error = false;
            jim_member_key(success, "content");
            jim_array_begin(success); {
                if (mcp_session->tools_call_clb != NULL) {
                    is_error = !mcp_session->tools_call_clb(mcp_session, mcp_session->tool_name, &mcp_session->tool_args);
                }
            } jim_array_end(success);
            if (is_error) {
                jim_member_key(success, "isError");
                jim_bool(success, is_error);
            }
        } jim_object_end(success);
        return NOB_JSONRPC_ERROR_CODE_SUCCESS;
    }
    return NOB_JSONRPC_ERROR_CODE_METHOD_NOT_FOUND;
}

Nob_MCP_Session nob_create_mcp_session(
    int fdin, const char *fdin_label,
    int fdout, const char *fdout_label,
    const char *mcp_server_name, const char *mcp_server_ver,
    bool (*tools_list_clb)(Nob_MCP_Session *session), // tools/list callback
    bool (*tools_call_clb)(Nob_MCP_Session *session, Nob_String_View tool_name, Jimp *tool_args), // tools/call callback
    void *ctx,
    const char *instructions) {
    Nob_MCP_Session session = {0};

    Nob_JSONRPC_Params_Parser params_parser = {.params=NULL, .parse_clb=nob_mcp__params_parser};
    session.jsonrpc_session = nob_create_jsonrpc_session(
        fdin, fdin_label,
        fdout, fdout_label,
        params_parser,
        nob_mcp__method_handler,
        NULL);
    session.mcp_protocol_ver = NOB_MCP_DEFAULT_PROTOCOL_VER;
    session.mcp_server_name = mcp_server_name;
    session.mcp_server_ver = mcp_server_ver;
    session.tools_list_clb = tools_list_clb;
    session.tools_call_clb = tools_call_clb;
    session.ctx = ctx;
    session.instructions = instructions;
    return session;
}

void nob_free_mcp_session(Nob_MCP_Session *session) {
    nob_free_jsonrpc_session(&session->jsonrpc_session);
    free(session->tools_list_scopes.items);
    free(session->tool_args.string);
    memset(session, 0, sizeof(Nob_MCP_Session));
}

bool nob_mcp_handle_request(Nob_MCP_Session *session) {
    // Setting the jsonrpc_session ctx here so that the callbacks
    // nob_mcp__params_parser and nob_mcp__method_handler have
    // access to it
    session->jsonrpc_session.ctx = session;
    return nob_jsonrpc_handle_request(&session->jsonrpc_session);
}

const char *nob_mcp__tools_list_scope_to_cstr(Nob_MCP_Tools_List_Scope scope) {
    switch (scope) {
        case NOB_MCP_SCOPE_TOOL          : return "SCOPE_TOOL";
        case NOB_MCP_SCOPE_ARRAY         : return "SCOPE_ARRAY";
        case NOB_MCP_SCOPE_OBJECT        : return "SCOPE_OBJECT";
        case NOB_MCP_SCOPE_SET_ARRAY_TYPE: return "SCOPE_SET_ARRAY_TYPE";
        case __count_Nob_MCP_Tools_List_Scope:
        default:
            UNREACHABLE("Unknown Nob_MCP_Tools_List_Scope");
    }
}

void nob_mcp__print_tools_list_scopes(Nob_MCP_Session session) {
    nob_log(ERROR, "Tools List Scopes Content:");
    da_foreach(Nob_MCP_Tools_List_Scope, it, &session.tools_list_scopes) {
        nob_log(ERROR, "  %s", nob_mcp__tools_list_scope_to_cstr(*it));
    }
}

void nob_mcp__assert_tool_list_scope(Nob_MCP_Session session, Nob_MCP_Tools_List_Scope expected, const char *file, size_t line_no) {
    Nob_MCP_Tools_List_Scope last = da_last(&session.tools_list_scopes);
    if ((last & expected) == 0) {
        fprintf(stderr, "ERROR: %s:%zu Expected tools_list scope: ", file, line_no);
        size_t count = 0;
        for (size_t i = 1; i < __count_Nob_MCP_Tools_List_Scope; i <<= 1) {
            if (i & expected) {
                if (count > 0) {
                    fprintf(stderr, " or ");
                }
                fprintf(stderr, "%s", nob_mcp__tools_list_scope_to_cstr(i));
                count += 1;
            }
        }
        fprintf(stderr, ", got: %s\n", nob_mcp__tools_list_scope_to_cstr(last));
        nob_mcp__print_tools_list_scopes(session);
        assert(false);
    }
}

void nob_mcp_begin_tool_impl(Nob_MCP_Session *session, Nob_MCP_Tool_Desc tool_desc, const char *file, size_t line_no) {
    if (session->tools_list_scopes.count != 0) {
        nob_log(ERROR, "%s:%zu tools_list_scopes must be empty to define a new tool", file, line_no);
        nob_mcp__print_tools_list_scopes(*session);
        assert(false);
    }
    Jim *jim = &session->jsonrpc_session.success;
    jim_object_begin(jim);
        // name
        assert(tool_desc.name != NULL);
        jim_member_key(jim, "name");
        jim_string(jim, tool_desc.name);

        if (tool_desc.desc != NULL) {
            // description
            jim_member_key(jim, "description");
            jim_string(jim, tool_desc.desc);
        }

        // inputSchema
        jim_member_key(jim, "inputSchema");
        jim_object_begin(jim);
            // type
            jim_member_key(jim, "type");
            jim_string(jim, "object");

            // properties
            jim_member_key(jim, "properties");
            jim_object_begin(jim);
    da_append(&session->tools_list_scopes, NOB_MCP_SCOPE_TOOL);
}

void nob_mcp_end_tool_impl(Nob_MCP_Session *session, const char *file, size_t line_no) {
    nob_mcp__assert_tool_list_scope(*session, NOB_MCP_SCOPE_TOOL, file, line_no);
    Jim *jim = &session->jsonrpc_session.success;
            jim_object_end(jim);
        jim_object_end(jim);
    jim_object_end(jim);
    UNUSED(da_pop(&session->tools_list_scopes));
}

void nob_mcp__type_and_constraints(Jim *jim, Nob_MCP_Tool_Param_Name_And_Desc_With_Constraints tool_param_desc) {
    jim_member_key(jim, "type");
    static_assert(__count_NOB_MCP_PARAM_TYPE_TYPE == 4, "Handle new NOB_MCP_PARAM_TYPE_TYPE");
    switch (tool_param_desc.type) {
        case NOB_MCP_PARAM_TYPE_STRING: {
            jim_string(jim, "string");

            if (tool_param_desc.constraints.string.default_ != NULL) {
                // default
                jim_member_key(jim, "default");
                jim_string(jim, tool_param_desc.constraints.string.default_);
            }

            if (tool_param_desc.constraints.string.enum_count > 0
                && tool_param_desc.constraints.string.enum_array != NULL) {
                // enum
                jim_member_key(jim, "enum");
                jim_array_begin(jim); {
                    for (size_t i = 0; i < tool_param_desc.constraints.string.enum_count; i++) {
                        jim_string(jim, tool_param_desc.constraints.string.enum_array[i]);
                    }
                } jim_array_end(jim);
            }
        } break;
        case NOB_MCP_PARAM_TYPE_NUMBER: {
            jim_string(jim, "number");

            // default
            jim_member_key(jim, "default");
            jim_float(jim, tool_param_desc.constraints.number.default_);

            if (tool_param_desc.constraints.number.minimum != 0
                || tool_param_desc.constraints.number.maximum != 0) {
                // minimum
                jim_member_key(jim, "minimum");
                jim_float(jim, tool_param_desc.constraints.number.minimum);
                // maximum
                jim_member_key(jim, "maximum");
                jim_float(jim, tool_param_desc.constraints.number.maximum);
            }
        } break;
        case NOB_MCP_PARAM_TYPE_INTEGER: {
            jim_string(jim, "integer");

            // default
            jim_member_key(jim, "default");
            jim_integer(jim, tool_param_desc.constraints.integer.default_);

            if (tool_param_desc.constraints.integer.minimum != 0
                || tool_param_desc.constraints.integer.maximum != 0) {
                // minimum
                jim_member_key(jim, "minimum");
                jim_integer(jim, tool_param_desc.constraints.integer.minimum);
                // maximum
                jim_member_key(jim, "maximum");
                jim_integer(jim, tool_param_desc.constraints.integer.maximum);
            }
        } break;
        case NOB_MCP_PARAM_TYPE_BOOL: {
            jim_string(jim, "boolean");

            // default
            jim_member_key(jim, "default");
            jim_integer(jim, tool_param_desc.constraints.boolean.default_);
        } break;
        case __count_NOB_MCP_PARAM_TYPE_TYPE:
        default:
            UNREACHABLE("Unknown NOB_MCP_PARAM_TYPE_TYPE");
    }
}

void nob_mcp_add_param_impl(Nob_MCP_Session *session, Nob_MCP_Tool_Param_Name_And_Desc_With_Constraints tool_param_desc, const char *file, size_t line_no) {
    nob_mcp__assert_tool_list_scope(
        *session,
        NOB_MCP_SCOPE_TOOL | NOB_MCP_SCOPE_OBJECT,
        file, line_no);
    Jim *jim = &session->jsonrpc_session.success;
    assert(tool_param_desc.name != NULL);
    jim_member_key(jim, tool_param_desc.name);
    jim_object_begin(jim); {
        if (tool_param_desc.desc != NULL) {
            // description
            jim_member_key(jim, "description");
            jim_string(jim, tool_param_desc.desc);
        }

        // type and constraints
        nob_mcp__type_and_constraints(jim, tool_param_desc);
    } jim_object_end(jim);
}

void nob_mcp_add_array_param_impl(
    Nob_MCP_Session *session, Nob_MCP_Tool_Param_Name_And_Desc_With_Constraints tool_param_desc, const char *file, size_t line_no) {
    nob_mcp__assert_tool_list_scope(
        *session,
        NOB_MCP_SCOPE_TOOL | NOB_MCP_SCOPE_ARRAY | NOB_MCP_SCOPE_OBJECT,
        file, line_no);
    Jim *jim = &session->jsonrpc_session.success;
    if (da_last(&session->tools_list_scopes) == NOB_MCP_SCOPE_ARRAY) {
        // type
        jim_member_key(jim, "type");
        jim_string(jim, "object");

        // properties
        jim_member_key(jim, "properties");
        jim_object_begin(jim);
    }
    assert(tool_param_desc.name != NULL);
    jim_member_key(jim, tool_param_desc.name);
    jim_object_begin(jim); {
        if (tool_param_desc.desc != NULL) {
            // description
            jim_member_key(jim, "description");
            jim_string(jim, tool_param_desc.desc);
        }
        // type
        jim_member_key(jim, "type");
        jim_string(jim, "array");

        // items
        jim_member_key(jim, "items");
        jim_object_begin(jim); {
            // type and constraints
            nob_mcp__type_and_constraints(jim, tool_param_desc);
        } jim_object_end(jim);
    } jim_object_end(jim);
    if (da_last(&session->tools_list_scopes) == NOB_MCP_SCOPE_ARRAY) {
        jim_object_end(jim);
    }
}

void nob_mcp_begin_array_impl(Nob_MCP_Session *session, Nob_MCP_Tool_Param_Name_And_Desc tool_param_desc, const char *file, size_t line_no) {
    nob_mcp__assert_tool_list_scope(
        *session,
        NOB_MCP_SCOPE_ARRAY,
        file, line_no);
    Jim *jim = &session->jsonrpc_session.success;
    if (tool_param_desc.desc != NULL) {
        // description
        jim_member_key(jim, "description");
        jim_string(jim, tool_param_desc.desc);
    }
    // type
    jim_member_key(jim, "type");
    jim_string(jim, "array");

    // items
    jim_member_key(jim, "items");
    jim_object_begin(jim);

    da_append(&session->tools_list_scopes, NOB_MCP_SCOPE_ARRAY);
}

void nob_mcp_end_array_impl(Nob_MCP_Session *session, const char *file, size_t line_no) {
    nob_mcp__assert_tool_list_scope(
        *session,
        NOB_MCP_SCOPE_ARRAY | NOB_MCP_SCOPE_SET_ARRAY_TYPE,
        file, line_no);
    if (da_last(&session->tools_list_scopes) == NOB_MCP_SCOPE_SET_ARRAY_TYPE) {
        UNUSED(da_pop(&session->tools_list_scopes));
        nob_mcp__assert_tool_list_scope(
            *session,
            NOB_MCP_SCOPE_ARRAY,
            file, line_no);
    }
    Jim *jim = &session->jsonrpc_session.success;
    jim_object_end(jim);
    UNUSED(da_pop(&session->tools_list_scopes));
}

void nob_mcp_begin_array_param_impl(Nob_MCP_Session *session, Nob_MCP_Tool_Param_Name_And_Desc tool_param_desc, const char *file, size_t line_no) {
    nob_mcp__assert_tool_list_scope(
        *session,
        NOB_MCP_SCOPE_TOOL | NOB_MCP_SCOPE_ARRAY | NOB_MCP_SCOPE_OBJECT,
        file, line_no);
    Jim *jim = &session->jsonrpc_session.success;
    if (da_last(&session->tools_list_scopes) == NOB_MCP_SCOPE_ARRAY) {
        // type
        jim_member_key(jim, "type");
        jim_string(jim, "object");

        // properties
        jim_member_key(jim, "properties");
        jim_object_begin(jim);
    }
    assert(tool_param_desc.name != NULL);
    jim_member_key(jim, tool_param_desc.name);
    jim_object_begin(jim);
        if (tool_param_desc.desc != NULL) {
            // description
            jim_member_key(jim, "description");
            jim_string(jim, tool_param_desc.desc);
        }
        // type
        jim_member_key(jim, "type");
        jim_string(jim, "array");

        // items
        jim_member_key(jim, "items");
        jim_object_begin(jim);

    da_append(&session->tools_list_scopes, NOB_MCP_SCOPE_ARRAY);
}

void nob_mcp_end_array_param_impl(Nob_MCP_Session *session, const char *file, size_t line_no) {
    nob_mcp__assert_tool_list_scope(
        *session,
        NOB_MCP_SCOPE_ARRAY | NOB_MCP_SCOPE_SET_ARRAY_TYPE,
        file, line_no);
    if (da_last(&session->tools_list_scopes) == NOB_MCP_SCOPE_SET_ARRAY_TYPE) {
        UNUSED(da_pop(&session->tools_list_scopes));
        nob_mcp__assert_tool_list_scope(
            *session,
            NOB_MCP_SCOPE_ARRAY,
            file, line_no);
    }
    Jim *jim = &session->jsonrpc_session.success;
        jim_object_end(jim);
    jim_object_end(jim);
    UNUSED(da_pop(&session->tools_list_scopes));
    if (da_last(&session->tools_list_scopes) == NOB_MCP_SCOPE_ARRAY) {
        jim_object_end(jim);
    }
}

void nob_mcp_set_array_type_impl(Nob_MCP_Session *session, Nob_MCP_Tool_Param_Name_And_Desc_With_Constraints tool_param_desc, const char *file, size_t line_no) {
    nob_mcp__assert_tool_list_scope(
        *session,
        NOB_MCP_SCOPE_ARRAY,
        file, line_no);
    Jim *jim = &session->jsonrpc_session.success;
    if (tool_param_desc.desc != NULL) {
        // description
        jim_member_key(jim, "description");
        jim_string(jim, tool_param_desc.desc);
    }

    // type and constraints
    nob_mcp__type_and_constraints(jim, tool_param_desc);

    da_append(&session->tools_list_scopes, NOB_MCP_SCOPE_SET_ARRAY_TYPE);
}

void nob_mcp_begin_object_param_impl(Nob_MCP_Session *session, Nob_MCP_Tool_Param_Name_And_Desc tool_param_desc, const char *file, size_t line_no) {
    nob_mcp__assert_tool_list_scope(
        *session,
         NOB_MCP_SCOPE_TOOL | NOB_MCP_SCOPE_ARRAY | NOB_MCP_SCOPE_OBJECT,
         file, line_no);
    Jim *jim = &session->jsonrpc_session.success;
    if (da_last(&session->tools_list_scopes) == NOB_MCP_SCOPE_ARRAY) {
        // type
        jim_member_key(jim, "type");
        jim_string(jim, "object");

        // properties
        jim_member_key(jim, "properties");
        jim_object_begin(jim);
    }
    assert(tool_param_desc.name != NULL);
    jim_member_key(jim, tool_param_desc.name);
    jim_object_begin(jim);
        if (tool_param_desc.desc != NULL) {
            // description
            jim_member_key(jim, "description");
            jim_string(jim, tool_param_desc.desc);
        }
        // type
        jim_member_key(jim, "type");
        jim_string(jim, "object");

        // properties
        jim_member_key(jim, "properties");
        jim_object_begin(jim);

    da_append(&session->tools_list_scopes, NOB_MCP_SCOPE_OBJECT);
}

void nob_mcp_end_object_param_impl(Nob_MCP_Session *session, const char *file, size_t line_no) {
    nob_mcp__assert_tool_list_scope(*session, NOB_MCP_SCOPE_OBJECT, file, line_no);
    Jim *jim = &session->jsonrpc_session.success;
        jim_object_end(jim);
    jim_object_end(jim);
    UNUSED(da_pop(&session->tools_list_scopes));
    if (da_last(&session->tools_list_scopes) == NOB_MCP_SCOPE_ARRAY) {
        jim_object_end(jim);
    }
}

void nob_mcp_write_text_content(Nob_MCP_Session *session, const char *text) {
    Jim *jim = &session->jsonrpc_session.success;
    jim_object_begin(jim); {
        jim_member_key(jim, "type");
        jim_string(jim, "text");

        jim_member_key(jim, "text");
        jim_string(jim, text);
    } jim_object_end(jim);
}

void nob_mcp_write_text_content_sized(Nob_MCP_Session *session, const char *text, size_t text_len) {
    Jim *jim = &session->jsonrpc_session.success;
    jim_object_begin(jim); {
        jim_member_key(jim, "type");
        jim_string(jim, "text");

        jim_member_key(jim, "text");
        jim_string_sized(jim, text, text_len);
    } jim_object_end(jim);
}

#endif // NOB_MCP_IMPLEMENTATION_GAURD_
#endif // NOB_MCP_IMPLEMENTATION

#ifndef NOB_MCP_STRIP_PREFIX_GUARD_
#define NOB_MCP_STRIP_PREFIX_GUARD_
    #ifndef NOB_UNSTRIP_PREFIX
        #define MCP_Session                  Nob_MCP_Session
        #define create_mcp_session           nob_create_mcp_session
        #define free_mcp_session             nob_free_mcp_session
        #define mcp_handle_request           nob_mcp_handle_request
        #define mcp_begin_tool               nob_mcp_begin_tool
        #define mcp_end_tool                 nob_mcp_end_tool
        #define MCP_PARAM_TYPE_STRING        NOB_MCP_PARAM_TYPE_STRING
        #define MCP_PARAM_TYPE_NUMBER        NOB_MCP_PARAM_TYPE_NUMBER
        #define MCP_PARAM_TYPE_INTEGER       NOB_MCP_PARAM_TYPE_INTEGER
        #define MCP_PARAM_TYPE_BOOL          NOB_MCP_PARAM_TYPE_BOOL
        #define mcp_add_param                nob_mcp_add_param
        #define mcp_add_array_param          nob_mcp_add_array_param
        #define mcp_begin_array              nob_mcp_begin_array
        #define mcp_end_array                nob_mcp_end_array
        #define mcp_begin_array_param        nob_mcp_begin_array_param
        #define mcp_end_array_param          nob_mcp_end_array_param
        #define mcp_set_array_type           nob_mcp_set_array_type
        #define mcp_begin_object_param       nob_mcp_begin_object_param
        #define mcp_end_object_param         nob_mcp_end_object_param
        #define mcp_write_text_content       nob_mcp_write_text_content
        #define mcp_write_text_content_sized nob_mcp_write_text_content_sized
    #endif // NOB_UNSTRIP_PREFIX
#endif // NOB_MCP_STRIP_PREFIX_GUARD_
