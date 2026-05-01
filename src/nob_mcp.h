#ifndef NOB_MCP_H_
#define NOB_MCP_H_

// NOTE: Include below headers before including this
// #define NOB_IMPLEMENTATION
// #define JIM_IMPLEMENTATION
// #define JIMP_IMPLEMENTATION
// #include "nob.h"
// #include "jim.h"
// #include "jimp.h"

#ifndef NOB_MCP_REQUEST_BUFFER_LEN
#define NOB_MCP_REQUEST_BUFFER_LEN 8129 // 8KB
#endif // NOB_MCP_REQUEST_BUFFER_LEN

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
    __count_Nob_MCP_Method_Kind,
} Nob_MCP_Method_Kind;

typedef struct {
    size_t id;
    Nob_MCP_Method_Kind method;
} Nob_MCP_Request;

typedef struct {
    Nob_MCP *mcp;
    const char *fdin_label;
    int fdin;
    const char *fdout_label;
    int fdout;

    char buffer[NOB_MCP_REQUEST_BUFFER_LEN];
    Jimp jimp;
    Jim jim;
} Nob_MCP_Session;

// Nob_MCP_Session functions:
Nob_MCP_Session nob_mcp_create_session(
    Nob_MCP *mcp, const char *fdin_label, int fdin, const char *fdout_label, int fdout);
bool nob_mcp_parse_request(Nob_MCP_Session *session, Nob_MCP_Request *req);
const char *nob_mcp_method_kind_to_cstr(Nob_MCP_Method_Kind kind);
bool nob_mcp_handle_initialize(Nob_MCP_Session *session, Nob_MCP_Request *req);
bool nob_mcp_handle_notifs__initialized(Nob_MCP_Session *session, Nob_MCP_Request *req);
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
    return ret;
}

void nob_free_mcp_session(Nob_MCP_Session *session) {
    free(session->jimp.string);
    free(session->jim.sink);
    free(session->jim.scopes);
    memset(session, 0, sizeof(Nob_MCP_Session));
}

bool nob_mcp_parse_request(Nob_MCP_Session *session, Nob_MCP_Request *req) {
    int nread = read(session->fdin, session->buffer, ARRAY_LEN(session->buffer));
    if (nread < 0) {
        nob_log(ERROR, "Could not read line from fdin(%s): %s", session->fdin_label, strerror(errno));
        return false;
    }
    String_View line = sv_trim_while(sv_from_parts(session->buffer, nread), isspace);
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
            static_assert(__count_Nob_MCP_Method_Kind == 2, "Handle new Nob_MCP_Method_Kind");
            if (strcmp(jimp->string, "initialize") == 0) {
                req->method = NOB_MCP_METHOD_INITIALIZE;
            } else if (strcmp(jimp->string, "notifications/initialized") == 0) {
                req->method = NOB_MCP_METHOD_NOTIFS_INITIALIZED;
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
    static_assert(__count_Nob_MCP_Method_Kind == 2, "Handle new Nob_MCP_Method_Kind");
    switch (kind) {
    case NOB_MCP_METHOD_INITIALIZE:         return "initialize";
    case NOB_MCP_METHOD_NOTIFS_INITIALIZED: return "notifications/initialized";
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

void nob_mcp__assert_request_method(Nob_MCP_Request *req, Nob_MCP_Method_Kind expected) {
    assert(req->method == expected
        && temp_sprintf("Expected method: %s, got: %s",
            nob_mcp_method_kind_to_cstr(expected),
            nob_mcp_method_kind_to_cstr(req->method)));
}

bool nob_mcp_handle_initialize(Nob_MCP_Session *session, Nob_MCP_Request *req) {
    nob_mcp__assert_request_method(req, NOB_MCP_METHOD_INITIALIZE);
    Jim *jim = &session->jim;
    jim_begin(jim);
    jim_object_begin(jim); {
        // jsonrpc version
        jim_member_key(jim, "jsonrpc");
        jim_string(jim, session->mcp->pinfo.jsonrpc_ver);

        // message id
        jim_member_key(jim, "id");
        jim_integer(jim, req->id);

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
    return nob_mcp__write_line(session, jim->sink, jim->sink_count);
}

bool nob_mcp_handle_notifs__initialized(Nob_MCP_Session *session, Nob_MCP_Request *req) {
    nob_mcp__assert_request_method(req, NOB_MCP_METHOD_NOTIFS_INITIALIZED);
    Jim *jim = &session->jim;
    jim_begin(jim);
    jim_object_begin(jim); {
    } jim_object_end(jim);
    return nob_mcp__write_line(session, jim->sink, jim->sink_count);
}

#ifndef NOB_MCP_STRIP_PREFIX_GUARD_
#define NOB_MCP_STRIP_PREFIX_GUARD_
    #ifndef NOB_UNSTRIP_PREFIX
       // Nob_MCP
       #define MCP_Protocol_Info  Nob_MCP_Protocol_Info
       #define mcp_default_pinfo  nob_mcp_default_pinfo
       #define MCP_Server_Info    Nob_MCP_Server_Info
       #define MCP_Tool           Nob_MCP_Tool
       #define MCP_Tools          Nob_MCP_Tools
       #define MCP                Nob_MCP

       // Nob_MCP_Session
       #define MCP_METHOD_INITIALIZE         NOB_MCP_METHOD_INITIALIZE
       #define MCP_METHOD_NOTIFS_INITIALIZED NOB_MCP_METHOD_NOTIFS_INITIALIZED
       #define MCP_Request                   Nob_MCP_Request
       #define MCP_Session                   Nob_MCP_Session

       // Nob_MCP_Session functions
       #define mcp_create_sesssion           nob_mcp_create_session
       #define mcp_parse_request             nob_mcp_parse_request
       #define mcp_method_kind_to_cstr       nob_mcp_method_kind_to_cstr
       #define mcp_handle_initialize         nob_mcp_handle_initialize
       #define mcp_handle_notifs_initialized nob_mcp_handle_notifs__initialized
       #define free_mcp_session              nob_free_mcp_session
    #endif // NOB_UNSTRIP_PREFIX
#endif // NOB_MCP_STRIP_PREFIX_GUARD_

#endif // NOB_MCP_IMPLEMENTATION

#endif // NOB_MCP_H_
