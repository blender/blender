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

from sverchok.node_tree import SverchCustomTreeNode


class Sv3DviewPropsNode(bpy.types.Node, SverchCustomTreeNode):

    ''' Sv 3Dview Props Node '''
    bl_idname = 'Sv3DviewPropsNode'
    bl_label = '3dview Props'
    bl_icon = 'SETTINGS'

    def draw_buttons(self, context, layout):
        context = bpy.context

        idx = -1
        for window in context.window_manager.windows:
            for area in window.screen.areas:
                if area.type == 'VIEW_3D':
                    idx += 1
                    n_panel = area.spaces[0]

                    row = layout.row(align=True)
                    row.label('3dview {idx}:'.format(idx=idx))

                    col = row.column()
                    col.prop(n_panel, 'show_only_render', text='render')

                    col = row.column()
                    col.active = not n_panel.show_only_render
                    col.prop(n_panel, 'show_floor', text='grid')

                    row = layout.row(align=True)
                    row.active = not n_panel.show_only_render
                    row.prop(n_panel, "show_axis_x", text="X", toggle=True)
                    row.prop(n_panel, "show_axis_y", text="Y", toggle=True)
                    row.prop(n_panel, "show_axis_z", text="Z", toggle=True)

        # bpy.context.scene.world?, should check properly
        world = bpy.data.worlds['World']
        prefs = bpy.context.user_preferences
        theme = prefs.themes['Default']

        row = layout.row(align=True)
        box = row.box()
        boxrow = box.row(align=True)
        boxrow.prop(theme.view_3d, 'grid', text='grid')

        gradients = theme.view_3d.space.gradients
        boxrow = box.row(align=True)
        boxrow.prop(gradients, 'show_grad', text='gradient')

        if not gradients.show_grad:
            boxrow.prop(gradients, 'high_gradient', text='background')
        else:
            boxrow.prop(gradients, 'high_gradient', text='')
            boxrow.prop(gradients, 'gradient', text='')

        row = layout.row(align=True)
        row.prop(world, 'horizon_color', text='horizon')

        row = layout.row(align=True)
        row.prop(prefs.inputs, 'view_rotate_method', text='orbit', expand=True)

    def update(self):
        pass

def register():
    bpy.utils.register_class(Sv3DviewPropsNode)


def unregister():
    bpy.utils.unregister_class(Sv3DviewPropsNode)
