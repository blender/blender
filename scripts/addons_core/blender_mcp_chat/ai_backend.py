# SPDX-FileCopyrightText: 2024 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""AI backend - Claude API with full Blender tools."""

import bpy
import os
import json
import base64
import tempfile
import threading
import zipfile
from urllib import request, error, parse
from queue import Queue, Empty
from mathutils import Vector

_response_queue = Queue()

SYSTEM_PROMPT = """You are an AI assistant integrated into Blender 4.0. Help users create 3D content.

IMPORTANT: When users ask you to create or modify anything, use tools immediately. Don't just explain - DO IT.

You have access to:
- execute_python: Run any Blender Python code
- get_scene_info: See all objects in scene
- get_object_info: Get details about specific object
- get_selected: Info about selected objects
- get_viewport_screenshot: Capture what user sees
- search_polyhaven: Find free HDRIs, textures, 3D models
- download_polyhaven_asset: Download and import PolyHaven assets
- search_sketchfab: Find 3D models on Sketchfab

Common Blender Python:
```python
# Create objects
bpy.ops.mesh.primitive_cube_add(size=2, location=(0,0,0))
bpy.ops.mesh.primitive_uv_sphere_add(radius=1)
bpy.ops.mesh.primitive_cylinder_add(radius=1, depth=2)
bpy.ops.object.light_add(type='SUN', location=(0,0,5))
bpy.ops.object.camera_add(location=(7,-7,5))

# Materials
mat = bpy.data.materials.new(name="Red")
mat.use_nodes = True
mat.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (1,0,0,1)
obj.data.materials.append(mat)

# Transform
obj.location = (1, 2, 3)
obj.rotation_euler = (0, 0, 1.57)
obj.scale = (2, 2, 2)

# Animation
obj.keyframe_insert(data_path="location", frame=1)
```

Be concise. Execute tools to accomplish tasks."""

TOOLS = [
    {
        "name": "execute_python",
        "description": "Execute Python code in Blender. bpy is already imported. Use for creating objects, materials, animations, etc.",
        "input_schema": {
            "type": "object",
            "properties": {
                "code": {"type": "string", "description": "Python code to execute"},
                "description": {"type": "string", "description": "What this code does"}
            },
            "required": ["code"]
        }
    },
    {
        "name": "get_scene_info",
        "description": "Get list of all objects in the current scene with types and locations",
        "input_schema": {"type": "object", "properties": {}, "required": []}
    },
    {
        "name": "get_object_info",
        "description": "Get detailed info about a specific object by name",
        "input_schema": {
            "type": "object",
            "properties": {
                "name": {"type": "string", "description": "Object name"}
            },
            "required": ["name"]
        }
    },
    {
        "name": "get_selected",
        "description": "Get info about currently selected objects",
        "input_schema": {"type": "object", "properties": {}, "required": []}
    },
    {
        "name": "get_viewport_screenshot",
        "description": "Capture a screenshot of the 3D viewport to see what user sees",
        "input_schema": {"type": "object", "properties": {}, "required": []}
    },
    {
        "name": "search_polyhaven",
        "description": "Search PolyHaven for free HDRIs, textures, or 3D models",
        "input_schema": {
            "type": "object",
            "properties": {
                "asset_type": {"type": "string", "enum": ["hdris", "textures", "models"], "description": "Type of asset"},
                "query": {"type": "string", "description": "Optional search query"}
            },
            "required": ["asset_type"]
        }
    },
    {
        "name": "download_polyhaven_asset",
        "description": "Download and import a PolyHaven asset into Blender",
        "input_schema": {
            "type": "object",
            "properties": {
                "asset_id": {"type": "string", "description": "Asset ID from search results"},
                "asset_type": {"type": "string", "enum": ["hdris", "textures", "models"]},
                "resolution": {"type": "string", "enum": ["1k", "2k", "4k"], "default": "2k"}
            },
            "required": ["asset_id", "asset_type"]
        }
    },
    {
        "name": "apply_texture_to_object",
        "description": "Apply a downloaded PolyHaven texture to an object",
        "input_schema": {
            "type": "object",
            "properties": {
                "object_name": {"type": "string", "description": "Name of object to texture"},
                "texture_id": {"type": "string", "description": "PolyHaven texture ID"}
            },
            "required": ["object_name", "texture_id"]
        }
    },
    {
        "name": "search_sketchfab",
        "description": "Search Sketchfab for 3D models (requires API token in settings)",
        "input_schema": {
            "type": "object",
            "properties": {
                "query": {"type": "string", "description": "Search query"},
                "downloadable": {"type": "boolean", "default": True}
            },
            "required": ["query"]
        }
    }
]


# =============================================================================
# Tool Implementations
# =============================================================================

def get_scene_info():
    scene = bpy.context.scene
    objs = []
    for o in scene.objects[:50]:
        objs.append({
            "name": o.name,
            "type": o.type,
            "location": [round(v, 2) for v in o.location],
            "visible": o.visible_get()
        })
    return {
        "scene": scene.name,
        "objects": objs,
        "total": len(scene.objects),
        "active": bpy.context.active_object.name if bpy.context.active_object else None
    }


def get_object_info(name):
    obj = bpy.data.objects.get(name)
    if not obj:
        return {"error": f"Object '{name}' not found"}

    info = {
        "name": obj.name,
        "type": obj.type,
        "location": [round(v, 2) for v in obj.location],
        "rotation": [round(v, 2) for v in obj.rotation_euler],
        "scale": [round(v, 2) for v in obj.scale],
        "dimensions": [round(v, 2) for v in obj.dimensions],
        "parent": obj.parent.name if obj.parent else None,
        "children": [c.name for c in obj.children]
    }

    if obj.type == 'MESH' and obj.data:
        info["vertices"] = len(obj.data.vertices)
        info["faces"] = len(obj.data.polygons)
        info["materials"] = [m.name for m in obj.data.materials if m]

        # World bounding box
        corners = [obj.matrix_world @ Vector(c) for c in obj.bound_box]
        info["bounds"] = {
            "min": [round(min(c[i] for c in corners), 2) for i in range(3)],
            "max": [round(max(c[i] for c in corners), 2) for i in range(3)]
        }

    if obj.modifiers:
        info["modifiers"] = [{"name": m.name, "type": m.type} for m in obj.modifiers]

    return info


def get_selected():
    return {
        "selected": [
            {"name": o.name, "type": o.type, "location": [round(v, 2) for v in o.location]}
            for o in bpy.context.selected_objects
        ],
        "active": bpy.context.active_object.name if bpy.context.active_object else None
    }


def get_viewport_screenshot():
    """Capture viewport screenshot."""
    for area in bpy.context.screen.areas:
        if area.type == 'VIEW_3D':
            break
    else:
        return {"error": "No 3D viewport found"}

    temp_path = os.path.join(tempfile.gettempdir(), "blender_viewport.png")

    try:
        # Store settings
        orig_format = bpy.context.scene.render.image_settings.file_format
        bpy.context.scene.render.image_settings.file_format = 'PNG'

        # Render viewport
        with bpy.context.temp_override(area=area):
            bpy.ops.render.opengl(write_still=False)

        # Save
        img = bpy.data.images.get('Render Result')
        if img:
            img.save_render(temp_path)
            with open(temp_path, 'rb') as f:
                data = base64.b64encode(f.read()).decode()
            os.remove(temp_path)
            return {"image_base64": data[:100] + "...", "captured": True}

        return {"error": "Failed to capture"}
    finally:
        bpy.context.scene.render.image_settings.file_format = orig_format


def execute_python(code):
    try:
        exec(code, {"bpy": bpy, "__builtins__": __builtins__})
        return {"success": True}
    except Exception as e:
        return {"error": str(e)}


# =============================================================================
# PolyHaven Integration
# =============================================================================

POLYHAVEN_API = "https://api.polyhaven.com"


def search_polyhaven(asset_type, query=None):
    """Search PolyHaven assets."""
    try:
        url = f"{POLYHAVEN_API}/assets?t={asset_type}"
        with request.urlopen(url, timeout=30) as r:
            data = json.loads(r.read())

        # Filter by query if provided
        results = []
        for asset_id, info in list(data.items())[:30]:
            name = info.get('name', asset_id)
            if query and query.lower() not in name.lower() and query.lower() not in asset_id.lower():
                continue
            results.append({
                "id": asset_id,
                "name": name,
                "categories": info.get('categories', [])
            })

        return {"assets": results[:20], "total": len(results)}
    except Exception as e:
        return {"error": str(e)}


def download_polyhaven_asset(asset_id, asset_type, resolution="2k"):
    """Download and import PolyHaven asset."""
    try:
        # Get file URLs
        url = f"{POLYHAVEN_API}/files/{asset_id}"
        with request.urlopen(url, timeout=30) as r:
            files = json.loads(r.read())

        download_url = None
        temp_dir = tempfile.gettempdir()

        if asset_type == "hdris":
            if "hdri" in files and resolution in files["hdri"]:
                download_url = files["hdri"][resolution].get("hdr", {}).get("url")
                if download_url:
                    ext = "hdr"
        elif asset_type == "textures":
            if "blend" in files and resolution in files["blend"]:
                download_url = files["blend"][resolution].get("blend", {}).get("url")
                ext = "blend"
        elif asset_type == "models":
            if "blend" in files and resolution in files["blend"]:
                download_url = files["blend"][resolution].get("blend", {}).get("url")
                ext = "blend"
            elif "gltf" in files:
                for res in [resolution, "2k", "1k"]:
                    if res in files["gltf"]:
                        download_url = files["gltf"][res].get("gltf", {}).get("url")
                        ext = "gltf"
                        break

        if not download_url:
            return {"error": f"No download URL for {asset_id}"}

        # Download
        file_path = os.path.join(temp_dir, f"polyhaven_{asset_id}.{ext}")
        with request.urlopen(download_url, timeout=120) as r:
            with open(file_path, 'wb') as f:
                f.write(r.read())

        # Import
        if ext == "hdr":
            img = bpy.data.images.load(file_path)
            img.name = asset_id

            # Set as world HDRI
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

            return {"success": True, "type": "hdri", "name": asset_id}

        elif ext == "blend":
            with bpy.data.libraries.load(file_path, link=False) as (src, dst):
                dst.objects = src.objects
                dst.materials = src.materials

            for obj in dst.objects:
                if obj:
                    bpy.context.collection.objects.link(obj)

            return {"success": True, "type": asset_type, "imported": len(dst.objects)}

        elif ext == "gltf":
            bpy.ops.import_scene.gltf(filepath=file_path)
            return {"success": True, "type": "model", "name": asset_id}

        return {"error": "Unknown format"}

    except Exception as e:
        return {"error": str(e)}


def apply_texture_to_object(object_name, texture_id, resolution="2k"):
    """Apply PolyHaven texture to object."""
    obj = bpy.data.objects.get(object_name)
    if not obj or obj.type != 'MESH':
        return {"error": f"Mesh object '{object_name}' not found"}

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
        texture_maps = {"diffuse": "Base Color", "rough": "Roughness", "nor_gl": "Normal"}

        for map_type, input_name in texture_maps.items():
            if map_type not in files or resolution not in files[map_type]:
                continue

            map_url = None
            for fmt in ["png", "jpg"]:
                if fmt in files[map_type][resolution]:
                    map_url = files[map_type][resolution][fmt].get("url")
                    break

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

                if input_name == "Normal":
                    nmap = nodes.new('ShaderNodeNormalMap')
                    links.new(tex.outputs['Color'], nmap.inputs['Color'])
                    links.new(nmap.outputs['Normal'], bsdf.inputs['Normal'])
                else:
                    links.new(tex.outputs['Color'], bsdf.inputs[input_name])

        if obj.data.materials:
            obj.data.materials[0] = mat
        else:
            obj.data.materials.append(mat)

        return {"success": True, "material": mat.name}

    except Exception as e:
        return {"error": str(e)}


# =============================================================================
# Sketchfab Integration
# =============================================================================

SKETCHFAB_API = "https://api.sketchfab.com/v3"


def search_sketchfab(query, downloadable=True):
    """Search Sketchfab models."""
    try:
        params = {"q": query, "type": "models", "downloadable": str(downloadable).lower()}
        url = f"{SKETCHFAB_API}/search?{parse.urlencode(params)}"

        with request.urlopen(url, timeout=30) as r:
            data = json.loads(r.read())

        results = []
        for m in data.get("results", [])[:20]:
            results.append({
                "uid": m.get("uid"),
                "name": m.get("name"),
                "user": m.get("user", {}).get("displayName"),
                "faces": m.get("faceCount"),
                "downloadable": m.get("isDownloadable")
            })

        return {"results": results, "total": data.get("totalResults", 0)}

    except Exception as e:
        return {"error": str(e)}


# =============================================================================
# Tool Router
# =============================================================================

def run_tool(name, params):
    if name == "execute_python":
        return execute_python(params.get("code", ""))
    elif name == "get_scene_info":
        return get_scene_info()
    elif name == "get_object_info":
        return get_object_info(params.get("name", ""))
    elif name == "get_selected":
        return get_selected()
    elif name == "get_viewport_screenshot":
        return get_viewport_screenshot()
    elif name == "search_polyhaven":
        return search_polyhaven(params.get("asset_type", "models"), params.get("query"))
    elif name == "download_polyhaven_asset":
        return download_polyhaven_asset(
            params.get("asset_id", ""),
            params.get("asset_type", ""),
            params.get("resolution", "2k")
        )
    elif name == "apply_texture_to_object":
        return apply_texture_to_object(
            params.get("object_name", ""),
            params.get("texture_id", ""),
            params.get("resolution", "2k")
        )
    elif name == "search_sketchfab":
        return search_sketchfab(params.get("query", ""), params.get("downloadable", True))
    return {"error": f"Unknown tool: {name}"}


# =============================================================================
# API Communication
# =============================================================================

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

    for _ in range(15):  # Max iterations
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

    return "Reached iteration limit."


def send_message(api_key, model, messages, callback):
    tool_results = {}

    def exec_tool(name, params):
        tid = id(params)
        tool_results[tid] = None

        def run():
            tool_results[tid] = run_tool(name, params)

        bpy.app.timers.register(run, first_interval=0)

        import time
        for _ in range(150):  # 15 sec timeout
            time.sleep(0.1)
            if tool_results[tid] is not None:
                return tool_results[tid]
        return {"error": "Tool timeout"}

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
