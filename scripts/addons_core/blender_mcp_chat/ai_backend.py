# SPDX-FileCopyrightText: 2024 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""AI backend - Claude API with Blender tools."""

import bpy
import json
import threading
from urllib import request, error
from queue import Queue, Empty

_response_queue = Queue()

SYSTEM_PROMPT = """You are an AI assistant integrated into Blender 4.0. Help users create 3D content by writing and executing Python code.

IMPORTANT: When users ask you to create or modify anything, use the execute_python tool immediately. Don't just explain - DO IT.

Common Blender Python:
```
# Objects
bpy.ops.mesh.primitive_cube_add(size=2, location=(0,0,0))
bpy.ops.mesh.primitive_uv_sphere_add(radius=1, location=(0,0,0))
bpy.ops.mesh.primitive_cylinder_add(radius=1, depth=2)
bpy.ops.mesh.primitive_cone_add(radius1=1, depth=2)
bpy.ops.mesh.primitive_torus_add(major_radius=1, minor_radius=0.25)
bpy.ops.mesh.primitive_plane_add(size=2)

# Lights
bpy.ops.object.light_add(type='SUN', location=(0,0,5))
bpy.ops.object.light_add(type='POINT', location=(0,0,3))
bpy.ops.object.light_add(type='AREA', location=(0,0,3))

# Camera
bpy.ops.object.camera_add(location=(7,-7,5))
cam = bpy.context.active_object
cam.rotation_euler = (1.1, 0, 0.8)

# Select/Active
obj = bpy.data.objects['Cube']
bpy.context.view_layer.objects.active = obj
obj.select_set(True)

# Transform
obj.location = (1, 2, 3)
obj.rotation_euler = (0, 0, 1.57)  # radians
obj.scale = (2, 2, 2)

# Delete
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete()

# Materials (colored)
mat = bpy.data.materials.new(name="Red")
mat.use_nodes = True
mat.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (1, 0, 0, 1)
obj.data.materials.append(mat)

# Modifiers
bpy.ops.object.modifier_add(type='SUBSURF')
bpy.context.object.modifiers["Subdivision"].levels = 2

# Animation
obj.location = (0, 0, 0)
obj.keyframe_insert(data_path="location", frame=1)
obj.location = (5, 0, 0)
obj.keyframe_insert(data_path="location", frame=50)
```

Be concise. Execute code to accomplish tasks. If something fails, try a different approach."""

TOOLS = [
    {
        "name": "execute_python",
        "description": "Execute Python code in Blender. Use this to create objects, modify the scene, add materials, animate, etc. bpy is already imported.",
        "input_schema": {
            "type": "object",
            "properties": {
                "code": {"type": "string", "description": "Python code to run"},
                "description": {"type": "string", "description": "What this code does"}
            },
            "required": ["code"]
        }
    },
    {
        "name": "get_scene_info",
        "description": "Get list of all objects in the scene with their types and locations.",
        "input_schema": {"type": "object", "properties": {}, "required": []}
    },
    {
        "name": "get_selected",
        "description": "Get details about currently selected objects.",
        "input_schema": {"type": "object", "properties": {}, "required": []}
    },
    {
        "name": "get_active_object",
        "description": "Get detailed info about the active object including modifiers and materials.",
        "input_schema": {"type": "object", "properties": {}, "required": []}
    }
]


def get_scene_info():
    scene = bpy.context.scene
    objs = []
    for o in scene.objects[:50]:
        objs.append({
            "name": o.name,
            "type": o.type,
            "location": [round(v, 2) for v in o.location]
        })
    return {"objects": objs, "total": len(scene.objects)}


def get_selected():
    sel = []
    for o in bpy.context.selected_objects:
        sel.append({
            "name": o.name,
            "type": o.type,
            "location": [round(v, 2) for v in o.location],
            "scale": [round(v, 2) for v in o.scale]
        })
    return {"selected": sel, "count": len(sel)}


def get_active_object():
    obj = bpy.context.active_object
    if not obj:
        return {"error": "No active object"}

    info = {
        "name": obj.name,
        "type": obj.type,
        "location": [round(v, 2) for v in obj.location],
        "rotation": [round(v, 2) for v in obj.rotation_euler],
        "scale": [round(v, 2) for v in obj.scale],
        "modifiers": [m.type for m in obj.modifiers],
        "materials": [m.name for m in obj.data.materials] if hasattr(obj.data, 'materials') else []
    }
    return info


def execute_python(code):
    try:
        exec(code, {"bpy": bpy, "__builtins__": __builtins__})
        return {"success": True}
    except Exception as e:
        return {"error": str(e)}


def run_tool(name, params):
    if name == "execute_python":
        return execute_python(params.get("code", ""))
    elif name == "get_scene_info":
        return get_scene_info()
    elif name == "get_selected":
        return get_selected()
    elif name == "get_active_object":
        return get_active_object()
    return {"error": f"Unknown: {name}"}


def call_api(api_key, model, messages):
    url = "https://api.anthropic.com/v1/messages"
    payload = {
        "model": model,
        "max_tokens": 4096,
        "system": SYSTEM_PROMPT,
        "tools": TOOLS,
        "messages": messages
    }

    data = json.dumps(payload).encode()
    req = request.Request(url, data=data, method='POST')
    req.add_header("Content-Type", "application/json")
    req.add_header("x-api-key", api_key)
    req.add_header("anthropic-version", "2023-06-01")

    try:
        with request.urlopen(req, timeout=120) as r:
            return json.loads(r.read())
    except error.HTTPError as e:
        return {"error": e.read().decode()}
    except Exception as e:
        return {"error": str(e)}


def process_response(api_key, model, messages, exec_tool):
    msgs = messages.copy()

    for _ in range(10):  # Max 10 tool iterations
        resp = call_api(api_key, model, msgs)
        if "error" in resp:
            return resp["error"]

        content = resp.get("content", [])
        texts = []
        tools = []

        for b in content:
            if b.get("type") == "text":
                texts.append(b["text"])
            elif b.get("type") == "tool_use":
                tools.append(b)

        if tools:
            msgs.append({"role": "assistant", "content": content})
            results = []
            for t in tools:
                r = exec_tool(t["name"], t.get("input", {}))
                results.append({
                    "type": "tool_result",
                    "tool_use_id": t["id"],
                    "content": json.dumps(r)
                })
            msgs.append({"role": "user", "content": results})
            continue

        return "\n".join(texts) if texts else "Done."

    return "Reached tool limit."


def send_message(api_key, model, messages, callback):
    tool_results = {}

    def exec_tool(name, params):
        tid = id(params)
        tool_results[tid] = None

        def run():
            tool_results[tid] = run_tool(name, params)

        bpy.app.timers.register(run, first_interval=0)

        import time
        for _ in range(100):
            time.sleep(0.1)
            if tool_results[tid] is not None:
                return tool_results[tid]
        return {"error": "Timeout"}

    def work():
        r = process_response(api_key, model, messages, exec_tool)
        _response_queue.put((callback, r))

    threading.Thread(target=work, daemon=True).start()


def check_queue():
    try:
        while True:
            cb, r = _response_queue.get_nowait()
            cb(r)
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
