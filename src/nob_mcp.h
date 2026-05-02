#ifndef NOB_MCP_H_
#define NOB_MCP_H_

// NOTE: Include below headers before including this
// #define NOB_IMPLEMENTATION
// #define JIM_IMPLEMENTATION
// #define JIMP_IMPLEMENTATION
// #include "nob.h"
// #include "jim.h"
// #include "jimp.h"
// #include "nob_buffered_reader.h"

typedef struct {
    const char *jsonrpc_ver;
    const char *protocol_ver;
} Nob_MCP_Protocol_Info;

typedef struct {
    const char *name;
    const char *ver;
} Nob_MCP_Server_Info;

typedef struct {
    Nob_MCP_Protocol_Info pinfo;
    Nob_MCP_Server_Info sinfo;
} Nob_MCP;

typedef enum {
    NOB_MCP_METHOD_INITIALIZE,
    NOB_MCP_METHOD_NOTIFS_INITIALIZED,
    NOB_MCP_METHOD_TOOLS_LIST,
    NOB_MCP_METHOD_RESOURCES_LIST,
    NOB_MCP_METHOD_PROMPTS_LIST,
    __count_Nob_MCP_Method_Kind,
} Nob_MCP_Method_Kind;

typedef struct {
    size_t id;
    Nob_MCP_Method_Kind method;
} Nob_MCP_Request;

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

typedef struct {
    Nob_MCP *mcp;
    const char *fdin_label;
    int fdin;
    const char *fdout_label;
    int fdout;

    Nob_Buffered_Reader buff;
    Nob_String_Builder sb;
    Jimp jimp;
    Jim jim;
    Nob_MCP_Tools_List_Scopes tools_list_scopes;
} Nob_MCP_Session;

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

// Nob_MCP_Session functions:
Nob_MCP_Session nob_mcp_create_session(
    Nob_MCP *mcp, const char *fdin_label, int fdin, const char *fdout_label, int fdout);
bool nob_mcp_parse_request(Nob_MCP_Session *session, Nob_MCP_Request *req);
const char *nob_mcp_method_kind_to_cstr(Nob_MCP_Method_Kind kind);

#define nob_mcp_handle_initialize(session, req) nob_mcp_handle_initialize_impl(session, req, __FILE__, __LINE__)
bool nob_mcp_handle_initialize_impl(Nob_MCP_Session *session, Nob_MCP_Request req, const char *file, size_t line_no);

#define nob_mcp_begin_tools_list(session, req) nob_mcp_begin_tools_list_impl(session, req, __FILE__, __LINE__)
void nob_mcp_begin_tools_list_impl(Nob_MCP_Session *session, Nob_MCP_Request req, const char *file, size_t line_no);
#define nob_mcp_end_tools_list(session) nob_mcp_end_tools_list_impl(session, __FILE__, __LINE__)
void nob_mcp_end_tools_list_impl(Nob_MCP_Session *session, const char *file, size_t line_no);

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

#define nob_mcp_flush_tools_list(session) nob_mcp_flush_tools_list_impl(session, __FILE__, __LINE__)
bool nob_mcp_flush_tools_list_impl(Nob_MCP_Session *session, const char *file, size_t line_no);

// TODO: Complete implementation of resources/list
#define nob_mcp_handle_resources_list(session, req) nob_mcp_handle_resources_list_impl(session, req, __FILE__, __LINE__)
bool nob_mcp_handle_resources_list_impl(Nob_MCP_Session *session, Nob_MCP_Request req, const char *file, size_t line_no);

// TODO: Complete implementation of prompts/list
#define nob_mcp_handle_prompts_list(session, req) nob_mcp_handle_prompts_list_impl(session, req, __FILE__, __LINE__)
bool nob_mcp_handle_prompts_list_impl(Nob_MCP_Session *session, Nob_MCP_Request req, const char *file, size_t line_no);

void nob_free_mcp_session(Nob_MCP_Session *session);

#ifdef NOB_MCP_IMPLEMENTATION

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

const Nob_MCP_Protocol_Info nob_mcp_default_pinfo = {.jsonrpc_ver="2.0", .protocol_ver="2024-11-05"};

// Nob_MCP_Session functions:
Nob_MCP_Session nob_mcp_create_session(
    Nob_MCP *mcp, const char *fdin_label, int fdin, const char *fdout_label, int fdout) {
    Nob_MCP_Session ret = {0};
    ret.mcp = mcp;
    ret.fdin_label = fdin_label;
    ret.fdin = fdin;
    ret.fdout_label = fdout_label;
    ret.fdout = fdout;
    ret.buff = nob_create_br(fdin);
    return ret;
}

void nob_free_mcp_session(Nob_MCP_Session *session) {
    free(session->jimp.string);
    free(session->jim.sink);
    free(session->jim.scopes);
    free(session->tools_list_scopes.items);
    free(session->sb.items);
    memset(session, 0, sizeof(Nob_MCP_Session));
}

bool nob_mcp_parse_request(Nob_MCP_Session *session, Nob_MCP_Request *req) {
    session->sb.count = 0;
    while (session->sb.count == 0) { // Busy looping until we read a line in string builder
        if (!nob_br_read_line_to_sb(&session->buff, &session->sb)) {
            nob_log(ERROR, "Could not read line from fdin(%s): %s", session->fdin_label, strerror(errno));
            return false;
        }
    }
    String_View line = nob_sb_to_sv(session->sb);
    nob_log(INFO, "Read line from fdin(%s): |"SV_Fmt"|", session->fdin_label, SV_Arg(line));

    Jimp *jimp = &session->jimp;
    jimp_begin(jimp, session->fdin_label, line.data, line.count);
    if (!jimp_object_begin(jimp)) return false;

    bool is_id_parsed = false;
    bool is_method_parsed = false;

    while (jimp_object_member(jimp)) {
        if(strcmp(jimp->string, "id") == 0) {
            jimp_number(jimp);
            req->id = (size_t) jimp->number;
            is_id_parsed = true;
        } else if (strcmp(jimp->string, "method") == 0) {
            if (!jimp_string(jimp)) return false;
            static_assert(__count_Nob_MCP_Method_Kind == 5, "Handle new Nob_MCP_Method_Kind");
            if (strcmp(jimp->string, "initialize") == 0) {
                req->method = NOB_MCP_METHOD_INITIALIZE;
            } else if (strcmp(jimp->string, "notifications/initialized") == 0) {
                req->method = NOB_MCP_METHOD_NOTIFS_INITIALIZED;
            } else if (strcmp(jimp->string, "tools/list") == 0) {
                req->method = NOB_MCP_METHOD_TOOLS_LIST;
            } else if (strcmp(jimp->string, "resources/list") == 0) {
                req->method = NOB_MCP_METHOD_RESOURCES_LIST;
            } else if (strcmp(jimp->string, "prompts/list") == 0) {
                req->method = NOB_MCP_METHOD_PROMPTS_LIST;
            } else {
                jimp_diagf(jimp, "ERROR: Don't recognize the method type: %s\n", jimp->string);
                return false;
            }
            is_method_parsed = true;
        } else {
            jimp_skip_member(jimp);
        }
    }
    if (!jimp_object_end(jimp)) return false;

    if (!is_method_parsed) {
        jimp_diagf(jimp, "ERROR: Expect `method` to be present in the json string\n");
        return false;
    }

    if (!is_id_parsed && req->method != NOB_MCP_METHOD_NOTIFS_INITIALIZED) {
        jimp_diagf(jimp, "ERROR: Expect `id` to be present in the json string\n");
        return false;
    }

    return true;
}

const char *nob_mcp_method_kind_to_cstr(Nob_MCP_Method_Kind kind) {
    static_assert(__count_Nob_MCP_Method_Kind == 5, "Handle new Nob_MCP_Method_Kind");
    switch (kind) {
        case NOB_MCP_METHOD_INITIALIZE        : return "initialize";
        case NOB_MCP_METHOD_NOTIFS_INITIALIZED: return "notifications/initialized";
        case NOB_MCP_METHOD_TOOLS_LIST        : return "tools/list";
        case NOB_MCP_METHOD_RESOURCES_LIST    : return "resources/list";
        case NOB_MCP_METHOD_PROMPTS_LIST      : return "prompts/list";
        case __count_Nob_MCP_Method_Kind:
        default:
            nob_log(ERROR, "Unknown Nob_MCP_Method_Kind: %d, returning `UNKNOWN`", kind);
            return "UNKNOWN";
    }
}

void nob_mcp__log_request(Nob_MCP_Request req) {
    nob_log(INFO, "Request[id: %lu]:", req.id);
    nob_log(INFO, "  Method:");
    nob_log(INFO, "    Kind: %s", nob_mcp_method_kind_to_cstr(req.method));
}

bool nob_mcp__write_line(Nob_MCP_Session *session, const char *message, size_t length) {
    bool result = false;
    if (write(session->fdout, message, length) < 0) return_defer(false);
    if (write(session->fdout, "\n", 1) < 0) return_defer(false);
    result = true;
defer:
    if (!result) {
        nob_log(ERROR, "Could not write to fdout(%s): %s", session->fdout_label, strerror(errno));
    }
    return result;
}

bool nob_mcp__flush_jim(Nob_MCP_Session *session) {
    nob_log(INFO, "Flushing jim content:");
    fprintf(stderr, SV_Fmt"\n", (int) session->jim.sink_count, session->jim.sink);
    bool result = nob_mcp__write_line(session, session->jim.sink, session->jim.sink_count);
    jim_begin(&session->jim); // Reset JIM
    return result;
}

void nob_mcp__assert_request_method(Nob_MCP_Request req, Nob_MCP_Method_Kind expected, const char *file, size_t line_no) {
    if (req.method != expected) {
        nob_log(ERROR, "%s:%zu Expected method: %s, got: %s",
            file, line_no,
            nob_mcp_method_kind_to_cstr(expected),
            nob_mcp_method_kind_to_cstr(req.method));
        assert(false);
    }
}

bool nob_mcp_handle_initialize_impl(Nob_MCP_Session *session, Nob_MCP_Request req, const char *file, size_t line_no) {
    nob_mcp__assert_request_method(req, NOB_MCP_METHOD_INITIALIZE, file, line_no);
    Jim *jim = &session->jim;
    jim_begin(jim);
    jim_object_begin(jim); {
        // jsonrpc version
        jim_member_key(jim, "jsonrpc");
        jim_string(jim, session->mcp->pinfo.jsonrpc_ver);

        // message id
        jim_member_key(jim, "id");
        jim_integer(jim, req.id);

        // Protocol and Server Info
        jim_member_key(jim, "result");
        jim_object_begin(jim); {
            // protocolVersion
            jim_member_key(jim, "protocolVersion");
            jim_string(jim, session->mcp->pinfo.protocol_ver);

            // TODO: Not currently providing the tools in the capabilities. This is just to conform with MCP Protocol
            // Providing tools description and handling tool calls has to be handled seperately
            // capabilities
            jim_member_key(jim, "capabilities");
            jim_object_begin(jim); {
                // tools
                jim_member_key(jim, "tools");
                jim_object_begin(jim); {
                } jim_object_end(jim);
            } jim_object_end(jim);

            // serverInfo
            jim_member_key(jim, "serverInfo");
            jim_object_begin(jim); {
                // name
                jim_member_key(jim, "name");
                jim_string(jim, session->mcp->sinfo.name);

                // version
                jim_member_key(jim, "version");
                jim_string(jim, session->mcp->sinfo.ver);
            } jim_object_end(jim);
        } jim_object_end(jim);
    } jim_object_end(jim);
    return nob_mcp__flush_jim(session);
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
    nob_log(INFO, "Tools List Scopes Content:");
    da_foreach(Nob_MCP_Tools_List_Scope, it, &session.tools_list_scopes) {
        nob_log(INFO, "  %s", nob_mcp__tools_list_scope_to_cstr(*it));
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

void nob_mcp_begin_tools_list_impl(Nob_MCP_Session *session, Nob_MCP_Request req, const char *file, size_t line_no) {
    nob_mcp__assert_request_method(req, NOB_MCP_METHOD_TOOLS_LIST, file, line_no);
    if (session->tools_list_scopes.count > 0) {
        nob_log(ERROR, "%s:%zu tools_list_scopes must be empty to begin tools_list", file, line_no);
        nob_mcp__print_tools_list_scopes(*session);
        assert(false);
    }
    Jim *jim = &session->jim;
    jim_begin(jim);
    jim_object_begin(jim);
        // jsonrpc version
        jim_member_key(jim, "jsonrpc");
        jim_string(jim, session->mcp->pinfo.jsonrpc_ver);

        // message id
        jim_member_key(jim, "id");
        jim_integer(jim, req.id);

        // result
        jim_member_key(jim, "result");
        jim_object_begin(jim);

            // tools
            jim_member_key(jim, "tools");
            jim_array_begin(jim);
}

void nob_mcp_end_tools_list_impl(Nob_MCP_Session *session, const char *file, size_t line_no) {
    UNUSED(file);
    UNUSED(line_no);
    Jim *jim = &session->jim;
            jim_array_end(jim);
        jim_object_end(jim);
    jim_object_end(jim);
}

void nob_mcp_begin_tool_impl(Nob_MCP_Session *session, Nob_MCP_Tool_Desc tool_desc, const char *file, size_t line_no) {
    if (session->tools_list_scopes.count != 0) {
        nob_log(ERROR, "%s:%zu tools_list_scopes must be empty to define a new tool", file, line_no);
        nob_mcp__print_tools_list_scopes(*session);
        assert(false);
    }
    Jim *jim = &session->jim;
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
    Jim *jim = &session->jim;
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
    Jim *jim = &session->jim;
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
    Jim *jim = &session->jim;
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
    Jim *jim = &session->jim;
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
    Jim *jim = &session->jim;
    jim_object_end(jim);
    UNUSED(da_pop(&session->tools_list_scopes));
}

void nob_mcp_begin_array_param_impl(Nob_MCP_Session *session, Nob_MCP_Tool_Param_Name_And_Desc tool_param_desc, const char *file, size_t line_no) {
    nob_mcp__assert_tool_list_scope(
        *session,
        NOB_MCP_SCOPE_TOOL | NOB_MCP_SCOPE_ARRAY | NOB_MCP_SCOPE_OBJECT,
        file, line_no);
    Jim *jim = &session->jim;
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
    Jim *jim = &session->jim;
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
    Jim *jim = &session->jim;
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
    Jim *jim = &session->jim;
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
    Jim *jim = &session->jim;
        jim_object_end(jim);
    jim_object_end(jim);
    UNUSED(da_pop(&session->tools_list_scopes));
    if (da_last(&session->tools_list_scopes) == NOB_MCP_SCOPE_ARRAY) {
        jim_object_end(jim);
    }
}

bool nob_mcp_flush_tools_list_impl(Nob_MCP_Session *session, const char *file, size_t line_no) {
    if (session->tools_list_scopes.count > 0) {
        nob_log(ERROR, "%s: %zu tools_list_scopes count must be zero to flush the tools list", file, line_no);
        nob_mcp__print_tools_list_scopes(*session);
        assert(false);
    }
    return nob_mcp__flush_jim(session);
}

bool nob_mcp_handle_resources_list_impl(Nob_MCP_Session *session, Nob_MCP_Request req, const char *file, size_t line_no) {
    nob_mcp__assert_request_method(req, NOB_MCP_METHOD_RESOURCES_LIST, file, line_no);
    Jim *jim = &session->jim;
    jim_begin(jim);
    jim_object_begin(jim); {
        // jsonrpc version
        jim_member_key(jim, "jsonrpc");
        jim_string(jim, session->mcp->pinfo.jsonrpc_ver);

        // message id
        jim_member_key(jim, "id");
        jim_integer(jim, req.id);

        // Protocol and Server Info
        jim_member_key(jim, "result");
        jim_object_begin(jim); {
            // resources
            jim_member_key(jim, "resources");
            jim_array_begin(jim); {
            } jim_array_end(jim);
        } jim_object_end(jim);
    } jim_object_end(jim);
    return nob_mcp__flush_jim(session);
}

bool nob_mcp_handle_prompts_list_impl(Nob_MCP_Session *session, Nob_MCP_Request req, const char *file, size_t line_no) {
    nob_mcp__assert_request_method(req, NOB_MCP_METHOD_PROMPTS_LIST, file, line_no);
    Jim *jim = &session->jim;
    jim_begin(jim);
    jim_object_begin(jim); {
        // jsonrpc version
        jim_member_key(jim, "jsonrpc");
        jim_string(jim, session->mcp->pinfo.jsonrpc_ver);

        // message id
        jim_member_key(jim, "id");
        jim_integer(jim, req.id);

        // Protocol and Server Info
        jim_member_key(jim, "result");
        jim_object_begin(jim); {
            // prompts
            jim_member_key(jim, "prompts");
            jim_array_begin(jim); {
            } jim_array_end(jim);
        } jim_object_end(jim);
    } jim_object_end(jim);
    return nob_mcp__flush_jim(session);
}

#ifndef NOB_MCP_STRIP_PREFIX_GUARD_
#define NOB_MCP_STRIP_PREFIX_GUARD_
    #ifndef NOB_UNSTRIP_PREFIX
       // Nob_MCP
       #define MCP_Protocol_Info  Nob_MCP_Protocol_Info
       #define mcp_default_pinfo  nob_mcp_default_pinfo
       #define MCP_Server_Info    Nob_MCP_Server_Info
       #define MCP                Nob_MCP

       // Nob_MCP_Session
       #define MCP_METHOD_INITIALIZE         NOB_MCP_METHOD_INITIALIZE
       #define MCP_METHOD_NOTIFS_INITIALIZED NOB_MCP_METHOD_NOTIFS_INITIALIZED
       #define MCP_METHOD_TOOLS_LIST         NOB_MCP_METHOD_TOOLS_LIST
       #define MCP_Request                   Nob_MCP_Request
       #define MCP_Session                   Nob_MCP_Session

       // Nob_MCP_Session functions
       #define mcp_create_sesssion           nob_mcp_create_session
       #define mcp_parse_request             nob_mcp_parse_request
       #define mcp_method_kind_to_cstr       nob_mcp_method_kind_to_cstr
       #define mcp_handle_initialize         nob_mcp_handle_initialize
       #define mcp_begin_tools_list          nob_mcp_begin_tools_list
       #define mcp_end_tools_list            nob_mcp_end_tools_list
       #define mcp_begin_tool                nob_mcp_begin_tool
       #define mcp_end_tool                  nob_mcp_end_tool
       #define MCP_PARAM_TYPE_STRING         NOB_MCP_PARAM_TYPE_STRING
       #define MCP_PARAM_TYPE_NUMBER         NOB_MCP_PARAM_TYPE_NUMBER
       #define MCP_PARAM_TYPE_INTEGER        NOB_MCP_PARAM_TYPE_INTEGER
       #define MCP_PARAM_TYPE_BOOL           NOB_MCP_PARAM_TYPE_BOOL
       #define mcp_add_param                 nob_mcp_add_param
       #define mcp_add_array_param           nob_mcp_add_array_param
       #define mcp_begin_array               nob_mcp_begin_array
       #define mcp_end_array                 nob_mcp_end_array
       #define mcp_begin_array_param         nob_mcp_begin_array_param
       #define mcp_end_array_param           nob_mcp_end_array_param
       #define mcp_set_array_type            nob_mcp_set_array_type
       #define mcp_begin_object_param        nob_mcp_begin_object_param
       #define mcp_end_object_param          nob_mcp_end_object_param
       #define mcp_flush_tools_list          nob_mcp_flush_tools_list
       #define mcp_handle_resources_list     nob_mcp_handle_resources_list
       #define mcp_handle_prompts_list       nob_mcp_handle_prompts_list
       #define free_mcp_session              nob_free_mcp_session
    #endif // NOB_UNSTRIP_PREFIX
#endif // NOB_MCP_STRIP_PREFIX_GUARD_

#endif // NOB_MCP_IMPLEMENTATION

#endif // NOB_MCP_H_
