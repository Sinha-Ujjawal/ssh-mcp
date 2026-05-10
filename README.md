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

## Current Tools List:

| Tool Name | Description |
| :--- | :--- |
| `ssh-mcp_ssh_connect` | Connect to a server via SSH. |
| `ssh-mcp_ssh_execute` | Execute a command on the SSH server, given its connection ID. |
| `ssh-mcp_ssh_list_connections` | List the available control master connections. |

## Copyrights

Licensed under [@MIT](./LICENSE)
