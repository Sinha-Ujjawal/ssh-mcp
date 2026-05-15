#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <openssl/rand.h>

#define NOB_IMPLEMENTATION
#define JIM_IMPLEMENTATION
#define JIMP_IMPLEMENTATION
#define NOB_BR_IMPLEMENTATION
#define NOB_JSONRPC_IMPLEMENTATION
#define NOB_MCP_IMPLEMENTATION
#define NOB_HASH_IMPLEMENTATION
#include "nob.h"
#include "jim.h"
#include "jimp.h"
#include "nob_br.h"
#include "nob_jsonrpc.h"
#include "nob_mcp.h"
#include "nob_fixed_deque.h"
#include "nob_channels.h"
#include "nob_hash.h"
#include "nob_ht.h"

#define DEFAULT_SSH_PORT 2222
#define MAX_SSH_USER 32
#define MAX_SSH_HOST 256

#define DEFAULT_SSH_MASTER_CONNECTION_PARAMS  \
    "-o", "ControlMaster=yes"                 \
  , "-o", "ControlPersist=4h"                 \
  , "-o", "StrictHostKeyChecking=accept-new"  \
  , "-fN"

#define TOOL_SSH_CONNECT          "ssh_connect"
#define TOOL_SSH_EXECUTE          "ssh_execute"
#define TOOL_SSH_DISCONNECT       "ssh_disconnect"
#define TOOL_SSH_LIST_CONNECTIONS "ssh_list_connections"

#define CONNECTION_ID_RAND_BYTES_LEN 4
typedef struct {
    char hex[(CONNECTION_ID_RAND_BYTES_LEN << 1) + 1];
} Connection_Id;

typedef struct {
    char user[MAX_SSH_USER + 1];
    char host[MAX_SSH_HOST + 1];
    size_t port;
} Connection_Detail;

typedef struct {
    embed_ht_kv_slot(Connection_Id, Connection_Detail);
} Connection_Ht_Slot;

typedef struct {
    embed_ht_with_slot(Connection_Ht_Slot);
    pthread_mutex_t lock;
} Connection_Ht;

typedef struct {
    String_Builder sb;
    Cmd cmd;
    const char *ssh_master_root;
    Connection_Ht *ht;
} My_Context;

typedef struct {
    const char *label;
    String_View sv;
} Line;

typedef embed_channel(Line, 64) Line_Chan;
typedef embed_channel(Jim, 64) Jim_Chan;

typedef struct {
    Line_Chan *in;
    Jim_Chan *out;
    const char *instructions;
    const char *ssh_master_root;
    Connection_Ht *ht;
} MCP_Args;

typedef struct {
    String_Builder *sb;
    size_t start;
    size_t end;
} String_Builder_Slice;

#define sbs_as_sv(sbs) sv_from_parts(sbs.sb->items + sbs.start, sbs.end - sbs.start)

bool set_env_if_missing(const char *name, const char *value) {
    // Check if the variable exists
    if (getenv(name) == NULL) {
        if (setenv(name, value, 0) == 0) {
            nob_log(INFO, "Set %s to %s\n", name, value);
            return true;
        } else {
            nob_log(ERROR, "setenv failed: %s", strerror(errno));
            return false;
        }
    } else {
        nob_log(INFO, "%s is already set to: %s (skipping)\n", name, getenv(name));
        return true;
    }
}

bool tools_list_clb(MCP_Request_Handler *request_handler) {
    mcp_begin_tool(
        request_handler,
        TOOL_SSH_CONNECT,
        .desc="Connect to a server via SSH. This tool will return with a connection_id which can be used with other tools for subsequent SSH operations"); {
        mcp_add_param(request_handler, "host", MCP_PARAM_TYPE_STRING, .desc="Hostname of the server");
        mcp_add_param(
            request_handler,
            "port",
            MCP_PARAM_TYPE_INTEGER,
            .desc="Port of the server",
            .constraints.integer.default_=DEFAULT_SSH_PORT
        );
        mcp_add_param(request_handler, "user", MCP_PARAM_TYPE_STRING, .desc="Username to use to connect with the server");
    } mcp_end_tool(request_handler);

    mcp_begin_tool(
        request_handler,
        TOOL_SSH_EXECUTE,
        .desc="Execute a command on the SSH server, given it's connection_id"); {
        mcp_add_param(request_handler, "connection_id", MCP_PARAM_TYPE_STRING, .desc="connection_id of the server (got from ssh_connect)");
        mcp_add_param(request_handler, "cmd", MCP_PARAM_TYPE_STRING, .desc="command to execute on the server");
    } mcp_end_tool(request_handler);

    mcp_begin_tool(
        request_handler,
        TOOL_SSH_DISCONNECT,
        .desc="Disconnect from an existing connection_id"); {
        mcp_add_param(request_handler, "connection_id", MCP_PARAM_TYPE_STRING, .desc="connection_id of the server (got from ssh_connect)");
    } mcp_end_tool(request_handler);

    mcp_begin_tool(
        request_handler,
        TOOL_SSH_LIST_CONNECTIONS,
        .desc="List the available control master connections"); {
    } mcp_end_tool(request_handler);

    return true;
}

#define hash_connection_id(c) \
    hash_bytes(c.hex, ARRAY_LEN(c.hex))

bool is_connection_id_equal(Connection_Id id1, Connection_Id id2) {
    return memcmp(id1.hex, id2.hex, ARRAY_LEN(id1.hex)) == 0;
}

bool create_new_connection_id(Connection_Id *connection_id) {
    unsigned char bytes[CONNECTION_ID_RAND_BYTES_LEN] = {0};
    int res = RAND_bytes(bytes, ARRAY_LEN(bytes));
    if (res != 1) return false;
    for (size_t i = 0; i < ARRAY_LEN(bytes); i++) {
        sprintf(connection_id->hex + (i * 2), "%02x", bytes[i]);
    }
    connection_id->hex[ARRAY_LEN(connection_id->hex) - 1] = '\0';
    return true;
}

void put_connection_detail(Connection_Ht *ht, Connection_Id id, Connection_Detail detail) {
    pthread_mutex_lock(&ht->lock);
    ssize_t out = HT_NOT_FOUND;
    nob_log(INFO, "put_connection_detail(%s), hash: %zu", id.hex, hash_connection_id(id));
    ht_insert_key(ht, hash_connection_id, is_connection_id_equal, id, &out);
    assert(out != (ssize_t) HT_NOT_FOUND);
    ht->items[out].value = detail;
    pthread_mutex_unlock(&ht->lock);
}

bool get_connection_detail(Connection_Ht *ht, Connection_Id id, Connection_Detail *detail) {
    bool result = false;
    pthread_mutex_lock(&ht->lock);

    ssize_t out = HT_NOT_FOUND;
    nob_log(INFO, "get_connection_detail(%s), hash: %zu", id.hex, hash_connection_id(id));
    ht_get_key(ht, hash_connection_id, is_connection_id_equal, id, &out);
    if (out != (ssize_t) HT_NOT_FOUND) {
        *detail = ht->items[out].value;
        return_defer(true);
    }

defer:
    pthread_mutex_unlock(&ht->lock);
    return result;
}

bool delete_connection_detail(Connection_Ht *ht, Connection_Id id, Connection_Detail *detail) {
    bool result = false;
    pthread_mutex_lock(&ht->lock);

    Connection_Ht_Slot slot = {0};
    nob_log(INFO, "delete_connection_detail(%s), hash: %zu", id.hex, hash_connection_id(id));
    ht_delete_key(ht, hash_connection_id, is_connection_id_equal, id, &slot);
    if (slot.is_occupied) {
        nob_log(INFO, "Deleted connection_id: %s", id.hex);
        *detail = slot.value;
        return_defer(true);
    }

defer:
    pthread_mutex_unlock(&ht->lock);
    return result;
}

bool handle_ssh_connect(MCP_Request_Handler *request_handler, My_Context *ctx, Jimp *tool_args) {
    String_View host = {0};
    String_View user = {0};
    size_t port = DEFAULT_SSH_PORT;

    if (!jimp_object_begin(tool_args)) return false;
    while (jimp_object_member(tool_args)) {
        if (strcmp(tool_args->string, "host") == 0) {
            if (!jimp_string(tool_args)) return false;
            host = jimp_string_as_sv(tool_args);
        } else if (strcmp(tool_args->string, "user") == 0) {
            if (!jimp_string(tool_args)) return false;
            user = jimp_string_as_sv(tool_args);
        } else if (strcmp(tool_args->string, "port") == 0) {
            if (!jimp_number(tool_args)) return false;
            port = (size_t) tool_args->number;
        } else if (!jimp_skip_member(tool_args)) return false;
    }
    if (!jimp_object_end(tool_args)) return false;

    if (!host.data) {
        mcp_write_text_content(request_handler, "Hostname not provided!");
        return false;
    }
    if (!user.data) {
        mcp_write_text_content(request_handler, "Username not provided!");
        return false;
    }

    if (host.count > MAX_SSH_HOST) {
        size_t start_count = ctx->sb.count;
        sb_appendf(&ctx->sb, "Hostname cannot be more than %d bytes!", MAX_SSH_HOST);
        size_t end_count = ctx->sb.count;
        mcp_write_text_content_sized(request_handler, ctx->sb.items + start_count, end_count - start_count);
        ctx->sb.count -= end_count - start_count;
        return false;
    }

    // Create connection id
    Connection_Id connection_id = {0};
    if (!create_new_connection_id(&connection_id)) {
        mcp_write_text_content(request_handler, "Could not create a new connection id!");
        return false;
    }

    // Contruct user:host
    String_Builder_Slice user_and_host = {0};
    {
        user_and_host.sb = &ctx->sb;
        user_and_host.start = ctx->sb.count;
        sb_appendf(&ctx->sb, SV_Fmt"@"SV_Fmt, SV_Arg(user), SV_Arg(host));
        sb_append_null(&ctx->sb);
        user_and_host.end = ctx->sb.count;
    }

    // Construct control socket path ssh option
    String_Builder_Slice control_path_ssh_opt = {0};
    {
        control_path_ssh_opt.sb = &ctx->sb;
        control_path_ssh_opt.start = ctx->sb.count;
        sb_appendf(&ctx->sb, "ControlPath=%s/master-%s", ctx->ssh_master_root, connection_id.hex);
        sb_append_null(&ctx->sb);
        control_path_ssh_opt.end = ctx->sb.count;
    }

    // String View for port
    String_Builder_Slice port_sbs = {0};
    {
        port_sbs.sb = &ctx->sb;
        port_sbs.start = ctx->sb.count;
        sb_appendf(&ctx->sb, "%zu", port);
        sb_append_null(&ctx->sb);
        port_sbs.end = ctx->sb.count;
    }

    // String View for temp path for capturing ssh stderr
    String_Builder_Slice stderr_temp_path = {0};
    {
        stderr_temp_path.sb = &ctx->sb;
        stderr_temp_path.start = ctx->sb.count;
        sb_appendf(&ctx->sb, "/tmp/master-%s", connection_id.hex);
        sb_append_null(&ctx->sb);
        stderr_temp_path.end = ctx->sb.count;
    }

    // Create a new master connection
    // ssh -p {port} -o ControlPath={control_path_ssh_opt} {DEFAULT_SSH_MASTER_CONNECTION_PARAMS}
    cmd_append(&ctx->cmd, "ssh", "-p", sbs_as_sv(port_sbs).data, "-o", sbs_as_sv(control_path_ssh_opt).data, DEFAULT_SSH_MASTER_CONNECTION_PARAMS, sbs_as_sv(user_and_host).data);
    if (cmd_run(&ctx->cmd, .stderr_path=sbs_as_sv(stderr_temp_path).data)) {
        size_t start_count = ctx->sb.count;
        sb_appendf(&ctx->sb, "Connection is successful, connection_id: %s", connection_id.hex);
        size_t end_count = ctx->sb.count;
        mcp_write_text_content_sized(request_handler, ctx->sb.items + start_count, end_count - start_count);
        ctx->sb.items -= end_count - start_count;
        Connection_Detail connection_detail = {0};
        snprintf(connection_detail.host, ARRAY_LEN(connection_detail.host), SV_Fmt, SV_Arg(host));
        snprintf(connection_detail.user, ARRAY_LEN(connection_detail.user), SV_Fmt, SV_Arg(user));
        connection_detail.port = port;
        put_connection_detail(ctx->ht, connection_id, connection_detail);
        return true;
    } else {
        size_t start_count = ctx->sb.count;
        sb_appendf(&ctx->sb, "Connection failed, Reason is below:\n");
        if (!read_entire_file(sbs_as_sv(stderr_temp_path).data, &ctx->sb)) {
            sb_appendf(&ctx->sb, "Could not open the stderr file to see the reason: `%s`", sbs_as_sv(stderr_temp_path).data);
        }
        size_t end_count = ctx->sb.count;
        mcp_write_text_content_sized(request_handler, ctx->sb.items + start_count, end_count - start_count);
        return false;
    }
}

bool handle_ssh_execute(MCP_Request_Handler *request_handler, My_Context *ctx, Jimp *tool_args) {
    String_Builder_Slice connection_id_hex = {0};
    String_Builder_Slice cmd = {0};

    if (!jimp_object_begin(tool_args)) return false;
    while (jimp_object_member(tool_args)) {
        if (strcmp(tool_args->string, "connection_id") == 0) {
            if (!jimp_string(tool_args)) return false;
            connection_id_hex.sb = &ctx->sb;
            connection_id_hex.start = ctx->sb.count;
            sb_appendf(&ctx->sb, "%s", tool_args->string);
            sb_append_null(&ctx->sb);
            connection_id_hex.end = ctx->sb.count;
        } else if (strcmp(tool_args->string, "cmd") == 0) {
            if (!jimp_string(tool_args)) return false;
            cmd.sb = &ctx->sb;
            cmd.start = ctx->sb.count;
            sb_appendf(&ctx->sb, "%s", tool_args->string);
            sb_append_null(&ctx->sb);
            cmd.end = ctx->sb.count;
        } else if (!jimp_skip_member(tool_args)) return false;
    }
    if (!jimp_object_end(tool_args)) return false;

    if (!connection_id_hex.sb) {
        mcp_write_text_content(request_handler, "connection_id was not provided!");
        return false;
    }
    if (!cmd.sb) {
        mcp_write_text_content(request_handler, "cmd was not provided!");
        return false;
    }

    Connection_Id connection_id = {0};
    if (sbs_as_sv(connection_id_hex).count != ARRAY_LEN(connection_id.hex)) {
        mcp_write_text_content(request_handler, "Invalid connection_id passed!");
        return false;
    }
    memcpy(connection_id.hex, sbs_as_sv(connection_id_hex).data, ARRAY_LEN(connection_id.hex));

    Connection_Detail connection_detail = {0};
    if (!get_connection_detail(ctx->ht, connection_id, &connection_detail)) {
        mcp_write_text_content(request_handler, "Unknown connection_id passed! Use ssh_list_connections to see the list of valid connections");
        return false;
    }

    // Contruct user:host
    String_Builder_Slice user_and_host = {0};
    {
        user_and_host.sb = &ctx->sb;
        user_and_host.start = ctx->sb.count;
        sb_appendf(&ctx->sb, "%s@%s", connection_detail.user, connection_detail.host);
        sb_append_null(&ctx->sb);
        user_and_host.end = ctx->sb.count;
    }

    // Construct control socket path ssh option
    String_Builder_Slice control_path_ssh_opt = {0};
    {
        control_path_ssh_opt.sb = &ctx->sb;
        control_path_ssh_opt.start = ctx->sb.count;
        sb_appendf(&ctx->sb, "ControlPath=%s/master-%s", ctx->ssh_master_root, connection_id.hex);
        sb_append_null(&ctx->sb);
        control_path_ssh_opt.end = ctx->sb.count;
    }

    // String View for port
    String_Builder_Slice port_sbs = {0};
    {
        port_sbs.sb = &ctx->sb;
        port_sbs.start = ctx->sb.count;
        sb_appendf(&ctx->sb, "%zu", connection_detail.port);
        sb_append_null(&ctx->sb);
        port_sbs.end = ctx->sb.count;
    }

    size_t request_id = request_handler->jsonrpc_request_handler.request_parser.id;
    // String View for temp path for capturing ssh stdout
    String_View stdout_temp_path = {0};
    {
        size_t start_count = ctx->sb.count;
        sb_appendf(&ctx->sb, "/tmp/master-%s__ssh_execute__%zu_stdout", connection_id.hex, request_id);
        sb_append_null(&ctx->sb);
        size_t end_count = ctx->sb.count;
        stdout_temp_path = sv_from_parts(ctx->sb.items + start_count, end_count - start_count);
    }
    // String View for temp path for capturing ssh stderr
    String_View stderr_temp_path = {0};
    {
        size_t start_count = ctx->sb.count;
        sb_appendf(&ctx->sb, "/tmp/master-%s__ssh_execute__%zu_stderr", connection_id.hex, request_id);
        sb_append_null(&ctx->sb);
        size_t end_count = ctx->sb.count;
        stderr_temp_path = sv_from_parts(ctx->sb.items + start_count, end_count - start_count);
    }
    // ssh -o ControlPath={control_path} -o ControlMaster=no -p {port} {user}@{host} {cmd}
    cmd_append(&ctx->cmd, "ssh", "-o", "ControlMaster=no", "-o", sbs_as_sv(control_path_ssh_opt).data, "-p", sbs_as_sv(port_sbs).data, sbs_as_sv(user_and_host).data, sbs_as_sv(cmd).data);
    if (cmd_run(&ctx->cmd, .stdout_path=stdout_temp_path.data, .stderr_path=stderr_temp_path.data)) {
        size_t start_count = ctx->sb.count;
        sb_append_cstr(&ctx->sb, "Successfully executed the command. Output is below:\n");
        if (!read_entire_file(stdout_temp_path.data, &ctx->sb)) {
            sb_appendf(&ctx->sb, "Successfully executed the command, but could not open the stdout file to see the result: `%s`", stdout_temp_path.data);
        }
        size_t end_count = ctx->sb.count;
        mcp_write_text_content_sized(request_handler, ctx->sb.items + start_count, end_count - start_count);
        return true;
    } else {
        size_t start_count = ctx->sb.count;
        sb_appendf(&ctx->sb, "Connection failed, Reason is below:\n");
        if (!read_entire_file(stderr_temp_path.data, &ctx->sb)) {
            sb_appendf(&ctx->sb, "Could not open the stderr file to see the reason: `%s`", stderr_temp_path.data);
        }
        size_t end_count = ctx->sb.count;
        mcp_write_text_content_sized(request_handler, ctx->sb.items + start_count, end_count - start_count);
        return false;
    }
}

bool handle_ssh_disconnect(MCP_Request_Handler *request_handler, My_Context *ctx, Jimp *tool_args) {
    String_Builder_Slice connection_id_hex = {0};

    if (!jimp_object_begin(tool_args)) return false;
    while (jimp_object_member(tool_args)) {
        if (strcmp(tool_args->string, "connection_id") == 0) {
            if (!jimp_string(tool_args)) return false;
            connection_id_hex.sb = &ctx->sb;
            connection_id_hex.start = ctx->sb.count;
            sb_appendf(&ctx->sb, "%s", tool_args->string);
            sb_append_null(&ctx->sb);
            connection_id_hex.end = ctx->sb.count;
        } else if (!jimp_skip_member(tool_args)) return false;
    }
    if (!jimp_object_end(tool_args)) return false;

    if (!connection_id_hex.sb) {
        mcp_write_text_content(request_handler, "connection_id was not provided!");
        return false;
    }

    Connection_Id connection_id = {0};
    if (sbs_as_sv(connection_id_hex).count != ARRAY_LEN(connection_id.hex)) {
        mcp_write_text_content(request_handler, "Invalid connection_id passed!");
        return false;
    }
    memcpy(connection_id.hex, sbs_as_sv(connection_id_hex).data, ARRAY_LEN(connection_id.hex));

    Connection_Detail connection_detail = {0};
    if (!delete_connection_detail(ctx->ht, connection_id, &connection_detail)) {
        mcp_write_text_content(request_handler, "Unknown connection_id passed! Use ssh_list_connections to see the list of valid connections");
        return false;
    }

    // Contruct user:host
    String_Builder_Slice user_and_host = {0};
    {
        user_and_host.sb = &ctx->sb;
        user_and_host.start = ctx->sb.count;
        sb_appendf(&ctx->sb, "%s@%s", connection_detail.user, connection_detail.host);
        sb_append_null(&ctx->sb);
        user_and_host.end = ctx->sb.count;
    }

    // Construct control socket path ssh option
    String_Builder_Slice control_path_ssh_opt = {0};
    {
        control_path_ssh_opt.sb = &ctx->sb;
        control_path_ssh_opt.start = ctx->sb.count;
        sb_appendf(&ctx->sb, "ControlPath=%s/master-%s", ctx->ssh_master_root, connection_id.hex);
        sb_append_null(&ctx->sb);
        control_path_ssh_opt.end = ctx->sb.count;
    }

    // ssh -O exit -o ControlPath={control_path} {user}@{host}
    cmd_append(&ctx->cmd, "ssh", "-o", sbs_as_sv(control_path_ssh_opt).data, sbs_as_sv(user_and_host).data);
    String_View control_path = sbs_as_sv(control_path_ssh_opt);
    sv_chop_prefix(&control_path, sv_from_cstr("ControlPath="));
    delete_file(control_path.data);
    return true;
}

bool handle_ssh_list_connections(MCP_Request_Handler *request_handler, My_Context *ctx) {
    assert(ctx != NULL);
    sb_append_cstr(&ctx->sb, "Below are the list of connections:");
    pthread_mutex_lock(&ctx->ht->lock);
    ht_foreach(Connection_Ht_Slot, it, ctx->ht) {
        size_t start_count = ctx->sb.count;
        sb_appendf(&ctx->sb, "[%s] %s@%s:%zu", it->key.hex, it->value.user, it->value.host, it->value.port);
        size_t end_count = ctx->sb.count;
        mcp_write_text_content_sized(request_handler, ctx->sb.items + start_count, end_count - start_count);
        ctx->sb.count -= end_count - start_count;
    }
    pthread_mutex_unlock(&ctx->ht->lock);
    return true;
}

bool tools_call_clb(MCP_Request_Handler *request_handler, String_View tool_name, Jimp *tool_args) {
    My_Context *ctx = request_handler->ctx;
    assert(ctx != NULL);

    assert(ctx->ht != NULL);

    ctx->sb.count = 0; // Cleanup String_Builder
    ctx->cmd.count = 0; // Cleanup CMD

    // Responding with thread id
    {
        pthread_t tid = pthread_self();
        size_t start_count = ctx->sb.count;
        sb_appendf(&ctx->sb, "Executing on thread: %lu", (unsigned long) tid);
        size_t end_count = ctx->sb.count;
        mcp_write_text_content_sized(request_handler, ctx->sb.items + start_count, end_count - start_count);
        ctx->sb.count -= end_count - start_count;
    }

    if (sv_eq(tool_name, sv_from_cstr(TOOL_SSH_CONNECT))) {
        return handle_ssh_connect(request_handler, ctx, tool_args);
    } else if (sv_eq(tool_name, sv_from_cstr(TOOL_SSH_EXECUTE))) {
        return handle_ssh_execute(request_handler, ctx, tool_args);
    } else if (sv_eq(tool_name, sv_from_cstr(TOOL_SSH_DISCONNECT))) {
        return handle_ssh_disconnect(request_handler, ctx, tool_args);
    } else if (sv_eq(tool_name, sv_from_cstr(TOOL_SSH_LIST_CONNECTIONS))) {
        return handle_ssh_list_connections(request_handler, ctx);
    }

    {
        size_t start_count = ctx->sb.count;
        sb_appendf(&ctx->sb, "Unknown/Unhandled tool_name: |"SV_Fmt"|", SV_Arg(tool_name));
        size_t end_count = ctx->sb.count;
        mcp_write_text_content_sized(request_handler, ctx->sb.items + start_count, end_count - start_count);
    }
    return false;
}

void *thread_mcp(void *arg) {
    MCP_Args *mcp_args = arg;
    My_Context ctx = {0};
    ctx.ssh_master_root = mcp_args->ssh_master_root;
    ctx.ht = mcp_args->ht;
    MCP_Request_Handler request_handler = create_mcp_request_handler(
        "ssh", "0.0.1",
        tools_list_clb,
        tools_call_clb,
        &ctx,
        mcp_args->instructions);
    Jim success = {0};
    Jim failure = {0};
    while (true) {
        Line line = {0};
        bool ok_line = false;
        channel_recv(mcp_args->in, &line, &ok_line);
        nob_log(INFO, "thread_mcp: Got line: |"SV_Fmt"|", SV_Arg(line.sv));
        if (!ok_line) break;
        if (mcp_handle_request(
            &request_handler,
            line.label,
            line.sv.data, line.sv.count,
            &success, &failure)) {
            if (success.sink_count > 0) {
                channel_send(mcp_args->out, success);
                nob_log(INFO, "thread_mcp: Sent line: |"SV_Fmt"|", (int) success.sink_count, success.sink);
                memset(&success, 0, sizeof(Jim));
            }
        } else {
            if (failure.sink_count > 0) {
                channel_send(mcp_args->out, failure);
                nob_log(INFO, "thread_mcp: Sent line: |"SV_Fmt"|", (int) failure.sink_count, failure.sink);
                memset(&failure, 0, sizeof(Jim));
            }
        }
        free((char *) line.sv.data); // Freeing the line here as we are done with the response
    }

    free_mcp_request_handler(&request_handler);
    free(ctx.sb.items);
    free(ctx.cmd.items);

    free(success.sink);
    free(success.scopes);

    free(failure.sink);
    free(failure.scopes);
    return NULL;
}

void *thread_write_to_stdout(void *arg) {
    Jim_Chan *in = arg;
    while (true) {
        Jim jim = {0};
        bool ok_jim = false;
        channel_recv(in, &jim, &ok_jim);
        if (!ok_jim) break;
        nob_log(INFO, "thread_write_to_stdout: Got line: |"SV_Fmt"|", (int) jim.sink_count, jim.sink);
        fprintf(stdout, SV_Fmt"\n", (int) jim.sink_count, jim.sink);
        fflush(stdout);
        free(jim.sink);
        free(jim.scopes);
    }
    return NULL;
}

int main(int argc, char **argv) {
    // Zenity setup
    {
        if (argc > 1 && strstr(argv[1], "password") != NULL) {
            // We act as the askpass tool and launch zenity via exec
            execlp("zenity", "zenity", "--entry", "--text", argv[1], "--hide-text", NULL);
            return 0;
        }
        char self_path[1024];
        int len = readlink("/proc/self/exe", self_path, sizeof(self_path) - 1);
        if (len == -1) {
            nob_log(ERROR, "Could not readlink self process: %s", strerror(errno));
        } else {
            self_path[len] = '\0';
            setenv("SSH_ASKPASS", self_path, 1);
            setenv("SSH_ASKPASS_REQUIRE", "force", 1);
        }
    }

    char ssh_master_root[1024];
    const char *home = getenv("HOME");
    assert(home != NULL);
    snprintf(ssh_master_root, sizeof(ssh_master_root), "%s/.ssh", home);
    if (!mkdir_if_not_exists(ssh_master_root)) return 1;

    Line_Chan line_chan = {0};
    Jim_Chan mcp_response_chan = {0};
    Buffered_Reader br = create_br(STDIN_FILENO);
    String_Builder sb = {0};
    Connection_Ht ht = {0};
    pthread_mutex_init(&ht.lock, NULL);

    pthread_t mcp_threads[10];
    MCP_Args mcp_args = {
        .in = &line_chan,
        .out = &mcp_response_chan,
        .ssh_master_root = ssh_master_root,
        .ht = &ht,
    };
    for (size_t i = 0; i < ARRAY_LEN(mcp_threads); i++) {
        pthread_create(&mcp_threads[i], NULL, thread_mcp, &mcp_args);
    }

    pthread_t writer;
    pthread_create(&writer, NULL, thread_write_to_stdout, &mcp_response_chan);

    while (true) {
        if (!br_read_line_to_sb(&br, &sb)) break;
        if (sb.count == 0) continue;
        String_View sv = sb_to_sv(sb);
        nob_log(INFO, "Read line: |"SV_Fmt"|", SV_Arg(sv));
        if (sv_eq(sv, sv_from_cstr("quit"))) break;
        memset(&sb, 0, sizeof(String_Builder)); // Loosing ownership of the memory as sending to channel
        Line line = {.label = "in", .sv = sv};
        channel_send(&line_chan, line);
    }
    channel_close(&line_chan);

    for (size_t i = 0; i < ARRAY_LEN(mcp_threads); i++) {
        pthread_join(mcp_threads[i], NULL);
    }
    channel_close(&mcp_response_chan);

    pthread_join(writer, NULL);

    free(br.items);
    free(sb.items);
    free(ht.items);
    return 0;
}
