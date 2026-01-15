# SPDX-FileCopyrightText: 2024 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
Blender AI Assistant - Built-in chat sidebar powered by Claude.
Loads automatically on startup.
"""

import bpy
import os
import json
import base64
import tempfile
import threading
import textwrap
from urllib import request, error, parse
from queue import Queue, Empty
from bpy.types import Panel, Operator, PropertyGroup
from bpy.props import StringProperty, BoolProperty, IntProperty, FloatProperty, EnumProperty, CollectionProperty, PointerProperty


# =============================================================================
# Properties
# =============================================================================

class AI_ChatMessage(PropertyGroup):
    role: StringProperty()
    content: StringProperty()
    timestamp: FloatProperty()


class AI_Settings(PropertyGroup):
    api_key: StringProperty(name="API Key", subtype='PASSWORD')
    model: EnumProperty(
        name="Model",
        items=[
            ('claude-sonnet-4-20250514', 'Claude Sonnet 4', ''),
            ('claude-3-5-sonnet-20241022', 'Claude 3.5 Sonnet', ''),
        ],
        default='claude-sonnet-4-20250514'
    )
    chat_input: StringProperty(name="Message")
    is_processing: BoolProperty(default=False)


# =============================================================================
# AI Backend
# =============================================================================

_response_queue = Queue()

SYSTEM_PROMPT = """You are an AI assistant integrated into Blender. Help users create 3D content by executing Python code.

IMPORTANT: When users ask to create or modify anything, use tools immediately. Don't just explain - DO IT.

Tools available:
- execute_python: Run Blender Python code (bpy is imported)
- get_scene_info: List all objects in scene
- get_object_info: Details about specific object
- get_selected: Info on selected objects
- search_polyhaven: Find free HDRIs, textures, 3D models
- download_polyhaven_asset: Import PolyHaven assets

Common Blender Python:
```python
bpy.ops.mesh.primitive_cube_add(size=2, location=(0,0,0))
bpy.ops.mesh.primitive_uv_sphere_add(radius=1)
bpy.ops.object.light_add(type='SUN', location=(0,0,5))

mat = bpy.data.materials.new(name="Red")
mat.use_nodes = True
mat.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (1,0,0,1)
obj.data.materials.append(mat)

obj.location = (1, 2, 3)
obj.keyframe_insert(data_path="location", frame=1)
```

Be concise. Execute tools to accomplish tasks."""

TOOLS = [
    {
        "name": "execute_python",
        "description": "Execute Python code in Blender. bpy is imported.",
        "input_schema": {
            "type": "object",
            "properties": {"code": {"type": "string"}},
            "required": ["code"]
        }
    },
    {
        "name": "get_scene_info",
        "description": "Get all objects in scene",
        "input_schema": {"type": "object", "properties": {}, "required": []}
    },
    {
        "name": "get_object_info",
        "description": "Get details about an object",
        "input_schema": {
            "type": "object",
            "properties": {"name": {"type": "string"}},
            "required": ["name"]
        }
    },
    {
        "name": "get_selected",
        "description": "Get selected objects info",
        "input_schema": {"type": "object", "properties": {}, "required": []}
    },
    {
        "name": "search_polyhaven",
        "description": "Search PolyHaven for HDRIs, textures, or models",
        "input_schema": {
            "type": "object",
            "properties": {
                "asset_type": {"type": "string", "enum": ["hdris", "textures", "models"]},
                "query": {"type": "string"}
            },
            "required": ["asset_type"]
        }
    },
    {
        "name": "download_polyhaven_asset",
        "description": "Download and import PolyHaven asset",
        "input_schema": {
            "type": "object",
            "properties": {
                "asset_id": {"type": "string"},
                "asset_type": {"type": "string", "enum": ["hdris", "textures", "models"]},
                "resolution": {"type": "string", "enum": ["1k", "2k", "4k"]}
            },
            "required": ["asset_id", "asset_type"]
        }
    },
    {
        "name": "apply_texture_to_object",
        "description": "Apply PolyHaven texture to object",
        "input_schema": {
            "type": "object",
            "properties": {
                "object_name": {"type": "string"},
                "texture_id": {"type": "string"}
            },
            "required": ["object_name", "texture_id"]
        }
    }
]


def get_scene_info():
    scene = bpy.context.scene
    objs = [{"name": o.name, "type": o.type, "location": [round(v, 2) for v in o.location]} for o in scene.objects[:50]]
    return {"scene": scene.name, "objects": objs, "total": len(scene.objects)}


def get_object_info(name):
    obj = bpy.data.objects.get(name)
    if not obj:
        return {"error": f"Object '{name}' not found"}
    info = {
        "name": obj.name, "type": obj.type,
        "location": [round(v, 2) for v in obj.location],
        "scale": [round(v, 2) for v in obj.scale]
    }
    if obj.type == 'MESH' and obj.data:
        info["vertices"] = len(obj.data.vertices)
        info["materials"] = [m.name for m in obj.data.materials if m]
    return info


def get_selected():
    return {
        "selected": [{"name": o.name, "type": o.type} for o in bpy.context.selected_objects],
        "active": bpy.context.active_object.name if bpy.context.active_object else None
    }


def execute_python(code):
    try:
        exec(code, {"bpy": bpy, "__builtins__": __builtins__})
        return {"success": True}
    except Exception as e:
        return {"error": str(e)}


POLYHAVEN_API = "https://api.polyhaven.com"


def search_polyhaven(asset_type, query=None):
    try:
        url = f"{POLYHAVEN_API}/assets?t={asset_type}"
        with request.urlopen(url, timeout=30) as r:
            data = json.loads(r.read())
        results = []
        for aid, info in list(data.items())[:30]:
            name = info.get('name', aid)
            if query and query.lower() not in name.lower() and query.lower() not in aid.lower():
                continue
            results.append({"id": aid, "name": name})
        return {"assets": results[:15]}
    except Exception as e:
        return {"error": str(e)}


def download_polyhaven_asset(asset_id, asset_type, resolution="2k"):
    try:
        url = f"{POLYHAVEN_API}/files/{asset_id}"
        with request.urlopen(url, timeout=30) as r:
            files = json.loads(r.read())

        download_url = None
        temp_dir = tempfile.gettempdir()

        if asset_type == "hdris" and "hdri" in files and resolution in files["hdri"]:
            download_url = files["hdri"][resolution].get("hdr", {}).get("url")
            ext = "hdr"
        elif asset_type in ["textures", "models"] and "blend" in files and resolution in files["blend"]:
            download_url = files["blend"][resolution].get("blend", {}).get("url")
            ext = "blend"

        if not download_url:
            return {"error": "No download URL"}

        file_path = os.path.join(temp_dir, f"ph_{asset_id}.{ext}")
        with request.urlopen(download_url, timeout=120) as r:
            with open(file_path, 'wb') as f:
                f.write(r.read())

        if ext == "hdr":
            img = bpy.data.images.load(file_path)
            world = bpy.context.scene.world or bpy.data.worlds.new("World")
            bpy.context.scene.world = world
            world.use_nodes = True
            nodes = world.node_tree.nodes
            links = world.node_tree.links
            nodes.clear()
            out = nodes.new('ShaderNodeOutputWorld')
            bg = nodes.new('ShaderNodeBackground')
            env = nodes.new('ShaderNodeTexEnvironment')
            env.image = img
            links.new(env.outputs['Color'], bg.inputs['Color'])
            links.new(bg.outputs['Background'], out.inputs['Surface'])
            return {"success": True, "type": "hdri"}
        elif ext == "blend":
            with bpy.data.libraries.load(file_path, link=False) as (src, dst):
                dst.objects = src.objects
            for obj in dst.objects:
                if obj:
                    bpy.context.collection.objects.link(obj)
            return {"success": True, "imported": len(dst.objects)}

        return {"error": "Unknown format"}
    except Exception as e:
        return {"error": str(e)}


def apply_texture_to_object(object_name, texture_id, resolution="2k"):
    obj = bpy.data.objects.get(object_name)
    if not obj or obj.type != 'MESH':
        return {"error": f"Mesh '{object_name}' not found"}
    try:
        url = f"{POLYHAVEN_API}/files/{texture_id}"
        with request.urlopen(url, timeout=30) as r:
            files = json.loads(r.read())

        mat = bpy.data.materials.new(name=f"PH_{texture_id}")
        mat.use_nodes = True
        nodes = mat.node_tree.nodes
        links = mat.node_tree.links
        nodes.clear()
        out = nodes.new('ShaderNodeOutputMaterial')
        bsdf = nodes.new('ShaderNodeBsdfPrincipled')
        links.new(bsdf.outputs['BSDF'], out.inputs['Surface'])

        temp_dir = tempfile.gettempdir()
        for map_type, input_name in [("diffuse", "Base Color"), ("rough", "Roughness")]:
            if map_type in files and resolution in files[map_type]:
                for fmt in ["png", "jpg"]:
                    if fmt in files[map_type][resolution]:
                        map_url = files[map_type][resolution][fmt].get("url")
                        if map_url:
                            map_path = os.path.join(temp_dir, f"{texture_id}_{map_type}.png")
                            with request.urlopen(map_url, timeout=60) as r:
                                with open(map_path, 'wb') as f:
                                    f.write(r.read())
                            img = bpy.data.images.load(map_path)
                            if map_type != "diffuse":
                                img.colorspace_settings.name = 'Non-Color'
                            tex = nodes.new('ShaderNodeTexImage')
                            tex.image = img
                            links.new(tex.outputs['Color'], bsdf.inputs[input_name])
                        break

        if obj.data.materials:
            obj.data.materials[0] = mat
        else:
            obj.data.materials.append(mat)
        return {"success": True}
    except Exception as e:
        return {"error": str(e)}


def run_tool(name, params):
    tools = {
        "execute_python": lambda p: execute_python(p.get("code", "")),
        "get_scene_info": lambda p: get_scene_info(),
        "get_object_info": lambda p: get_object_info(p.get("name", "")),
        "get_selected": lambda p: get_selected(),
        "search_polyhaven": lambda p: search_polyhaven(p.get("asset_type"), p.get("query")),
        "download_polyhaven_asset": lambda p: download_polyhaven_asset(p.get("asset_id"), p.get("asset_type"), p.get("resolution", "2k")),
        "apply_texture_to_object": lambda p: apply_texture_to_object(p.get("object_name"), p.get("texture_id"))
    }
    return tools.get(name, lambda p: {"error": f"Unknown: {name}"})(params)


def call_api(api_key, model, messages):
    url = "https://api.anthropic.com/v1/messages"
    payload = {"model": model, "max_tokens": 4096, "system": SYSTEM_PROMPT, "tools": TOOLS, "messages": messages}
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
    for _ in range(15):
        resp = call_api(api_key, model, msgs)
        if "error" in resp:
            return resp["error"]
        content = resp.get("content", [])
        texts, tools = [], []
        for b in content:
            if b.get("type") == "text":
                texts.append(b["text"])
            elif b.get("type") == "tool_use":
                tools.append(b)
        if tools:
            msgs.append({"role": "assistant", "content": content})
            results = [{"type": "tool_result", "tool_use_id": t["id"], "content": json.dumps(exec_tool(t["name"], t.get("input", {})))} for t in tools]
            msgs.append({"role": "user", "content": results})
            continue
        return "\n".join(texts) if texts else "Done."
    return "Reached limit."


def send_message(api_key, model, messages, callback):
    tool_results = {}

    def exec_tool(name, params):
        tid = id(params)
        tool_results[tid] = None
        def run():
            tool_results[tid] = run_tool(name, params)
        bpy.app.timers.register(run, first_interval=0)
        import time
        for _ in range(150):
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


# =============================================================================
# Operators
# =============================================================================

class AI_OT_send(Operator):
    bl_idname = "ai.send"
    bl_label = "Send"

    def execute(self, context):
        import time
        ai = context.scene.ai_assistant
        msg = ai.chat_input.strip()
        if not msg or not ai.api_key or ai.is_processing:
            return {'CANCELLED'}

        m = context.scene.ai_messages.add()
        m.role = "user"
        m.content = msg
        m.timestamp = time.time()

        ai.chat_input = ""
        ai.is_processing = True

        messages = [{"role": x.role, "content": x.content} for x in context.scene.ai_messages]
        send_message(ai.api_key, ai.model, messages, self._on_response)
        return {'FINISHED'}

    def _on_response(self, result):
        import time
        scene = bpy.context.scene
        m = scene.ai_messages.add()
        m.role = "assistant"
        m.content = result if isinstance(result, str) else str(result)
        m.timestamp = time.time()
        scene.ai_assistant.is_processing = False
        for win in bpy.context.window_manager.windows:
            for area in win.screen.areas:
                area.tag_redraw()


class AI_OT_clear(Operator):
    bl_idname = "ai.clear"
    bl_label = "Clear"

    def execute(self, context):
        context.scene.ai_messages.clear()
        return {'FINISHED'}


# =============================================================================
# UI Panel
# =============================================================================

def draw_message(layout, role, content):
    box = layout.box()
    box.label(text="You" if role == "user" else "Claude", icon='USER' if role == "user" else 'LIGHT')
    col = box.column(align=True)
    col.scale_y = 0.85
    for line in content.split('\n'):
        for wrap in textwrap.wrap(line, width=42) or ['']:
            col.label(text=wrap)


class VIEW3D_PT_ai(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "AI"
    bl_label = "Assistant"

    def draw(self, context):
        layout = self.layout
        ai = context.scene.ai_assistant

        if not ai.api_key:
            box = layout.box()
            col = box.column(align=True)
            col.label(text="Enter API Key", icon='KEY_HLT')
            col.prop(ai, "api_key", text="")
            col.separator()
            col.scale_y = 0.8
            col.label(text="console.anthropic.com")
            return

        col = layout.column(align=True)
        if not context.scene.ai_messages:
            box = col.box()
            c = box.column(align=True)
            c.scale_y = 0.9
            c.label(text="What would you like to create?")
            c.separator()
            c.label(text="Try: 'Add a red cube'")
        else:
            for msg in list(context.scene.ai_messages)[-6:]:
                draw_message(col, msg.role, msg.content)

        if ai.is_processing:
            col.box().label(text="Thinking...", icon='SORTTIME')

        layout.separator()
        layout.prop(ai, "chat_input", text="")
        row = layout.row(align=True)
        row.scale_y = 1.3
        sub = row.row(align=True)
        sub.enabled = not ai.is_processing and bool(ai.chat_input.strip())
        sub.operator("ai.send", text="Send", icon='PLAY')
        row.operator("ai.clear", text="", icon='TRASH')


class VIEW3D_PT_ai_settings(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "AI"
    bl_label = "Settings"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        ai = context.scene.ai_assistant
        layout.prop(ai, "api_key")
        layout.prop(ai, "model")


# =============================================================================
# Registration
# =============================================================================

classes = (
    AI_ChatMessage,
    AI_Settings,
    AI_OT_send,
    AI_OT_clear,
    VIEW3D_PT_ai,
    VIEW3D_PT_ai_settings,
)


def open_sidebar():
    for window in bpy.context.window_manager.windows:
        for area in window.screen.areas:
            if area.type == 'VIEW_3D':
                if not area.spaces[0].show_region_ui:
                    with bpy.context.temp_override(window=window, area=area):
                        bpy.ops.wm.context_toggle(data_path="space_data.show_region_ui")
                break
    return None


def register():
    for cls in classes:
        bpy.utils.register_class(cls)
    bpy.types.Scene.ai_assistant = PointerProperty(type=AI_Settings)
    bpy.types.Scene.ai_messages = CollectionProperty(type=AI_ChatMessage)
    bpy.app.timers.register(check_queue, persistent=True)
    bpy.app.timers.register(open_sidebar, first_interval=0.5)


def unregister():
    try:
        bpy.app.timers.unregister(check_queue)
    except:
        pass
    del bpy.types.Scene.ai_messages
    del bpy.types.Scene.ai_assistant
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)


if __name__ == "__main__":
    register()
