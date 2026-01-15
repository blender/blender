# SPDX-FileCopyrightText: 2024 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""Operators for MCP Chat addon."""

import bpy
from bpy.types import Operator
from bpy.props import StringProperty, IntProperty, FloatProperty, BoolProperty

from . import server, handlers, integrations


class MCP_OT_start_server(Operator):
    """Start the MCP server to allow AI connections"""
    bl_idname = "mcp.start_server"
    bl_label = "Start MCP Server"
    bl_description = "Start the MCP server to allow AI assistant connections"

    def execute(self, context):
        scene = context.scene
        settings = scene.mcp_chat

        # Create and configure server
        mcp_server = server.create_server(settings.server_host, settings.server_port)

        # Register all handlers
        handlers.register_handlers(mcp_server)
        integrations.register_handlers(mcp_server)

        # Start server
        if mcp_server.start():
            settings.server_running = True
            self.report({'INFO'}, f"MCP Server started on {settings.server_host}:{settings.server_port}")
            return {'FINISHED'}
        else:
            self.report({'ERROR'}, "Failed to start MCP server")
            return {'CANCELLED'}


class MCP_OT_stop_server(Operator):
    """Stop the MCP server"""
    bl_idname = "mcp.stop_server"
    bl_label = "Stop MCP Server"
    bl_description = "Stop the MCP server and disconnect all clients"

    def execute(self, context):
        scene = context.scene
        settings = scene.mcp_chat

        mcp_server = server.get_server()
        if mcp_server:
            mcp_server.stop()

        settings.server_running = False
        settings.connected_clients = 0
        self.report({'INFO'}, "MCP Server stopped")
        return {'FINISHED'}


class MCP_OT_refresh_status(Operator):
    """Refresh the server status"""
    bl_idname = "mcp.refresh_status"
    bl_label = "Refresh Status"
    bl_description = "Update the server status and client count"

    def execute(self, context):
        scene = context.scene
        settings = scene.mcp_chat

        mcp_server = server.get_server()
        if mcp_server and mcp_server.running:
            settings.connected_clients = mcp_server.get_client_count()
            settings.server_running = True
        else:
            settings.connected_clients = 0
            settings.server_running = False

        return {'FINISHED'}


class MCP_OT_clear_chat(Operator):
    """Clear the chat history"""
    bl_idname = "mcp.clear_chat"
    bl_label = "Clear Chat"
    bl_description = "Clear all chat messages"

    def execute(self, context):
        scene = context.scene
        scene.mcp_chat_messages.clear()
        scene.mcp_chat.chat_message_index = 0
        return {'FINISHED'}


class MCP_OT_send_message(Operator):
    """Send a message to the AI assistant"""
    bl_idname = "mcp.send_message"
    bl_label = "Send Message"
    bl_description = "Send the current message to the AI assistant"

    def execute(self, context):
        import time
        scene = context.scene
        settings = scene.mcp_chat

        message = settings.chat_input.strip()
        if not message:
            return {'CANCELLED'}

        # Add user message to chat
        msg = scene.mcp_chat_messages.add()
        msg.role = "user"
        msg.content = message
        msg.timestamp = time.time()

        # Clear input
        settings.chat_input = ""

        # Update index to show latest message
        settings.chat_message_index = len(scene.mcp_chat_messages) - 1

        # The actual AI response comes via MCP protocol from connected client
        return {'FINISHED'}


class MCP_OT_add_message(Operator):
    """Add a message to the chat (for internal use)"""
    bl_idname = "mcp.add_message"
    bl_label = "Add Message"
    bl_options = {'INTERNAL'}

    role: StringProperty(name="Role", default="user")
    content: StringProperty(name="Content", default="")

    def execute(self, context):
        import time
        scene = context.scene
        msg = scene.mcp_chat_messages.add()
        msg.role = self.role
        msg.content = self.content
        msg.timestamp = time.time()
        return {'FINISHED'}


class MCP_OT_test_connection(Operator):
    """Test the MCP server connection"""
    bl_idname = "mcp.test_connection"
    bl_label = "Test Connection"
    bl_description = "Send a test command to verify the server is working"

    def execute(self, context):
        import socket
        import json

        scene = context.scene
        settings = scene.mcp_chat

        if not settings.server_running:
            self.report({'ERROR'}, "Server is not running")
            return {'CANCELLED'}

        try:
            # Create a test client connection
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(5.0)
            sock.connect((settings.server_host, settings.server_port))

            # Send test command
            test_command = {"type": "get_scene_info", "params": {}}
            sock.sendall(json.dumps(test_command).encode('utf-8'))

            # Receive response
            response = sock.recv(4096).decode('utf-8')
            result = json.loads(response)

            sock.close()

            if result.get("status") == "success":
                self.report({'INFO'}, "Connection test successful!")
                return {'FINISHED'}
            else:
                self.report({'WARNING'}, f"Server responded with error: {result.get('message', 'Unknown error')}")
                return {'CANCELLED'}

        except Exception as e:
            self.report({'ERROR'}, f"Connection test failed: {str(e)}")
            return {'CANCELLED'}


class MCP_OT_execute_code(Operator):
    """Execute Python code via the MCP server"""
    bl_idname = "mcp.execute_code"
    bl_label = "Execute Code"
    bl_description = "Execute Python code in Blender"
    bl_options = {'REGISTER', 'UNDO'}

    code: StringProperty(
        name="Code",
        description="Python code to execute",
        default=""
    )

    def execute(self, context):
        if not self.code:
            self.report({'ERROR'}, "No code provided")
            return {'CANCELLED'}

        try:
            result = handlers.execute_code({"code": self.code})

            if result.get("executed"):
                self.report({'INFO'}, "Code executed successfully")
                return {'FINISHED'}
            else:
                self.report({'ERROR'}, f"Execution failed: {result.get('error', 'Unknown error')}")
                return {'CANCELLED'}

        except Exception as e:
            self.report({'ERROR'}, f"Execution error: {str(e)}")
            return {'CANCELLED'}


class MCP_OT_get_scene_info(Operator):
    """Get information about the current scene"""
    bl_idname = "mcp.get_scene_info"
    bl_label = "Get Scene Info"
    bl_description = "Retrieve detailed information about the current Blender scene"

    def execute(self, context):
        try:
            result = handlers.get_scene_info({})
            context.scene.mcp_chat.last_result = str(result)
            self.report({'INFO'}, f"Scene: {result.get('scene_name', 'Unknown')}, Objects: {result.get('object_count', 0)}")
            return {'FINISHED'}
        except Exception as e:
            self.report({'ERROR'}, f"Failed to get scene info: {str(e)}")
            return {'CANCELLED'}


class MCP_OT_take_screenshot(Operator):
    """Take a screenshot of the viewport"""
    bl_idname = "mcp.take_screenshot"
    bl_label = "Take Screenshot"
    bl_description = "Capture a screenshot of the 3D viewport"

    max_size: IntProperty(
        name="Max Size",
        description="Maximum dimension in pixels",
        default=800,
        min=100,
        max=4096
    )

    def execute(self, context):
        try:
            result = handlers.get_viewport_screenshot({"max_size": self.max_size})

            if "error" in result:
                self.report({'ERROR'}, result["error"])
                return {'CANCELLED'}

            self.report({'INFO'}, "Screenshot captured successfully")
            return {'FINISHED'}

        except Exception as e:
            self.report({'ERROR'}, f"Screenshot failed: {str(e)}")
            return {'CANCELLED'}


class MCP_OT_search_polyhaven(Operator):
    """Search PolyHaven assets"""
    bl_idname = "mcp.search_polyhaven"
    bl_label = "Search PolyHaven"
    bl_description = "Search for assets on PolyHaven"

    asset_type: StringProperty(
        name="Asset Type",
        description="Type of asset to search for",
        default="all"
    )
    categories: StringProperty(
        name="Categories",
        description="Filter by category",
        default=""
    )

    def execute(self, context):
        if not context.scene.mcp_chat.polyhaven.enabled:
            self.report({'ERROR'}, "PolyHaven integration is not enabled")
            return {'CANCELLED'}

        try:
            params = {"asset_type": self.asset_type}
            if self.categories:
                params["categories"] = self.categories

            result = integrations.search_polyhaven_assets(params)

            if "error" in result:
                self.report({'ERROR'}, result["error"])
                return {'CANCELLED'}

            assets = result.get("assets", [])
            self.report({'INFO'}, f"Found {len(assets)} assets (total: {result.get('total', 0)})")
            return {'FINISHED'}

        except Exception as e:
            self.report({'ERROR'}, f"Search failed: {str(e)}")
            return {'CANCELLED'}


class MCP_OT_search_sketchfab(Operator):
    """Search Sketchfab models"""
    bl_idname = "mcp.search_sketchfab"
    bl_label = "Search Sketchfab"
    bl_description = "Search for models on Sketchfab"

    query: StringProperty(
        name="Query",
        description="Search query",
        default=""
    )
    count: IntProperty(
        name="Count",
        description="Number of results to return",
        default=20,
        min=1,
        max=24
    )

    def execute(self, context):
        if not context.scene.mcp_chat.sketchfab.enabled:
            self.report({'ERROR'}, "Sketchfab integration is not enabled")
            return {'CANCELLED'}

        if not self.query:
            self.report({'ERROR'}, "Search query is required")
            return {'CANCELLED'}

        try:
            result = integrations.search_sketchfab_models({
                "query": self.query,
                "count": self.count
            })

            if "error" in result:
                self.report({'ERROR'}, result["error"])
                return {'CANCELLED'}

            results = result.get("results", [])
            self.report({'INFO'}, f"Found {len(results)} models (total: {result.get('total', 0)})")
            return {'FINISHED'}

        except Exception as e:
            self.report({'ERROR'}, f"Search failed: {str(e)}")
            return {'CANCELLED'}


class MCP_OT_copy_connection_info(Operator):
    """Copy MCP connection information to clipboard"""
    bl_idname = "mcp.copy_connection_info"
    bl_label = "Copy Connection Info"
    bl_description = "Copy the MCP server connection details to clipboard"

    def execute(self, context):
        settings = context.scene.mcp_chat
        connection_info = f"Host: {settings.server_host}\nPort: {settings.server_port}"

        context.window_manager.clipboard = connection_info
        self.report({'INFO'}, "Connection info copied to clipboard")
        return {'FINISHED'}


classes = (
    MCP_OT_start_server,
    MCP_OT_stop_server,
    MCP_OT_refresh_status,
    MCP_OT_clear_chat,
    MCP_OT_send_message,
    MCP_OT_add_message,
    MCP_OT_test_connection,
    MCP_OT_execute_code,
    MCP_OT_get_scene_info,
    MCP_OT_take_screenshot,
    MCP_OT_search_polyhaven,
    MCP_OT_search_sketchfab,
    MCP_OT_copy_connection_info,
)


def register():
    for cls in classes:
        bpy.utils.register_class(cls)


def unregister():
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)
