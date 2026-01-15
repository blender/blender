# SPDX-FileCopyrightText: 2024 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
Blender AI Assistant - Built-in chat sidebar powered by Claude.
Full blender-mcp functionality: scene control, PolyHaven, Sketchfab, Hyper3D, Hunyuan3D.
"""

import bpy
import os
import json
import base64
import tempfile
import threading
import textwrap
import hashlib
import hmac
import time as time_module
import zipfile
from datetime import datetime
from urllib import request, error, parse
from queue import Queue, Empty
from mathutils import Vector
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

    # Integration keys
    sketchfab_token: StringProperty(name="Sketchfab Token", subtype='PASSWORD')
    hyper3d_key: StringProperty(name="Hyper3D Key", subtype='PASSWORD')
    hunyuan_secret_id: StringProperty(name="Hunyuan Secret ID", subtype='PASSWORD')
    hunyuan_secret_key: StringProperty(name="Hunyuan Secret Key", subtype='PASSWORD')


# =============================================================================
# AI Backend
# =============================================================================

_response_queue = Queue()

SYSTEM_PROMPT = """You are an AI assistant integrated into Blender. Help users create 3D content.

IMPORTANT: When users ask to create or modify anything, use tools immediately. Don't explain - DO IT.

Available tools:
- execute_python: Run Blender Python code
- get_scene_info: List all objects
- get_object_info: Details about specific object
- get_selected: Selected objects info
- get_viewport_screenshot: Capture viewport image
- search_polyhaven: Find free HDRIs, textures, 3D models
- download_polyhaven_asset: Import PolyHaven assets
- apply_texture_to_object: Apply PBR textures
- search_sketchfab: Find 3D models on Sketchfab
- download_sketchfab: Download Sketchfab model
- create_hyper3d_job: Generate 3D from text/image (Hyper3D Rodin)
- check_hyper3d_job: Check generation status
- import_hyper3d_result: Import generated model
- create_hunyuan3d_job: Generate 3D with Hunyuan3D
- check_hunyuan3d_job: Check Hunyuan status
- import_hunyuan3d_result: Import Hunyuan model

Common Blender Python:
```python
bpy.ops.mesh.primitive_cube_add(size=2, location=(0,0,0))
bpy.ops.mesh.primitive_uv_sphere_add(radius=1)
bpy.ops.object.light_add(type='SUN', location=(0,0,5))
mat = bpy.data.materials.new(name="Red")
mat.use_nodes = True
mat.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (1,0,0,1)
```

Be concise. Execute tools to accomplish tasks."""

TOOLS = [
    {
        "name": "execute_python",
        "description": "Execute Python code in Blender. bpy is imported.",
        "input_schema": {"type": "object", "properties": {"code": {"type": "string"}}, "required": ["code"]}
    },
    {
        "name": "get_scene_info",
        "description": "Get all objects in scene with types and locations",
        "input_schema": {"type": "object", "properties": {}, "required": []}
    },
    {
        "name": "get_object_info",
        "description": "Get detailed info about an object including vertices, materials, bounds",
        "input_schema": {"type": "object", "properties": {"name": {"type": "string"}}, "required": ["name"]}
    },
    {
        "name": "get_selected",
        "description": "Get info about currently selected objects",
        "input_schema": {"type": "object", "properties": {}, "required": []}
    },
    {
        "name": "get_viewport_screenshot",
        "description": "Capture screenshot of 3D viewport",
        "input_schema": {"type": "object", "properties": {}, "required": []}
    },
    {
        "name": "search_polyhaven",
        "description": "Search PolyHaven for free HDRIs, textures, or 3D models",
        "input_schema": {
            "type": "object",
            "properties": {
                "asset_type": {"type": "string", "enum": ["hdris", "textures", "models"]},
                "query": {"type": "string", "description": "Optional search term"}
            },
            "required": ["asset_type"]
        }
    },
    {
        "name": "download_polyhaven_asset",
        "description": "Download and import a PolyHaven asset",
        "input_schema": {
            "type": "object",
            "properties": {
                "asset_id": {"type": "string"},
                "asset_type": {"type": "string", "enum": ["hdris", "textures", "models"]},
                "resolution": {"type": "string", "enum": ["1k", "2k", "4k"], "default": "2k"}
            },
            "required": ["asset_id", "asset_type"]
        }
    },
    {
        "name": "apply_texture_to_object",
        "description": "Apply a PolyHaven PBR texture to an object",
        "input_schema": {
            "type": "object",
            "properties": {"object_name": {"type": "string"}, "texture_id": {"type": "string"}},
            "required": ["object_name", "texture_id"]
        }
    },
    {
        "name": "search_sketchfab",
        "description": "Search Sketchfab for 3D models",
        "input_schema": {
            "type": "object",
            "properties": {"query": {"type": "string"}, "downloadable": {"type": "boolean", "default": True}},
            "required": ["query"]
        }
    },
    {
        "name": "download_sketchfab",
        "description": "Download and import a Sketchfab model (requires API token)",
        "input_schema": {
            "type": "object",
            "properties": {"uid": {"type": "string"}, "normalize_size": {"type": "number", "default": 2.0}},
            "required": ["uid"]
        }
    },
    {
        "name": "create_hyper3d_job",
        "description": "Create 3D model from text prompt using Hyper3D Rodin",
        "input_schema": {
            "type": "object",
            "properties": {"prompt": {"type": "string"}, "image_url": {"type": "string"}},
            "required": ["prompt"]
        }
    },
    {
        "name": "check_hyper3d_job",
        "description": "Check status of Hyper3D generation job",
        "input_schema": {"type": "object", "properties": {"job_id": {"type": "string"}}, "required": ["job_id"]}
    },
    {
        "name": "import_hyper3d_result",
        "description": "Import completed Hyper3D model",
        "input_schema": {"type": "object", "properties": {"job_id": {"type": "string"}}, "required": ["job_id"]}
    },
    {
        "name": "create_hunyuan3d_job",
        "description": "Create 3D model using Tencent Hunyuan3D",
        "input_schema": {
            "type": "object",
            "properties": {"prompt": {"type": "string"}, "image_url": {"type": "string"}},
            "required": ["prompt"]
        }
    },
    {
        "name": "check_hunyuan3d_job",
        "description": "Check Hunyuan3D job status",
        "input_schema": {"type": "object", "properties": {"job_id": {"type": "string"}}, "required": ["job_id"]}
    },
    {
        "name": "import_hunyuan3d_result",
        "description": "Import completed Hunyuan3D model",
        "input_schema": {"type": "object", "properties": {"job_id": {"type": "string"}}, "required": ["job_id"]}
    }
]


# =============================================================================
# Core Tools
# =============================================================================

def get_scene_info():
    scene = bpy.context.scene
    objs = []
    for o in scene.objects[:50]:
        objs.append({"name": o.name, "type": o.type, "location": [round(v, 2) for v in o.location], "visible": o.visible_get()})
    return {"scene": scene.name, "objects": objs, "total": len(scene.objects), "active": bpy.context.active_object.name if bpy.context.active_object else None}


def get_object_info(name):
    obj = bpy.data.objects.get(name)
    if not obj:
        return {"error": f"Object '{name}' not found"}
    info = {
        "name": obj.name, "type": obj.type,
        "location": [round(v, 2) for v in obj.location],
        "rotation": [round(v, 2) for v in obj.rotation_euler],
        "scale": [round(v, 2) for v in obj.scale],
        "dimensions": [round(v, 2) for v in obj.dimensions],
    }
    if obj.type == 'MESH' and obj.data:
        info["vertices"] = len(obj.data.vertices)
        info["faces"] = len(obj.data.polygons)
        info["materials"] = [m.name for m in obj.data.materials if m]
        corners = [obj.matrix_world @ Vector(c) for c in obj.bound_box]
        info["bounds"] = {
            "min": [round(min(c[i] for c in corners), 2) for i in range(3)],
            "max": [round(max(c[i] for c in corners), 2) for i in range(3)]
        }
    if obj.modifiers:
        info["modifiers"] = [m.type for m in obj.modifiers]
    return info


def get_selected():
    return {
        "selected": [{"name": o.name, "type": o.type, "location": [round(v, 2) for v in o.location]} for o in bpy.context.selected_objects],
        "active": bpy.context.active_object.name if bpy.context.active_object else None
    }


def get_viewport_screenshot():
    for area in bpy.context.screen.areas:
        if area.type == 'VIEW_3D':
            break
    else:
        return {"error": "No 3D viewport"}

    temp_path = os.path.join(tempfile.gettempdir(), "viewport.png")
    try:
        orig = bpy.context.scene.render.image_settings.file_format
        bpy.context.scene.render.image_settings.file_format = 'PNG'
        with bpy.context.temp_override(area=area):
            bpy.ops.render.opengl(write_still=False)
        img = bpy.data.images.get('Render Result')
        if img:
            img.save_render(temp_path)
            with open(temp_path, 'rb') as f:
                data = base64.b64encode(f.read()).decode()
            os.remove(temp_path)
            return {"captured": True, "base64_preview": data[:200] + "..."}
        return {"error": "Capture failed"}
    finally:
        bpy.context.scene.render.image_settings.file_format = orig


def execute_python(code):
    try:
        exec(code, {"bpy": bpy, "__builtins__": __builtins__})
        return {"success": True}
    except Exception as e:
        return {"error": str(e)}


# =============================================================================
# PolyHaven
# =============================================================================

POLYHAVEN = "https://api.polyhaven.com"


def search_polyhaven(asset_type, query=None):
    try:
        with request.urlopen(f"{POLYHAVEN}/assets?t={asset_type}", timeout=30) as r:
            data = json.loads(r.read())
        results = []
        for aid, info in list(data.items())[:50]:
            name = info.get('name', aid)
            if query and query.lower() not in name.lower() and query.lower() not in aid.lower():
                continue
            results.append({"id": aid, "name": name, "categories": info.get('categories', [])})
        return {"assets": results[:20], "total": len(results)}
    except Exception as e:
        return {"error": str(e)}


def download_polyhaven_asset(asset_id, asset_type, resolution="2k"):
    try:
        with request.urlopen(f"{POLYHAVEN}/files/{asset_id}", timeout=30) as r:
            files = json.loads(r.read())

        temp_dir = tempfile.gettempdir()
        download_url = ext = None

        if asset_type == "hdris" and "hdri" in files and resolution in files["hdri"]:
            download_url = files["hdri"][resolution].get("hdr", {}).get("url")
            ext = "hdr"
        elif asset_type in ["textures", "models"] and "blend" in files and resolution in files["blend"]:
            download_url = files["blend"][resolution].get("blend", {}).get("url")
            ext = "blend"
        elif asset_type == "models" and "gltf" in files:
            for res in [resolution, "2k", "1k"]:
                if res in files["gltf"]:
                    download_url = files["gltf"][res].get("gltf", {}).get("url")
                    ext = "gltf"
                    break

        if not download_url:
            return {"error": f"No URL for {asset_id}"}

        path = os.path.join(temp_dir, f"ph_{asset_id}.{ext}")
        with request.urlopen(download_url, timeout=120) as r:
            with open(path, 'wb') as f:
                f.write(r.read())

        if ext == "hdr":
            img = bpy.data.images.load(path)
            world = bpy.context.scene.world or bpy.data.worlds.new("World")
            bpy.context.scene.world = world
            world.use_nodes = True
            nodes, links = world.node_tree.nodes, world.node_tree.links
            nodes.clear()
            out, bg, env = nodes.new('ShaderNodeOutputWorld'), nodes.new('ShaderNodeBackground'), nodes.new('ShaderNodeTexEnvironment')
            env.image = img
            links.new(env.outputs['Color'], bg.inputs['Color'])
            links.new(bg.outputs['Background'], out.inputs['Surface'])
            return {"success": True, "type": "hdri", "name": asset_id}
        elif ext == "blend":
            with bpy.data.libraries.load(path, link=False) as (src, dst):
                dst.objects = src.objects
            for obj in dst.objects:
                if obj:
                    bpy.context.collection.objects.link(obj)
            return {"success": True, "type": asset_type, "imported": len([o for o in dst.objects if o])}
        elif ext == "gltf":
            bpy.ops.import_scene.gltf(filepath=path)
            return {"success": True, "type": "model"}
        return {"error": "Unknown format"}
    except Exception as e:
        return {"error": str(e)}


def apply_texture_to_object(object_name, texture_id, resolution="2k"):
    obj = bpy.data.objects.get(object_name)
    if not obj or obj.type != 'MESH':
        return {"error": f"Mesh '{object_name}' not found"}
    try:
        with request.urlopen(f"{POLYHAVEN}/files/{texture_id}", timeout=30) as r:
            files = json.loads(r.read())

        mat = bpy.data.materials.new(name=f"PH_{texture_id}")
        mat.use_nodes = True
        nodes, links = mat.node_tree.nodes, mat.node_tree.links
        nodes.clear()
        out, bsdf = nodes.new('ShaderNodeOutputMaterial'), nodes.new('ShaderNodeBsdfPrincipled')
        links.new(bsdf.outputs['BSDF'], out.inputs['Surface'])

        temp_dir = tempfile.gettempdir()
        for map_type, inp in [("diffuse", "Base Color"), ("rough", "Roughness"), ("nor_gl", "Normal")]:
            if map_type not in files or resolution not in files[map_type]:
                continue
            for fmt in ["png", "jpg"]:
                if fmt in files[map_type][resolution]:
                    url = files[map_type][resolution][fmt].get("url")
                    if url:
                        p = os.path.join(temp_dir, f"{texture_id}_{map_type}.{fmt}")
                        with request.urlopen(url, timeout=60) as r:
                            with open(p, 'wb') as f:
                                f.write(r.read())
                        img = bpy.data.images.load(p)
                        if map_type != "diffuse":
                            img.colorspace_settings.name = 'Non-Color'
                        tex = nodes.new('ShaderNodeTexImage')
                        tex.image = img
                        if inp == "Normal":
                            nm = nodes.new('ShaderNodeNormalMap')
                            links.new(tex.outputs['Color'], nm.inputs['Color'])
                            links.new(nm.outputs['Normal'], bsdf.inputs['Normal'])
                        else:
                            links.new(tex.outputs['Color'], bsdf.inputs[inp])
                    break

        if obj.data.materials:
            obj.data.materials[0] = mat
        else:
            obj.data.materials.append(mat)
        return {"success": True, "material": mat.name}
    except Exception as e:
        return {"error": str(e)}


# =============================================================================
# Sketchfab
# =============================================================================

SKETCHFAB = "https://api.sketchfab.com/v3"


def search_sketchfab(query, downloadable=True):
    try:
        params = {"q": query, "type": "models", "downloadable": str(downloadable).lower()}
        with request.urlopen(f"{SKETCHFAB}/search?{parse.urlencode(params)}", timeout=30) as r:
            data = json.loads(r.read())
        return {
            "results": [{"uid": m["uid"], "name": m["name"], "user": m.get("user", {}).get("displayName"), "faces": m.get("faceCount")} for m in data.get("results", [])[:15]],
            "total": data.get("totalResults", 0)
        }
    except Exception as e:
        return {"error": str(e)}


def download_sketchfab(uid, normalize_size=2.0):
    token = bpy.context.scene.ai_assistant.sketchfab_token
    if not token:
        return {"error": "Sketchfab token not set in Settings"}
    try:
        req = request.Request(f"{SKETCHFAB}/models/{uid}/download")
        req.add_header("Authorization", f"Token {token}")
        with request.urlopen(req, timeout=30) as r:
            data = json.loads(r.read())

        gltf_url = data.get("gltf", {}).get("url")
        if not gltf_url:
            return {"error": "No GLTF download"}

        temp_dir = tempfile.gettempdir()
        zip_path = os.path.join(temp_dir, f"sf_{uid}.zip")
        with request.urlopen(gltf_url, timeout=300) as r:
            with open(zip_path, 'wb') as f:
                f.write(r.read())

        extract_dir = os.path.join(temp_dir, f"sf_{uid}")
        os.makedirs(extract_dir, exist_ok=True)
        with zipfile.ZipFile(zip_path, 'r') as z:
            z.extractall(extract_dir)

        gltf_file = None
        for root, _, files in os.walk(extract_dir):
            for f in files:
                if f.endswith(('.gltf', '.glb')):
                    gltf_file = os.path.join(root, f)
                    break

        if not gltf_file:
            return {"error": "No GLTF in download"}

        bpy.ops.import_scene.gltf(filepath=gltf_file)

        if normalize_size > 0 and bpy.context.selected_objects:
            max_dim = max(max(o.dimensions) for o in bpy.context.selected_objects if o.dimensions)
            if max_dim > 0:
                scale = normalize_size / max_dim
                for o in bpy.context.selected_objects:
                    o.scale *= scale

        return {"success": True, "uid": uid}
    except Exception as e:
        return {"error": str(e)}


# =============================================================================
# Hyper3D Rodin
# =============================================================================

HYPER3D = "https://hyperhuman.deemos.com/api"
_hyper3d_jobs = {}


def create_hyper3d_job(prompt, image_url=None):
    key = bpy.context.scene.ai_assistant.hyper3d_key
    if not key:
        return {"error": "Hyper3D key not set in Settings"}
    try:
        payload = {"prompt": prompt, "tier": "Regular"}
        if image_url:
            payload["image_url"] = image_url

        data = json.dumps(payload).encode()
        req = request.Request(f"{HYPER3D}/task/rodin_mesh", data=data, method='POST')
        req.add_header("Content-Type", "application/json")
        req.add_header("Authorization", f"Bearer {key}")

        with request.urlopen(req, timeout=60) as r:
            result = json.loads(r.read())

        job_id = result.get("uuid") or result.get("subscription_key")
        _hyper3d_jobs[job_id] = result
        return {"job_id": job_id, "status": "created"}
    except Exception as e:
        return {"error": str(e)}


def check_hyper3d_job(job_id):
    key = bpy.context.scene.ai_assistant.hyper3d_key
    if not key:
        return {"error": "Hyper3D key not set"}
    try:
        payload = json.dumps({"subscription_key": job_id}).encode()
        req = request.Request(f"{HYPER3D}/task/status", data=payload, method='POST')
        req.add_header("Content-Type", "application/json")
        req.add_header("Authorization", f"Bearer {key}")

        with request.urlopen(req, timeout=30) as r:
            result = json.loads(r.read())
        return {"status": result.get("status"), "progress": result.get("progress", 0)}
    except Exception as e:
        return {"error": str(e)}


def import_hyper3d_result(job_id):
    key = bpy.context.scene.ai_assistant.hyper3d_key
    if not key:
        return {"error": "Hyper3D key not set"}
    try:
        payload = json.dumps({"task_uuid": job_id}).encode()
        req = request.Request(f"{HYPER3D}/task/download", data=payload, method='POST')
        req.add_header("Content-Type", "application/json")
        req.add_header("Authorization", f"Bearer {key}")

        with request.urlopen(req, timeout=30) as r:
            result = json.loads(r.read())

        url = result.get("download_url")
        if not url:
            return {"error": "No download URL"}

        path = os.path.join(tempfile.gettempdir(), f"hyper3d_{job_id}.glb")
        with request.urlopen(url, timeout=120) as r:
            with open(path, 'wb') as f:
                f.write(r.read())

        bpy.ops.import_scene.gltf(filepath=path)
        return {"success": True, "imported": True}
    except Exception as e:
        return {"error": str(e)}


# =============================================================================
# Hunyuan3D
# =============================================================================

HUNYUAN = "https://hunyuan.tencentcloudapi.com"
_hunyuan_jobs = {}


def _sign_hunyuan(secret_id, secret_key, action, payload):
    service = "hunyuan"
    host = "hunyuan.tencentcloudapi.com"
    timestamp = int(time_module.time())
    date = datetime.utcfromtimestamp(timestamp).strftime("%Y-%m-%d")

    payload_str = json.dumps(payload)
    hashed_payload = hashlib.sha256(payload_str.encode()).hexdigest()

    canonical = f"POST\n/\n\ncontent-type:application/json; charset=utf-8\nhost:{host}\n\ncontent-type;host\n{hashed_payload}"
    hashed_canonical = hashlib.sha256(canonical.encode()).hexdigest()

    credential_scope = f"{date}/{service}/tc3_request"
    string_to_sign = f"TC3-HMAC-SHA256\n{timestamp}\n{credential_scope}\n{hashed_canonical}"

    def hmac_sha256(key, msg):
        return hmac.new(key, msg.encode(), hashlib.sha256).digest()

    secret_date = hmac_sha256(f"TC3{secret_key}".encode(), date)
    secret_service = hmac_sha256(secret_date, service)
    secret_signing = hmac_sha256(secret_service, "tc3_request")
    signature = hmac.new(secret_signing, string_to_sign.encode(), hashlib.sha256).hexdigest()

    auth = f"TC3-HMAC-SHA256 Credential={secret_id}/{credential_scope}, SignedHeaders=content-type;host, Signature={signature}"

    return {
        "Authorization": auth,
        "Content-Type": "application/json; charset=utf-8",
        "Host": host,
        "X-TC-Action": action,
        "X-TC-Timestamp": str(timestamp),
        "X-TC-Version": "2024-06-17"
    }


def create_hunyuan3d_job(prompt, image_url=None):
    sid = bpy.context.scene.ai_assistant.hunyuan_secret_id
    skey = bpy.context.scene.ai_assistant.hunyuan_secret_key
    if not sid or not skey:
        return {"error": "Hunyuan credentials not set in Settings"}
    try:
        payload = {"Prompt": prompt}
        if image_url:
            payload["ImageUrl"] = image_url

        headers = _sign_hunyuan(sid, skey, "SubmitHunyuan3DJob", payload)
        data = json.dumps(payload).encode()
        req = request.Request(f"https://hunyuan.tencentcloudapi.com/", data=data, method='POST')
        for k, v in headers.items():
            req.add_header(k, v)

        with request.urlopen(req, timeout=60) as r:
            result = json.loads(r.read())

        job_id = result.get("Response", {}).get("JobId")
        _hunyuan_jobs[job_id] = result
        return {"job_id": job_id, "status": "created"}
    except Exception as e:
        return {"error": str(e)}


def check_hunyuan3d_job(job_id):
    sid = bpy.context.scene.ai_assistant.hunyuan_secret_id
    skey = bpy.context.scene.ai_assistant.hunyuan_secret_key
    if not sid or not skey:
        return {"error": "Hunyuan credentials not set"}
    try:
        payload = {"JobId": job_id}
        headers = _sign_hunyuan(sid, skey, "QueryHunyuan3DJob", payload)
        data = json.dumps(payload).encode()
        req = request.Request(f"https://hunyuan.tencentcloudapi.com/", data=data, method='POST')
        for k, v in headers.items():
            req.add_header(k, v)

        with request.urlopen(req, timeout=30) as r:
            result = json.loads(r.read())
        return {"status": result.get("Response", {}).get("Status"), "result": result}
    except Exception as e:
        return {"error": str(e)}


def import_hunyuan3d_result(job_id):
    sid = bpy.context.scene.ai_assistant.hunyuan_secret_id
    skey = bpy.context.scene.ai_assistant.hunyuan_secret_key
    if not sid or not skey:
        return {"error": "Hunyuan credentials not set"}
    try:
        status = check_hunyuan3d_job(job_id)
        url = status.get("result", {}).get("Response", {}).get("ResultUrl")
        if not url:
            return {"error": "No result URL - job may not be complete"}

        temp_dir = tempfile.gettempdir()
        zip_path = os.path.join(temp_dir, f"hunyuan_{job_id}.zip")
        with request.urlopen(url, timeout=120) as r:
            with open(zip_path, 'wb') as f:
                f.write(r.read())

        extract_dir = os.path.join(temp_dir, f"hunyuan_{job_id}")
        os.makedirs(extract_dir, exist_ok=True)
        with zipfile.ZipFile(zip_path, 'r') as z:
            z.extractall(extract_dir)

        model_file = None
        for root, _, files in os.walk(extract_dir):
            for f in files:
                if f.endswith(('.obj', '.glb', '.gltf')):
                    model_file = os.path.join(root, f)
                    break

        if not model_file:
            return {"error": "No model in download"}

        if model_file.endswith('.obj'):
            bpy.ops.wm.obj_import(filepath=model_file)
        else:
            bpy.ops.import_scene.gltf(filepath=model_file)

        return {"success": True}
    except Exception as e:
        return {"error": str(e)}


# =============================================================================
# Tool Router
# =============================================================================

def run_tool(name, params):
    tools = {
        "execute_python": lambda p: execute_python(p.get("code", "")),
        "get_scene_info": lambda p: get_scene_info(),
        "get_object_info": lambda p: get_object_info(p.get("name", "")),
        "get_selected": lambda p: get_selected(),
        "get_viewport_screenshot": lambda p: get_viewport_screenshot(),
        "search_polyhaven": lambda p: search_polyhaven(p.get("asset_type"), p.get("query")),
        "download_polyhaven_asset": lambda p: download_polyhaven_asset(p.get("asset_id"), p.get("asset_type"), p.get("resolution", "2k")),
        "apply_texture_to_object": lambda p: apply_texture_to_object(p.get("object_name"), p.get("texture_id")),
        "search_sketchfab": lambda p: search_sketchfab(p.get("query"), p.get("downloadable", True)),
        "download_sketchfab": lambda p: download_sketchfab(p.get("uid"), p.get("normalize_size", 2.0)),
        "create_hyper3d_job": lambda p: create_hyper3d_job(p.get("prompt"), p.get("image_url")),
        "check_hyper3d_job": lambda p: check_hyper3d_job(p.get("job_id")),
        "import_hyper3d_result": lambda p: import_hyper3d_result(p.get("job_id")),
        "create_hunyuan3d_job": lambda p: create_hunyuan3d_job(p.get("prompt"), p.get("image_url")),
        "check_hunyuan3d_job": lambda p: check_hunyuan3d_job(p.get("job_id")),
        "import_hunyuan3d_result": lambda p: import_hunyuan3d_result(p.get("job_id")),
    }
    return tools.get(name, lambda p: {"error": f"Unknown: {name}"})(params)


# =============================================================================
# API Communication
# =============================================================================

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
    for _ in range(20):
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
        for _ in range(200):
            time_module.sleep(0.1)
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
        ai = context.scene.ai_assistant
        msg = ai.chat_input.strip()
        if not msg or not ai.api_key or ai.is_processing:
            return {'CANCELLED'}
        m = context.scene.ai_messages.add()
        m.role = "user"
        m.content = msg
        m.timestamp = time_module.time()
        ai.chat_input = ""
        ai.is_processing = True
        messages = [{"role": x.role, "content": x.content} for x in context.scene.ai_messages]
        send_message(ai.api_key, ai.model, messages, self._on_response)
        return {'FINISHED'}

    def _on_response(self, result):
        scene = bpy.context.scene
        m = scene.ai_messages.add()
        m.role = "assistant"
        m.content = result if isinstance(result, str) else str(result)
        m.timestamp = time_module.time()
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
# UI
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
            c.label(text="or: 'Find an HDRI on PolyHaven'")
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
        layout.separator()
        layout.label(text="Integrations:")
        layout.prop(ai, "sketchfab_token")
        layout.prop(ai, "hyper3d_key")
        layout.prop(ai, "hunyuan_secret_id")
        layout.prop(ai, "hunyuan_secret_key")


# =============================================================================
# Registration
# =============================================================================

classes = (AI_ChatMessage, AI_Settings, AI_OT_send, AI_OT_clear, VIEW3D_PT_ai, VIEW3D_PT_ai_settings)


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
