#ifndef NOB_JSONRPC_H_
#define NOB_JSONRPC_H_

// NOTE: Include below headers before usage-
// #include "nob.h"
// #include "jimp.h"
// #include "jim.h"
// #include "nob_br.h"

#define NOB_JSONRPC_VER "2.0"

typedef struct {
    size_t id;
    bool is_id_parsed;

    const char *jsonrpc_ver;

    Nob_String_View method;
    bool is_method_parsed;

    bool is_params_parsed;

    Jimp jimp;
} Nob_JSONRPC_Request_Parser;

typedef struct {
    void *params;
    bool(*parse_clb)(void *ctx, Nob_String_View method, Jimp *jimp, void *params);
} Nob_JSONRPC_Params_Parser;

typedef enum {
    NOB_JSONRPC_ERROR_CODE_SUCCESS          = 0,
    NOB_JSONRPC_ERROR_CODE_NO_RESPONSE      = 1,
    NOB_JSONRPC_ERROR_CODE_PARSE_ERROR      = -32700,
    NOB_JSONRPC_ERROR_CODE_INVALID_REQUEST  = -32600,
    NOB_JSONRPC_ERROR_CODE_METHOD_NOT_FOUND = -32601,
    NOB_JSONRPC_ERROR_CODE_INVALID_PARAMS   = -32602,
    NOB_JSONRPC_ERROR_CODE_INTERNAL_ERROR   = -32603,
    NOB_JSONRPC_ERROR_CODE_SERVER_ERROR_MIN = -32099,
    NOB_JSONRPC_ERROR_CODE_SERVER_ERROR_MAX = -32000,
} Nob_JSONRPC_Error_Code;

typedef Nob_JSONRPC_Error_Code(*Nob_JSONRPC_Method_Handler)(void *ctx, Nob_String_View method, void *params, Jim *success, Jim *failure, char **error_message);

typedef struct {
    // Where to read Request
    int fdin;
    const char *fdin_label;

    // Where to write Response
    int fdout;
    const char *fdout_label;

    void *ctx;

    // For Reading and Parsing Request
    Nob_JSONRPC_Request_Parser request_parser;
    Nob_JSONRPC_Params_Parser params_parser;
    Nob_Buffered_Reader buff;
    Nob_String_Builder sb;

    // Method handler
    Nob_JSONRPC_Method_Handler method_handler;

    // For Generating JSON Response
    Jim success;
    Jim failure;
} Nob_JSONRPC_Session;

bool nob_jsonrpc_parse_request(
    Nob_JSONRPC_Request_Parser *parser,
    const char *label, const char *data, size_t count,
    Nob_JSONRPC_Params_Parser params_parser,
    void *ctx);
void nob_free_jsonrpc_request_parser(Nob_JSONRPC_Request_Parser *parser);

Nob_JSONRPC_Session nob_create_jsonrpc_session(
    int fdin, const char *fdin_label,
    int fdout, const char *fdout_label,
    Nob_JSONRPC_Params_Parser params_parser,
    Nob_JSONRPC_Method_Handler method_handler,
    void *ctx);
bool nob_jsonrpc_handle_request(Nob_JSONRPC_Session *session);
void nob_free_jsonrpc_session(Nob_JSONRPC_Session *session);

#endif // NOB_JSONRPC_H_

#ifdef NOB_JSONRPC_IMPLEMENTATION
#ifndef NOB_JSONRPC_IMPLEMENTATION_GAURD_
#define NOB_JSONRPC_IMPLEMENTATION_GAURD_

#include <string.h>
#include <unistd.h>

bool nob_jsonrpc__parse_params(Nob_JSONRPC_Params_Parser parser, void *ctx, Nob_String_View method, Jimp *jimp) {
    assert(parser.parse_clb != NULL);
    return parser.parse_clb(ctx, method, jimp, parser.params);
}

bool nob_jsonrpc_parse_request(
    Nob_JSONRPC_Request_Parser *parser,
    const char *label, const char *data, size_t count,
    Nob_JSONRPC_Params_Parser params_parser,
    void *ctx) {
    // Reset
    parser->id = 0;
    parser->is_id_parsed = false;

    parser->jsonrpc_ver = NOB_JSONRPC_VER;

    memset(&parser->method, 0, sizeof(Nob_String_View));
    parser->is_method_parsed = false;

    parser->is_params_parsed = false;

    // Parsing
    Jimp *jimp = &parser->jimp;
    jimp_begin(jimp, label, data, count);
    Nob_String_View params = {0};
    if (!jimp_object_begin(jimp)) return false;
    while (jimp_object_member(jimp)) {
        if(strcmp(jimp->string, "id") == 0) {
            if (!jimp_number(jimp)) return false;
            parser->id = (size_t) jimp->number;
            parser->is_id_parsed = true;
        } else if (strcmp(jimp->string, "method") == 0) {
            if (!jimp_string(jimp)) return false;
            parser->method = jimp_string_as_sv(jimp);
            parser->is_method_parsed = true;
        } else if (strcmp(jimp->string, "params") == 0) {
            if (params_parser.parse_clb != NULL) {
                const char *params_start = jimp->point;
                if (!jimp_skip_member(jimp)) return false;
                const char *params_end = jimp->point;
                params = nob_sv_from_parts(params_start, params_end - params_start);
            } else {
                if (!jimp_skip_member(jimp)) return false;
            }
        } else {
            if (!jimp_skip_member(jimp)) return false;
        }
    }
    if (!jimp_object_end(jimp)) return false;
    if (!parser->is_method_parsed) {
        nob_log(ERROR, "Expect `method` to be present in the json string");
        return false;
    }
    if (params_parser.parse_clb != NULL && params.data != NULL) {
        jimp_begin(jimp, label, params.data, params.count);
        if (!nob_jsonrpc__parse_params(params_parser, ctx, parser->method, jimp)) {
            nob_log(ERROR, "%s:0: Unable to parse the params", label);
            return false;
        }
        parser->is_params_parsed = true;
    }

    return true;
}

void nob_free_jsonrpc_request_parser(Nob_JSONRPC_Request_Parser *parser) {
    free(parser->jimp.string);
    memset(parser, 0, sizeof(Nob_JSONRPC_Request_Parser));
}

Nob_JSONRPC_Session nob_create_jsonrpc_session(
    int fdin, const char *fdin_label,
    int fdout, const char *fdout_label,
    Nob_JSONRPC_Params_Parser params_parser,
    Nob_JSONRPC_Method_Handler method_handler,
    void *ctx) {
    Nob_JSONRPC_Session session = {0};

    session.ctx = ctx;

    // Where to read Request
    session.fdin = fdin;
    session.fdin_label = fdin_label;

    // Where to write Response
    session.fdout = fdout;
    session.fdout_label = fdout_label;

    session.params_parser = params_parser;
    session.buff = nob_create_br(fdin);

    session.method_handler = method_handler;

    return session;
}

bool nob_jsonrpc__write_line(int fdout, const char *fdout_label, const char *message, size_t length) {
    bool result = false;
    nob_log(INFO, "Writing line to fdout(%s): |"SV_Fmt"|", fdout_label, (int) length, message);
    if (write(fdout, message, length) < 0) return_defer(false);
    if (write(fdout, "\n", 1) < 0) return_defer(false);
    result = true;
defer:
    if (!result) {
        nob_log(ERROR, "Could not write to fdout(%s): %s", fdout_label, strerror(errno));
    }
    return result;
}

void nob_jsonrpc__object_begin_with_jsonrpc_ver_and_id(Jim *jim, Nob_JSONRPC_Request_Parser request_parser) {
    jim_object_begin(jim);
    jim_member_key(jim, "jsonrpc");
    jim_string(jim, NOB_JSONRPC_VER);
    jim_member_key(jim, "id");
    if (request_parser.is_id_parsed) {
        jim_integer(jim, request_parser.id);
    } else {
        jim_null(jim);
    }
}

bool nob_jsonrpc_handle_request(Nob_JSONRPC_Session *session) {
    char *error_message = NULL;
    Nob_JSONRPC_Error_Code error_code = NOB_JSONRPC_ERROR_CODE_SUCCESS;

    session->sb.count = 0;
    while (session->sb.count == 0) { // Busy loop to look for new line
        if (!nob_br_read_line_to_sb(&session->buff, &session->sb)) return false;
    }
    nob_log(INFO, "Read line from fdin(%s): |"SV_Fmt"|", session->fdin_label, (int) session->sb.count, session->sb.items);
    if (!nob_jsonrpc_parse_request(&session->request_parser, session->fdin_label, session->sb.items, session->sb.count, session->params_parser, session->ctx)) {
        error_code = NOB_JSONRPC_ERROR_CODE_PARSE_ERROR;
    }

    // Initialize Success Jim
    Jim *success = &session->success;
    jim_begin(success); // success_main
    nob_jsonrpc__object_begin_with_jsonrpc_ver_and_id(success, session->request_parser);
        jim_member_key(success, "result");
    size_t success_sink_saved = success->sink_count;

    // Initialize Failure Jim
    Jim *failure = &session->failure;
    jim_begin(failure); // failure_main
    nob_jsonrpc__object_begin_with_jsonrpc_ver_and_id(failure, session->request_parser);
        jim_member_key(failure, "error");
        jim_object_begin(failure);
            jim_member_key(failure, "data");
    size_t failure_sink_saved = failure->sink_count;

    if (error_code == NOB_JSONRPC_ERROR_CODE_SUCCESS && session->method_handler != NULL) {
        error_code = session->method_handler(
            session->ctx, session->request_parser.method, session->params_parser.params, success, failure, &error_message);
    }

    if (error_code == NOB_JSONRPC_ERROR_CODE_NO_RESPONSE) return true;

    if (success->sink_count == success_sink_saved) {
        // Nothing added in success jim, hence setting result to null
        jim_null(success);
    }
    if (failure->sink_count == failure_sink_saved) {
        // Nothing added in failure jim, hence setting data to null
        jim_null(failure);
    }

    if (error_code == NOB_JSONRPC_ERROR_CODE_SUCCESS) {
        // Success
        jim_object_end(success); // success_main
        return nob_jsonrpc__write_line(session->fdout, session->fdout_label, success->sink, success->sink_count);
    } else {
        // Failure
        jim_member_key(failure, "code");
        jim_integer(failure, error_code);

        jim_member_key(failure, "message");
        char *error_prefix = NULL;
        if (error_code == NOB_JSONRPC_ERROR_CODE_PARSE_ERROR) {
            error_prefix = "Parse Error";
        } else if (error_code == NOB_JSONRPC_ERROR_CODE_INVALID_REQUEST) {
            error_prefix = "Invalid Request";
        } else if (error_code == NOB_JSONRPC_ERROR_CODE_METHOD_NOT_FOUND) {
            error_prefix = "Method not found";
        } else if (error_code == NOB_JSONRPC_ERROR_CODE_INVALID_PARAMS) {
            error_prefix = "Invalid params";
        } else if (error_code == NOB_JSONRPC_ERROR_CODE_INTERNAL_ERROR) {
            error_prefix = "Internal error";
        } else if (error_code >= NOB_JSONRPC_ERROR_CODE_SERVER_ERROR_MIN
                && error_code <= NOB_JSONRPC_ERROR_CODE_SERVER_ERROR_MAX) {
            error_prefix = "Server error";
        } else {
            error_prefix = "Application error";
        }
        if (error_message == NULL) {
            jim_string(failure, error_prefix);
        } else {
            size_t saved = temp_save();
            jim_string(failure, temp_sprintf("%s: %s", error_prefix, error_message));
            temp_rewind(saved);
        }

        jim_object_end(failure); // error
        jim_object_end(failure); // failure_main
        return nob_jsonrpc__write_line(session->fdout, session->fdout_label, failure->sink, failure->sink_count);
    }
}

void nob_free_jsonrpc_session(Nob_JSONRPC_Session *session) {
    nob_free_jsonrpc_request_parser(&session->request_parser);

    free(session->buff.items);

    free(session->sb.items);

    free(session->success.sink);
    free(session->success.scopes);

    free(session->failure.sink);
    free(session->failure.scopes);

    memset(session, 0, sizeof(Nob_JSONRPC_Session));
}

#endif // NOB_JSONRPC_IMPLEMENTATION_GAURD_
#endif // NOB_JSONRPC_IMPLEMENTATION

#ifndef NOB_JSONRPC_STRIP_PREFIX_GUARD_
#define NOB_JSONRPC_STRIP_PREFIX_GUARD_
    #ifndef NOB_UNSTRIP_PREFIX
        #define JSONRPC_Request_Parser               Nob_JSONRPC_Request_Parser
        #define JSONRPC_Params_Parser                Nob_JSONRPC_Params_Parser
        #define jsonrpc_parse_request                nob_jsonrpc_parse_request
        #define free_jsonrpc_request_parser          nob_free_jsonrpc_request_parser
        #define JSONRPC_Session                      Nob_JSONRPC_Session
        #define JSONRPC_Error_Code                   Nob_JSONRPC_Error_Code
        #define JSONRPC_Method_Handler               Nob_JSONRPC_Method_Handler
        #define create_jsonrpc_session               nob_create_jsonrpc_session
        #define jsonrpc_handle_request               nob_jsonrpc_handle_request
        #define JSONRPC_ERROR_CODE_SUCCESS           NOB_JSONRPC_ERROR_CODE_SUCCESS
        #define JSONRPC_ERROR_CODE_NO_RESPONSE       NOB_JSONRPC_ERROR_CODE_NO_RESPONSE
        #define JSONRPC_ERROR_CODE_PARSE_ERROR       NOB_JSONRPC_ERROR_CODE_PARSE_ERROR
        #define JSONRPC_ERROR_CODE_INVALID_REQUEST   NOB_JSONRPC_ERROR_CODE_INVALID_REQUEST
        #define JSONRPC_ERROR_CODE_METHOD_NOT_FOUND  NOB_JSONRPC_ERROR_CODE_METHOD_NOT_FOUND
        #define JSONRPC_ERROR_CODE_INVALID_PARAMS    NOB_JSONRPC_ERROR_CODE_INVALID_PARAMS
        #define JSONRPC_ERROR_CODE_INTERNAL_ERROR    NOB_JSONRPC_ERROR_CODE_INTERNAL_ERROR
        #define JSONRPC_ERROR_CODE_SERVER_ERROR_MIN  NOB_JSONRPC_ERROR_CODE_SERVER_ERROR_MIN
        #define JSONRPC_ERROR_CODE_SERVER_ERROR_MAX  NOB_JSONRPC_ERROR_CODE_SERVER_ERROR_MAX
        #define free_jsonrpc_session                 nob_free_jsonrpc_session
    #endif // NOB_UNSTRIP_PREFIX
#endif // NOB_JSONRPC_STRIP_PREFIX_GUARD_
