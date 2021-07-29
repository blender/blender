# -*- coding:utf-8 -*-

# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110- 1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>

# ----------------------------------------------------------
# Author: Stephen Leger (s-leger)
# Inspired by Asset-Flinguer
# ----------------------------------------------------------
import sys
from mathutils import Vector
import bpy


def log(s):
    print("[log]" + s)


def generateThumb(context, cls, preset):
    log("### RENDER THUMB ############################")
    log("Start generating: " + cls)

    # engine settings
    context.scene.render.engine = 'CYCLES'
    render = context.scene.cycles
    render.progressive = 'PATH'
    render.samples = 24
    try:
        render.use_square_samples = True
    except:
        pass
    render.preview_samples = 24
    render.aa_samples = 24
    render.transparent_max_bounces = 8
    render.transparent_min_bounces = 8
    render.transmission_bounces = 8
    render.max_bounces = 8
    render.min_bounces = 6
    render.caustics_refractive = False
    render.caustics_reflective = False
    render.use_transparent_shadows = True
    render.diffuse_bounces = 1
    render.glossy_bounces = 4
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete()
    # create object, loading preset
    getattr(bpy.ops.archipack, cls)('INVOKE_DEFAULT', filepath=preset, auto_manipulate=False)

    o = context.active_object
    size = o.dimensions
    x, y, z = o.bound_box[0]
    min_x = x
    min_y = y
    min_z = z
    x, y, z = o.bound_box[6]
    max_x = x
    max_y = y
    max_z = z
    center = Vector((
        min_x + 0.5 * (max_x - min_x),
        min_y + 0.5 * (max_y - min_y),
        min_z + 0.5 * (max_z - min_z)))

    # oposite / tan (0.5 * fov)  where fov is 49.134 deg
    dist = max(size) / 0.32
    loc = center + dist * Vector((0.5, -1, 0.5)).normalized()

    log("Prepare camera")
    bpy.ops.object.camera_add(view_align=True,
        enter_editmode=False,
        location=loc,
        rotation=(1.150952, 0.0, 0.462509))
    cam = context.active_object
    cam.data.lens = 50
    cam.select = True
    context.scene.camera = cam

    bpy.ops.object.select_all(action="DESELECT")
    o.select = True
    bpy.ops.view3d.camera_to_view_selected()

    log("Prepare scene")
    # add plane
    bpy.ops.mesh.primitive_plane_add(
        radius=1000,
        view_align=False,
        enter_editmode=False,
        location=(0, 0, 0)
        )
    p = context.active_object
    m = bpy.data.materials.new("Plane")
    m.use_nodes = True
    m.node_tree.nodes[1].inputs[0].default_value = (1, 1, 1, 1)
    p.data.materials.append(m)

    # add 3 lights
    bpy.ops.object.lamp_add(
        type='POINT',
        radius=1,
        view_align=False,
        location=(3.69736, -7, 6.0))
    l = context.active_object
    l.data.use_nodes = True
    tree = l.data.node_tree
    nodes = l.data.node_tree.nodes
    emit = nodes["Emission"]
    emit.inputs[1].default_value = 2000.0

    bpy.ops.object.lamp_add(
        type='POINT',
        radius=1,
        view_align=False,
        location=(9.414563179016113, 5.446230888366699, 5.903861999511719))
    l = context.active_object
    l.data.use_nodes = True
    tree = l.data.node_tree
    nodes = l.data.node_tree.nodes
    emit = nodes["Emission"]
    falloff = nodes.new(type="ShaderNodeLightFalloff")
    falloff.inputs[0].default_value = 5
    tree.links.new(falloff.outputs[2], emit.inputs[1])

    bpy.ops.object.lamp_add(
        type='POINT',
        radius=1,
        view_align=False,
        location=(-7.847615718841553, 1.03135085105896, 5.903861999511719))
    l = context.active_object
    l.data.use_nodes = True
    tree = l.data.node_tree
    nodes = l.data.node_tree.nodes
    emit = nodes["Emission"]
    falloff = nodes.new(type="ShaderNodeLightFalloff")
    falloff.inputs[0].default_value = 5
    tree.links.new(falloff.outputs[2], emit.inputs[1])

    # Set output filename.
    render = context.scene.render
    render.filepath = preset[:-3] + ".png"
    render.use_file_extension = True
    render.use_overwrite = True
    render.use_compositing = False
    render.use_sequencer = False
    render.resolution_x = 150
    render.resolution_y = 100
    render.resolution_percentage = 100
    # render.image_settings.file_format = 'PNG'
    # render.image_settings.color_mode = 'RGBA'
    # render.image_settings.color_depth = '8'

    # Configure output size.
    log("Render")

    # Render thumbnail
    bpy.ops.render.render(write_still=True)

    log("### COMPLETED ############################")


if __name__ == "__main__":
    preset = ""

    for arg in sys.argv:
        if arg.startswith("cls:"):
            cls = arg[4:]
        if arg.startswith("preset:"):
            preset = arg[7:]
        if arg.startswith("matlib:"):
            matlib = arg[7:]
        if arg.startswith("addon:"):
            module = arg[6:]
    try:
        bpy.ops.wm.addon_enable(module=module)
        bpy.context.user_preferences.addons[module].preferences.matlib_path = matlib
    except:
        raise RuntimeError("module name not found")
    generateThumb(bpy.context, cls, preset)
