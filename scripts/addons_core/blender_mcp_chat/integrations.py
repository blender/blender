# SPDX-FileCopyrightText: 2024 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""Third-party integrations for MCP Chat: PolyHaven, Sketchfab, Hyper3D, Hunyuan3D."""

import bpy
import os
import json
import tempfile
import base64
import hashlib
import hmac
import datetime
import time
import zipfile
from urllib import request, parse, error

# ============================================================================
# PolyHaven Integration
# ============================================================================

POLYHAVEN_API_BASE = "https://api.polyhaven.com"


def get_polyhaven_status(params):
    """Check if PolyHaven integration is enabled."""
    try:
        scene = bpy.context.scene
        if hasattr(scene, 'mcp_chat'):
            return {"enabled": scene.mcp_chat.polyhaven.enabled}
        return {"enabled": False}
    except Exception:
        return {"enabled": False}


def get_polyhaven_categories(params):
    """Get available categories for a specific asset type."""
    asset_type = params.get("asset_type", "hdris")

    if asset_type not in ("hdris", "textures", "models", "all"):
        return {"error": "Invalid asset_type. Must be one of: hdris, textures, models, all"}

    try:
        url = f"{POLYHAVEN_API_BASE}/categories/{asset_type}"
        with request.urlopen(url, timeout=30) as response:
            data = json.loads(response.read().decode('utf-8'))
            return {"categories": data}
    except Exception as e:
        return {"error": f"Failed to fetch categories: {str(e)}"}


def search_polyhaven_assets(params):
    """Search PolyHaven assets by type and category."""
    asset_type = params.get("asset_type", "all")
    categories = params.get("categories", None)

    try:
        url = f"{POLYHAVEN_API_BASE}/assets"
        query_params = {"t": asset_type}
        if categories:
            query_params["c"] = categories

        full_url = f"{url}?{parse.urlencode(query_params)}"

        with request.urlopen(full_url, timeout=30) as response:
            data = json.loads(response.read().decode('utf-8'))

            # Sort by popularity/downloads
            sorted_assets = sorted(
                data.items(),
                key=lambda x: x[1].get('download_count', 0),
                reverse=True
            )

            # Return top 50 results
            results = []
            for asset_id, asset_data in sorted_assets[:50]:
                results.append({
                    "id": asset_id,
                    "name": asset_data.get('name', asset_id),
                    "type": asset_type,
                    "categories": asset_data.get('categories', []),
                    "download_count": asset_data.get('download_count', 0),
                })

            return {"assets": results, "total": len(data)}

    except Exception as e:
        return {"error": f"Failed to search assets: {str(e)}"}


def download_polyhaven_asset(params):
    """Download a PolyHaven asset and import it into Blender."""
    asset_id = params.get("asset_id", "")
    asset_type = params.get("asset_type", "")
    resolution = params.get("resolution", "2k")
    file_format = params.get("format", "blend")

    if not asset_id or not asset_type:
        return {"error": "asset_id and asset_type are required"}

    try:
        # Get asset files info
        url = f"{POLYHAVEN_API_BASE}/files/{asset_id}"
        with request.urlopen(url, timeout=30) as response:
            files_data = json.loads(response.read().decode('utf-8'))

        # Find the appropriate file to download
        download_url = None

        if asset_type == "hdris":
            if "hdri" in files_data and resolution in files_data["hdri"]:
                download_url = files_data["hdri"][resolution].get("hdr", {}).get("url")
        elif asset_type == "textures":
            # For textures, download the blend file if available
            if "blend" in files_data and resolution in files_data["blend"]:
                download_url = files_data["blend"][resolution].get("blend", {}).get("url")
        elif asset_type == "models":
            if "blend" in files_data and resolution in files_data["blend"]:
                download_url = files_data["blend"][resolution].get("blend", {}).get("url")
            elif "gltf" in files_data:
                download_url = files_data["gltf"].get(resolution, {}).get("gltf", {}).get("url")

        if not download_url:
            return {"error": f"No download URL found for {asset_id} at {resolution}"}

        # Download the file
        temp_dir = tempfile.gettempdir()
        ext = download_url.split('.')[-1].split('?')[0]
        temp_path = os.path.join(temp_dir, f"polyhaven_{asset_id}.{ext}")

        with request.urlopen(download_url, timeout=120) as response:
            with open(temp_path, 'wb') as f:
                f.write(response.read())

        # Import into Blender
        if ext == "hdr" or ext == "exr":
            # Load as environment texture
            img = bpy.data.images.load(temp_path)
            img.name = asset_id

            # Set as world environment if desired
            world = bpy.context.scene.world
            if not world:
                world = bpy.data.worlds.new("World")
                bpy.context.scene.world = world
            world.use_nodes = True
            nodes = world.node_tree.nodes
            links = world.node_tree.links

            # Clear existing nodes
            nodes.clear()

            # Create environment nodes
            output = nodes.new(type='ShaderNodeOutputWorld')
            background = nodes.new(type='ShaderNodeBackground')
            env_tex = nodes.new(type='ShaderNodeTexEnvironment')

            env_tex.image = img
            env_tex.location = (-300, 0)
            background.location = (0, 0)
            output.location = (200, 0)

            links.new(env_tex.outputs['Color'], background.inputs['Color'])
            links.new(background.outputs['Background'], output.inputs['Surface'])

            return {"success": True, "type": "hdri", "name": asset_id, "path": temp_path}

        elif ext == "blend":
            # Append from blend file
            with bpy.data.libraries.load(temp_path, link=False) as (data_from, data_to):
                if asset_type == "models":
                    data_to.objects = data_from.objects
                elif asset_type == "textures":
                    data_to.materials = data_from.materials

            # Link appended objects to scene
            if asset_type == "models":
                for obj in data_to.objects:
                    if obj:
                        bpy.context.collection.objects.link(obj)

            return {"success": True, "type": asset_type, "name": asset_id, "path": temp_path}

        else:
            return {"success": True, "downloaded": True, "path": temp_path}

    except Exception as e:
        import traceback
        return {"error": f"Download failed: {str(e)}", "traceback": traceback.format_exc()}


def set_polyhaven_texture(params):
    """Apply a downloaded PolyHaven texture to an object."""
    object_name = params.get("object_name", "")
    texture_id = params.get("texture_id", "")
    resolution = params.get("resolution", "2k")

    if not object_name or not texture_id:
        return {"error": "object_name and texture_id are required"}

    obj = bpy.data.objects.get(object_name)
    if not obj:
        return {"error": f"Object '{object_name}' not found"}

    if obj.type != 'MESH':
        return {"error": f"Object '{object_name}' is not a mesh"}

    try:
        # Get texture files info
        url = f"{POLYHAVEN_API_BASE}/files/{texture_id}"
        with request.urlopen(url, timeout=30) as response:
            files_data = json.loads(response.read().decode('utf-8'))

        # Create PBR material
        mat_name = f"PH_{texture_id}"
        mat = bpy.data.materials.get(mat_name)
        if not mat:
            mat = bpy.data.materials.new(name=mat_name)
        mat.use_nodes = True
        nodes = mat.node_tree.nodes
        links = mat.node_tree.links

        # Clear existing nodes
        nodes.clear()

        # Create principled BSDF
        output = nodes.new(type='ShaderNodeOutputMaterial')
        principled = nodes.new(type='ShaderNodeBsdfPrincipled')
        output.location = (300, 0)
        principled.location = (0, 0)
        links.new(principled.outputs['BSDF'], output.inputs['Surface'])

        # Download and connect texture maps
        temp_dir = tempfile.gettempdir()
        texture_maps = {
            "diffuse": ("Base Color", False),
            "rough": ("Roughness", True),
            "nor_gl": ("Normal", False),
            "metal": ("Metallic", True),
            "disp": ("Displacement", True),
        }

        for map_type, (input_name, is_data) in texture_maps.items():
            if map_type in files_data and resolution in files_data[map_type]:
                map_data = files_data[map_type][resolution]
                map_url = None

                # Get URL for the map
                for fmt in ["png", "jpg", "exr"]:
                    if fmt in map_data:
                        map_url = map_data[fmt].get("url")
                        break

                if map_url:
                    # Download texture
                    ext = map_url.split('.')[-1].split('?')[0]
                    map_path = os.path.join(temp_dir, f"polyhaven_{texture_id}_{map_type}.{ext}")

                    with request.urlopen(map_url, timeout=60) as response:
                        with open(map_path, 'wb') as f:
                            f.write(response.read())

                    # Load image
                    img = bpy.data.images.load(map_path)
                    img.name = f"{texture_id}_{map_type}"
                    if is_data:
                        img.colorspace_settings.name = 'Non-Color'

                    # Create image texture node
                    tex_node = nodes.new(type='ShaderNodeTexImage')
                    tex_node.image = img
                    tex_node.location = (-400, 300 - list(texture_maps.keys()).index(map_type) * 300)

                    # Connect to principled BSDF
                    if input_name == "Normal":
                        normal_map = nodes.new(type='ShaderNodeNormalMap')
                        normal_map.location = (-200, tex_node.location.y)
                        links.new(tex_node.outputs['Color'], normal_map.inputs['Color'])
                        links.new(normal_map.outputs['Normal'], principled.inputs['Normal'])
                    elif input_name == "Displacement":
                        # Create displacement setup
                        disp_node = nodes.new(type='ShaderNodeDisplacement')
                        disp_node.location = (-200, tex_node.location.y)
                        links.new(tex_node.outputs['Color'], disp_node.inputs['Height'])
                        links.new(disp_node.outputs['Displacement'], output.inputs['Displacement'])
                    elif input_name in principled.inputs:
                        links.new(tex_node.outputs['Color'], principled.inputs[input_name])

        # Assign material to object
        if obj.data.materials:
            obj.data.materials[0] = mat
        else:
            obj.data.materials.append(mat)

        return {"success": True, "material": mat_name, "object": object_name}

    except Exception as e:
        import traceback
        return {"error": f"Failed to apply texture: {str(e)}", "traceback": traceback.format_exc()}


# ============================================================================
# Sketchfab Integration
# ============================================================================

SKETCHFAB_API_BASE = "https://api.sketchfab.com/v3"


def get_sketchfab_status(params):
    """Check if Sketchfab integration is enabled."""
    try:
        scene = bpy.context.scene
        if hasattr(scene, 'mcp_chat'):
            settings = scene.mcp_chat.sketchfab
            return {
                "enabled": settings.enabled,
                "has_token": bool(settings.api_token)
            }
        return {"enabled": False, "has_token": False}
    except Exception:
        return {"enabled": False, "has_token": False}


def search_sketchfab_models(params):
    """Search for models on Sketchfab."""
    query = params.get("query", "")
    categories = params.get("categories", None)
    count = params.get("count", 20)
    downloadable = params.get("downloadable", True)

    if not query:
        return {"error": "query parameter required"}

    try:
        scene = bpy.context.scene
        api_token = ""
        if hasattr(scene, 'mcp_chat'):
            api_token = scene.mcp_chat.sketchfab.api_token

        # Build search URL
        query_params = {
            "q": query,
            "type": "models",
            "count": min(count, 24),
        }
        if downloadable:
            query_params["downloadable"] = "true"
        if categories:
            query_params["categories"] = categories

        url = f"{SKETCHFAB_API_BASE}/search?{parse.urlencode(query_params)}"

        req = request.Request(url)
        if api_token:
            req.add_header("Authorization", f"Token {api_token}")

        with request.urlopen(req, timeout=30) as response:
            data = json.loads(response.read().decode('utf-8'))

            results = []
            for model in data.get("results", []):
                results.append({
                    "uid": model.get("uid"),
                    "name": model.get("name"),
                    "description": model.get("description", "")[:200],
                    "viewCount": model.get("viewCount", 0),
                    "likeCount": model.get("likeCount", 0),
                    "user": model.get("user", {}).get("displayName", "Unknown"),
                    "license": model.get("license", {}).get("label", "Unknown"),
                    "faceCount": model.get("faceCount", 0),
                    "isDownloadable": model.get("isDownloadable", False),
                    "thumbnailUrl": model.get("thumbnails", {}).get("images", [{}])[0].get("url", ""),
                })

            return {"results": results, "total": data.get("totalResults", 0)}

    except Exception as e:
        return {"error": f"Search failed: {str(e)}"}


def get_sketchfab_model_preview(params):
    """Get a preview thumbnail for a Sketchfab model."""
    uid = params.get("uid", "")

    if not uid:
        return {"error": "uid parameter required"}

    try:
        url = f"{SKETCHFAB_API_BASE}/models/{uid}"
        with request.urlopen(url, timeout=30) as response:
            data = json.loads(response.read().decode('utf-8'))

            thumbnails = data.get("thumbnails", {}).get("images", [])
            if thumbnails:
                # Get the largest thumbnail
                thumbnail_url = max(thumbnails, key=lambda x: x.get("width", 0)).get("url", "")

                if thumbnail_url:
                    with request.urlopen(thumbnail_url, timeout=30) as img_response:
                        image_data = img_response.read()
                        return {
                            "image_base64": base64.b64encode(image_data).decode('utf-8'),
                            "format": "jpeg"
                        }

            return {"error": "No thumbnail available"}

    except Exception as e:
        return {"error": f"Failed to get preview: {str(e)}"}


def download_sketchfab_model(params):
    """Download a Sketchfab model and import it into Blender."""
    uid = params.get("uid", "")
    target_size = params.get("target_size", 2.0)  # Target size in Blender units

    if not uid:
        return {"error": "uid parameter required"}

    try:
        scene = bpy.context.scene
        api_token = ""
        if hasattr(scene, 'mcp_chat'):
            api_token = scene.mcp_chat.sketchfab.api_token

        if not api_token:
            return {"error": "Sketchfab API token required for downloads"}

        # Request download
        url = f"{SKETCHFAB_API_BASE}/models/{uid}/download"
        req = request.Request(url)
        req.add_header("Authorization", f"Token {api_token}")

        with request.urlopen(req, timeout=30) as response:
            data = json.loads(response.read().decode('utf-8'))

        # Get GLTF download URL
        gltf_data = data.get("gltf", {})
        download_url = gltf_data.get("url")

        if not download_url:
            return {"error": "GLTF format not available for this model"}

        # Download the file
        temp_dir = tempfile.gettempdir()
        zip_path = os.path.join(temp_dir, f"sketchfab_{uid}.zip")

        with request.urlopen(download_url, timeout=300) as response:
            with open(zip_path, 'wb') as f:
                f.write(response.read())

        # Extract zip
        extract_dir = os.path.join(temp_dir, f"sketchfab_{uid}")
        os.makedirs(extract_dir, exist_ok=True)

        with zipfile.ZipFile(zip_path, 'r') as zip_ref:
            # Security check for path traversal
            for member in zip_ref.namelist():
                member_path = os.path.realpath(os.path.join(extract_dir, member))
                if not member_path.startswith(os.path.realpath(extract_dir)):
                    return {"error": "Security error: zip contains path traversal"}
            zip_ref.extractall(extract_dir)

        # Find GLTF/GLB file
        gltf_file = None
        for root, dirs, files in os.walk(extract_dir):
            for file in files:
                if file.endswith(('.gltf', '.glb')):
                    gltf_file = os.path.join(root, file)
                    break
            if gltf_file:
                break

        if not gltf_file:
            return {"error": "No GLTF file found in download"}

        # Import GLTF
        bpy.ops.import_scene.gltf(filepath=gltf_file)

        # Normalize size if target_size specified
        if target_size > 0:
            imported_objects = bpy.context.selected_objects
            if imported_objects:
                # Calculate bounding box
                min_co = None
                max_co = None
                for obj in imported_objects:
                    if obj.type == 'MESH':
                        for corner in obj.bound_box:
                            world_corner = obj.matrix_world @ bpy.Vector(corner)
                            if min_co is None:
                                min_co = world_corner.copy()
                                max_co = world_corner.copy()
                            else:
                                min_co.x = min(min_co.x, world_corner.x)
                                min_co.y = min(min_co.y, world_corner.y)
                                min_co.z = min(min_co.z, world_corner.z)
                                max_co.x = max(max_co.x, world_corner.x)
                                max_co.y = max(max_co.y, world_corner.y)
                                max_co.z = max(max_co.z, world_corner.z)

                if min_co and max_co:
                    size = max_co - min_co
                    max_dim = max(size.x, size.y, size.z)
                    if max_dim > 0:
                        scale_factor = target_size / max_dim
                        for obj in imported_objects:
                            obj.scale *= scale_factor

        return {
            "success": True,
            "uid": uid,
            "imported_objects": [obj.name for obj in bpy.context.selected_objects]
        }

    except Exception as e:
        import traceback
        return {"error": f"Download failed: {str(e)}", "traceback": traceback.format_exc()}


# ============================================================================
# Hyper3D Rodin Integration
# ============================================================================

RODIN_API_BASE = "https://hyperhuman.deemos.com/api"
FAL_AI_API_BASE = "https://fal.run/fal-ai"


def get_hyper3d_status(params):
    """Check if Hyper3D integration is enabled."""
    try:
        scene = bpy.context.scene
        if hasattr(scene, 'mcp_chat'):
            settings = scene.mcp_chat.hyper3d
            return {
                "enabled": settings.enabled,
                "mode": settings.mode,
                "has_api_key": bool(settings.api_key)
            }
        return {"enabled": False, "mode": "MAIN_SITE", "has_api_key": False}
    except Exception:
        return {"enabled": False, "mode": "MAIN_SITE", "has_api_key": False}


def create_rodin_job(params):
    """Create a new Hyper3D Rodin generation job."""
    text_prompt = params.get("text_prompt", "")
    image_paths = params.get("image_paths", [])
    image_urls = params.get("image_urls", [])
    bbox_condition = params.get("bbox_condition", None)

    if not text_prompt and not image_paths and not image_urls:
        return {"error": "Either text_prompt, image_paths, or image_urls required"}

    try:
        scene = bpy.context.scene
        if not hasattr(scene, 'mcp_chat'):
            return {"error": "MCP Chat not initialized"}

        settings = scene.mcp_chat.hyper3d
        if not settings.enabled:
            return {"error": "Hyper3D integration is not enabled"}

        api_key = settings.api_key
        if not api_key:
            return {"error": "Hyper3D API key not configured"}

        # Prepare request based on mode
        if settings.mode == 'FAL_AI':
            # FAL.ai endpoint
            url = f"{FAL_AI_API_BASE}/rodin"

            payload = {}
            if text_prompt:
                payload["prompt"] = text_prompt
            if image_urls:
                payload["image_url"] = image_urls[0]
            if bbox_condition:
                payload["bbox_condition"] = bbox_condition

            data = json.dumps(payload).encode('utf-8')
            req = request.Request(url, data=data, method='POST')
            req.add_header("Content-Type", "application/json")
            req.add_header("Authorization", f"Key {api_key}")

        else:
            # Main site endpoint
            url = f"{RODIN_API_BASE}/task/rodin_mesh"

            payload = {"tier": "Regular"}
            if text_prompt:
                payload["prompt"] = text_prompt
            if bbox_condition:
                payload["bbox_condition"] = bbox_condition

            data = json.dumps(payload).encode('utf-8')
            req = request.Request(url, data=data, method='POST')
            req.add_header("Content-Type", "application/json")
            req.add_header("Authorization", f"Bearer {api_key}")

        with request.urlopen(req, timeout=60) as response:
            result = json.loads(response.read().decode('utf-8'))

        return {
            "success": True,
            "job_id": result.get("uuid") or result.get("request_id"),
            "subscription_key": result.get("subscription_key"),
        }

    except Exception as e:
        import traceback
        return {"error": f"Failed to create job: {str(e)}", "traceback": traceback.format_exc()}


def poll_rodin_job_status(params):
    """Check the status of a Hyper3D Rodin generation job."""
    subscription_key = params.get("subscription_key", "")
    request_id = params.get("request_id", "")

    if not subscription_key and not request_id:
        return {"error": "subscription_key or request_id required"}

    try:
        scene = bpy.context.scene
        settings = scene.mcp_chat.hyper3d

        if settings.mode == 'FAL_AI':
            # FAL.ai status check
            url = f"{FAL_AI_API_BASE}/rodin/status/{request_id}"
            req = request.Request(url)
            req.add_header("Authorization", f"Key {settings.api_key}")
        else:
            # Main site status check
            url = f"{RODIN_API_BASE}/task/status"
            payload = {"subscription_key": subscription_key}
            data = json.dumps(payload).encode('utf-8')
            req = request.Request(url, data=data, method='POST')
            req.add_header("Content-Type", "application/json")
            req.add_header("Authorization", f"Bearer {settings.api_key}")

        with request.urlopen(req, timeout=30) as response:
            result = json.loads(response.read().decode('utf-8'))

        status = result.get("status", "unknown")
        return {
            "status": status,
            "progress": result.get("progress", 0),
            "result": result
        }

    except Exception as e:
        return {"error": f"Failed to poll status: {str(e)}"}


def import_rodin_asset(params):
    """Import a completed Hyper3D Rodin asset into Blender."""
    name = params.get("name", "RodinModel")
    task_uuid = params.get("task_uuid", "")
    request_id = params.get("request_id", "")
    download_url = params.get("download_url", "")

    if not download_url and not task_uuid and not request_id:
        return {"error": "download_url, task_uuid, or request_id required"}

    try:
        scene = bpy.context.scene
        settings = scene.mcp_chat.hyper3d

        # Get download URL if not provided
        if not download_url:
            if settings.mode == 'FAL_AI':
                url = f"{FAL_AI_API_BASE}/rodin/result/{request_id}"
                req = request.Request(url)
                req.add_header("Authorization", f"Key {settings.api_key}")
            else:
                url = f"{RODIN_API_BASE}/task/download"
                payload = {"task_uuid": task_uuid}
                data = json.dumps(payload).encode('utf-8')
                req = request.Request(url, data=data, method='POST')
                req.add_header("Content-Type", "application/json")
                req.add_header("Authorization", f"Bearer {settings.api_key}")

            with request.urlopen(req, timeout=30) as response:
                result = json.loads(response.read().decode('utf-8'))
                download_url = result.get("download_url") or result.get("output_url")

        if not download_url:
            return {"error": "Could not get download URL"}

        # Download GLB file
        temp_dir = tempfile.gettempdir()
        glb_path = os.path.join(temp_dir, f"rodin_{name}.glb")

        with request.urlopen(download_url, timeout=120) as response:
            with open(glb_path, 'wb') as f:
                f.write(response.read())

        # Import GLB
        bpy.ops.import_scene.gltf(filepath=glb_path)

        # Rename imported object
        if bpy.context.selected_objects:
            bpy.context.selected_objects[0].name = name

        return {
            "success": True,
            "name": name,
            "imported_objects": [obj.name for obj in bpy.context.selected_objects]
        }

    except Exception as e:
        import traceback
        return {"error": f"Import failed: {str(e)}", "traceback": traceback.format_exc()}


# ============================================================================
# Hunyuan3D Integration
# ============================================================================

HUNYUAN_API_BASE = "https://hunyuan.cloud.tencent.com"


def get_hunyuan3d_status(params):
    """Check if Hunyuan3D integration is enabled."""
    try:
        scene = bpy.context.scene
        if hasattr(scene, 'mcp_chat'):
            settings = scene.mcp_chat.hunyuan3d
            return {
                "enabled": settings.enabled,
                "mode": settings.mode,
                "has_credentials": bool(settings.secret_id and settings.secret_key) or settings.mode == 'LOCAL'
            }
        return {"enabled": False, "mode": "TENCENT", "has_credentials": False}
    except Exception:
        return {"enabled": False, "mode": "TENCENT", "has_credentials": False}


def _sign_tencent_request(secret_id, secret_key, service, host, action, payload):
    """Sign a Tencent Cloud API request using TC3-HMAC-SHA256."""
    algorithm = "TC3-HMAC-SHA256"
    timestamp = int(time.time())
    date = datetime.datetime.utcfromtimestamp(timestamp).strftime("%Y-%m-%d")

    # Build canonical request
    http_request_method = "POST"
    canonical_uri = "/"
    canonical_querystring = ""
    ct = "application/json; charset=utf-8"
    payload_str = json.dumps(payload)
    canonical_headers = f"content-type:{ct}\nhost:{host}\n"
    signed_headers = "content-type;host"
    hashed_request_payload = hashlib.sha256(payload_str.encode("utf-8")).hexdigest()
    canonical_request = (f"{http_request_method}\n{canonical_uri}\n{canonical_querystring}\n"
                         f"{canonical_headers}\n{signed_headers}\n{hashed_request_payload}")

    # Build string to sign
    credential_scope = f"{date}/{service}/tc3_request"
    hashed_canonical_request = hashlib.sha256(canonical_request.encode("utf-8")).hexdigest()
    string_to_sign = f"{algorithm}\n{timestamp}\n{credential_scope}\n{hashed_canonical_request}"

    # Calculate signature
    def hmac_sha256(key, msg):
        return hmac.new(key, msg.encode("utf-8"), hashlib.sha256).digest()

    secret_date = hmac_sha256(("TC3" + secret_key).encode("utf-8"), date)
    secret_service = hmac_sha256(secret_date, service)
    secret_signing = hmac_sha256(secret_service, "tc3_request")
    signature = hmac.new(secret_signing, string_to_sign.encode("utf-8"), hashlib.sha256).hexdigest()

    # Build authorization header
    authorization = (f"{algorithm} Credential={secret_id}/{credential_scope}, "
                     f"SignedHeaders={signed_headers}, Signature={signature}")

    return {
        "Authorization": authorization,
        "Content-Type": ct,
        "Host": host,
        "X-TC-Action": action,
        "X-TC-Timestamp": str(timestamp),
        "X-TC-Version": "2024-06-17",
    }


def create_hunyuan_job(params):
    """Create a new Hunyuan3D generation job."""
    text_prompt = params.get("text_prompt", "")
    image_url = params.get("image_url", "")

    if not text_prompt and not image_url:
        return {"error": "Either text_prompt or image_url required"}

    try:
        scene = bpy.context.scene
        settings = scene.mcp_chat.hunyuan3d

        if not settings.enabled:
            return {"error": "Hunyuan3D integration is not enabled"}

        if settings.mode == 'LOCAL':
            # Local API
            url = f"{settings.local_url}/generate"
            payload = {
                "prompt": text_prompt,
                "image_url": image_url,
                "octree_resolution": settings.octree_resolution,
                "num_inference_steps": settings.num_inference_steps,
                "guidance_scale": settings.guidance_scale,
            }
            data = json.dumps(payload).encode('utf-8')
            req = request.Request(url, data=data, method='POST')
            req.add_header("Content-Type", "application/json")

        else:
            # Tencent Cloud API
            host = "hunyuan.tencentcloudapi.com"
            service = "hunyuan"
            action = "SubmitHunyuan3DJob"

            payload = {
                "Prompt": text_prompt,
            }
            if image_url:
                payload["ImageUrl"] = image_url

            headers = _sign_tencent_request(
                settings.secret_id,
                settings.secret_key,
                service,
                host,
                action,
                payload
            )

            url = f"https://{host}/"
            data = json.dumps(payload).encode('utf-8')
            req = request.Request(url, data=data, method='POST')
            for key, value in headers.items():
                req.add_header(key, value)

        with request.urlopen(req, timeout=60) as response:
            result = json.loads(response.read().decode('utf-8'))

        job_id = result.get("job_id") or result.get("Response", {}).get("JobId")
        return {
            "success": True,
            "job_id": job_id,
            "result": result
        }

    except Exception as e:
        import traceback
        return {"error": f"Failed to create job: {str(e)}", "traceback": traceback.format_exc()}


def poll_hunyuan_job_status(params):
    """Check the status of a Hunyuan3D generation job."""
    job_id = params.get("job_id", "")

    if not job_id:
        return {"error": "job_id required"}

    try:
        scene = bpy.context.scene
        settings = scene.mcp_chat.hunyuan3d

        if settings.mode == 'LOCAL':
            url = f"{settings.local_url}/status/{job_id}"
            req = request.Request(url)
        else:
            host = "hunyuan.tencentcloudapi.com"
            service = "hunyuan"
            action = "QueryHunyuan3DJob"

            payload = {"JobId": job_id}
            headers = _sign_tencent_request(
                settings.secret_id,
                settings.secret_key,
                service,
                host,
                action,
                payload
            )

            url = f"https://{host}/"
            data = json.dumps(payload).encode('utf-8')
            req = request.Request(url, data=data, method='POST')
            for key, value in headers.items():
                req.add_header(key, value)

        with request.urlopen(req, timeout=30) as response:
            result = json.loads(response.read().decode('utf-8'))

        status = result.get("status") or result.get("Response", {}).get("Status", "unknown")
        return {
            "status": status,
            "result": result
        }

    except Exception as e:
        return {"error": f"Failed to poll status: {str(e)}"}


def import_hunyuan_asset(params):
    """Import a completed Hunyuan3D asset into Blender."""
    name = params.get("name", "HunyuanModel")
    zip_url = params.get("zip_url", "")
    job_id = params.get("job_id", "")

    if not zip_url and not job_id:
        return {"error": "zip_url or job_id required"}

    try:
        scene = bpy.context.scene
        settings = scene.mcp_chat.hunyuan3d

        # Get download URL if not provided
        if not zip_url and job_id:
            status_result = poll_hunyuan_job_status({"job_id": job_id})
            if "error" in status_result:
                return status_result

            result_data = status_result.get("result", {})
            if settings.mode == 'LOCAL':
                zip_url = result_data.get("output_url")
            else:
                zip_url = result_data.get("Response", {}).get("ResultUrl")

        if not zip_url:
            return {"error": "Could not get download URL"}

        # Download ZIP file
        temp_dir = tempfile.gettempdir()
        zip_path = os.path.join(temp_dir, f"hunyuan_{name}.zip")

        with request.urlopen(zip_url, timeout=120) as response:
            with open(zip_path, 'wb') as f:
                f.write(response.read())

        # Extract ZIP
        extract_dir = os.path.join(temp_dir, f"hunyuan_{name}")
        os.makedirs(extract_dir, exist_ok=True)

        with zipfile.ZipFile(zip_path, 'r') as zip_ref:
            # Security check
            for member in zip_ref.namelist():
                member_path = os.path.realpath(os.path.join(extract_dir, member))
                if not member_path.startswith(os.path.realpath(extract_dir)):
                    return {"error": "Security error: zip contains path traversal"}
            zip_ref.extractall(extract_dir)

        # Find OBJ or GLB file
        model_file = None
        for root, dirs, files in os.walk(extract_dir):
            for file in files:
                if file.endswith('.obj'):
                    model_file = os.path.join(root, file)
                    break
                elif file.endswith('.glb'):
                    model_file = os.path.join(root, file)
            if model_file:
                break

        if not model_file:
            return {"error": "No model file found in download"}

        # Import model
        if model_file.endswith('.obj'):
            bpy.ops.wm.obj_import(filepath=model_file)
        else:
            bpy.ops.import_scene.gltf(filepath=model_file)

        # Rename imported object
        if bpy.context.selected_objects:
            bpy.context.selected_objects[0].name = name

        return {
            "success": True,
            "name": name,
            "imported_objects": [obj.name for obj in bpy.context.selected_objects]
        }

    except Exception as e:
        import traceback
        return {"error": f"Import failed: {str(e)}", "traceback": traceback.format_exc()}


# ============================================================================
# Handler Registry
# ============================================================================

INTEGRATION_HANDLERS = {
    # PolyHaven
    "get_polyhaven_status": get_polyhaven_status,
    "get_polyhaven_categories": get_polyhaven_categories,
    "search_polyhaven_assets": search_polyhaven_assets,
    "download_polyhaven_asset": download_polyhaven_asset,
    "set_polyhaven_texture": set_polyhaven_texture,

    # Sketchfab
    "get_sketchfab_status": get_sketchfab_status,
    "search_sketchfab_models": search_sketchfab_models,
    "get_sketchfab_model_preview": get_sketchfab_model_preview,
    "download_sketchfab_model": download_sketchfab_model,

    # Hyper3D Rodin
    "get_hyper3d_status": get_hyper3d_status,
    "create_rodin_job": create_rodin_job,
    "poll_rodin_job_status": poll_rodin_job_status,
    "import_rodin_asset": import_rodin_asset,

    # Hunyuan3D
    "get_hunyuan3d_status": get_hunyuan3d_status,
    "create_hunyuan_job": create_hunyuan_job,
    "poll_hunyuan_job_status": poll_hunyuan_job_status,
    "import_hunyuan_asset": import_hunyuan_asset,
}


def register_handlers(server):
    """Register all integration handlers with the server."""
    for cmd_type, handler in INTEGRATION_HANDLERS.items():
        server.register_handler(cmd_type, handler)


def register():
    """Register the integrations module."""
    pass


def unregister():
    """Unregister the integrations module."""
    pass
