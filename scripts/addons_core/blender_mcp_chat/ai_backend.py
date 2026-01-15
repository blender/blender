# SPDX-FileCopyrightText: 2024 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""AI backend - Claude API integration with Blender tools."""

import bpy
import json
import threading
from urllib import request, error
from queue import Queue, Empty

_response_queue = Queue()

SYSTEM_PROMPT = """You are an AI assistant built into Blender. You help users create 3D content by executing Python code.

When users ask you to create or modify things in Blender, use the execute_python tool to run bpy commands.

Key Blender Python patterns:
- Create cube: bpy.ops.mesh.primitive_cube_add(location=(0,0,0))
- Create sphere: bpy.ops.mesh.primitive_uv_sphere_add(radius=1, location=(0,0,0))
- Create light: bpy.ops.object.light_add(type='SUN', location=(0,0,5))
- Create camera: bpy.ops.object.camera_add(location=(0,-5,2))
- Select object: bpy.data.objects['name'].select_set(True)
- Delete selected: bpy.ops.object.delete()
- Set material color: Create material with nodes, set Base Color
- Move object: bpy.context.active_object.location = (x, y, z)
- Rotate object: bpy.context.active_object.rotation_euler = (x, y, z)
- Scale object: bpy.context.active_object.scale = (x, y, z)

Always be concise in responses. Execute code to accomplish tasks rather than just explaining."""

TOOLS = [
    {
        "name": "execute_python",
        "description": "Execute Python code in Blender to create/modify 3D content. The code runs with 'bpy' already imported.",
        "input_schema": {
            "type": "object",
            "properties": {
                "code": {
                    "type": "string",
                    "description": "Python code to execute"
                }
            },
            "required": ["code"]
        }
    },
    {
        "name": "get_scene_info",
        "description": "Get list of objects in the current scene",
        "input_schema": {
            "type": "object",
            "properties": {},
            "required": []
        }
    },
    {
        "name": "get_selected",
        "description": "Get info about selected objects",
        "input_schema": {
            "type": "object",
            "properties": {},
            "required": []
        }
    }
]


def get_scene_info():
    """Get scene objects."""
    scene = bpy.context.scene
    objects = []
    for obj in scene.objects[:30]:
        objects.append(f"{obj.name} ({obj.type})")
    return {"objects": objects, "count": len(scene.objects)}


def get_selected():
    """Get selected objects."""
    selected = []
    for obj in bpy.context.selected_objects:
        selected.append({
            "name": obj.name,
            "type": obj.type,
            "location": [round(v, 2) for v in obj.location]
        })
    return {"selected": selected}


def execute_python(code):
    """Execute Python in Blender."""
    try:
        exec(code, {"bpy": bpy, "__builtins__": __builtins__})
        return {"success": True}
    except Exception as e:
        return {"error": str(e)}


def run_tool(name, params):
    """Run a tool by name."""
    if name == "execute_python":
        return execute_python(params.get("code", ""))
    elif name == "get_scene_info":
        return get_scene_info()
    elif name == "get_selected":
        return get_selected()
    return {"error": f"Unknown tool: {name}"}


def call_api(api_key, model, messages):
    """Call Claude API."""
    url = "https://api.anthropic.com/v1/messages"

    payload = {
        "model": model,
        "max_tokens": 4096,
        "system": SYSTEM_PROMPT,
        "tools": TOOLS,
        "messages": messages
    }

    data = json.dumps(payload).encode('utf-8')
    req = request.Request(url, data=data, method='POST')
    req.add_header("Content-Type", "application/json")
    req.add_header("x-api-key", api_key)
    req.add_header("anthropic-version", "2023-06-01")

    try:
        with request.urlopen(req, timeout=120) as resp:
            return json.loads(resp.read().decode('utf-8'))
    except error.HTTPError as e:
        body = e.read().decode('utf-8')
        return {"error": f"API error: {body}"}
    except Exception as e:
        return {"error": str(e)}


def process_response(api_key, model, messages, tool_executor):
    """Process API response, handling tool calls."""
    msgs = messages.copy()

    while True:
        resp = call_api(api_key, model, msgs)

        if "error" in resp:
            return resp["error"]

        content = resp.get("content", [])
        stop = resp.get("stop_reason")

        # Extract text and tool uses
        text_parts = []
        tool_uses = []

        for block in content:
            if block.get("type") == "text":
                text_parts.append(block.get("text", ""))
            elif block.get("type") == "tool_use":
                tool_uses.append(block)

        # If tools were called, execute them
        if tool_uses:
            msgs.append({"role": "assistant", "content": content})

            tool_results = []
            for tool in tool_uses:
                # Execute tool in main thread
                result = tool_executor(tool["name"], tool.get("input", {}))
                tool_results.append({
                    "type": "tool_result",
                    "tool_use_id": tool["id"],
                    "content": json.dumps(result)
                })

            msgs.append({"role": "user", "content": tool_results})
            continue

        # Return final text
        return "\n".join(text_parts) if text_parts else "Done."


def send_message(api_key, model, messages, callback):
    """Send message async."""

    # Tool results need to be collected from main thread
    tool_results = {}
    tool_event = threading.Event()

    def tool_executor(name, params):
        """Queue tool for main thread execution."""
        tool_id = id(params)
        tool_results[tool_id] = None

        def run_in_main():
            tool_results[tool_id] = run_tool(name, params)
            return None

        bpy.app.timers.register(run_in_main, first_interval=0)

        # Wait for result
        import time
        for _ in range(100):  # 10 second timeout
            time.sleep(0.1)
            if tool_results[tool_id] is not None:
                return tool_results[tool_id]

        return {"error": "Tool timeout"}

    def worker():
        result = process_response(api_key, model, messages, tool_executor)
        _response_queue.put((callback, result))

    thread = threading.Thread(target=worker, daemon=True)
    thread.start()


def check_queue():
    """Process response queue."""
    try:
        while True:
            callback, result = _response_queue.get_nowait()
            callback(result)
    except Empty:
        pass
    return 0.1


def register():
    bpy.app.timers.register(check_queue, persistent=True)


def unregister():
    try:
        bpy.app.timers.unregister(check_queue)
    except:
        pass
