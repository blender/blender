# SPDX-FileCopyrightText: 2024 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""Command handlers for core MCP functionality."""

import bpy
import os
import tempfile
import base64
from mathutils import Vector


def get_scene_info(params):
    """Get detailed information about the current Blender scene."""
    scene = bpy.context.scene

    objects_info = []
    for obj in scene.objects:
        obj_data = {
            "name": obj.name,
            "type": obj.type,
            "location": list(obj.location),
            "rotation": list(obj.rotation_euler),
            "scale": list(obj.scale),
            "visible": obj.visible_get(),
        }

        # Add material info if available
        if obj.type == 'MESH' and obj.data.materials:
            obj_data["materials"] = [mat.name for mat in obj.data.materials if mat]

        objects_info.append(obj_data)

    return {
        "scene_name": scene.name,
        "frame_current": scene.frame_current,
        "frame_start": scene.frame_start,
        "frame_end": scene.frame_end,
        "render_engine": scene.render.engine,
        "object_count": len(scene.objects),
        "objects": objects_info,
        "active_object": bpy.context.active_object.name if bpy.context.active_object else None,
        "selected_objects": [obj.name for obj in bpy.context.selected_objects],
    }


def get_object_info(params):
    """Get detailed information about a specific object."""
    object_name = params.get("object_name", "")

    if not object_name:
        return {"error": "object_name parameter required"}

    obj = bpy.data.objects.get(object_name)
    if not obj:
        return {"error": f"Object '{object_name}' not found"}

    info = {
        "name": obj.name,
        "type": obj.type,
        "location": list(obj.location),
        "rotation_euler": list(obj.rotation_euler),
        "rotation_quaternion": list(obj.rotation_quaternion),
        "scale": list(obj.scale),
        "dimensions": list(obj.dimensions),
        "visible": obj.visible_get(),
        "parent": obj.parent.name if obj.parent else None,
        "children": [child.name for child in obj.children],
    }

    # Mesh-specific information
    if obj.type == 'MESH' and obj.data:
        mesh = obj.data
        info["mesh"] = {
            "vertices": len(mesh.vertices),
            "edges": len(mesh.edges),
            "polygons": len(mesh.polygons),
            "has_uv_layers": len(mesh.uv_layers) > 0,
            "uv_layers": [layer.name for layer in mesh.uv_layers],
        }

        # Bounding box in world space
        bbox_corners = [obj.matrix_world @ Vector(corner) for corner in obj.bound_box]
        min_corner = Vector((
            min(c.x for c in bbox_corners),
            min(c.y for c in bbox_corners),
            min(c.z for c in bbox_corners)
        ))
        max_corner = Vector((
            max(c.x for c in bbox_corners),
            max(c.y for c in bbox_corners),
            max(c.z for c in bbox_corners)
        ))
        info["world_bounding_box"] = {
            "min": list(min_corner),
            "max": list(max_corner),
            "center": list((min_corner + max_corner) / 2),
            "size": list(max_corner - min_corner)
        }

    # Material information
    if obj.type == 'MESH' and obj.data.materials:
        materials = []
        for mat in obj.data.materials:
            if mat:
                mat_info = {
                    "name": mat.name,
                    "use_nodes": mat.use_nodes,
                }
                if mat.use_nodes and mat.node_tree:
                    # Get principled BSDF node if exists
                    for node in mat.node_tree.nodes:
                        if node.type == 'BSDF_PRINCIPLED':
                            mat_info["base_color"] = list(node.inputs['Base Color'].default_value)
                            mat_info["metallic"] = node.inputs['Metallic'].default_value
                            mat_info["roughness"] = node.inputs['Roughness'].default_value
                            break
                materials.append(mat_info)
        info["materials"] = materials

    # Light-specific information
    if obj.type == 'LIGHT' and obj.data:
        light = obj.data
        info["light"] = {
            "type": light.type,
            "color": list(light.color),
            "energy": light.energy,
        }
        if light.type in ('POINT', 'SPOT'):
            info["light"]["shadow_soft_size"] = light.shadow_soft_size
        if light.type == 'SPOT':
            info["light"]["spot_size"] = light.spot_size
            info["light"]["spot_blend"] = light.spot_blend

    # Camera-specific information
    if obj.type == 'CAMERA' and obj.data:
        cam = obj.data
        info["camera"] = {
            "type": cam.type,
            "lens": cam.lens,
            "sensor_width": cam.sensor_width,
            "clip_start": cam.clip_start,
            "clip_end": cam.clip_end,
        }
        if cam.type == 'ORTHO':
            info["camera"]["ortho_scale"] = cam.ortho_scale

    # Modifiers
    if obj.modifiers:
        info["modifiers"] = [{"name": mod.name, "type": mod.type} for mod in obj.modifiers]

    # Constraints
    if obj.constraints:
        info["constraints"] = [{"name": con.name, "type": con.type} for con in obj.constraints]

    return info


def get_viewport_screenshot(params):
    """Capture a screenshot of the 3D viewport."""
    max_size = params.get("max_size", 800)

    # Find a 3D viewport area
    viewport_area = None
    for area in bpy.context.screen.areas:
        if area.type == 'VIEW_3D':
            viewport_area = area
            break

    if not viewport_area:
        return {"error": "No 3D viewport found"}

    # Create temporary file for screenshot
    temp_dir = tempfile.gettempdir()
    temp_path = os.path.join(temp_dir, "blender_mcp_screenshot.png")

    # Store original settings
    original_format = bpy.context.scene.render.image_settings.file_format
    original_color_mode = bpy.context.scene.render.image_settings.color_mode

    try:
        # Set up render settings for screenshot
        bpy.context.scene.render.image_settings.file_format = 'PNG'
        bpy.context.scene.render.image_settings.color_mode = 'RGBA'

        # Override context for viewport render
        with bpy.context.temp_override(area=viewport_area):
            # Use OpenGL render
            bpy.ops.render.opengl(write_still=False)

        # Get the rendered image
        image = bpy.data.images.get('Render Result')
        if image:
            image.save_render(temp_path)

            # Read and encode as base64
            with open(temp_path, 'rb') as f:
                image_data = f.read()

            # Clean up temp file
            os.remove(temp_path)

            return {
                "image_base64": base64.b64encode(image_data).decode('utf-8'),
                "format": "png",
                "path": temp_path
            }
        else:
            return {"error": "Failed to capture viewport"}

    except Exception as e:
        return {"error": f"Screenshot failed: {str(e)}"}

    finally:
        # Restore original settings
        bpy.context.scene.render.image_settings.file_format = original_format
        bpy.context.scene.render.image_settings.color_mode = original_color_mode


def execute_code(params):
    """Execute arbitrary Python code in Blender."""
    code = params.get("code", "")

    if not code:
        return {"error": "code parameter required"}

    # Create a namespace for code execution
    exec_globals = {
        "bpy": bpy,
        "__builtins__": __builtins__,
    }
    exec_locals = {}

    try:
        # Execute the code
        exec(code, exec_globals, exec_locals)

        # Try to get a result if one was defined
        result = exec_locals.get("result", None)

        return {
            "executed": True,
            "result": result,
            "locals": {k: str(v) for k, v in exec_locals.items() if not k.startswith('_')}
        }

    except Exception as e:
        import traceback
        return {
            "executed": False,
            "error": str(e),
            "traceback": traceback.format_exc()
        }


def get_telemetry_consent(params):
    """Get the user's telemetry consent status."""
    try:
        scene = bpy.context.scene
        if hasattr(scene, 'mcp_chat'):
            return {"consent": scene.mcp_chat.telemetry_consent}
        return {"consent": False}
    except Exception:
        return {"consent": False}


def get_available_commands(params):
    """Get a list of all available commands."""
    return {
        "commands": [
            {
                "name": "get_scene_info",
                "description": "Get detailed information about the current Blender scene",
                "params": []
            },
            {
                "name": "get_object_info",
                "description": "Get detailed information about a specific object",
                "params": [{"name": "object_name", "type": "string", "required": True}]
            },
            {
                "name": "get_viewport_screenshot",
                "description": "Capture a screenshot of the 3D viewport",
                "params": [{"name": "max_size", "type": "int", "required": False, "default": 800}]
            },
            {
                "name": "execute_code",
                "description": "Execute arbitrary Python code in Blender",
                "params": [{"name": "code", "type": "string", "required": True}]
            },
            {
                "name": "get_telemetry_consent",
                "description": "Get the user's telemetry consent status",
                "params": []
            },
            {
                "name": "get_polyhaven_status",
                "description": "Check if PolyHaven integration is enabled",
                "params": []
            },
            {
                "name": "get_sketchfab_status",
                "description": "Check if Sketchfab integration is enabled",
                "params": []
            },
            {
                "name": "get_hyper3d_status",
                "description": "Check if Hyper3D Rodin integration is enabled",
                "params": []
            },
            {
                "name": "get_hunyuan3d_status",
                "description": "Check if Hunyuan3D integration is enabled",
                "params": []
            },
        ]
    }


def add_chat_message(params):
    """Add a message to the chat history."""
    import time

    role = params.get("role", "assistant")
    content = params.get("content", "")

    if not content:
        return {"error": "content parameter required"}

    try:
        scene = bpy.context.scene
        msg = scene.mcp_chat_messages.add()
        msg.role = role
        msg.content = content
        msg.timestamp = time.time()
        scene.mcp_chat.chat_message_index = len(scene.mcp_chat_messages) - 1

        return {"success": True, "message_count": len(scene.mcp_chat_messages)}
    except Exception as e:
        return {"error": str(e)}


def get_chat_history(params):
    """Get the chat message history."""
    try:
        scene = bpy.context.scene
        messages = []
        for msg in scene.mcp_chat_messages:
            messages.append({
                "role": msg.role,
                "content": msg.content,
                "timestamp": msg.timestamp
            })
        return {"messages": messages}
    except Exception as e:
        return {"error": str(e)}


# Handler registry - maps command types to handler functions
HANDLERS = {
    "get_scene_info": get_scene_info,
    "get_object_info": get_object_info,
    "get_viewport_screenshot": get_viewport_screenshot,
    "execute_code": execute_code,
    "get_telemetry_consent": get_telemetry_consent,
    "get_available_commands": get_available_commands,
    "add_chat_message": add_chat_message,
    "get_chat_history": get_chat_history,
}


def register_handlers(server):
    """Register all command handlers with the server."""
    for cmd_type, handler in HANDLERS.items():
        server.register_handler(cmd_type, handler)


def register():
    """Register the handlers module."""
    pass


def unregister():
    """Unregister the handlers module."""
    pass
