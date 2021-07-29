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

import bpy
import bmesh
from bpy.props import StringProperty
from mathutils import Vector
from . import muv_common


class MUV_PreserveUVAspect(bpy.types.Operator):
    """
    Operation class: Preserve UV Aspect
    """

    bl_idname = "uv.muv_preserve_uv_aspect"
    bl_label = "Preserve UV Aspect"
    bl_description = "Choose Image"
    bl_options = {'REGISTER', 'UNDO'}

    dest_img_name = StringProperty(options={'HIDDEN'})

    @classmethod
    def poll(cls, context):
        obj = context.active_object
        return obj and obj.type == 'MESH'

    def execute(self, context):
        # Note: the current system only works if the
        # f[tex_layer].image doesn't return None
        # which will happen in certain cases
        obj = context.active_object
        bm = bmesh.from_edit_mesh(obj.data)

        if muv_common.check_version(2, 73, 0) >= 0:
            bm.faces.ensure_lookup_table()

        if not bm.loops.layers.uv:
            self.report({'WARNING'}, "Object must have more than one UV map")
            return {'CANCELLED'}

        uv_layer = bm.loops.layers.uv.verify()
        tex_layer = bm.faces.layers.tex.verify()

        sel_faces = [f for f in bm.faces if f.select]
        dest_img = bpy.data.images[self.dest_img_name]

        info = {}

        for f in sel_faces:
            if not f[tex_layer].image in info.keys():
                info[f[tex_layer].image] = {}
                info[f[tex_layer].image]['faces'] = []
            info[f[tex_layer].image]['faces'].append(f)

        for img in info:
            if img is None:
                continue

            src_img = img
            ratio = Vector((
                dest_img.size[0] / src_img.size[0],
                dest_img.size[1] / src_img.size[1]))
            origin = Vector((100000.0, 100000.0))
            for f in info[img]['faces']:
                for l in f.loops:
                    uv = l[uv_layer].uv
                    origin.x = min(uv.x, origin.x)
                    origin.y = min(uv.y, origin.y)
            info[img]['ratio'] = ratio
            info[img]['origin'] = origin

        for img in info:
            if img is None:
                continue

            for f in info[img]['faces']:
                f[tex_layer].image = dest_img
                for l in f.loops:
                    uv = l[uv_layer].uv
                    diff = uv - info[img]['origin']
                    diff.x = diff.x / info[img]['ratio'].x
                    diff.y = diff.y / info[img]['ratio'].y
                    uv.x = origin.x + diff.x
                    uv.y = origin.y + diff.y

        bmesh.update_edit_mesh(obj.data)

        return {'FINISHED'}


class MUV_PreserveUVAspectMenu(bpy.types.Menu):
    """
    Menu class: Preserve UV Aspect
    """

    bl_idname = "uv.muv_preserve_uv_aspect_menu"
    bl_label = "Preserve UV Aspect"
    bl_description = "Preserve UV Aspect"

    def draw(self, _):
        layout = self.layout

        # create sub menu
        for key in bpy.data.images.keys():
            layout.operator(
                MUV_PreserveUVAspect.bl_idname,
                text=key, icon="IMAGE_COL").dest_img_name = key
