# SPDX-FileCopyrightText: 2024 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""AI backend for Blender assistant - communicates with Claude API."""

import bpy
import json
import threading
from urllib import request, error
from queue import Queue

# Response queue for thread-safe communication
_response_queue = Queue()

# Tool definitions for Claude
TOOLS = [
    {
        "name": "execute_python",
        "description": "Execute Python code in Blender. Use this to create objects, modify the scene, animate, etc. The code runs in Blender's Python environment with 'bpy' available.",
        "input_schema": {
            "type": "object",
            "properties": {
                "code": {
                    "type": "string",
                    "description": "Python code to execute. Must be valid Blender Python."
                },
                "description": {
                    "type": "string",
                    "description": "Brief description of what this code does"
                }
            },
            "required": ["code"]
        }
    },
    {
        "name": "get_scene_info",
        "description": "Get information about the current Blender scene including objects, materials, and settings.",
        "input_schema": {
            "type": "object",
            "properties": {},
            "required": []
        }
    },
    {
        "name": "get_selected_objects",
        "description": "Get detailed information about currently selected objects.",
        "input_schema": {
            "type": "object",
            "properties": {},
            "required": []
        }
    }
]


def get_scene_info():
    """Get current scene information."""
    scene = bpy.context.scene
    objects = []
    for obj in scene.objects:
        objects.append({
            "name": obj.name,
            "type": obj.type,
            "location": [round(v, 3) for v in obj.location],
            "selected": obj.select_get()
        })

    return {
        "scene_name": scene.name,
        "object_count": len(scene.objects),
        "objects": objects[:20],  # Limit to 20
        "frame_current": scene.frame_current,
        "active_object": bpy.context.active_object.name if bpy.context.active_object else None
    }


def get_selected_objects():
    """Get info about selected objects."""
    selected = []
    for obj in bpy.context.selected_objects:
        info = {
            "name": obj.name,
            "type": obj.type,
            "location": [round(v, 3) for v in obj.location],
            "rotation": [round(v, 3) for v in obj.rotation_euler],
            "scale": [round(v, 3) for v in obj.scale],
        }
        if obj.type == 'MESH' and obj.data:
            info["vertices"] = len(obj.data.vertices)
            info["faces"] = len(obj.data.polygons)
        selected.append(info)

    return {"selected_objects": selected, "count": len(selected)}


def execute_python(code):
    """Execute Python code in Blender."""
    exec_globals = {"bpy": bpy, "__builtins__": __builtins__}
    exec_locals = {}

    try:
        exec(code, exec_globals, exec_locals)
        return {"success": True, "result": exec_locals.get("result")}
    except Exception as e:
        return {"success": False, "error": str(e)}


def handle_tool_call(tool_name, tool_input):
    """Execute a tool and return the result."""
    if tool_name == "execute_python":
        return execute_python(tool_input.get("code", ""))
    elif tool_name == "get_scene_info":
        return get_scene_info()
    elif tool_name == "get_selected_objects":
        return get_selected_objects()
    else:
        return {"error": f"Unknown tool: {tool_name}"}


def call_claude_api(api_key, model, messages, system_prompt):
    """Call Claude API with tools."""
    url = "https://api.anthropic.com/v1/messages"

    payload = {
        "model": model,
        "max_tokens": 4096,
        "system": system_prompt,
        "tools": TOOLS,
        "messages": messages
    }

    data = json.dumps(payload).encode('utf-8')

    req = request.Request(url, data=data, method='POST')
    req.add_header("Content-Type", "application/json")
    req.add_header("x-api-key", api_key)
    req.add_header("anthropic-version", "2023-06-01")

    try:
        with request.urlopen(req, timeout=120) as response:
            return json.loads(response.read().decode('utf-8'))
    except error.HTTPError as e:
        error_body = e.read().decode('utf-8')
        return {"error": f"API error {e.code}: {error_body}"}
    except Exception as e:
        return {"error": str(e)}


def process_ai_response(api_key, model, messages, system_prompt, scene_name):
    """Process AI response, handling tool calls in a loop."""
    current_messages = messages.copy()

    while True:
        response = call_claude_api(api_key, model, current_messages, system_prompt)

        if "error" in response:
            return {"role": "assistant", "content": f"Error: {response['error']}"}

        # Check stop reason
        stop_reason = response.get("stop_reason")
        content_blocks = response.get("content", [])

        # Collect text and tool uses
        text_parts = []
        tool_uses = []

        for block in content_blocks:
            if block.get("type") == "text":
                text_parts.append(block.get("text", ""))
            elif block.get("type") == "tool_use":
                tool_uses.append(block)

        # If there are tool calls, execute them
        if tool_uses:
            # Add assistant message with tool uses
            current_messages.append({"role": "assistant", "content": content_blocks})

            # Execute tools and collect results
            tool_results = []
            for tool_use in tool_uses:
                tool_name = tool_use.get("name")
                tool_input = tool_use.get("input", {})
                tool_id = tool_use.get("id")

                # Queue tool execution for main thread
                result = None

                def execute_tool():
                    nonlocal result
                    result = handle_tool_call(tool_name, tool_input)

                # Execute in main thread via timer
                bpy.app.timers.register(execute_tool, first_interval=0)

                # Wait a bit for execution
                import time
                time.sleep(0.1)

                tool_results.append({
                    "type": "tool_result",
                    "tool_use_id": tool_id,
                    "content": json.dumps(result) if result else '{"error": "execution pending"}'
                })

            # Add tool results
            current_messages.append({"role": "user", "content": tool_results})

            # Continue loop to get final response
            continue

        # No more tool calls, return the text response
        final_text = "\n".join(text_parts) if text_parts else "Done."
        return {"role": "assistant", "content": final_text}


def send_message_async(api_key, model, messages, system_prompt, scene_name, callback):
    """Send message to AI in background thread."""

    def worker():
        result = process_ai_response(api_key, model, messages, system_prompt, scene_name)
        _response_queue.put((callback, result))

    thread = threading.Thread(target=worker, daemon=True)
    thread.start()


def check_responses():
    """Check for AI responses (called from timer)."""
    try:
        while not _response_queue.empty():
            callback, result = _response_queue.get_nowait()
            callback(result)
    except Exception:
        pass
    return 0.1  # Check every 100ms


def register():
    bpy.app.timers.register(check_responses, persistent=True)


def unregister():
    try:
        bpy.app.timers.unregister(check_responses)
    except Exception:
        pass
