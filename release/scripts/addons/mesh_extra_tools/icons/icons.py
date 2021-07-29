import os
import bpy
import bpy.utils.previews

mesh_check_icon_collections = {}
mesh_check_icons_loaded = False


def load_icons():
    global mesh_check_icon_collections
    global mesh_check_icons_loaded

    if mesh_check_icons_loaded:
        return mesh_check_icon_collections["main"]

    custom_icons = bpy.utils.previews.new()

    icons_dir = os.path.join(os.path.dirname(__file__))

    custom_icons.load("ngons", os.path.join(icons_dir, "ngon.png"), 'IMAGE')
    custom_icons.load("triangles", os.path.join(icons_dir, "triangle.png"), 'IMAGE')

    mesh_check_icon_collections["main"] = custom_icons
    mesh_check_icons_loaded = True

    return mesh_check_icon_collections["main"]


def clear_icons():
    global mesh_check_icons_loaded
    for icon in mesh_check_icon_collections.values():
        bpy.utils.previews.remove(icon)
    mesh_check_icon_collections.clear()
    mesh_check_icons_loaded = False
