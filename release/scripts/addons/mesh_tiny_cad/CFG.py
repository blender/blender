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

# <pep8 compliant>


import os
import bpy

ICONS = 'BIX CCEN V2X VTX XALL E2F'.split(' ')
icon_collection = {}


class TinyCADProperties(bpy.types.PropertyGroup):

    num_verts = bpy.props.IntProperty(
        min=3, max=60, default=12)

    rescale = bpy.props.FloatProperty(
        default=1.0,
        precision=4,
        min=0.0001)


class VIEW3D_MT_edit_mesh_tinycad(bpy.types.Menu):
    bl_label = "TinyCAD"

    @classmethod
    def poll(cls, context):
        return bool(context.object)

    def draw(self, context):

        pcoll = icon_collection["main"]

        def cicon(name):
            return pcoll[name].icon_id

        op = self.layout.operator
        op('tinycad.autovtx', text='VTX | AUTO', icon_value=cicon('VTX'))
        op('tinycad.vertintersect', text='V2X | Vertex at intersection', icon_value=cicon('V2X'))
        op('tinycad.intersectall', text='XALL | Intersect selected edges', icon_value=cicon('XALL'))
        op('tinycad.linetobisect', text='BIX |  Bisector of 2 planar edges', icon_value=cicon('BIX'))
        op('tinycad.circlecenter', text='CCEN | Resurrect circle center', icon_value=cicon('CCEN'))
        op('tinycad.edge_to_face', text='E2F | Extend Edge to Face', icon_value=cicon('E2F'))


def register_icons():
    import bpy.utils.previews
    pcoll = bpy.utils.previews.new()
    icons_dir = os.path.join(os.path.dirname(__file__), "icons")
    for icon_name in ICONS:
        pcoll.load(icon_name, os.path.join(icons_dir, icon_name + '.png'), 'IMAGE')

    icon_collection["main"] = pcoll


def unregister_icons():
    for pcoll in icon_collection.values():
        bpy.utils.previews.remove(pcoll)
    icon_collection.clear()


def register():
    bpy.utils.register_module(__name__)


def unregister():
    bpy.utils.unregister_module(__name__)
