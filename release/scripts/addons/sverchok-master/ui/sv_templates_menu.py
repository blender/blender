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


import bpy
import os
from sverchok.utils.sv_update_utils import sv_get_local_path

sv_path = os.path.dirname(sv_get_local_path()[0])

# Node Templates Menu
class SV_MT_layouts_templates(bpy.types.Menu):
    bl_idname = 'SV_MT_layouts_templates'
    bl_space_type = 'NODE_EDITOR'
    bl_label = "Templates"
    bl_description = "List of Sverchok Templates"


    def avail_templates():
        fullpath = [sv_path, "json_examples"]
        templates_path = os.path.join(*fullpath)
        items = [[os.path.join(templates_path,t),t] for t in next(os.walk(templates_path))[2]]
        items = [item for item in items if not item[0].endswith(".zip")]
        items.sort()
        return items

    sv_templates = avail_templates()

    @classmethod
    def poll(cls, context):
        try:
            return context.space_data.node_tree.bl_idname == 'SverchCustomTreeType' and context.scene.node_tree
        except:
            return False

    def draw(self, context):
        layout = self.layout
        ntree = context.space_data.node_tree
        if not ntree:
            ntree = lambda: None
            ntree.name = '____make_new____'

        for svt in self.sv_templates:
            a = layout.operator(
                'node.tree_importer_silent',
                text=str(svt[1]),
                icon='RNA')
            a.id_tree = ntree.name
            a.filepath = svt[0]


def node_templates_pulldown(self, context):
    if context.space_data.tree_type == 'SverchCustomTreeType':
        layout = self.layout
        row = layout.row(align=True)
        row.scale_x = 1.3
        row.menu("SV_MT_layouts_templates",
                 icon="RNA")

from sverchok.ui import sv_panels




def register():
    bpy.utils.register_class(SV_MT_layouts_templates)
    bpy.types.NODE_HT_header.append(node_templates_pulldown)


def unregister():
    bpy.types.NODE_HT_header.remove(node_templates_pulldown)
    bpy.utils.unregister_class(SV_MT_layouts_templates)

if __name__ == '__main__':
    register()
