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

# ----------------------------------------------------------
# Main panel for different Archimesh general actions
# Author: Antonio Vazquez (antonioya)
#
# ----------------------------------------------------------
# noinspection PyUnresolvedReferences
import bpy
# noinspection PyUnresolvedReferences
import bgl
from bpy.types import Operator, Panel, SpaceView3D
from math import sqrt, fabs, pi, asin
from .achm_tools import *
from .achm_gltools import *


# -----------------------------------------------------
# Verify if boolean already exist
# -----------------------------------------------------
def isboolean(myobject, childobject):
    flag = False
    for mod in myobject.modifiers:
        if mod.type == 'BOOLEAN':
            if mod.object == childobject:
                flag = True
                break
    return flag


# ------------------------------------------------------
# Button: Action to link windows and doors
# ------------------------------------------------------
class AchmHoleAction(Operator):
    bl_idname = "object.archimesh_cut_holes"
    bl_label = "Auto Holes"
    bl_description = "Enable windows and doors holes for any selected object (needs wall thickness)"
    bl_category = 'Archimesh'

    # ------------------------------
    # Execute
    # ------------------------------
    # noinspection PyMethodMayBeStatic
    def execute(self, context):
        scene = context.scene
        listobj = []
        # ---------------------------------------------------------------------
        # Save the list of selected objects because the select flag is missed
        # only can be windows or doors
        # ---------------------------------------------------------------------
        for obj in bpy.context.scene.objects:
            # noinspection PyBroadException
            try:
                if obj["archimesh.hole_enable"]:
                    if obj.select is True or scene.archimesh_select_only is False:
                        listobj.extend([obj])
            except:
                continue
        # ---------------------------
        # Get the baseboard object
        # ---------------------------
        mybaseboard = None
        for child in context.object.children:
            # noinspection PyBroadException
            try:
                if child["archimesh.room_baseboard"]:
                    mybaseboard = child
            except:
                continue
        # ---------------------------
        # Get the shell object
        # ---------------------------
        myshell = None
        for child in context.object.children:
            # noinspection PyBroadException
            try:
                if child["archimesh.room_shell"]:
                    myshell = child
            except:
                continue

        # -----------------------------
        # Remove all empty Boolean modifiers
        # -----------------------------
        for mod in context.object.modifiers:
            if mod.type == 'BOOLEAN':
                if mod.object is None:
                    bpy.ops.object.modifier_remove(modifier=mod.name)

        # if thickness is 0, must be > 0
        myroom = context.object
        if myroom.RoomGenerator[0].wall_width == 0:
            self.report({'WARNING'}, "Walls must have thickness for using autohole function. Change it and run again")
        # -----------------------------
        # Now apply Wall holes
        # -----------------------------
        for obj in listobj:
            parentobj = context.object
            # Parent the empty to the room (the parent of frame)
            if obj.parent is not None:
                bpy.ops.object.select_all(action='DESELECT')
                parentobj.select = True
                obj.parent.select = True  # parent of object
                bpy.ops.object.parent_set(type='OBJECT', keep_transform=False)
            # ---------------------------------------
            # Add the modifier to controller
            # and the scale to use the same thickness
            # ---------------------------------------
            for child in obj.parent.children:
                # noinspection PyBroadException
                try:
                    if child["archimesh.ctrl_hole"]:
                        # apply scale
                        t = parentobj.RoomGenerator[0].wall_width
                        if t > 0:
                            child.scale.y = (t + 0.45) / (child.dimensions.y / child.scale.y)  # Add some gap
                        else:
                            child.scale.y = 1
                        # add boolean modifier
                        if isboolean(myroom, child) is False:
                            set_modifier_boolean(myroom, child)
                except:
                    # print("Unexpected error:" + str(sys.exc_info()))
                    pass

        # ---------------------------------------
        # Now add the modifiers to baseboard
        # ---------------------------------------
        if mybaseboard is not None:
            for obj in bpy.context.scene.objects:
                # noinspection PyBroadException
                try:
                    if obj["archimesh.ctrl_base"]:
                        if obj.select is True or scene.archimesh_select_only is False:
                            # add boolean modifier
                            if isboolean(mybaseboard, obj) is False:
                                set_modifier_boolean(mybaseboard, obj)
                except:
                    pass

        # ---------------------------------------
        # Now add the modifiers to shell
        # ---------------------------------------
        if myshell is not None:
            # Remove all empty Boolean modifiers
            for mod in myshell.modifiers:
                if mod.type == 'BOOLEAN':
                    if mod.object is None:
                        bpy.ops.object.modifier_remove(modifier=mod.name)

            for obj in bpy.context.scene.objects:
                # noinspection PyBroadException
                try:
                    if obj["archimesh.ctrl_hole"]:
                        if obj.select is True or scene.archimesh_select_only is False:
                            # add boolean modifier
                            if isboolean(myshell, obj) is False:
                                set_modifier_boolean(myshell, obj)
                except:
                    pass

        return {'FINISHED'}


# ------------------------------------------------------
# Button: Action to create room from grease pencil
# ------------------------------------------------------
class AchmPencilAction(Operator):
    bl_idname = "object.archimesh_pencil_room"
    bl_label = "Room from Draw"
    bl_description = "Create a room base on grease pencil strokes (draw from top view (7 key))"
    bl_category = 'Archimesh'

    # ------------------------------
    # Execute
    # ------------------------------
    def execute(self, context):
        # Enable for debugging code
        debugmode = False

        scene = context.scene
        mypoints = None
        clearangles = None

        if debugmode is True:
            print("======================================================================")
            print("==                                                                  ==")
            print("==  Grease pencil strokes analysis                                  ==")
            print("==                                                                  ==")
            print("======================================================================")

        # -----------------------------------
        # Get grease pencil points
        # -----------------------------------
        # noinspection PyBroadException
        try:

            # noinspection PyBroadException
            try:
                pencil = bpy.context.object.grease_pencil.layers.active
            except:
                pencil = bpy.context.scene.grease_pencil.layers.active

            if pencil.active_frame is not None:
                for i, stroke in enumerate(pencil.active_frame.strokes):
                    stroke_points = pencil.active_frame.strokes[i].points
                    allpoints = [(point.co.x, point.co.y)
                                 for point in stroke_points]

                    mypoints = []
                    idx = 0
                    x = 0
                    y = 0
                    orientation = None
                    old_orientation = None

                    for point in allpoints:
                        if idx == 0:
                            x = point[0]
                            y = point[1]
                        else:
                            abs_x = abs(point[0] - x)
                            abs_y = abs(point[1] - y)

                            if abs_y > abs_x:
                                orientation = "V"
                            else:
                                orientation = "H"

                            if old_orientation == orientation:
                                x = point[0]
                                y = point[1]
                            else:
                                mypoints.extend([(x, y)])
                                x = point[0]
                                y = point[1]
                                old_orientation = orientation

                        idx += 1
                    # Last point
                    mypoints.extend([(x, y)])

                    if debugmode is True:
                        print("\nPoints\n====================")
                        i = 0
                        for p in mypoints:
                            print(str(i) + ":" + str(p))
                            i += 1
                    # -----------------------------------
                    # Calculate distance between points
                    # -----------------------------------
                    if debugmode is True:
                        print("\nDistance\n====================")
                    i = len(mypoints)
                    distlist = []
                    for e in range(1, i):
                        d = sqrt(
                            ((mypoints[e][0] - mypoints[e - 1][0]) ** 2) + ((mypoints[e][1] - mypoints[e - 1][1]) ** 2))
                        # Imperial units if needed
                        if bpy.context.scene.unit_settings.system == "IMPERIAL":
                            d *= 3.2808399

                        distlist.extend([d])

                        if debugmode is True:
                            print(str(e - 1) + ":" + str(d))
                    # -----------------------------------
                    # Calculate angle of walls
                    # clamped to right angles
                    # -----------------------------------
                    if debugmode is True:
                        print("\nAngle\n====================")

                    i = len(mypoints)
                    anglelist = []
                    for e in range(1, i):
                        sinv = (mypoints[e][1] - mypoints[e - 1][1]) / sqrt(
                            ((mypoints[e][0] - mypoints[e - 1][0]) ** 2) + ((mypoints[e][1] - mypoints[e - 1][1]) ** 2))
                        a = asin(sinv)
                        # Clamp to 90 or 0 degrees
                        if fabs(a) > pi / 4:
                            b = pi / 2
                        else:
                            b = 0

                        anglelist.extend([b])
                        # Reverse de distance using angles (inverse angle to axis) for Vertical lines
                        if a < 0.0 and b != 0:
                            distlist[e - 1] *= -1  # reverse distance

                        # Reverse de distance for horizontal lines
                        if b == 0:
                            if mypoints[e - 1][0] > mypoints[e][0]:
                                distlist[e - 1] *= -1  # reverse distance

                        if debugmode is True:
                            print(str(e - 1) + ":" + str((a * 180) / pi) + "...:" + str(
                                (b * 180) / pi) + "--->" + str(distlist[e - 1]))

                    # ---------------------------------------
                    # Verify duplications and reduce noise
                    # ---------------------------------------
                    if len(anglelist) >= 1:
                        clearangles = []
                        cleardistan = []
                        i = len(anglelist)
                        oldangle = anglelist[0]
                        olddist = 0
                        for e in range(0, i):
                            if oldangle != anglelist[e]:
                                clearangles.extend([oldangle])
                                cleardistan.extend([olddist])
                                oldangle = anglelist[e]
                                olddist = distlist[e]
                            else:
                                olddist += distlist[e]
                        # last
                        clearangles.extend([oldangle])
                        cleardistan.extend([olddist])

            # ----------------------------
            # Create the room
            # ----------------------------
            if len(mypoints) > 1 and len(clearangles) > 0:
                # Move cursor
                bpy.context.scene.cursor_location.x = mypoints[0][0]
                bpy.context.scene.cursor_location.y = mypoints[0][1]
                bpy.context.scene.cursor_location.z = 0  # always on grid floor

                # Add room mesh
                bpy.ops.mesh.archimesh_room()
                myroom = context.object
                mydata = myroom.RoomGenerator[0]
                # Number of walls
                mydata.wall_num = len(mypoints) - 1
                mydata.ceiling = scene.archimesh_ceiling
                mydata.floor = scene.archimesh_floor
                mydata.merge = scene.archimesh_merge

                i = len(mypoints)
                for e in range(0, i - 1):
                    if clearangles[e] == pi / 2:
                        if cleardistan[e] > 0:
                            mydata.walls[e].w = round(fabs(cleardistan[e]), 2)
                            mydata.walls[e].r = (fabs(clearangles[e]) * 180) / pi  # from radians
                        else:
                            mydata.walls[e].w = round(fabs(cleardistan[e]), 2)
                            mydata.walls[e].r = (fabs(clearangles[e]) * 180 * -1) / pi  # from radians

                    else:
                        mydata.walls[e].w = round(cleardistan[e], 2)
                        mydata.walls[e].r = (fabs(clearangles[e]) * 180) / pi  # from radians

                # Remove Grease pencil
                if pencil is not None:
                    for frame in pencil.frames:
                        pencil.frames.remove(frame)

                self.report({'INFO'}, "Archimesh: Room created from grease pencil strokes")
            else:
                self.report({'WARNING'}, "Archimesh: Not enough grease pencil strokes for creating room.")

            return {'FINISHED'}
        except:
            self.report({'WARNING'}, "Archimesh: No grease pencil strokes. Do strokes in top view before creating room")
            return {'CANCELLED'}


# ------------------------------------------------------------------
# Define panel class for main functions.
# ------------------------------------------------------------------
class ArchimeshMainPanel(Panel):
    bl_idname = "archimesh_main_panel"
    bl_label = "Archimesh"
    bl_space_type = "VIEW_3D"
    bl_region_type = "TOOLS"
    bl_category = "Create"
    bl_context = "objectmode"

    # ------------------------------
    # Draw UI
    # ------------------------------
    def draw(self, context):
        layout = self.layout
        scene = context.scene

        myobj = context.object
        # -------------------------------------------------------------------------
        # If the selected object didn't be created with the group 'RoomGenerator',
        # this button is not created.
        # -------------------------------------------------------------------------
        # noinspection PyBroadException
        try:
            if 'RoomGenerator' in myobj:
                box = layout.box()
                box.label("Room Tools", icon='MODIFIER')
                row = box.row(align=False)
                row.operator("object.archimesh_cut_holes", icon='GRID')
                row.prop(scene, "archimesh_select_only")

                # Export/Import
                row = box.row(align=False)
                row.operator("io_import.roomdata", text="Import", icon='COPYDOWN')
                row.operator("io_export.roomdata", text="Export", icon='PASTEDOWN')
        except:
            pass

        # -------------------------------------------------------------------------
        # If the selected object isn't a kitchen
        # this button is not created.
        # -------------------------------------------------------------------------
        # noinspection PyBroadException
        try:
            if myobj["archimesh.sku"] is not None:
                box = layout.box()
                box.label("Kitchen Tools", icon='MODIFIER')
                # Export
                row = box.row(align=False)
                row.operator("io_export.kitchen_inventory", text="Export inventory", icon='PASTEDOWN')
        except:
            pass

        # ------------------------------
        # Elements Buttons
        # ------------------------------
        box = layout.box()
        box.label("Elements", icon='GROUP')
        row = box.row()
        row.operator("mesh.archimesh_room")
        row.operator("mesh.archimesh_column")
        row = box.row()
        row.operator("mesh.archimesh_door")
        row = box.row()
        row.operator("mesh.archimesh_window")
        row.operator("mesh.archimesh_winpanel")
        row = box.row()
        row.operator("mesh.archimesh_kitchen")
        row.operator("mesh.archimesh_shelves")
        row = box.row()
        row.operator("mesh.archimesh_stairs")
        row.operator("mesh.archimesh_roof")

        # ------------------------------
        # Prop Buttons
        # ------------------------------
        box = layout.box()
        box.label("Props", icon='LAMP_DATA')
        row = box.row()
        row.operator("mesh.archimesh_books")
        row.operator("mesh.archimesh_lamp")
        row = box.row()
        row.operator("mesh.archimesh_venetian")
        row.operator("mesh.archimesh_roller")
        row = box.row()
        row.operator("mesh.archimesh_japan")

        # ------------------------------
        # OpenGL Buttons
        # ------------------------------
        box = layout.box()
        box.label("Display hints", icon='QUESTION')
        row = box.row()
        if context.window_manager.archimesh_run_opengl is False:
            icon = 'PLAY'
            txt = 'Show'
        else:
            icon = "PAUSE"
            txt = 'Hide'
        row.operator("archimesh.runopenglbutton", text=txt, icon=icon)
        row = box.row()
        row.prop(scene, "archimesh_gl_measure", toggle=True, icon="ALIGN")
        row.prop(scene, "archimesh_gl_name", toggle=True, icon="OUTLINER_OB_FONT")
        row.prop(scene, "archimesh_gl_ghost", icon='GHOST_ENABLED')
        row = box.row()
        row.prop(scene, "archimesh_text_color", text="")
        row.prop(scene, "archimesh_walltext_color", text="")
        row = box.row()
        row.prop(scene, "archimesh_font_size")
        row.prop(scene, "archimesh_wfont_size")
        row = box.row()
        row.prop(scene, "archimesh_hint_space")
        # ------------------------------
        # Grease pencil tools
        # ------------------------------
        box = layout.box()
        box.label("Pencil Tools", icon='MODIFIER')
        row = box.row(align=False)
        row.operator("object.archimesh_pencil_room", icon='GREASEPENCIL')
        row = box.row(align=False)
        row.prop(scene, "archimesh_ceiling")
        row.prop(scene, "archimesh_floor")
        row.prop(scene, "archimesh_merge")


# -------------------------------------------------------------
# Defines button for enable/disable the tip display
#
# -------------------------------------------------------------
class AchmRunHintDisplayButton(Operator):
    bl_idname = "archimesh.runopenglbutton"
    bl_label = "Display hint data manager"
    bl_description = "Display aditional information in the viewport"
    bl_category = 'Archimesh'

    _handle = None  # keep function handler

    # ----------------------------------
    # Enable gl drawing adding handler
    # ----------------------------------
    @staticmethod
    def handle_add(self, context):
        if AchmRunHintDisplayButton._handle is None:
            AchmRunHintDisplayButton._handle = SpaceView3D.draw_handler_add(draw_callback_px, (self, context),
                                                                                      'WINDOW',
                                                                                      'POST_PIXEL')
            context.window_manager.archimesh_run_opengl = True

    # ------------------------------------
    # Disable gl drawing removing handler
    # ------------------------------------
    # noinspection PyUnusedLocal
    @staticmethod
    def handle_remove(self, context):
        if AchmRunHintDisplayButton._handle is not None:
            SpaceView3D.draw_handler_remove(AchmRunHintDisplayButton._handle, 'WINDOW')
        AchmRunHintDisplayButton._handle = None
        context.window_manager.archimesh_run_opengl = False

    # ------------------------------
    # Execute button action
    # ------------------------------
    def execute(self, context):
        if context.area.type == 'VIEW_3D':
            if context.window_manager.archimesh_run_opengl is False:
                self.handle_add(self, context)
                context.area.tag_redraw()
            else:
                self.handle_remove(self, context)
                context.area.tag_redraw()

            return {'FINISHED'}
        else:
            self.report({'WARNING'},
                        "View3D not found, cannot run operator")

        return {'CANCELLED'}


# -------------------------------------------------------------
# Handler for drawing OpenGl
# -------------------------------------------------------------
# noinspection PyUnusedLocal
def draw_callback_px(self, context):
    draw_main(context)
