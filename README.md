# ssh-mcp
A simple MCP Server Implementation for managing SSH connections in C. This is a simple `stdin` and `stdout` MCP server and is meant to be used by running locally on a desktop OS.

**NOTE: This is WIP...**
**NOTE: Also note that currently it supports POSIX only**

## Dependencies:
- [libssl-dev](https://packages.debian.org/sid/libssl-dev)
- [Zenity](https://en.wikipedia.org/wiki/Zenity)

## Build:
```console
cc -o nob nob.c && ./nob
```

## Usage:
```console
./build/ssh-mcp
```

## How it works?

1. `ssh_connect` will create master connection using ControlMaster. It will create that socket `~/.ssh/`.
2. `ssh_execute` will create slave connection to a previously created connection_id (`ssh_connect`) and execute the commands.
3. `ssh_list_connections` will list the available connection that was created in the current session.
4. `ssh_disconnect` will disconnect from an existing connection_id. It will also delete the corresponding socker file.

**Note that the MCP server maintains a hashtable of connections in the current session, and it is not shared with different sessions. You will need to manually delete any stale socket connection file from `~/.ssh`.**

## Current Tools List:

| Tool Name | Description |
| :--- | :--- |
| `ssh_connect` | Connect to a server via SSH. |
| `ssh_execute` | Execute a command on the SSH server, given its connection ID. |
| `ssh_list_connections` | List the available control master connections. |
| `ssh_disconnect` | Disconnect from an existing SSH connection. |

## Copyrights

Licensed under [@MIT](./LICENSE)
