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

from __future__ import annotations

from bpy.types import Operator

class SPREADSHEET_OT_toggle_pin(Operator):
    '''Turn on or off pinning'''
    bl_idname = "spreadsheet.toggle_pin"
    bl_label = "Toggle Pin"
    bl_options = {'REGISTER'}

    @classmethod
    def poll(cls, context):
        space = context.space_data
        return space and space.type == 'SPREADSHEET'

    def execute(self, context):
        space = context.space_data

        if space.is_pinned:
            self.unpin(context)
        else:
            self.pin(context)
        return {'FINISHED'}

    def pin(self, context):
        space = context.space_data
        space.is_pinned = True

    def unpin(self, context):
        space = context.space_data
        space.is_pinned = False

        space.context_path.clear()

        # Try to find a node with an active preview in any open editor.
        if space.object_eval_state == 'EVALUATED':
            node_editors = self.find_geometry_node_editors(context)
            for node_editor in node_editors:
                ntree = node_editor.edit_tree
                for node in ntree.nodes:
                    if node.active_preview:
                        space.set_geometry_node_context(node_editor, node)
                        return

    def find_geometry_node_editors(self, context):
        editors = []
        for window in context.window_manager.windows:
            for area in window.screen.areas:
                space = area.spaces.active
                if space.type != 'NODE_EDITOR':
                    continue
                if space.edit_tree is None:
                    continue
                if space.edit_tree.type == 'GEOMETRY':
                    editors.append(space)
        return editors


classes = (
    SPREADSHEET_OT_toggle_pin,
)

if __name__ == "__main__":  # Only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
