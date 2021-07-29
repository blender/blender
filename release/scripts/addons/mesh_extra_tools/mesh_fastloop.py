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

bl_info = {
    "name": "Fast Loop",
    "description": "Add loops fast",
    "author": "Andy Davies (metalliandy)",
    "version": (0, 1, 7),
    "blender": (2, 5, 6),
    "location": "Tool Shelf",
    "warning": "",
    "wiki_url": "",
    "category": "Mesh"
    }

"""
About this script:-
This script enables the fast creation of multiple loops on a mesh

Usage:-
1)Click the FastLoop button on the Tool Shelf to activate the tool
2)Hover over the mesh in the general area where you would like a loop to be added
  (shown by a highlight on the mesh)
3)Click once to confirm the loop placement
4)place the loop and then slide to fine tune its position
5)Repeat 1-4 if needed
6)Press Esc. twice to exit the tool

Related Links:-
http://blenderartists.org/forum/showthread.php?t=206989
http://www.metalliandy.com

Thanks to:-
Bartius Crouch (Crouch) - http://sites.google.com/site/bartiuscrouch/
Dealga McArdle (zeffii) - http://www.digitalaphasia.com

Version history:-
v0.16 - Ammended script for compatibility with recent API changes
v0.15 - Ammended script meta information and button rendering code for
        compatibility with recent API changes
v0.14 - Modal operator
v0.13 - Initial revision
"""

import bpy
from bpy.types import Operator
from bpy.props import BoolProperty


class OBJECT_OT_FastLoop(Operator):
    bl_idname = "object_ot.fastloop"
    bl_label = "FastLoop"
    bl_description = ("Create multiple edge loops in succession\n"
                      "Runs modal until ESC is pressed twice")

    active = BoolProperty(
                name="active",
                default=False
                )

    @classmethod
    def poll(cls, context):
        return bpy.ops.mesh.loopcut_slide.poll()

    def modal(self, context, event):
        if event.type == 'ESC':
            context.area.header_text_set()
            return {'CANCELLED'}

        elif event.type == 'LEFTMOUSE' and event.value == 'RELEASE':
            self.active = False

        if not self.active:
            self.active = True
            bpy.ops.mesh.loopcut_slide('INVOKE_DEFAULT')
            context.area.header_text_set("Press ESC twice to stop FastLoop")

        return {'RUNNING_MODAL'}

    def invoke(self, context, event):
        context.window_manager.modal_handler_add(self)

        return {'RUNNING_MODAL'}


def register():
    bpy.utils.register_module(__name__)
    pass


def unregister():
    bpy.utils.unregister_module(__name__)
    pass


if __name__ == "__main__":
    register()
