# <pep8-80 compliant>

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
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

__author__ = "Nutti <nutti.metro@gmail.com>"
__status__ = "production"
__version__ = "4.4"
__date__ = "2 Aug 2017"

from collections import namedtuple

import bpy
import bgl
import bmesh
import mathutils
from bpy_extras import view3d_utils

from . import muv_common


Rect = namedtuple('Rect', 'x0 y0 x1 y1')
Rect2 = namedtuple('Rect2', 'x y width height')


def get_canvas(context, magnitude):
    """
    Get canvas to be renderred texture
    """
    sc = context.scene
    prefs = context.user_preferences.addons["uv_magic_uv"].preferences

    region_w = context.region.width
    region_h = context.region.height
    canvas_w = region_w - prefs.texproj_canvas_padding[0] * 2.0
    canvas_h = region_h - prefs.texproj_canvas_padding[1] * 2.0

    img = bpy.data.images[sc.muv_texproj_tex_image]
    tex_w = img.size[0]
    tex_h = img.size[1]

    center_x = region_w * 0.5
    center_y = region_h * 0.5

    if sc.muv_texproj_adjust_window:
        ratio_x = canvas_w / tex_w
        ratio_y = canvas_h / tex_h
        if sc.muv_texproj_apply_tex_aspect:
            ratio = ratio_y if ratio_x > ratio_y else ratio_x
            len_x = ratio * tex_w
            len_y = ratio * tex_h
        else:
            len_x = canvas_w
            len_y = canvas_h
    else:
        if sc.muv_texproj_apply_tex_aspect:
            len_x = tex_w * magnitude
            len_y = tex_h * magnitude
        else:
            len_x = region_w * magnitude
            len_y = region_h * magnitude

    x0 = int(center_x - len_x * 0.5)
    y0 = int(center_y - len_y * 0.5)
    x1 = int(center_x + len_x * 0.5)
    y1 = int(center_y + len_y * 0.5)

    return Rect(x0, y0, x1, y1)


def rect_to_rect2(rect):
    """
    Convert Rect1 to Rect2
    """

    return Rect2(rect.x0, rect.y0, rect.x1 - rect.x0, rect.y1 - rect.y0)


def region_to_canvas(rg_vec, canvas):
    """
    Convert screen region to canvas
    """

    cv_rect = rect_to_rect2(canvas)
    cv_vec = mathutils.Vector()
    cv_vec.x = (rg_vec.x - cv_rect.x) / cv_rect.width
    cv_vec.y = (rg_vec.y - cv_rect.y) / cv_rect.height

    return cv_vec


class MUV_TexProjRenderer(bpy.types.Operator):
    """
    Operation class: Render selected texture
    No operation (only rendering texture)
    """

    bl_idname = "uv.muv_texproj_renderer"
    bl_description = "Render selected texture"
    bl_label = "Texture renderer"

    __handle = None

    @staticmethod
    def handle_add(obj, context):
        MUV_TexProjRenderer.__handle = bpy.types.SpaceView3D.draw_handler_add(
            MUV_TexProjRenderer.draw_texture,
            (obj, context), 'WINDOW', 'POST_PIXEL')

    @staticmethod
    def handle_remove():
        if MUV_TexProjRenderer.__handle is not None:
            bpy.types.SpaceView3D.draw_handler_remove(
                MUV_TexProjRenderer.__handle, 'WINDOW')
            MUV_TexProjRenderer.__handle = None

    @staticmethod
    def draw_texture(_, context):
        sc = context.scene

        # no textures are selected
        if sc.muv_texproj_tex_image == "None":
            return

        # get texture to be renderred
        img = bpy.data.images[sc.muv_texproj_tex_image]

        # setup rendering region
        rect = get_canvas(context, sc.muv_texproj_tex_magnitude)
        positions = [
            [rect.x0, rect.y0],
            [rect.x0, rect.y1],
            [rect.x1, rect.y1],
            [rect.x1, rect.y0]
        ]
        tex_coords = [
            [0.0, 0.0],
            [0.0, 1.0],
            [1.0, 1.0],
            [1.0, 0.0]
        ]

        # OpenGL configuration
        bgl.glEnable(bgl.GL_BLEND)
        bgl.glEnable(bgl.GL_TEXTURE_2D)
        if img.bindcode:
            bind = img.bindcode[0]
            bgl.glBindTexture(bgl.GL_TEXTURE_2D, bind)
            bgl.glTexParameteri(
                bgl.GL_TEXTURE_2D, bgl.GL_TEXTURE_MIN_FILTER, bgl.GL_LINEAR)
            bgl.glTexParameteri(
                bgl.GL_TEXTURE_2D, bgl.GL_TEXTURE_MAG_FILTER, bgl.GL_LINEAR)
            bgl.glTexEnvi(
                bgl.GL_TEXTURE_ENV, bgl.GL_TEXTURE_ENV_MODE, bgl.GL_MODULATE)

        # render texture
        bgl.glBegin(bgl.GL_QUADS)
        bgl.glColor4f(1.0, 1.0, 1.0, sc.muv_texproj_tex_transparency)
        for (v1, v2), (u, v) in zip(positions, tex_coords):
            bgl.glTexCoord2f(u, v)
            bgl.glVertex2f(v1, v2)
        bgl.glEnd()


class MUV_TexProjStart(bpy.types.Operator):
    """
    Operation class: Start Texture Projection
    """

    bl_idname = "uv.muv_texproj_start"
    bl_label = "Start Texture Projection"
    bl_description = "Start Texture Projection"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        props = context.scene.muv_props.texproj
        if props.running is False:
            MUV_TexProjRenderer.handle_add(self, context)
            props.running = True
        if context.area:
            context.area.tag_redraw()

        return {'FINISHED'}


class MUV_TexProjStop(bpy.types.Operator):
    """
    Operation class: Stop Texture Projection
    """

    bl_idname = "uv.muv_texproj_stop"
    bl_label = "Stop Texture Projection"
    bl_description = "Stop Texture Projection"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        props = context.scene.muv_props.texproj
        if props.running is True:
            MUV_TexProjRenderer.handle_remove()
            props.running = False
        if context.area:
            context.area.tag_redraw()

        return {'FINISHED'}


class MUV_TexProjProject(bpy.types.Operator):
    """
    Operation class: Project texture
    """

    bl_idname = "uv.muv_texproj_project"
    bl_label = "Project Texture"
    bl_description = "Project Texture"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        obj = context.active_object
        return obj is not None and obj.type == "MESH"

    def execute(self, context):
        sc = context.scene

        if context.mode != "EDIT_MESH":
            self.report({'WARNING'}, "Mesh must be in Edit mode")
            return {'CANCELLED'}

        if sc.muv_texproj_tex_image == "None":
            self.report({'WARNING'}, "No textures are selected")
            return {'CANCELLED'}

        _, region, space = muv_common.get_space(
            'VIEW_3D', 'WINDOW', 'VIEW_3D')

        # get faces to be texture projected
        obj = context.active_object
        world_mat = obj.matrix_world
        bm = bmesh.from_edit_mesh(obj.data)
        if muv_common.check_version(2, 73, 0) >= 0:
            bm.faces.ensure_lookup_table()

        # get UV and texture layer
        if not bm.loops.layers.uv:
            self.report({'WARNING'}, "Object must have more than one UV map")
            return {'CANCELLED'}

        uv_layer = bm.loops.layers.uv.verify()
        tex_layer = bm.faces.layers.tex.verify()

        sel_faces = [f for f in bm.faces if f.select]

        # transform 3d space to screen region
        v_screen = [
            view3d_utils.location_3d_to_region_2d(
                region,
                space.region_3d,
                world_mat * l.vert.co)
            for f in sel_faces for l in f.loops
        ]

        # transform screen region to canvas
        v_canvas = [
            region_to_canvas(
                v,
                get_canvas(bpy.context, sc.muv_texproj_tex_magnitude))
            for v in v_screen
        ]

        # project texture to object
        i = 0
        for f in sel_faces:
            f[tex_layer].image = bpy.data.images[sc.muv_texproj_tex_image]
            for l in f.loops:
                l[uv_layer].uv = v_canvas[i].to_2d()
                i = i + 1

        muv_common.redraw_all_areas()
        bmesh.update_edit_mesh(obj.data)

        return {'FINISHED'}


class OBJECT_PT_TP(bpy.types.Panel):
    """
    Panel class: Texture Projection Menu on Property Panel on View3D
    """

    bl_label = "Texture Projection"
    bl_description = "Texture Projection Menu"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_context = 'mesh_edit'

    @classmethod
    def poll(cls, context):
        prefs = context.user_preferences.addons["uv_magic_uv"].preferences
        return prefs.enable_texproj

    def draw_header(self, _):
        layout = self.layout
        layout.label(text="", icon='IMAGE_COL')

    def draw(self, context):
        sc = context.scene
        layout = self.layout
        props = sc.muv_props.texproj
        if props.running is False:
            layout.operator(
                MUV_TexProjStart.bl_idname, text="Start", icon='PLAY')
        else:
            layout.operator(
                MUV_TexProjStop.bl_idname, text="Stop", icon='PAUSE')
            layout.prop(sc, "muv_texproj_tex_image", text="Image")
            layout.prop(
                sc, "muv_texproj_tex_transparency", text="Transparency"
            )
            layout.prop(sc, "muv_texproj_adjust_window", text="Adjust Window")
            if not sc.muv_texproj_adjust_window:
                layout.prop(sc, "muv_texproj_tex_magnitude", text="Magnitude")
            layout.prop(
                sc, "muv_texproj_apply_tex_aspect", text="Texture Aspect Ratio"
            )
            layout.operator(MUV_TexProjProject.bl_idname, text="Project")
