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
from bpy.types import Panel

from sverchok.utils.monad import SvTreePathParent

def sv_back_to_parent(self, context):
    """
    Draw the back to parent operator in node view header
    """
    op_poll = SvTreePathParent.poll
    if op_poll(context):
        layout = self.layout
        layout.operator("node.sv_tree_path_parent", text='sv parent', icon='FILE_PARENT')


def set_multiple_attrs(cls_ref, **kwargs):
    for arg_name, value in kwargs.items():
        setattr(cls_ref, arg_name, value)


class SvCustomGroupInterface(Panel):
    bl_idname = "SvCustomGroupInterface"
    bl_label = "Sv Custom Group Interface"
    bl_space_type = 'NODE_EDITOR'
    bl_region_type = 'UI'
    bl_category = 'Sverchok'
    use_pin = True

    @classmethod
    def poll(cls, context):
        try:
            if context.space_data.edit_tree.bl_idname == 'SverchGroupTreeType':
                return not context.space_data.edit_tree.library
        except:
            return False

    def draw(self, context):
        monad = context.space_data.edit_tree

        layout = self.layout
        row = layout.row()

        # draw left and right columns corresponding to sockets_types, display_name, move_operator
        in_node = monad.input_node
        out_node = monad.output_node

        if not (in_node and out_node):
            return

        width = context.region.width
        # should ideally take dpi into account,
        if width > 310:
            row = layout.row()
            split = row.split(percentage=0.5)
            column1 = split.box().column()
            split = split.split()
            column2 = split.box().column()
        else:
            column1 = layout.row().box().column()
            layout.separator()
            column2 = layout.row().box().column()

        move = 'node.sverchok_move_socket_exp'
        rename = 'node.sverchok_rename_socket_exp'
        edit = 'node.sverchok_edit_socket_exp'

        def draw_socket_row(_column, s, index):
            if s.bl_idname == 'SvDummySocket':
                return

            # < type | (re)name     | /\  \/  X >

            # lots of repetition here...
            socket_ref = dict(pos=index, node_name=s.node.name)

            r = _column.row(align=True)
            r.template_node_socket(color=s.draw_color(s.node, context))

            m = r.operator(edit, icon='PLUGIN', text='')
            set_multiple_attrs(m, **socket_ref)

            m = r.operator(rename, text=s.name)
            set_multiple_attrs(m, **socket_ref)

            m = r.operator(move, icon='TRIA_UP', text='')
            set_multiple_attrs(m, direction=-1, **socket_ref)

            m = r.operator(move, icon='TRIA_DOWN', text='')
            set_multiple_attrs(m, direction=1, **socket_ref)

            m = r.operator(move, icon='X', text='')
            set_multiple_attrs(m, direction=0, **socket_ref)


        column1.label('inputs')
        for i, s in enumerate(in_node.outputs):
            draw_socket_row(column1, s, i)

        column2.label('outputs')
        for i, s in enumerate(out_node.inputs):
            draw_socket_row(column2, s, i)

      
        if len(monad.instances) == 1:
            origin_node = monad.instances[0]
            layout.separator()
            layout.label('Monad UI:')
            box = layout.column().box()
            origin_node.draw_buttons(None, box)



def register():
    bpy.types.NODE_HT_header.prepend(sv_back_to_parent)
    bpy.utils.register_class(SvCustomGroupInterface)


def unregister():
    bpy.utils.unregister_class(SvCustomGroupInterface)
    bpy.types.NODE_HT_header.remove(sv_back_to_parent)
