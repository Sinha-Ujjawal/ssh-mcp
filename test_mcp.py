import asyncio

from mcp import ClientSession, StdioServerParameters
from mcp.client.stdio import stdio_client


async def test_mcp_server():
    # Define how to start your server
    server_params = StdioServerParameters(command="./build/ssh-mcp")

    async with stdio_client(server_params) as (read, write):
        async with ClientSession(read, write) as session:
            # 1. Handshake
            await session.initialize()

            # 2. List tools to verify discovery
            tools = await session.list_tools()
            print(f"Discovered tools: {[t.name for t in tools.tools]}")

            # 3. Test ssh_connect
            await session.call_tool(
                "ssh_connect",
                arguments={"host": "localhost", "port": 2222, "user": "ujsinha"},
            )

            # 4. Test ssh_list_connections
            await session.call_tool("ssh_list_connections")


if __name__ == "__main__":
    asyncio.run(test_mcp_server())
