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
# Automatic generation of rooms
# Author: Antonio Vazquez (antonioya) and Eduardo Gutierrez
#
# ----------------------------------------------------------
# noinspection PyUnresolvedReferences
import bpy
from math import sin, cos, fabs, radians
from mathutils import Vector
from datetime import datetime
from time import time
from os import path
from bpy.types import Operator, PropertyGroup, Object, Panel
from bpy.props import StringProperty, FloatProperty, BoolProperty, IntProperty, FloatVectorProperty, \
    CollectionProperty, EnumProperty
from bpy_extras.io_utils import ExportHelper, ImportHelper
from .achm_tools import *


# ----------------------------------------------------------
# Export menu UI
# ----------------------------------------------------------
class AchmExportRoom(Operator, ExportHelper):
    bl_idname = "io_export.roomdata"
    bl_description = 'Export Room data (.dat)'
    bl_category = 'Archimesh'
    bl_label = "Export"

    # From ExportHelper. Filter filenames.
    filename_ext = ".dat"
    filter_glob = StringProperty(
            default="*.dat",
            options={'HIDDEN'},
            )

    filepath = StringProperty(
            name="File Path",
            description="File path used for exporting room data file",
            maxlen=1024, default="",
            )

    # ----------------------------------------------------------
    # Execute
    # ----------------------------------------------------------
    # noinspection PyUnusedLocal
    def execute(self, context):
        print("Exporting:", self.properties.filepath)
        # noinspection PyBroadException
        try:
            myobj = bpy.context.active_object
            mydata = myobj.RoomGenerator[0]

            # -------------------------------
            # extract path and filename
            # -------------------------------
            (filepath, filename) = path.split(self.properties.filepath)
            print('Exporting %s' % filename)
            # -------------------------------
            # Open output file
            # -------------------------------
            realpath = path.realpath(path.expanduser(self.properties.filepath))
            fout = open(realpath, 'w')

            st = datetime.fromtimestamp(time()).strftime('%Y-%m-%d %H:%M:%S')
            fout.write("# Archimesh room export data\n")
            fout.write("# " + st + "\n")
            fout.write("#======================================================\n")

            fout.write("name=" + myobj.name + "\n")
            fout.write("height=" + str(round(mydata.room_height, 3)) + "\n")
            fout.write("thickness=" + str(round(mydata.wall_width, 3)) + "\n")
            fout.write("inverse=" + str(mydata.inverse) + "\n")
            fout.write("ceiling=" + str(mydata.ceiling) + "\n")
            fout.write("floor=" + str(mydata.floor) + "\n")
            fout.write("close=" + str(mydata.merge) + "\n")

            # Walls
            fout.write("#\n# Walls\n#\n")
            fout.write("walls=" + str(mydata.wall_num) + "\n")
            i = 0
            for w in mydata.walls:
                if i < mydata.wall_num:
                    i += 1
                    fout.write("w=" + str(round(w.w, 3)))
                    # if w.a == True: # advance
                    fout.write(",a=" + str(w.a) + ",")
                    fout.write("r=" + str(round(w.r, 1)) + ",")
                    fout.write("h=" + str(w.h) + ",")
                    fout.write("m=" + str(round(w.m, 3)) + ",")
                    fout.write("f=" + str(round(w.f, 3)) + ",")
                    fout.write("c=" + str(w.curved) + ",")
                    fout.write("cf=" + str(round(w.curve_factor, 1)) + ",")
                    fout.write("cd=" + str(round(w.curve_arc_deg, 1)) + ",")
                    fout.write("cs=" + str(w.curve_steps) + "\n")
                    # else:
                    # fOut.write("\n")

            # Baseboard
            fout.write("#\n# Baseboard\n#\n")
            fout.write("baseboard=" + str(mydata.baseboard) + "\n")
            fout.write("baseh=" + str(round(mydata.base_height, 3)) + "\n")
            fout.write("baset=" + str(round(mydata.base_width, 3)) + "\n")
            # Shell
            fout.write("#\n# Wall Cover\n#\n")
            fout.write("shell=" + str(mydata.shell) + "\n")
            fout.write("shellh=" + str(round(mydata.shell_height, 3)) + "\n")
            fout.write("shellt=" + str(round(mydata.shell_thick, 3)) + "\n")
            fout.write("shellf=" + str(round(mydata.shell_factor, 3)) + "\n")
            fout.write("shellb=" + str(round(mydata.shell_bfactor, 3)) + "\n")

            # Materials
            fout.write("#\n# Materials\n#\n")
            fout.write("materials=" + str(mydata.crt_mat) + "\n")

            fout.close()
            self.report({'INFO'}, realpath + "successfully exported")
        except:
            self.report({'ERROR'}, "Unable to export room data")

        return {'FINISHED'}

    # ----------------------------------------------------------
    # Invoke
    # ----------------------------------------------------------

    # noinspection PyUnusedLocal
    def invoke(self, context, event):
        context.window_manager.fileselect_add(self)
        return {'RUNNING_MODAL'}


# ----------------------------------------------------------
# Import menu UI
# ----------------------------------------------------------
class AchmImportRoom(Operator, ImportHelper):
    bl_idname = "io_import.roomdata"
    bl_description = 'Import Room data (.dat)'
    bl_category = 'Archimesh'
    bl_label = "Import"

    # From Helper. Filter filenames.
    filename_ext = ".dat"
    filter_glob = StringProperty(
            default="*.dat",
            options={'HIDDEN'},
            )

    filepath = StringProperty(
            name="File Path",
            description="File path used for exporting room data file",
            maxlen=1024, default="",
            )

    # ----------------------------------------------------------
    # Execute
    # ----------------------------------------------------------
    # noinspection PyUnusedLocal
    def execute(self, context):
        print("Importing:", self.properties.filepath)
        # noinspection PyBroadException
        try:
            realpath = path.realpath(path.expanduser(self.properties.filepath))
            finput = open(realpath)
            line = finput.readline()

            myobj = bpy.context.active_object
            mydata = myobj.RoomGenerator[0]
            # ----------------------------------
            # Loop all records from file
            # ----------------------------------
            idx = 0  # index of each wall
            while line:
                if line[:1] != '#':
                    if "name=" in line.lower():
                        myobj.name = line[5:-1]

                    elif "height=" in line.lower():
                        mydata.room_height = float(line[7:-1])

                    elif "thickness=" in line.lower():
                        mydata.wall_width = float(line[10:-1])

                    elif "inverse=" in line.lower():
                        if line[8:-4].upper() == "T":
                            mydata.inverse = True
                        else:
                            mydata.inverse = False

                    elif "ceiling=" in line.lower():
                        if line[8:-4].upper() == "T":
                            mydata.ceiling = True
                        else:
                            mydata.ceiling = False

                    elif "floor=" in line.lower():
                        if line[6:-4].upper() == "T":
                            mydata.floor = True
                        else:
                            mydata.floor = False

                    elif "close=" in line.lower():
                        if line[6:-4].upper() == "T":
                            mydata.merge = True
                        else:
                            mydata.merge = False
                    elif "baseboard=" in line.lower():
                        if line[10:-4].upper() == "T":
                            mydata.baseboard = True
                        else:
                            mydata.baseboard = False
                    elif "baseh=" in line.lower():
                        mydata.base_height = float(line[6:-1])
                    elif "baset=" in line.lower():
                        mydata.base_width = float(line[6:-1])
                    elif "shell=" in line.lower():
                        if line[6:-4].upper() == "T":
                            mydata.shell = True
                        else:
                            mydata.shell = False
                    elif "shellh=" in line.lower():
                        mydata.shell_height = float(line[7:-1])
                    elif "shellt=" in line.lower():
                        mydata.shell_thick = float(line[6:-1])
                    elif "shellf=" in line.lower():
                        mydata.shell_factor = float(line[6:-1])
                    elif "shellb=" in line.lower():
                        mydata.shell_bfactor = float(line[6:-1])
                    elif "walls=" in line.lower():
                        mydata.wall_num = int(line[6:-1])

                    # ---------------------
                    # Walls Data
                    # ---------------------
                    elif "w=" in line.lower() and idx < mydata.wall_num:
                        # get all pieces
                        buf = line[:-1] + ","
                        s = buf.split(",")
                        for e in s:
                            param = e.lower()
                            if "w=" in param:
                                mydata.walls[idx].w = float(e[2:])
                            elif "a=" in param:
                                if "true" == param[2:]:
                                    mydata.walls[idx].a = True
                                else:
                                    mydata.walls[idx].a = False
                            elif "r=" in param:
                                mydata.walls[idx].r = float(e[2:])
                            elif "h=" in param:
                                mydata.walls[idx].h = e[2:]
                            elif "m=" in param:
                                mydata.walls[idx].m = float(e[2:])
                            elif "f=" == param[0:2]:
                                mydata.walls[idx].f = float(e[2:])
                            elif "c=" in param:
                                if "true" == param[2:]:
                                    mydata.walls[idx].curved = True
                                else:
                                    mydata.walls[idx].curved = False
                            elif "cf=" in param:
                                mydata.walls[idx].curve_factor = float(e[3:])
                            elif "cd=" in param:
                                mydata.walls[idx].curve_arc_deg = float(e[3:])
                            elif "cs=" in param:
                                mydata.walls[idx].curve_steps = int(e[3:])
                        idx += 1

                    elif "materials=" in line.lower():
                        if line[10:-4].upper() == "T":
                            mydata.crt_mat = True
                        else:
                            mydata.crt_mat = False

                line = finput.readline()

            finput.close()
            self.report({'INFO'}, realpath + "successfully imported")
        except:
            self.report({'ERROR'}, "Unable to import room data")

        return {'FINISHED'}

    # ----------------------------------------------------------
    # Invoke
    # ----------------------------------------------------------
    # noinspection PyUnusedLocal
    def invoke(self, context, event):
        context.window_manager.fileselect_add(self)
        return {'RUNNING_MODAL'}


# ------------------------------------------------------------------
# Define operator class to create rooms
# ------------------------------------------------------------------
class AchmRoom(Operator):
    bl_idname = "mesh.archimesh_room"
    bl_label = "Room"
    bl_description = "Generate room with walls, baseboard, floor and ceiling"
    bl_category = 'Archimesh'
    bl_options = {'REGISTER', 'UNDO'}

    # -----------------------------------------------------
    # Draw (create UI interface)
    # -----------------------------------------------------
    # noinspection PyUnusedLocal
    def draw(self, context):
        layout = self.layout
        row = layout.row()
        row.label("Use Properties panel (N) to define parms", icon='INFO')
        row = layout.row(align=False)
        row.operator("io_import.roomdata", text="Import", icon='COPYDOWN')

    # -----------------------------------------------------
    # Execute
    # -----------------------------------------------------
    def execute(self, context):
        if bpy.context.mode == "OBJECT":
            create_room(self, context)
            return {'FINISHED'}
        else:
            self.report({'WARNING'}, "Archimesh: Option only valid in Object mode")
            return {'CANCELLED'}


# ------------------------------------------------------------------------------
# Create main object for the room. The other objects of room will be children of this.
# ------------------------------------------------------------------------------
# noinspection PyUnusedLocal
def create_room(self, context):
    # deselect all objects
    for o in bpy.data.objects:
        o.select = False

    # we create main object and mesh for walls
    roommesh = bpy.data.meshes.new("Room")
    roomobject = bpy.data.objects.new("Room", roommesh)
    roomobject.location = bpy.context.scene.cursor_location
    bpy.context.scene.objects.link(roomobject)
    roomobject.RoomGenerator.add()
    roomobject.RoomGenerator[0].walls.add()

    # we shape the walls and create other objects as children of 'RoomObject'.
    shape_walls_and_create_children(roomobject, roommesh)

    # we select, and activate, main object for the room.
    roomobject.select = True
    bpy.context.scene.objects.active = roomobject


# -----------------------------------------------------
# Verify if solidify exist
# -----------------------------------------------------
def is_solidify(myobject):
    flag = False
    try:
        if myobject.modifiers is None:
            return False

        for mod in myobject.modifiers:
            if mod.type == 'SOLIDIFY':
                flag = True
                break
        return flag
    except AttributeError:
        return False


# ------------------------------------------------------------------------------
# Update wall mesh and children objects (baseboard, floor and ceiling).
# ------------------------------------------------------------------------------
# noinspection PyUnusedLocal
def update_room(self, context):
    # When we update, the active object is the main object of the room.
    o = bpy.context.active_object
    oldmesh = o.data
    oldname = o.data.name
    # Now we deselect that room object to not delete it.
    o.select = False
    # and we create a new mesh for the walls:
    tmp_mesh = bpy.data.meshes.new("temp")
    # deselect all objects
    for obj in bpy.data.objects:
        obj.select = False
    # Remove children created by this addon:
    for child in o.children:
        # noinspection PyBroadException
        try:
            if child["archimesh.room_object"]:
                # noinspection PyBroadException
                try:
                    # remove child relationship
                    for grandchild in child.children:
                        grandchild.parent = None
                    # remove modifiers
                    for mod in child.modifiers:
                        bpy.ops.object.modifier_remove(mod)
                except:
                    pass
                # clear data
                old = child.data
                child.select = True
                bpy.ops.object.delete()
                bpy.data.meshes.remove(old)
        except:
            pass
    # Finally we create all that again (except main object),
    shape_walls_and_create_children(o, tmp_mesh, True)
    o.data = tmp_mesh
    # Remove data (mesh of active object),
    bpy.data.meshes.remove(oldmesh)
    tmp_mesh.name = oldname
    # and select, and activate, the main object of the room.
    o.select = True
    bpy.context.scene.objects.active = o


# -----------------------------------------------------
# Move Solidify to Top
# -----------------------------------------------------
def movetotopsolidify(myobject):
    mymod = None
    try:
        if myobject.modifiers is not None:
            for mod in myobject.modifiers:
                if mod.type == 'SOLIDIFY':
                    mymod = mod

            if mymod is not None:
                while myobject.modifiers[0] != mymod:
                    bpy.ops.object.modifier_move_up(modifier=mymod.name)
    except AttributeError:
        return


# ------------------------------------------------------------------------------
# Generate walls, baseboard, floor, ceiling and materials.
# For walls, it only shapes mesh and creates modifier solidify (the modifier, only the first time).
# And, for the others, it creates object and mesh.
# ------------------------------------------------------------------------------
def shape_walls_and_create_children(myroom, tmp_mesh, update=False):
    rp = myroom.RoomGenerator[0]  # "rp" means "room properties".
    mybase = None
    myfloor = None
    myceiling = None
    myshell = None
    # Create the walls (only mesh, because the object is 'myRoom', created before).
    create_walls(rp, tmp_mesh, get_blendunits(rp.room_height))
    myroom.data = tmp_mesh
    # Mark Seams
    select_vertices(myroom, [0, 1])
    mark_seam(myroom)
    # Unwrap
    unwrap_mesh(myroom)

    remove_doubles(myroom)
    set_normals(myroom, not rp.inverse)  # inside/outside

    if rp.wall_width > 0.0:
        if update is False or is_solidify(myroom) is False:
            set_modifier_solidify(myroom, get_blendunits(rp.wall_width))
        else:
            for mod in myroom.modifiers:
                if mod.type == 'SOLIDIFY':
                    mod.thickness = rp.wall_width
        # Move to Top SOLIDIFY
        movetotopsolidify(myroom)

    else:  # clear not used SOLIDIFY
        for mod in myroom.modifiers:
            if mod.type == 'SOLIDIFY':
                myroom.modifiers.remove(mod)

    # Create baseboard
    if rp.baseboard:
        baseboardmesh = bpy.data.meshes.new("Baseboard")
        mybase = bpy.data.objects.new("Baseboard", baseboardmesh)
        mybase.location = (0, 0, 0)
        bpy.context.scene.objects.link(mybase)
        mybase.parent = myroom
        mybase.select = True
        mybase["archimesh.room_object"] = True
        mybase["archimesh.room_baseboard"] = True

        create_walls(rp, baseboardmesh, get_blendunits(rp.base_height), True)
        set_normals(mybase, rp.inverse)  # inside/outside room
        if rp.base_width > 0.0:
            set_modifier_solidify(mybase, get_blendunits(rp.base_width))
            # Move to Top SOLIDIFY
            movetotopsolidify(mybase)
        # Mark Seams
        select_vertices(mybase, [0, 1])
        mark_seam(mybase)
        # Unwrap
        unwrap_mesh(mybase)

    # Create floor
    if rp.floor and rp.wall_num > 1:
        myfloor = create_floor(rp, "Floor", myroom)
        myfloor["archimesh.room_object"] = True
        myfloor.parent = myroom
        # Unwrap
        unwrap_mesh(myfloor)

    # Create ceiling
    if rp.ceiling and rp.wall_num > 1:
        myceiling = create_floor(rp, "Ceiling", myroom)
        myceiling["archimesh.room_object"] = True
        myceiling.parent = myroom
        # Unwrap
        unwrap_mesh(myceiling)

    # Create Shell
    #
    if rp.shell:
        myshell = add_shell(myroom, "Wall_cover", rp)
        myshell["archimesh.room_object"] = True
        myshell["archimesh.room_shell"] = True
        parentobject(myroom, myshell)
        myshell.rotation_euler = myroom.rotation_euler
        if rp.wall_width > 0.0:
            # Solidify (need for boolean)
            set_modifier_solidify(myshell, 0.01)
            # Move to Top SOLIDIFY
            movetotopsolidify(mybase)

    # Create materials
    if rp.crt_mat and bpy.context.scene.render.engine == 'CYCLES':
        # Wall material (two faces)
        mat = create_diffuse_material("Wall_material", False, 0.765, 0.650, 0.588, 0.8, 0.621, 0.570, 0.1, True)
        set_material(myroom, mat)

        # Baseboard material
        if rp.baseboard and mybase is not None:
            mat = create_diffuse_material("Baseboard_material", False, 0.8, 0.8, 0.8)
            set_material(mybase, mat)

        # Ceiling material
        if rp.ceiling and myceiling is not None:
            mat = create_diffuse_material("Ceiling_material", False, 0.95, 0.95, 0.95)
            set_material(myceiling, mat)

        # Floor material
        if rp.floor and myfloor is not None:
            mat = create_brick_material("Floor_material", False, 0.711, 0.668, 0.668, 0.8, 0.636, 0.315)
            set_material(myfloor, mat)

        # Shell material
        if rp.shell and myshell is not None:
            mat = create_diffuse_material("Wall_cover_material", False, 0.507, 0.309, 0.076, 0.507, 0.309, 0.076)
            set_material(myshell, mat)

    # deactivate others
    for o in bpy.data.objects:
        if o.select is True and o.name != myroom.name:
            o.select = False


# ------------------------------------------------------------------------------
# Create walls or baseboard (indicated with baseboard parameter).
# Some custom values are passed using the rp ("room properties" group) parameter (rp.myvariable).
# ------------------------------------------------------------------------------
def create_walls(rp, mymesh, height, baseboard=False):
    myvertex = [(0.0, 0.0, height), (0.0, 0.0, 0.0)]
    myfaces = []
    lastface = 0
    lastx = lasty = 0
    idf = 0
    # Iterate the walls
    for i in range(0, rp.wall_num):
        if 0 == i:
            prv = False
        else:
            prv = rp.walls[i - 1].a and not rp.walls[i - 1].curved

        mydat = make_wall(prv, rp.walls[i], baseboard, lastface,
                          lastx, lasty, height, myvertex, myfaces)
        lastx = mydat[0]
        lasty = mydat[1]
        lastface = mydat[2]

        # --------------------------------------
        # saves vertex data for opengl
        # --------------------------------------
        point_a = None
        point_b = None
        try:
            for mf in myfaces[idf]:
                if myvertex[mf][2] == 0:
                    if point_a is None:
                        point_a = myvertex[mf]
                    else:
                        point_b = myvertex[mf]

            rp.walls[i].glpoint_a = point_a
            rp.walls[i].glpoint_b = point_b
        except IndexError:
            pass

        idf = len(myfaces)

    # Close room
    if rp.merge is True:
        if baseboard is False:
            if rp.walls[rp.wall_num - 1].a is not True:
                myfaces.extend([(0, 1, lastface + 1, lastface)])
            else:
                if rp.walls[rp.wall_num - 1].curved is True:
                    myfaces.extend([(0, 1, lastface + 1, lastface)])
                else:
                    myfaces.extend([(0, 1, lastface, lastface + 1)])
        else:
            myfaces.extend([(0, 1, lastface + 1, lastface)])

    mymesh.from_pydata(myvertex, [], myfaces)
    mymesh.update(calc_edges=True)


# ------------------------------------------------------------------------------
# Make a Wall
#   prv: If previous wall has 'curved' activate.
#   lastFace: Number of faces of all before walls.
#   lastX: X position of the end of the last wall.
#   lastY: Y position of the end of the last wall.
#   height: Height of the last wall, without peak.
# ------------------------------------------------------------------------------
def make_wall(prv, wall, baseboard, lastface, lastx, lasty, height, myvertex, myfaces):
    #   size: Length of the wall.
    #   over: Height of the peak from "height".
    #   factor: Displacement of the peak (between -1 and 1; 0 is the middle of the wall).
    advanced = wall.a
    size = wall.w
    over = wall.m
    factor = wall.f
    angle = wall.r
    hide = wall.h

    # if angle negative, calculate real
    # use add because the angle is negative
    if angle < 0:
        angle += 360
    # Verify Units
    size = get_blendunits(size)
    over = get_blendunits(over)

    # Calculate size using angle
    sizex = cos(radians(angle)) * size
    sizey = sin(radians(angle)) * size

    # Create faces
    if advanced is False or baseboard is True:
        # Cases of this first option: Baseboard or wall without peak and without curve.
        if baseboard is True and advanced is True and wall.curved is True:
            (myvertex, myfaces, sizex, sizey, lastface) = make_curved_wall(myvertex, myfaces, size, angle,
                                                                           lastx, lasty, height, lastface,
                                                                           wall.curve_factor, int(wall.curve_arc_deg),
                                                                           int(wall.curve_arc_deg / wall.curve_steps),
                                                                           hide, baseboard)
        else:
            myvertex.extend([(lastx + sizex, lasty + sizey, height),
                             (lastx + sizex, lasty + sizey, 0.0)])
            if check_visibility(hide, baseboard):
                if prv is False or baseboard is True:
                    # Previous no advance or advance with curve
                    myfaces.extend([(lastface, lastface + 2, lastface + 3, lastface + 1)])
                else:
                    # Previous advance without curve
                    myfaces.extend([(lastface, lastface + 1, lastface + 2, lastface + 3)])
            lastface += 2
    else:
        # Case of this second option: Wall with advanced features (orientation, visibility and peak or curve).
        # Orientation and visibility options ('angle' and 'hide' variables) are only visible in panel
        # with advanced features, but are taken in account in any case.
        if wall.curved:
            # Wall with curve and without peak.
            (myvertex, myfaces, sizex, sizey, lastface) = make_curved_wall(myvertex, myfaces, size, angle,
                                                                           lastx, lasty, height, lastface,
                                                                           wall.curve_factor, int(wall.curve_arc_deg),
                                                                           int(wall.curve_arc_deg / wall.curve_steps),
                                                                           hide, baseboard)
        else:
            # Wall with peak and without curve.
            mid = size / 2 + ((size / 2) * factor)
            midx = cos(radians(angle)) * mid
            midy = sin(radians(angle)) * mid
            # first face
            myvertex.extend([(lastx + midx, lasty + midy, height + over),
                             (lastx + midx, lasty + midy, 0.0)])
            if check_visibility(hide, baseboard):
                if fabs(factor) != 1:
                    if prv is False:
                        # Previous no advance or advance with curve
                        myfaces.extend([(lastface, lastface + 2, lastface + 3, lastface + 1)])
                    else:
                        # Previous advance without curve
                        myfaces.extend([(lastface, lastface + 1, lastface + 2, lastface + 3)])
            # second face
            myvertex.extend([(lastx + sizex, lasty + sizey, 0.0),
                             (lastx + sizex, lasty + sizey, height)])
            if check_visibility(hide, baseboard):
                if fabs(factor) != 1:
                    myfaces.extend([(lastface + 2, lastface + 5, lastface + 4, lastface + 3)])
                else:
                    if prv is False:
                        myfaces.extend([(lastface, lastface + 5, lastface + 4, lastface + 1),
                                        (lastface, lastface + 2, lastface + 5)])
                    else:
                        myfaces.extend([(lastface, lastface + 4, lastface + 5, lastface + 1),
                                        (lastface + 1, lastface + 2, lastface + 5)])

            lastface += 4

    lastx += sizex
    lasty += sizey

    return lastx, lasty, lastface


# ------------------------------------------------------------------------------
# Verify visibility of walls
# ------------------------------------------------------------------------------
def check_visibility(h, base):
    # Visible
    if h == '0':
        return True
    # Wall
    if h == '2':
        if base is True:
            return False
        else:
            return True
    # Baseboard
    if h == '1':
        if base is True:
            return True
        else:
            return False
    # Hidden
    if h == '3':
        return False


# ------------------------------------------------------------------------------
# Create a curved wall.
# ------------------------------------------------------------------------------
def make_curved_wall(myvertex, myfaces, size, wall_angle, lastx, lasty, height,
                     lastface, curve_factor, arc_angle, step_angle, hide, baseboard):
    curvex = None
    curvey = None
    # Calculate size using angle
    sizex = cos(radians(wall_angle)) * size
    sizey = sin(radians(wall_angle)) * size

    for step in range(0, arc_angle + step_angle, step_angle):
        curvex = sizex / 2 - cos(radians(step + wall_angle)) * size / 2
        curvey = sizey / 2 - sin(radians(step + wall_angle)) * size / 2
        curvey = curvey * curve_factor
        myvertex.extend([(lastx + curvex, lasty + curvey, height),
                         (lastx + curvex, lasty + curvey, 0.0)])
        if check_visibility(hide, baseboard):
            myfaces.extend([(lastface, lastface + 2, lastface + 3, lastface + 1)])
        lastface += 2
    return myvertex, myfaces, curvex, curvey, lastface


# ------------------------------------------------------------------------------
# Create floor or ceiling (create object and mesh)
# Parameters:
#   rm: "room properties" group
#   typ: Name of new object and mesh ('Floor' or 'Ceiling')
#   myRoom: Main object for the room
# ------------------------------------------------------------------------------

def create_floor(rp, typ, myroom):
    bpy.context.scene.objects.active = myroom

    myvertex = []
    myfaces = []
    verts = []

    obverts = bpy.context.active_object.data.vertices
    for vertex in obverts:
        verts.append(tuple(vertex.co))
    # Loop only selected
    i = 0
    for e in verts:
        if typ == "Floor":
            if e[2] == 0.0:
                myvertex.extend([(e[0], e[1], e[2])])
                i += 1
        else:  # ceiling
            if round(e[2], 5) == round(get_blendunits(rp.room_height), 5):
                myvertex.extend([(e[0], e[1], e[2])])
                i += 1

    # Create faces
    fa = []
    for f in range(0, i):
        fa.extend([f])

    myfaces.extend([fa])

    mymesh = bpy.data.meshes.new(typ)
    myobject = bpy.data.objects.new(typ, mymesh)

    myobject.location = (0, 0, 0)
    bpy.context.scene.objects.link(myobject)

    mymesh.from_pydata(myvertex, [], myfaces)
    mymesh.update(calc_edges=True)

    return myobject


# ------------------------------------------------------------------
# Define property group class to create, or modify, room walls.
# ------------------------------------------------------------------
class WallProperties(PropertyGroup):
    w = FloatProperty(
            name='Length',
            min=-150, max=150,
            default=1, precision=3,
            description='Length of the wall (negative to reverse direction)',
            update=update_room,
            )

    a = BoolProperty(
            name="Advanced",
            description="Define advanced parameters of the wall",
            default=False,
            update=update_room,
            )

    curved = BoolProperty(
            name="Curved",
            description="Enable curved wall parameters",
            default=False,
            update=update_room,
            )
    curve_factor = FloatProperty(
            name='Factor',
            min=-5, max=5,
            default=1, precision=1,
            description='Curvature variation',
            update=update_room,
            )
    curve_arc_deg = FloatProperty(
            name='Degrees', min=1, max=359,
            default=180, precision=1,
            description='Degrees of the curve arc (must be >= steps)',
            update=update_room,
            )
    curve_steps = IntProperty(
            name='Steps',
            min=2, max=50,
            default=12,
            description='Curve steps',
            update=update_room,
            )

    m = FloatProperty(
            name='Peak', min=0, max=50,
            default=0, precision=3,
            description='Middle height variation',
            update=update_room,
            )
    f = FloatProperty(
            name='Factor', min=-1, max=1,
            default=0, precision=3,
            description='Middle displacement',
            update=update_room,
            )
    r = FloatProperty(
            name='Angle',
            min=-180, max=180,
            default=0, precision=1,
            description='Wall Angle (-180 to +180)',
            update=update_room,
            )

    h = EnumProperty(
            items=(
                ('0', "Visible", ""),
                ('1', "Baseboard", ""),
                ('2', "Wall", ""),
                ('3', "Hidden", ""),
                ),
            name="",
            description="Wall visibility",
            update=update_room,
            )

    # opengl internal data
    glpoint_a = FloatVectorProperty(
            name="glpointa",
            description="Hidden property for opengl",
            default=(0, 0, 0),
            )
    glpoint_b = FloatVectorProperty(
            name="glpointb",
            description="Hidden property for opengl",
            default=(0, 0, 0),
            )

bpy.utils.register_class(WallProperties)


# ------------------------------------------------------------------
# Add a new room wall.
# First add a parameter group for that new wall, and then update the room.
# ------------------------------------------------------------------
def add_room_wall(self, context):
    rp = context.object.RoomGenerator[0]
    for cont in range(len(rp.walls) - 1, rp.wall_num):
        rp.walls.add()
        # by default, we alternate the direction of the walls.
        if 1 == cont % 2:
            rp.walls[cont].r = 90
    update_room(self, context)


# ------------------------------------
# Get if some vertex is highest
# ------------------------------------
def get_hight(verts, faces_4, faces_3, face_index, face_num):
    rtn = face_index
    a = faces_4[face_num][0]
    b = faces_4[face_num][1]
    c = faces_4[face_num][2]
    d = faces_4[face_num][3]

    for face3 in faces_3:
        for idx3 in face3:
            if idx3 != face_index:
                # check x and y position (must be equal)
                if verts[idx3][0] == verts[face_index][0] and verts[idx3][1] == verts[face_index][1]:
                    # only if z is > that previous z
                    if verts[idx3][2] > verts[face_index][2]:
                        # checking if the original vertex is in the same face
                        # must have 2 vertices on the original face
                        t = 0
                        for e in face3:
                            if e == a or e == b or e == c or e == d:
                                t += 1
                        if t >= 2:
                            rtn = idx3

    return rtn


# ------------------------------------
# Sort list of faces
# ------------------------------------
def sort_facelist(activefaces, activenormals):
    totfaces = len(activefaces)
    newlist = []
    newnormal = []
    # -----------------------
    # Only one face
    # -----------------------
    if totfaces == 1:
        newlist.append(activefaces[0])
        newnormal.append(activenormals[0])
        return newlist, newnormal

    # -----------------------
    # Look for first element
    # -----------------------
    idx = 0
    for face in activefaces:
        c = 0
        for i in face:
            if i == 0 or i == 1:
                c += 1

        if c >= 2 and face not in newlist:
            newlist.append(face)
            newnormal.append(activenormals[idx])
            break
        idx += 1

    # -----------------------
    # Look for second element
    # -----------------------
    idx = 0
    for face in activefaces:
        c = 0
        for i in face:
            if i == 2 or i == 3:
                c += 1
        if c >= 2 and face not in newlist:
            newlist.append(face)
            newnormal.append(activenormals[idx])
            break
        idx += 1

    # -----------------------
    # Add next faces
    # -----------------------
    for x in range(2, totfaces):
        idx = 0
        for face in activefaces:
            c = 0
            for i in face:
                if i == newlist[x - 1][0] or i == newlist[x - 1][1] or i == newlist[x - 1][2] or i == newlist[x - 1][3]:
                    c += 1
            if c >= 2 and face not in newlist:
                newlist.append(face)
                newnormal.append(activenormals[idx])
            idx += 1

    return newlist, newnormal


# ------------------------------------
# Get points of the walls
# selobject: room
# ------------------------------------
def get_wall_points(selobject):
    obverts = selobject.data.vertices
    obfaces = selobject.data.polygons

    verts = []
    faces_3 = []
    faces_4 = []
    normals = []
    activefaces = []
    activenormals = []

    # --------------------------
    # Recover all vertex
    # --------------------------
    for vertex in obverts:
        verts.append(list(vertex.co))

    # --------------------------
    # Recover 3 faces
    # --------------------------
    for face in obfaces:
        # get only 4 corners faces
        if len(list(face.vertices)) == 3:
            faces_3.append(list(face.vertices))
    # --------------------------
    # Recover 4 faces
    # --------------------------
    for face in obfaces:
        # get only 4 corners faces
        if len(list(face.vertices)) == 4:
            faces_4.append(list(face.vertices))
            normals.append(face.normal)
    # --------------------------
    # Replace highest
    # --------------------------
    idx = 0
    for face in faces_4:
        mylist = []
        for e in face:  # e contains the number of vertex element
            if verts[e][2] == 0:
                mylist.append(e)
            # Only if Z > 0, recalculate
            if verts[e][2] != 0:
                mylist.append(get_hight(verts, faces_4, faces_3, e, idx))

        activefaces.append(mylist)
        activenormals.append(normals[idx])
        idx += 1

    # ------------------------
    # Sort faces
    # ------------------------
    newlist, newnormal = sort_facelist(activefaces, activenormals)

    return verts, newlist, newnormal


# ------------------------------------
# Create a shell of boards
# selobject: room
# objname: Name for new object
# rp: room properties
# ------------------------------------
def add_shell(selobject, objname, rp):

    myvertex = []
    myfaces = []

    verts, activefaces, activenormals = get_wall_points(selobject)

    # --------------------------
    # Get line points
    # --------------------------
    i = 0
    idx = 0
    for face in activefaces:
        a1 = None
        b1 = None
        a2 = None
        b2 = None
        # Bottom
        for e in face:
            if verts[e][2] == 0:
                if a1 is None:
                    a1 = e
                else:
                    b1 = e
        # Top
        for e in face:
            if verts[e][2] != 0:
                if verts[a1][0] == verts[e][0] and verts[a1][1] == verts[e][1]:
                    a2 = e
                else:
                    b2 = e
        # Create the mesh
        mydata = create_cover_mesh(idx, verts, activefaces, activenormals, i, a1, a2, b1, b2,
                                   rp.merge, 0.005,
                                   rp.shell_height, rp.shell_thick, rp.shell_factor, rp.shell_bfactor)
        i = mydata[0]
        myvertex.extend(mydata[1])
        myfaces.extend(mydata[2])
        idx += 1
    # --------------------------
    # Create the mesh
    # --------------------------
    mesh = bpy.data.meshes.new(objname)
    myobject = bpy.data.objects.new(objname, mesh)

    myobject.location = selobject.location
    bpy.context.scene.objects.link(myobject)

    mesh.from_pydata(myvertex, [], myfaces)
    mesh.update(calc_edges=True)

    remove_doubles(myobject)
    set_normals(myobject)

    return myobject


# ---------------------------------------------------------
# Project point using face normals
#
# m: Magnitud
# pf: Comparision face +/-
# ---------------------------------------------------------
def project_point(idx, point, normals, m, pf):
    v1 = Vector(normals[idx])
    if idx + pf >= len(normals):
        vf = v1
    elif idx + pf < 0:
        vf = v1
    else:
        v2 = Vector(normals[idx + pf])
        if v1 != v2:
            vf = v1 + v2
            vf.normalize()  # must be length equal to 1
        else:
            vf = v1

    n1 = (vf[0] * m, vf[1] * m, vf[2] * m)
    p1 = (point[0] + n1[0], point[1] + n1[1], point[2] + n1[2])
    return p1


# ---------------------------------------------------------
# Create wall cover mesh
#
# Uses linear equation for cutting
#
# Z = This value is the z axis value
# so, we can replace t with ((Z-Z1) / (Z2-Z1))
#
# X = X1 + ((X2 - X1) * t)
#
# X = X1 + ((X2 - X1) * ((Z-Z1) / (Z2-Z1)))
# Y = Y1 + ((Y2 - Y1) * ((Z-Z1) / (Z2-Z1)))
#
# height refers to the height of the cover piece
# width refers to the width of the cover piece
# ---------------------------------------------------------


def create_cover_mesh(idx, verts, activefaces, normals, i, a1, a2, b1, b2, merge, space=0.005,
                      height=0.20, thickness=0.025, shell_factor=1, shell_bfactor=1):
    pvertex = []
    pfaces = []

    a1_x = verts[a1][0]
    a1_y = verts[a1][1]
    a1_z = verts[a1][2]

    a2_x = verts[a2][0]
    a2_y = verts[a2][1]
    a2_z = verts[a2][2]

    b1_x = verts[b1][0]
    b1_y = verts[b1][1]
    b1_z = verts[b1][2]

    b2_x = verts[b2][0]
    b2_y = verts[b2][1]
    b2_z = verts[b2][2]

    # Get highest
    if a2_z >= b2_z:
        top = a2_z
        limit = b2_z
    else:
        top = b2_z
        limit = a2_z

    # apply factor
    # get high point of walls
    maxh = 0
    for v in verts:
        if v[2] > maxh:
            maxh = v[2]
    maxh *= shell_factor
    minh = maxh * (1 - shell_bfactor)
    if minh < 0:
        minh = 0

    if shell_factor < 1:
        if top > maxh:
            top = maxh

    # --------------------------------------
    # Loop to generate each piece of cover
    # --------------------------------------
    zpos = minh  # initial position
    f = 0
    f2 = 0
    # detect what face must use to compare
    face_num = len(activefaces) - 1
    if idx == 0 and merge is True:
        if is_in_nextface(idx + 1, activefaces, verts, a1_x, a1_y) is True:
            side_a = 1
            side_b = face_num
        else:
            side_a = face_num
            side_b = 1
    elif idx == face_num and merge is True:
        if is_in_nextface(face_num, activefaces, verts, a1_x, a1_y) is False:
            side_b = -face_num
            side_a = -1
        else:
            side_b = -1
            side_a = -face_num
    else:
        if is_in_nextface(idx + 1, activefaces, verts, a1_x, a1_y) is True:
            side_a = 1
            side_b = -1
        else:
            side_a = -1
            side_b = 1
            # Last wall
            if idx + 1 >= len(activefaces):
                if is_in_nextface(idx - 1, activefaces, verts, a1_x, a1_y) is True:
                    side_a = -1
                    side_b = 1
                else:
                    side_a = 1
                    side_b = -1

    na1_x = 0
    na1_y = 0
    na2_x = 0
    na2_y = 0

    nb1_x = 0
    nb1_y = 0
    nb2_x = 0
    nb2_y = 0

    nc1_x = 0
    nc1_y = 0
    nc2_x = 0
    nc2_y = 0

    nd1_x = 0
    nd1_y = 0
    nd2_x = 0
    nd2_y = 0

    while zpos <= top:
        # ----------------------
        # Full cover piece
        # ----------------------
        if zpos <= limit:
            # ----------------
            # Point A
            # ----------------
            mypoint = project_point(idx, (a1_x, a1_y, zpos), normals, space, side_a)

            pvertex.extend([mypoint])
            na1_x = mypoint[0]
            na1_y = mypoint[1]
            # external point
            mypoint = project_point(idx, (a1_x, a1_y, zpos), normals, space + thickness, side_a)
            pvertex.extend([mypoint])
            nc1_x = mypoint[0]
            nc1_y = mypoint[1]
            # get second point (vertical)
            mypoint = project_point(idx, (a2_x, a2_y, zpos), normals, space, side_a)
            na2_x = mypoint[0]
            na2_y = mypoint[1]
            mypoint = project_point(idx, (a2_x, a2_y, zpos), normals, space + thickness, side_a)
            nc2_x = mypoint[0]
            nc2_y = mypoint[1]

            # ----------------
            # Point B
            # ----------------
            mypoint = project_point(idx, (b1_x, b1_y, zpos), normals, space, side_b)
            pvertex.extend([mypoint])
            nb1_x = mypoint[0]
            nb1_y = mypoint[1]
            # external point
            mypoint = project_point(idx, (b1_x, b1_y, zpos), normals, space + thickness, side_b)
            pvertex.extend([mypoint])
            nd1_x = mypoint[0]
            nd1_y = mypoint[1]
            # get second point (vertical)
            mypoint = project_point(idx, (b2_x, b2_y, zpos), normals, space, side_b)
            nb2_x = mypoint[0]
            nb2_y = mypoint[1]
            mypoint = project_point(idx, (b2_x, b2_y, zpos), normals, space + thickness, side_b)
            nd2_x = mypoint[0]
            nd2_y = mypoint[1]

            # Faces
            if zpos != top:
                pfaces.extend([(i, i + 1, i + 3, i + 2)])

            if f >= 1:
                pfaces.extend([(i - 3, i, i + 2, i - 1)])

            i += 4
            f += 1
        # ----------------------
        # Cut pieces
        # ----------------------
        else:
            # -------------------------------
            # Internal Points
            # -------------------------------
            # Get highest
            if a2_z >= b2_z:
                ax1 = na1_x
                ay1 = na1_y
                az1 = a1_z
                ax2 = na2_x
                ay2 = na2_y
                az2 = a2_z

                bx1 = na2_x
                by1 = na2_y
                bz1 = a2_z
                bx2 = nb2_x
                by2 = nb2_y
                bz2 = b2_z
            else:
                ax1 = na2_x
                ay1 = na2_y
                az1 = a2_z
                ax2 = nb2_x
                ay2 = nb2_y
                az2 = b2_z

                bx1 = nb1_x
                by1 = nb1_y
                bz1 = b1_z
                bx2 = nb2_x
                by2 = nb2_y
                bz2 = b2_z

            # ----------------
            # Point A
            # ----------------
            x = ax1 + ((ax2 - ax1) * ((zpos - az1) / (az2 - az1)))
            y = ay1 + ((ay2 - ay1) * ((zpos - az1) / (az2 - az1)))
            pvertex.extend([(x, y, zpos)])
            # ----------------
            # Point B
            # ----------------
            x = bx1 + ((bx2 - bx1) * ((zpos - bz1) / (bz2 - bz1)))
            y = by1 + ((by2 - by1) * ((zpos - bz1) / (bz2 - bz1)))
            pvertex.extend([(x, y, zpos)])
            # -------------------------------
            # External Points
            # -------------------------------
            # Get highest
            if a2_z >= b2_z:
                ax1 = nc1_x
                ay1 = nc1_y
                az1 = a1_z
                ax2 = nc2_x
                ay2 = nc2_y
                az2 = a2_z

                bx1 = nc2_x
                by1 = nc2_y
                bz1 = a2_z
                bx2 = nd2_x
                by2 = nd2_y
                bz2 = b2_z
            else:
                ax1 = nc2_x
                ay1 = nc2_y
                az1 = a2_z
                ax2 = nd2_x
                ay2 = nd2_y
                az2 = b2_z

                bx1 = nd1_x
                by1 = nd1_y
                bz1 = b1_z
                bx2 = nd2_x
                by2 = nd2_y
                bz2 = b2_z

            # ----------------
            # Point A
            # ----------------
            x = ax1 + ((ax2 - ax1) * ((zpos - az1) / (az2 - az1)))
            y = ay1 + ((ay2 - ay1) * ((zpos - az1) / (az2 - az1)))
            pvertex.extend([(x, y, zpos)])
            # ----------------
            # Point B
            # ----------------
            x = bx1 + ((bx2 - bx1) * ((zpos - bz1) / (bz2 - bz1)))
            y = by1 + ((by2 - by1) * ((zpos - bz1) / (bz2 - bz1)))
            pvertex.extend([(x, y, zpos)])
            # Faces
            if zpos != top:
                pfaces.extend([(i, i + 1, i + 3, i + 2)])

            if f2 == 0:
                pfaces.extend([(i - 1, i - 3, i, i + 1)])
            else:
                pfaces.extend([(i - 1, i - 2, i, i + 1)])

            i += 4
            f2 += 1
        # avoid infinite loop
        if zpos == top:
            break

        # add new piece
        zpos += height
        # cut oversized
        if zpos > top:
            zpos = top

    return i, pvertex, pfaces


# -------------------------------------------------------------
# Detect if the vertex is face
# -------------------------------------------------------------
def is_in_nextface(idx, activefaces, verts, x, y):
    if idx >= len(activefaces):
        return False

    for f in activefaces[idx]:
        if verts[f][2] == 0:  # only ground
            if verts[f][0] == x and verts[f][1] == y:
                return True

    return False


# ------------------------------------------------------------------
# Define property group class to create or modify a rooms.
# ------------------------------------------------------------------
class RoomProperties(PropertyGroup):
    room_height = FloatProperty(
            name='Height', min=0.001, max=50,
            default=2.4, precision=3,
            description='Room height', update=update_room,
            )
    wall_width = FloatProperty(
            name='Thickness', min=0.000, max=10,
            default=0.0, precision=3,
            description='Thickness of the walls', update=update_room,
            )
    inverse = BoolProperty(
            name="Inverse", description="Inverse normals to outside",
            default=False,
            update=update_room,
            )
    crt_mat = BoolProperty(
            name="Create default Cycles materials",
            description="Create default materials for Cycles render",
            default=True,
            update=update_room,
            )

    wall_num = IntProperty(
            name='Number of Walls', min=1, max=50,
            default=1,
            description='Number total of walls in the room', update=add_room_wall,
            )

    baseboard = BoolProperty(
            name="Baseboard", description="Create a baseboard automatically",
            default=True,
            update=update_room,
            )

    base_width = FloatProperty(
            name='Width', min=0.001, max=10,
            default=0.015, precision=3,
            description='Baseboard width', update=update_room,
            )
    base_height = FloatProperty(
            name='Height', min=0.05, max=20,
            default=0.12, precision=3,
            description='Baseboard height', update=update_room,
            )

    ceiling = BoolProperty(
            name="Ceiling", description="Create a ceiling",
            default=False, update=update_room,
            )
    floor = BoolProperty(
            name="Floor", description="Create a floor automatically",
            default=False,
            update=update_room,
            )

    merge = BoolProperty(
            name="Close walls", description="Close walls to create a full closed room",
            default=False, update=update_room,
            )

    walls = CollectionProperty(
            type=WallProperties,
            )

    shell = BoolProperty(
            name="Wall cover", description="Create a cover of boards",
            default=False, update=update_room,
            )
    shell_thick = FloatProperty(
            name='Thickness', min=0.001, max=1,
            default=0.025, precision=3,
            description='Cover board thickness', update=update_room,
            )
    shell_height = FloatProperty(
            name='Height', min=0.05, max=1,
            default=0.20, precision=3,
            description='Cover board height', update=update_room,
            )
    shell_factor = FloatProperty(
            name='Top', min=0.1, max=1,
            default=1, precision=1,
            description='Percentage for top covering (1 Full)', update=update_room,
            )
    shell_bfactor = FloatProperty(
            name='Bottom', min=0.1, max=1,
            default=1, precision=1,
            description='Percentage for bottom covering (1 Full)', update=update_room,
            )

bpy.utils.register_class(RoomProperties)
Object.RoomGenerator = CollectionProperty(type=RoomProperties)


# -----------------------------------------------------
# Add wall parameters to the panel.
# -----------------------------------------------------
def add_wall(idx, box, wall):
    box.label("Wall " + str(idx))
    row = box.row()
    row.prop(wall, 'w')
    row.prop(wall, 'a')
    # row.prop(wall, 'curved')
    if wall.a is True:
        srow = box.row()
        srow.prop(wall, 'r')
        srow.prop(wall, 'h')

        srow = box.row()
        srow.prop(wall, 'curved')

        if wall.curved is False:
            srow.prop(wall, 'm')
            srow.prop(wall, 'f')

        if wall.curved is True:
            srow.prop(wall, 'curve_factor')
            srow.prop(wall, 'curve_arc_deg')
            srow.prop(wall, 'curve_steps')


# ------------------------------------------------------------------
# Define panel class to modify rooms.
# ------------------------------------------------------------------
class AchmRoomGeneratorPanel(Panel):
    bl_idname = "OBJECT_PT_room_generator"
    bl_label = "Room"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = 'Archimesh'

    # -----------------------------------------------------
    # Verify if visible
    # -----------------------------------------------------
    @classmethod
    def poll(cls, context):
        o = context.object
        if o is None:
            return False
        if 'RoomGenerator' not in o:
            return False
        else:
            return True

    # -----------------------------------------------------
    # Draw (create UI interface)
    # -----------------------------------------------------
    def draw(self, context):
        o = context.object
        # If the selected object didn't be created with the group 'RoomGenerator', this panel is not created.
        # noinspection PyBroadException
        try:
            if 'RoomGenerator' not in o:
                return
        except:
            return

        layout = self.layout
        if bpy.context.mode == 'EDIT_MESH':
            layout.label('Warning: Operator does not work in edit mode.', icon='ERROR')
        else:
            room = o.RoomGenerator[0]
            row = layout.row()
            row.prop(room, 'room_height')
            row.prop(room, 'wall_width')
            row.prop(room, 'inverse')

            row = layout.row()
            if room.wall_num > 1:
                row.prop(room, 'ceiling')
                row.prop(room, 'floor')
                row.prop(room, 'merge')

            # Wall number
            row = layout.row()
            row.prop(room, 'wall_num')

            # Add menu for walls
            if room.wall_num > 0:
                for wall_index in range(0, room.wall_num):
                    box = layout.box()
                    add_wall(wall_index + 1, box, room.walls[wall_index])

            box = layout.box()
            box.prop(room, 'baseboard')
            if room.baseboard is True:
                row = box.row()
                row.prop(room, 'base_width')
                row.prop(room, 'base_height')

            box = layout.box()
            box.prop(room, 'shell')
            if room.shell is True:
                row = box.row()
                row.prop(room, 'shell_height')
                row.prop(room, 'shell_thick')
                row = box.row()
                row.prop(room, 'shell_factor', slider=True)
                row.prop(room, 'shell_bfactor', slider=True)

            box = layout.box()
            if not context.scene.render.engine == 'CYCLES':
                box.enabled = False
            box.prop(room, 'crt_mat')
