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

# By CoDEmanX
# updated by lijenstina

import bpy
import bmesh
from bpy.types import Panel
import time

# Define Globals
STORE_COUNT = (0, 0, 0)  # Store the previous count
TIMER_STORE = 1          # Store the time.time floats


def check_the_obj_polycount(context, delay=0.0):
    global STORE_COUNT
    global TIMER_STORE

    info_str = ""
    tris = quads = ngons = 0
    try:
        # it's weak sauce but this will in certain cases run many times a second
        if TIMER_STORE == 1 or delay == 0 or time.time() > TIMER_STORE + delay:
            ob = context.active_object
            if ob.mode == 'EDIT':
                me = ob.data
                bm = bmesh.from_edit_mesh(me)
                for f in bm.faces:
                    v = len(f.verts)
                    if v == 3:
                        tris += 1
                    elif v == 4:
                        quads += 1
                    else:
                        ngons += 1
                bmesh.update_edit_mesh(me)
            else:
                for p in ob.data.polygons:
                    count = p.loop_total
                    if count == 3:
                        tris += 1
                    elif count == 4:
                        quads += 1
                    else:
                        ngons += 1
            STORE_COUNT = (ngons, quads, tris)
            info_str = "  Ngons: %i  Quads: %i  Tris: %i" % (ngons, quads, tris)
            TIMER_STORE = time.time()
        else:
            info_str = "  Ngons: %i  Quads: %i  Tris: %i" % STORE_COUNT
    except:
        info_str = "  Polygon info could not be retrieved"

    return info_str


class DATA_PT_info_panel(Panel):
    """Creates a face info / select panel in the Object properties window"""
    bl_label = "Face Info / Select"
    bl_idname = "DATA_PT_face_info"
    bl_space_type = "PROPERTIES"
    bl_region_type = "WINDOW"
    bl_context = "data"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(self, context):
        return (context.active_object is not None and
                context.active_object.type == 'MESH')

    def draw(self, context):
        layout = self.layout
        mesh_extra_tools = context.scene.mesh_extra_tools
        check_used = mesh_extra_tools.mesh_info_show
        check_delay = mesh_extra_tools.mesh_info_delay
        info_str = ""

        box = layout.box()
        col = box.column()
        split = col.split(percentage=0.6 if check_used else 0.75, align=True)
        split.prop(mesh_extra_tools, "mesh_info_show", toggle=True)
        split.prop(mesh_extra_tools, "mesh_info_delay")

        if check_used:
            info_str = check_the_obj_polycount(context, check_delay)
            col.label(info_str, icon='MESH_DATA')

        col = layout.column()
        col.label("Select faces by type:")

        row = layout.row()
        row.operator("data.facetype_select", text="Ngons").face_type = "5"
        row.operator("data.facetype_select", text="Quads").face_type = "4"
        row.operator("data.facetype_select", text="Tris").face_type = "3"
