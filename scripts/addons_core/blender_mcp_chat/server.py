# SPDX-FileCopyrightText: 2024 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
import json
import socket
import threading
import traceback
from queue import Queue, Empty

# Global server instance
_server_instance = None


class MCPServer:
    """Socket server for MCP protocol communication."""

    def __init__(self, host="127.0.0.1", port=9876):
        self.host = host
        self.port = port
        self.socket = None
        self.running = False
        self.clients = []
        self.command_queue = Queue()
        self.response_queues = {}
        self._server_thread = None
        self._timer_registered = False
        self._command_handlers = {}
        self._lock = threading.Lock()

    def register_handler(self, command_type, handler):
        """Register a command handler function."""
        self._command_handlers[command_type] = handler

    def start(self):
        """Start the MCP server."""
        if self.running:
            return False

        try:
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            self.socket.bind((self.host, self.port))
            self.socket.listen(5)
            self.socket.settimeout(1.0)  # Allow periodic checking for shutdown
            self.running = True

            # Start server thread
            self._server_thread = threading.Thread(target=self._server_loop, daemon=True)
            self._server_thread.start()

            # Register timer for processing commands in main thread
            if not self._timer_registered:
                bpy.app.timers.register(self._process_commands, persistent=True)
                self._timer_registered = True

            print(f"MCP Server started on {self.host}:{self.port}")
            return True

        except Exception as e:
            print(f"Failed to start MCP server: {e}")
            traceback.print_exc()
            self.running = False
            return False

    def stop(self):
        """Stop the MCP server."""
        self.running = False

        # Close all client connections
        with self._lock:
            for client in self.clients:
                try:
                    client.close()
                except Exception:
                    pass
            self.clients.clear()

        # Close server socket
        if self.socket:
            try:
                self.socket.close()
            except Exception:
                pass
            self.socket = None

        # Unregister timer
        if self._timer_registered:
            try:
                bpy.app.timers.unregister(self._process_commands)
            except Exception:
                pass
            self._timer_registered = False

        print("MCP Server stopped")

    def _server_loop(self):
        """Main server loop running in a separate thread."""
        while self.running:
            try:
                client_socket, address = self.socket.accept()
                print(f"Client connected from {address}")
                with self._lock:
                    self.clients.append(client_socket)

                # Handle client in a new thread
                client_thread = threading.Thread(
                    target=self._handle_client,
                    args=(client_socket, address),
                    daemon=True
                )
                client_thread.start()

            except socket.timeout:
                continue
            except Exception as e:
                if self.running:
                    print(f"Server error: {e}")
                break

    def _handle_client(self, client_socket, address):
        """Handle a single client connection."""
        buffer = ""
        client_id = id(client_socket)
        self.response_queues[client_id] = Queue()

        try:
            client_socket.settimeout(None)  # Blocking mode
            while self.running:
                try:
                    data = client_socket.recv(4096)
                    if not data:
                        break

                    buffer += data.decode('utf-8')

                    # Process complete JSON messages
                    while buffer:
                        try:
                            # Try to parse as JSON
                            command = json.loads(buffer)
                            buffer = ""

                            # Queue command for main thread execution
                            self.command_queue.put((client_id, command))

                            # Wait for response from main thread
                            try:
                                response = self.response_queues[client_id].get(timeout=60.0)
                                response_json = json.dumps(response) + "\n"
                                client_socket.sendall(response_json.encode('utf-8'))
                            except Empty:
                                error_response = {
                                    "status": "error",
                                    "message": "Command timeout"
                                }
                                client_socket.sendall(
                                    (json.dumps(error_response) + "\n").encode('utf-8')
                                )

                        except json.JSONDecodeError:
                            # Incomplete JSON, wait for more data
                            break

                except socket.timeout:
                    continue
                except Exception as e:
                    print(f"Client handler error: {e}")
                    break

        except Exception as e:
            print(f"Client {address} error: {e}")

        finally:
            print(f"Client disconnected: {address}")
            with self._lock:
                if client_socket in self.clients:
                    self.clients.remove(client_socket)
            if client_id in self.response_queues:
                del self.response_queues[client_id]
            try:
                client_socket.close()
            except Exception:
                pass

    def _process_commands(self):
        """Process queued commands in the main Blender thread."""
        if not self.running:
            return None  # Unregister timer

        # Process up to 10 commands per timer call
        for _ in range(10):
            try:
                client_id, command = self.command_queue.get_nowait()
            except Empty:
                break

            try:
                response = self._execute_command(command)
            except Exception as e:
                response = {
                    "status": "error",
                    "message": str(e),
                    "traceback": traceback.format_exc()
                }

            # Send response back to client handler
            if client_id in self.response_queues:
                self.response_queues[client_id].put(response)

        return 0.01  # Run again in 10ms

    def _execute_command(self, command):
        """Execute a command and return the response."""
        if not isinstance(command, dict):
            return {"status": "error", "message": "Invalid command format"}

        cmd_type = command.get("type", "")
        params = command.get("params", {})

        # Update last command in scene
        try:
            scene = bpy.context.scene
            if hasattr(scene, 'mcp_chat'):
                scene.mcp_chat.last_command = cmd_type
        except Exception:
            pass

        # Find and execute handler
        if cmd_type in self._command_handlers:
            try:
                result = self._command_handlers[cmd_type](params)
                return {"status": "success", "result": result}
            except Exception as e:
                return {
                    "status": "error",
                    "message": str(e),
                    "traceback": traceback.format_exc()
                }
        else:
            return {"status": "error", "message": f"Unknown command: {cmd_type}"}

    def get_client_count(self):
        """Get the number of connected clients."""
        with self._lock:
            return len(self.clients)


def get_server():
    """Get the global server instance."""
    global _server_instance
    return _server_instance


def create_server(host="127.0.0.1", port=9876):
    """Create a new server instance."""
    global _server_instance
    if _server_instance is not None:
        _server_instance.stop()
    _server_instance = MCPServer(host, port)
    return _server_instance


def register():
    """Register the server module."""
    pass  # Server is started via operator


def unregister():
    """Unregister the server module."""
    global _server_instance
    if _server_instance is not None:
        _server_instance.stop()
        _server_instance = None
