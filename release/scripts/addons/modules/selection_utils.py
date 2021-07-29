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

# <pep8-80 compliant>

# Contributors: Mackraken, Andrew Hale (TrumanBlending)
# Adapted from Mackraken's "Tools for Curves" addon

import bpy

selected = []


class SelectionOrder(bpy.types.Operator):
    """Store the object names in the order they are selected, """ \
    """use RETURN key to confirm selection, ESCAPE key to cancel"""
    bl_idname = "object.select_order"
    bl_label = "Select with Order"
    bl_options = {'UNDO'}

    num_selected = 0

    @classmethod
    def poll(self, context):
        return bpy.context.mode == 'OBJECT'

    def update(self, context):
        # Get the currently selected objects
        sel = context.selected_objects
        num = len(sel)

        if num == 0:
            # Reset the list
            del selected[:]
        elif num > self.num_selected:
            # Get all the newly selected objects and add
            new = [ob.name for ob in sel if ob.name not in selected]
            selected.extend(new)
        elif num < self.num_selected:
            # Get the selected objects and remove from list
            curnames = {ob.name for ob in sel}
            selected[:] = [name for name in selected if name in curnames]

        # Set the number of currently select objects
        self.num_selected = len(selected)

    def modal(self, context, event):
        if event.type == 'RET':
            # If return is pressed, finish the operator
            return {'FINISHED'}
        elif event.type == 'ESC':
            # If escape is pressed, cancel the operator
            return {'CANCELLED'}

        # Update selection if we need to
        self.update(context)
        return {'PASS_THROUGH'}

    def invoke(self, context, event):
        self.update(context)

        context.window_manager.modal_handler_add(self)
        return {'RUNNING_MODAL'}


bpy.utils.register_module(__name__)
