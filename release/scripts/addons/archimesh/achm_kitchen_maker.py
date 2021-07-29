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
# Automatic generation of kitchen cabinet
# Author: Antonio Vazquez (antonioya)
#
# ----------------------------------------------------------
import bpy
from math import pi, fabs
from copy import copy
from sys import exc_info
from datetime import datetime
from time import time
from bpy.types import Operator, PropertyGroup
from bpy.props import StringProperty, EnumProperty, FloatProperty, IntProperty, BoolProperty, CollectionProperty
from bpy_extras.io_utils import ExportHelper
from .achm_tools import *

# ----------------------------------------------------------
#  Define rotation types
# ----------------------------------------------------------
RotationType_Default = 9
RotationType_R90CW = 1
RotationType_R90CCW = 2
RotationType_R180 = 3


# ----------------------------------------------------------
#    Export menu UI
# ----------------------------------------------------------
class AchmExportInventory(Operator, ExportHelper):
    bl_idname = "io_export.kitchen_inventory"
    bl_description = 'Export kitchen inventory (.txt)'
    bl_category = 'Archimesh'
    bl_label = "Export"

    # From ExportHelper. Filter filenames.
    filename_ext = ".txt"
    filter_glob = StringProperty(
            default="*.txt",
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
        # noinspection PyBroadException
        try:
            # -------------------------------
            # extract path and filename
            # -------------------------------
            (filepath, filename) = os.path.split(self.properties.filepath)
            print('Exporting %s' % filename)
            # -------------------------------
            # Open output file
            # -------------------------------
            realpath = os.path.realpath(os.path.expanduser(self.properties.filepath))
            fout = open(realpath, 'w')

            st = datetime.fromtimestamp(time()).strftime('%Y-%m-%d %H:%M:%S')
            fout.write("# Archimesh kitchen inventory\n")
            fout.write("# " + st + "\n")
            mylist = getinventory()
            for e in mylist:
                fout.write(e + "\n")

            fout.close()
            self.report({'INFO'}, realpath + "successfully exported")
        except:
            e = exc_info()[0]
            self.report({'ERROR'}, "Unable to export inventory " + e)

        return {'FINISHED'}


# ----------------------------------------------------------
# Generate inventory list
# ----------------------------------------------------------
def getinventory():
    # Get List of boxes in the scene
    unitobj = []
    for obj in bpy.context.scene.objects:
        # noinspection PyBroadException
        try:
            if obj["archimesh.sku"] is not None:
                unitobj.extend([obj["archimesh.sku"]])
        except:
            pass
    # ----------------------------------------
    # Get number of unit structures (boxes)
    # ----------------------------------------
    boxes = []
    boxestot = []
    for u in unitobj:
        key = u[:1] + u[8:28]
        if key not in boxes:
            boxes.extend([key])
            boxestot.extend([1])
        else:
            x = boxes.index(key)
            boxestot[x] += 1
    # ----------------------------------------
    # Get number of doors and drawer fronts
    # ----------------------------------------
    door = []
    doortot = []
    handles = 0
    for u in unitobj:
        if u[1:2] != "W":
            w = float(u[36:42])
            key = u[1:2] + "%06.3f" % w + u[22:28]
        else:  # Drawers
            # calculate separation
            sz = float(u[22:28])
            gap = 0.001
            dist = sz - (gap * int(u[2:4]))
            space = dist / int(u[2:4])
            key = u[1:2] + u[8:15] + "%06.3f" % space

        n = int(u[2:4])
        # handles
        if u[4:5] == "1":
            handles += n
        if key not in door:
            door.extend([key])
            doortot.extend([n])
        else:
            x = door.index(key)
            doortot[x] += n
    # ----------------------------------------
    # Get number of Shelves
    # ----------------------------------------
    shelves = []
    shelvestot = []
    for u in unitobj:
        if int(u[5:7]) > 0:
            w = float(u[8:14])
            n = int(u[5:7])
            th = float(u[29:35])

            key = "%0.2f x %0.2f x %0.3f" % (w - (th * 2), float(u[15:21]) - th, th)  # subtract board thickness

            if key not in shelves:
                shelves.extend([key])
                shelvestot.extend([n])
            else:
                x = shelves.index(key)
                shelvestot[x] += n

    # ----------------------------------------
    # Get Countertop size
    # "T%06.3fx%06.3fx%06.3f-%06.3f"
    # ----------------------------------------
    t = 0
    z = 0
    for obj in bpy.context.scene.objects:
        # noinspection PyBroadException
        try:
            if obj["archimesh.top_sku"] is not None:
                u = obj["archimesh.top_sku"]
                t += float(u[1:7])
                z += float(u[22:28])
        except:
            pass

    # ----------------------------------------
    # Get Baseboard size
    # ----------------------------------------
    b = 0
    btxt = None
    for obj in bpy.context.scene.objects:
        # noinspection PyBroadException
        try:
            if obj["archimesh.base_sku"] is not None:
                u = obj["archimesh.base_sku"]
                b += float(u[1:6])
                btxt = "%0.3f x %0.3f" % (float(u[8:14]), float(u[15:21]))
        except:
            pass

    # ----------------------------------------
    # Prepare output data
    # ----------------------------------------
    output = []
    output.extend(["Units\tDescription\tDimensions"])
    for i in range(0, len(boxes)):
        if boxes[i][:1] == "F":
            typ = "Floor unit\t"
        else:
            typ = "Wall unit\t"

        txt = "%0.2f x %0.2f x %0.2f" % (float(boxes[i][1:7]), float(boxes[i][8:14]), float(boxes[i][15:21]))
        output.extend([str(boxestot[i]) + "\t" + typ + txt])

    for i in range(0, len(door)):
        if door[i][:1] == "D" or door[i][:1] == "L":
            typ = "Solid door\t"
        elif door[i][:1] == "G":
            typ = "Glass door\t"
        elif door[i][:1] == "W":
            typ = "Drawer front\t"
        else:
            typ = "????\t"

        txt = "%0.3f x %0.3f" % (float(door[i][1:7]), float(door[i][8:14]))
        output.extend([str(doortot[i]) + "\t" + typ + txt])

    for i in range(0, len(shelves)):
        output.extend([str(shelvestot[i]) + "\tShelf\t" + shelves[i]])

    output.extend([str(handles) + "\tHandle"])
    if t > 0:
        output.extend([str(round(t, 2)) + "\tCountertop (linear length)"])
    if z > 0:
        output.extend([str(round(z, 2)) + "\tCountertop wall piece(linear length)"])
    if b > 0:
        output.extend([str(round(b, 2)) + "\tBaseboard (linear length) " + btxt])

    return output


# ------------------------------------------------------------------
# Define property group class for cabinet properties
# This is managed as an array of objects
# ------------------------------------------------------------------
class CabinetProperties(PropertyGroup):
    # Cabinet width
    sX = FloatProperty(
            name='width', min=0.001, max=10, default=0.60, precision=3,
            description='Cabinet width',
            )
    wY = FloatProperty(
            name='', min=-10, max=10, default=0, precision=3,
            description='Modify depth size',
            )
    wZ = FloatProperty(
            name='', min=-10, max=10, default=0, precision=3,
            description='Modify height size',
            )

    # Cabinet position shift
    pX = FloatProperty(
            name='', min=-10, max=10, default=0, precision=3,
            description='Position x shift',
            )
    pY = FloatProperty(
            name='', min=-10, max=10, default=0, precision=3,
            description='Position y shift',
            )
    pZ = FloatProperty(
            name='', min=-10, max=10, default=0, precision=3,
            description='Position z shift',
            )

    # Door type
    dType = EnumProperty(
            items=(
                ('1', "Single R", ""),
                ('2', "Single L", ""),
                ('3', "Single T", ""),
                ('4', "Glass R", ""),
                ('5', "Glass L", ""),
                ('6', "Glass T", ""),
                ('7', "Drawers", ""),
                ('8', "Double", ""),
                ('11', "Double Glass", ""),
                ('10', "Corner R", ""),
                ('9', "Corner L", ""),
                ('99', "None", "")),
            name="Door",
            description="Type of front door or drawers",
            )

    # Shelves
    sNum = IntProperty(
            name='Shelves', min=0, max=10, default=1,
            description='Number total of shelves',
            )
    # Drawers
    dNum = IntProperty(
            name='Num', min=1, max=10, default=3,
            description='Number total of drawers',
            )
    # Glass Factor
    gF = FloatProperty(
            name='', min=0.001, max=1, default=0.1, precision=3,
            description='Glass ratio',
            )
    # Handle flag
    hand = BoolProperty(
            name="Handle",
            description="Create a handle", default=True,
            )
    # Left baseboard
    bL = BoolProperty(
            name="Left Baseboard",
            description="Create a left baseboard", default=False,
            )
    # Right baseboard
    bR = BoolProperty(
            name="Right Baseboard",
            description="Create a left baseboard", default=False,
            )
    # Fill countertop spaces
    tC = BoolProperty(
            name="Countertop fill",
            description="Fill empty spaces with countertop", default=True,
            )
    # Add countertop edge
    tE = BoolProperty(
            name="Countertop edge",
            description="Add edge to countertop", default=True,
            )
    # cabinet rotation
    rotate = EnumProperty(
            items=(
                ('9', "Default", ""),
                ('1', "90 CW", ""),
                ('2', "90 CCW", ""),
                ('3', "180", ""),
                ),
            name="Rot",
            description="Rotate cabinet relative to previous one",
            )


bpy.utils.register_class(CabinetProperties)


# ------------------------------------------------------------------
# Define UI class
# Kitchens
# ------------------------------------------------------------------
class AchmKitchen(Operator):
    bl_idname = "mesh.archimesh_kitchen"
    bl_label = "Cabinets"
    bl_description = "Cabinet Generator"
    bl_category = 'Archimesh'
    bl_options = {'REGISTER', 'UNDO'}

    # Define properties
    type_cabinet = EnumProperty(
            items=(('1', "Floor", ""),
                ('2', "Wall", "")),
            name="Type",
            description="Type of cabinets",
            )
    oldtype = type_cabinet

    thickness = FloatProperty(
            name='Thickness', min=0.001, max=5, default=0.018, precision=3,
            description='Board thickness',
            )
    depth = FloatProperty(
            name='Depth', min=0.001, max=50, default=0.59, precision=3,
            description='Default cabinet depth',
            )
    height = FloatProperty(
            name='Height', min=0.001, max=50, default=0.70, precision=3,
            description='Default cabinet height',
            )
    handle = EnumProperty(
            items=(
                ('1', "Model 1", ""),
                ('2', "Model 2", ""),
                ('3', "Model 3", ""),
                ('4', "Model 4", ""),
                ('5', "Model 5", ""),
                ('6', "Model 6", ""),
                ('7', "Model 7", ""),
                ('8', "Model 8", ""),
                ('9', "None", ""),
                ),
            name="Handle",
            description="Type of handle",
            )
    handle_x = FloatProperty(
            name='', min=0.001, max=10,
            default=0.05, precision=3,
            description='Displacement in X relative position (limited to door size)',
            )
    handle_z = FloatProperty(
            name='', min=0.001, max=10,
            default=0.05, precision=3,
            description='Displacement in Z relative position (limited to door size)',
            )

    baseboard = BoolProperty(
            name="Baseboard",
            description="Create a baseboard automatically",
            default=True,
            )
    baseheight = FloatProperty(
            name='height', min=0.001, max=10,
            default=0.16, precision=3,
            description='Baseboard height',
            )
    basefactor = FloatProperty(
            name='sink', min=0, max=1,
            default=0.90, precision=3,
            description='Baseboard sink',
            )

    countertop = BoolProperty(
            name="Countertop",
            description="Create a countertop automatically (only default cabinet height)",
            default=True,
            )
    counterheight = FloatProperty(
            name='height', min=0.001, max=10,
            default=0.02, precision=3,
            description='Countertop height',
            )
    counterextend = FloatProperty(
            name='extend', min=0.001, max=10,
            default=0.03, precision=3,
            description='Countertop extent',
            )

    fitZ = BoolProperty(
            name="Floor origin in Z=0",
            description="Use Z=0 axis as vertical origin floor position",
            default=True,
            )
    moveZ = FloatProperty(
            name='Z position', min=0.001, max=10,
            default=1.5, precision=3,
            description='Wall cabinet Z position from floor',
            )

    cabinet_num = IntProperty(
            name='Number of Cabinets', min=1, max=30,
            default=1,
            description='Number total of cabinets in the Kitchen',
            )
    cabinets = CollectionProperty(type=CabinetProperties)

    # Materials
    crt_mat = BoolProperty(
            name="Create default Cycles materials",
            description="Create default materials for Cycles render",
            default=True,
            )

    # -----------------------------------------------------
    # Draw (create UI interface)
    # -----------------------------------------------------
    # noinspection PyUnusedLocal
    def draw(self, context):
        layout = self.layout
        space = bpy.context.space_data
        if not space.local_view:
            # Imperial units warning
            if bpy.context.scene.unit_settings.system == "IMPERIAL":
                row = layout.row()
                row.label("Warning: Imperial units not supported", icon='COLOR_RED')

            box = layout.box()
            row = box.row()
            row.prop(self, 'type_cabinet')

            row.prop(self, 'thickness')
            row = box.row()
            row.prop(self, 'depth')
            row.prop(self, 'height')
            row = box.row()
            row.prop(self, 'handle')
            if self.handle != "9":
                row.prop(self, 'handle_x')
                row.prop(self, 'handle_z')

            if self.type_cabinet == "1":
                row = box.row()
                row.prop(self, "countertop")
                if self.countertop:
                    row.prop(self, "counterheight")
                    row.prop(self, "counterextend")
                row = box.row()
                row.prop(self, 'baseboard')
                if self.baseboard:
                    row.prop(self, 'baseheight')
                    row.prop(self, 'basefactor', slider=True)

            row = box.row()
            row.prop(self, 'fitZ')
            if self.type_cabinet == "2":
                row.prop(self, 'moveZ')

            # Cabinet number
            row = box.row()
            row.prop(self, 'cabinet_num')
            # Add menu for cabinets
            if self.cabinet_num > 0:
                for idx in range(0, self.cabinet_num):
                    box = layout.box()
                    add_cabinet(self, box, idx + 1, self.cabinets[idx])

            box = layout.box()
            if not context.scene.render.engine == 'CYCLES':
                box.enabled = False
            box.prop(self, 'crt_mat')
        else:
            row = layout.row()
            row.label("Warning: Operator does not work in local view mode", icon='ERROR')

    # -----------------------------------------------------
    # Execute
    # -----------------------------------------------------
    # noinspection PyUnusedLocal
    def execute(self, context):
        if bpy.context.mode == "OBJECT":
            # Set default values
            if self.oldtype != self.type_cabinet:
                if self.type_cabinet == "1":  # Floor
                    self.depth = 0.59
                    self.height = 0.70

                if self.type_cabinet == "2":  # Wall
                    self.depth = 0.35
                    self.height = 0.70
                self.oldtype = self.type_cabinet

            # Create all elements
            for i in range(len(self.cabinets) - 1, self.cabinet_num):
                self.cabinets.add()

            # Create cabinets
            create_kitchen_mesh(self)
            return {'FINISHED'}
        else:
            self.report({'WARNING'}, "Archimesh: Option only valid in Object mode")
            return {'CANCELLED'}


# -----------------------------------------------------
# Add cabinet parameters
# -----------------------------------------------------
def add_cabinet(self, box, num, cabinet):
    doortype = cabinet.dType
    row = box.row()
    row.label("Cabinet " + str(num))
    row.prop(cabinet, 'sX')

    row = box.row()
    row.prop(cabinet, 'wY')
    row.prop(cabinet, 'wZ')
    row.prop(cabinet, 'rotate')

    row = box.row()
    row.prop(cabinet, 'pX')
    row.prop(cabinet, 'pY')
    row.prop(cabinet, 'pZ')

    row = box.row()
    row.prop(cabinet, 'dType')
    if doortype == "7":  # Drawers
        row.prop(cabinet, 'dNum')  # drawers number
    else:
        row.prop(cabinet, 'sNum')  # shelves number
    # Glass ratio
    if doortype == "4" or doortype == "5" or doortype == "6" or doortype == "11":
        row.prop(cabinet, 'gF', slider=True)  # shelves number
    # Handle
    row = box.row()
    if self.handle != "9":
        row.prop(cabinet, 'hand')
    if self.baseboard and self.type_cabinet == "1":
        row.prop(cabinet, 'bL')
        row.prop(cabinet, 'bR')

    if self.countertop and self.type_cabinet == "1":
        row = box.row()
        row.prop(cabinet, 'tC')
        row.prop(cabinet, 'tE')


# ------------------------------------------------------------------------------
# Generate mesh data
# All custom values are passed using self container (self.myvariable)
# ------------------------------------------------------------------------------
def create_kitchen_mesh(self):
    # deactivate others
    for o in bpy.data.objects:
        if o.select is True:
            o.select = False
    bpy.ops.object.select_all(False)
    # Create cabinets
    generate_cabinets(self)

    return


# ------------------------------------------------------------------------------
# Generate cabinet
# All custom values are passed using self container (self.myvariable)
# ------------------------------------------------------------------------------
def generate_cabinets(self):

    boxes = []
    bases = []
    location = bpy.context.scene.cursor_location
    myloc = copy(location)  # copy location to keep 3D cursor position
    # Fit to floor
    if self.fitZ:
        myloc[2] = 0
    # Move to wall position
    if self.type_cabinet == "2":  # wall
        myloc[2] = myloc[2] + self.moveZ
    # Baseboard
    if self.type_cabinet == "1" and self.baseboard:  # floor
        myloc[2] = myloc[2] + self.baseheight  # add baseboard position for bottom

    # Create cabinets
    lastx = myloc[0]
    lasty = myloc[1]
    lastrot = 0  # last rotation
    # ------------------------------------------------------------------------------
    # Cabinets
    #
    # By default all cabinets are created in X axis and later are rotated if needed
    # the default rotation means keep last rotation, not 0, so if the previous
    # cabinet is 90CW, the next one will be the same. To back to 0, you must select
    # 90 CCW.
    # ------------------------------------------------------------------------------
    for i in range(0, self.cabinet_num):
        mydata = create_box(self.type_cabinet, "Cabinet" + str(i + 1),
                            self.thickness,
                            self.cabinets[i].sX, self.depth + self.cabinets[i].wY, self.height + self.cabinets[i].wZ,
                            self.cabinets[i].pX + lastx,
                            self.cabinets[i].pY + lasty,
                            myloc[2] + self.cabinets[i].pZ,
                            self.cabinets[i].dType, self.cabinets[i].dNum, self.cabinets[i].sNum, self.cabinets[i].gF,
                            self.crt_mat,
                            self.cabinets[i].hand, self.handle, self.handle_x, self.handle_z, self.depth)
        mybox = mydata[0]
        # LastX is the sum of initial position + width of the cabinet.
        lastx = mydata[1]
        # add SKU property
        sku = createunitsku(self, self.cabinets[i])
        mydata[0]["archimesh.sku"] = sku

        # Save rotation type
        myrotationtype = int(self.cabinets[i].rotate)

        # ---------------------------------------------------------
        # Calculate new rotation angle
        #
        # ---------------------------------------------------------
        myrot = lastrot
        # ----------
        # Default
        # ----------
        if myrotationtype == RotationType_Default:
            myrot = myrot  # do no change rotation
        # ----------
        # 90 CW
        # ----------
        if myrotationtype == RotationType_R90CW:
            myrot += -pi / 2
        # ----------
        # 90 CCW
        # ----------
        if myrotationtype == RotationType_R90CCW:
            myrot += pi / 2
        # ----------
        # 180
        # ----------
        if myrotationtype == RotationType_R180:
            myrot = myrot + pi

        # Save the rotation for next cabinet
        lastrot = myrot
        angle = myrot - ((2 * pi) * (myrot // (2 * pi)))  # clamp one revolution

        # -------------------------------------------
        # Countertop (only default height cabinet)
        # 9-Default, 1-90CW, 2-90CCW, 3-180
        # -------------------------------------------
        if self.countertop and self.type_cabinet == "1" and self.cabinets[i].wZ == 0:
            w = self.cabinets[i].sX
            # fill (depend on orientation)
            if self.cabinets[i].tC:
                # 0 or 180 degrees
                if angle == 0 or angle == pi:
                    w += fabs(self.cabinets[i].pX)
                # 90 or 270 degrees
                if angle == (3 * pi) / 2 or angle == pi / 2:
                    w += fabs(self.cabinets[i].pY)

            mycountertop = create_countertop("Countertop" + str(i + 1),
                                             w,
                                             self.depth + self.cabinets[i].wY,
                                             self.counterheight, self.counterextend,
                                             self.crt_mat, self.cabinets[i].dType, self.depth,
                                             self.cabinets[i].tE)
            # -------------------------------
            # Fill countertop spaces
            # -------------------------------
            if self.cabinets[i].tC:
                # Default
                if angle == 0:
                    if self.cabinets[i].pX >= 0:
                        mycountertop.location[0] = -self.cabinets[i].pX
                    else:
                        mycountertop.location[0] = 0

                # 90CW
                if angle == (3 * pi) / 2:
                    if self.cabinets[i].pY >= 0:
                        mycountertop.location[0] = 0
                    else:
                        mycountertop.location[0] = self.cabinets[i].pY
                # 90CCW
                if angle == pi / 2:
                    if self.cabinets[i].pY >= 0:
                        mycountertop.location[0] = self.cabinets[i].pY * -1
                    else:
                        mycountertop.location[0] = 0
                # 180
                if angle == pi:
                    mycountertop.location[0] = 0

            mycountertop.location[2] = self.height
            mycountertop.parent = mydata[0]
            # --------------------
            # add countertop SKU
            # --------------------
            t = w
            # if corner, remove size
            if self.cabinets[i].dType == "9" or self.cabinets[i].dType == "10":
                t = t - self.cabinets[i].sX

            mycountertop["archimesh.top_sku"] = "T%06.3fx%06.3fx%06.3f-%06.3f" % (t,
                                                                                  self.depth + self.cabinets[
                                                                                      i].wY + self.counterextend,
                                                                                  self.counterheight,
                                                                                  w)
        # ----------------
        # Baseboard
        # ----------------
        if self.baseboard and self.type_cabinet == "1":
            gap = (self.depth + self.cabinets[i].wY) - ((self.depth + self.cabinets[i].wY) * self.basefactor)
            mybase = create_baseboard("Baseboard" + str(i + 1),
                                      self.cabinets[i].sX, self.thickness, self.baseheight,
                                      self.crt_mat, self.cabinets[i].bL, self.cabinets[i].bR,
                                      (self.depth + self.cabinets[i].wY) * self.basefactor, self.cabinets[i].dType, gap)
            bases.extend([mybase])
            mybase.location[1] = (self.depth + self.cabinets[i].wY) * self.basefactor * -1
            mybase.location[2] = -self.baseheight
            mybase.parent = mydata[0]
            # --------------------
            # add base SKU
            # --------------------
            t = self.cabinets[i].sX
            # Add sides
            if self.cabinets[i].bR is True:
                t = t + (self.depth + self.cabinets[i].wY) * self.basefactor
            if self.cabinets[i].bL is True:
                t = t + (self.depth + self.cabinets[i].wY) * self.basefactor

            mybase["archimesh.base_sku"] = "B%06.3fx%06.3fx%06.3f" % (t, self.thickness, self.baseheight)

        # Rotate
        mybox.rotation_euler = (0, 0, myrot)

        # -----------------------------------------
        # Calculate new position for next cabinet
        # -----------------------------------------
        xm = 0
        ym = 0

        # 0 degrees
        if angle == 0:
            lastx = lastx
            lasty = lasty
        # 90 degrees
        if angle == pi / 2:
            ym = -self.cabinets[i].sX
            lastx = lastx - self.cabinets[i].sX - self.cabinets[i].pX
            lasty = lasty + self.cabinets[i].sX + self.cabinets[i].pY
        # 180 degrees
        if angle == pi:
            lastx -= 2 * (self.cabinets[i].sX + self.cabinets[i].pX)
        # 270 degrees
        if angle == (3 * pi) / 2:
            xm = self.depth - self.counterextend
            lastx = lastx - self.cabinets[i].sX - self.cabinets[i].pX
            lasty = lasty - self.cabinets[i].sX - self.cabinets[i].pX + self.cabinets[i].pY

        myl = mybox.location
        # noinspection PyUnresolvedReferences
        mybox.location = (myl.x + xm, myl.y + ym, myl.z)

        # ---------------------------------------
        # Save box
        # ---------------------------------------
        boxes.extend([mybox])

    # refine cabinets
    for box in boxes:
        remove_doubles(box)
        set_normals(box)

    # refine baseboard
    for base in bases:
        remove_doubles(base)
        set_normals(base)

    # Create materials
    if self.crt_mat and bpy.context.scene.render.engine == 'CYCLES':
        mat = create_diffuse_material("Cabinet_material", False, 0.8, 0.8, 0.8)
        for box in boxes:
            set_material(box, mat)

    return


# ------------------------------------------------------------------------------
# Create cabinet box
#
# thickness: wood thickness
# sX: Size in X axis
# sY: Size in Y axis
# sZ: Size in Z axis
# pX: position X axis
# pY: position Y axis
# pZ: position Z axis
# doorType: Type of door or drawers
# drawers: Number of drawers
# shelves: Number of shelves
# gF: Glass size factor
# mat: Flag for creating materials
# handle: handle visibility flag
# handle_model: Type of handle
# handle_x: Position of handle in X axis
# handle_z: Position of handle in Z axis
# depth: Default depth
# ------------------------------------------------------------------------------
def create_box(type_cabinet, objname, thickness, sx, sy, sz, px, py, pz, doortype, drawers, shelves, gf, mat,
               handle, handle_model, handle_x, handle_z, depth):
    myvertex = []
    myfaces = []
    # external faces
    myvertex.extend(
        [(0, 0, 0), (0, -sy, 0), (0, -sy, sz), (0, 0, sz), (sx, 0, 0), (sx, -sy, 0), (sx, -sy, sz), (sx, 0, sz)])
    myfaces.extend([(0, 1, 2, 3), (4, 5, 6, 7), (0, 4, 7, 3), (0, 1, 5, 4), (3, 2, 6, 7)])

    # internal faces
    myvertex.extend([(thickness, -thickness, thickness), (thickness, -sy, thickness),
                     (thickness, -sy, sz - thickness), (thickness, -thickness, sz - thickness),
                     (sx - thickness, -thickness, thickness), (sx - thickness, -sy, thickness),
                     (sx - thickness, -sy, sz - thickness), (sx - thickness, -thickness, sz - thickness)])

    myfaces.extend([(8, 9, 10, 11), (12, 13, 14, 15), (8, 12, 15, 11), (8, 9, 13, 12), (11, 10, 14, 15)])
    myfaces.extend([(1, 9, 10, 2), (2, 6, 14, 10), (6, 5, 13, 14), (5, 1, 9, 13)])

    # -----------------
    # shelves
    # -----------------
    v = 16  # vertice number
    if doortype != "7":  # Drawers
        # calculate separation
        dist = sz - (thickness * 2)
        space = dist / (shelves + 1)
        posz1 = thickness + space

        for x in range(shelves):
            posz2 = posz1 - thickness
            myvertex.extend([(thickness, -thickness, posz1), (thickness, -sy, posz1),
                             (thickness, -sy, posz2), (thickness, -thickness, posz2),
                             (sx - thickness, -thickness, posz1), (sx - thickness, -sy, posz1),
                             (sx - thickness, -sy, posz2), (sx - thickness, -thickness, posz2)])

            myfaces.extend([(v, v + 1, v + 2, v + 3), (v + 4, v + 5, v + 6, v + 7), (v, v + 4, v + 7, v + 3),
                            (v, v + 1, v + 5, v + 4), (v + 3, v + 2, v + 6, v + 7), (v + 1, v + 2, v + 6, v + 5)])
            v += 8
            posz1 += space

    mymesh = bpy.data.meshes.new(objname)
    myobject = bpy.data.objects.new(objname, mymesh)

    myobject.location[0] = px
    myobject.location[1] = py
    myobject.location[2] = pz
    bpy.context.scene.objects.link(myobject)

    mymesh.from_pydata(myvertex, [], myfaces)
    mymesh.update(calc_edges=True)

    # ---------------------------------------
    # Drawers
    # ---------------------------------------
    if doortype == "7":  # Drawers
        # calculate separation
        gap = 0.001
        dist = sz - (gap * drawers)
        space = dist / drawers
        posz1 = 0

        for x in range(drawers):
            mydrawer = create_drawer("Drawer", thickness, sx, sy, space, mat, handle,
                                     handle_model, handle_z)
            mydrawer.location[1] = -sy
            mydrawer.location[2] = posz1
            mydrawer.parent = myobject
            remove_doubles(mydrawer)
            set_normals(mydrawer)
            posz1 = posz1 + space + gap  # gap

    # ---------------------------------------
    # Doors
    # ---------------------------------------
    if doortype != "99" and doortype != "7":  # None or Drawers
        if doortype == "1" or doortype == "2" or doortype == "3" or doortype == "4" \
                or doortype == "5" or doortype == "6":  # single door
            mydoor = create_door(type_cabinet, objname + "_Door", thickness, sx, sz, doortype, gf, mat, handle,
                                 handle_model, handle_x, handle_z, 0.001)
            mydoor.parent = myobject
            mydoor.location[1] = -sy - 0.001  # add 1 mm gap
            remove_doubles(mydoor)
            set_normals(mydoor)

        else:  # double doors
            if doortype == "8" or doortype == "10" or doortype == "11":
                # Glass or not
                if doortype != "11":
                    typ = "2"
                else:
                    typ = "5"

                # Adjust corner doors
                dwidth = sx / 2
                if doortype == "10":
                    dwidth = sx - depth - thickness - 0.001

                mydoor1 = create_door(type_cabinet, objname + "_Door_L", thickness, dwidth, sz, typ, gf, mat,
                                      handle,
                                      handle_model, handle_x, handle_z, 0.0005)  # left
                mydoor1.location[1] = -sy - 0.001  # add 1 mm gap
                mydoor1.parent = myobject
                remove_doubles(mydoor1)
                set_normals(mydoor1)

            if doortype == "8" or doortype == "9" or doortype == "11":
                # Glass or not
                if doortype != "11":
                    typ = "1"
                else:
                    typ = "4"

                # Adjust corner doors
                dwidth = sx / 2
                if doortype == "9":
                    dwidth = sx - depth - thickness - 0.001

                mydoor2 = create_door(type_cabinet, objname + "_Door_R", thickness, dwidth, sz, typ, gf, mat,
                                      handle,
                                      handle_model, handle_x, handle_z, 0.0005)  # right
                mydoor2.location[1] = -sy - 0.001  # add 1 mm gap
                mydoor2.location[0] = sx
                mydoor2.parent = myobject
                remove_doubles(mydoor2)
                set_normals(mydoor2)

    return myobject, px + sx


# ------------------------------------------------------------------------------
# Create baseboard
#
# sX: Size in X axis
# sY: Size in Y axis
# sZ: Size in Z axis
# mat: Flag for creating materials
# bL: Flag to create left side
# bR: Flag to create right side
# depth: depth or position of baseboard
# gap: space to close in corners
# ------------------------------------------------------------------------------
def create_baseboard(objname, sx, sy, sz, mat, bl, br, depth, doortype, gap):
    myvertex = []
    myfaces = []
    p = 0
    # external faces
    myvertex.extend(
        [(0, 0, 0), (0, -sy, 0), (0, -sy, sz), (0, 0, sz), (sx, 0, 0), (sx, -sy, 0), (sx, -sy, sz), (sx, 0, sz)])
    myfaces.extend([(0, 1, 2, 3), (4, 5, 6, 7), (0, 4, 7, 3), (0, 1, 5, 4), (3, 2, 6, 7), (1, 5, 6, 2)])
    # left side
    f = 8
    if bl:
        myvertex.extend(
            [(0, 0, 0), (0, depth, 0), (0, depth, sz), (0, 0, sz), (sy, 0, 0), (sy, depth, 0), (sy, depth, sz),
             (sy, 0, sz)])
        myfaces.extend(
            [(f, f + 1, f + 2, f + 3), (f + 4, f + 5, f + 6, f + 7), (f, f + 4, f + 7, f + 3), (f, f + 1, f + 5, f + 4),
             (f + 3, f + 2, f + 6, f + 7), (f + 1, f + 5, f + 6, f + 2)])
        f += 8
    # right side
    if br:
        p = sx - sy
        myvertex.extend([(p, 0, 0), (p, depth, 0), (p, depth, sz), (p, 0, sz), (p + sy, 0, 0), (p + sy, depth, 0),
                         (p + sy, depth, sz), (p + sy, 0, sz)])
        myfaces.extend(
            [(f, f + 1, f + 2, f + 3), (f + 4, f + 5, f + 6, f + 7), (f, f + 4, f + 7, f + 3), (f, f + 1, f + 5, f + 4),
             (f + 3, f + 2, f + 6, f + 7), (f + 1, f + 5, f + 6, f + 2)])
        f += 8
    # Corners
    if doortype == "9" or doortype == "10":
        if doortype == "9":
            p = depth + sy
        if doortype == "10":
            p = sx - depth - sy

        size = gap * -2
        myvertex.extend([(p, -sy, 0), (p, size, 0), (p, size, sz), (p, -sy, sz), (p + sy, -sy, 0), (p + sy, size, 0),
                         (p + sy, size, sz), (p + sy, -sy, sz)])
        myfaces.extend(
            [(f, f + 1, f + 2, f + 3), (f + 4, f + 5, f + 6, f + 7), (f, f + 4, f + 7, f + 3), (f, f + 1, f + 5, f + 4),
             (f + 3, f + 2, f + 6, f + 7), (f + 1, f + 5, f + 6, f + 2)])

    mymesh = bpy.data.meshes.new(objname)
    mybaseboard = bpy.data.objects.new(objname, mymesh)

    mybaseboard.location[0] = 0
    mybaseboard.location[1] = 0
    mybaseboard.location[2] = 0
    bpy.context.scene.objects.link(mybaseboard)

    mymesh.from_pydata(myvertex, [], myfaces)
    mymesh.update(calc_edges=True)

    # Material
    if mat and bpy.context.scene.render.engine == 'CYCLES':
        mat = create_diffuse_material("Baseboard_material", False, 0.8, 0.8, 0.8)
        set_material(mybaseboard, mat)

    return mybaseboard


# ------------------------------------------------------------------------------
# Create Countertop
#
# sX: Size in X axis
# sY: Size in Y axis
# sZ: Size in Z axis
# mat: Flag for creating materials
# doorType: Type of door
# depth: Depth of the cabinets
# edge: add countertop edge
# ------------------------------------------------------------------------------
def create_countertop(objname, sx, sy, sz, over, mat, doortype, depth, edge):
    oy = 0.02
    oz = 0.05 + sz

    myvertex = []
    myfaces = []
    # if corner the size is less
    ts = 0
    tx = sx

    if doortype == "9":
        ts = sx - (sx - over - depth)
        tx = sx

    if doortype == "10":
        ts = 0
        tx = sx - over - depth

    # external faces
    myvertex.extend([(ts, 0, 0), (ts, -sy - over, 0), (ts, -sy - over, sz), (ts, 0, sz),
                     (tx, 0, 0), (tx, -sy - over, 0), (tx, -sy - over, sz), (tx, 0, sz)])
    myfaces.extend([(0, 1, 2, 3), (4, 5, 6, 7), (0, 4, 7, 3), (0, 1, 5, 4), (3, 2, 6, 7), (1, 5, 6, 2)])
    # Back
    ts = 0
    tx = sx

    if doortype == "9":
        ts = oy

    if doortype == "10":
        tx -= oy
    # Add edge
    if edge is True:
        myvertex.extend([(ts, 0, sz), (ts, -oy, sz), (ts, -oy, oz), (ts, 0, oz),
                         (tx, 0, sz), (tx, -oy, sz), (tx, -oy, oz), (tx, 0, oz)])
        myfaces.extend(
            [(8, 9, 10, 11), (12, 13, 14, 15), (8, 12, 15, 11), (8, 9, 13, 12), (11, 10, 14, 15), (9, 13, 14, 10)])

    mymesh = bpy.data.meshes.new(objname)
    mycountertop = bpy.data.objects.new(objname, mymesh)

    mycountertop.location[0] = 0
    mycountertop.location[1] = 0
    mycountertop.location[2] = 0
    bpy.context.scene.objects.link(mycountertop)

    mymesh.from_pydata(myvertex, [], myfaces)
    mymesh.update(calc_edges=True)

    # Material
    if mat and bpy.context.scene.render.engine == 'CYCLES':
        mat = create_diffuse_material("countertop_material", False, 0, 0, 0, 0.2, 0.2, 0.2, 0.15)
        set_material(mycountertop, mat)

    return mycountertop


# ------------------------------------------------------------------------------
# Create cabinet door
#
# type_cabinet: Type of cabinet (floor or wall)
# objName: Name of the created object
# thickness: wood thickness
# sX: Size in X axis
# sY: Size in Y axis
# sZ: Size in Z axis
# doorType: Type of door or drawers
# gF: Glass size factor
# mat: Flag for creating materials
# handle: handle visibility flag
# handle_model: Type of handle
# handle_x: Position of handle in X axis
# handle_z: Position of handle in Z axis
# gapX: size of the horizontal gap
# ------------------------------------------------------------------------------
def create_door(type_cabinet, objname, thickness, sx, sz, doortype, gf, mat, handle, handle_model, handle_x,
                handle_z, gapx):

    myvertex = []
    myfaces = []

    # Left open
    f = -1  # right
    if doortype == "2" or doortype == "5" or doortype == "10":
        f = 1
    # add small gap in width
    sx = sx - gapx
    # add small gap in top zone
    sz -= 0.002
    # External Frame
    myvertex.extend([(0, 0, 0), (0, -thickness, 0), (0, -thickness, sz), (0, 0, sz), (sx * f, 0, 0),
                     (sx * f, -thickness, 0), (sx * f, -thickness, sz), (sx * f, 0, sz)])
    myfaces.extend([(0, 1, 2, 3), (4, 5, 6, 7), (0, 1, 5, 4), (3, 2, 6, 7)])
    # ---------------
    # Solid door
    # ---------------
    if doortype == "1" or doortype == "2" or doortype == "3" \
            or doortype == "8" or doortype == "9" or doortype == "10":
        myfaces.extend([(0, 4, 7, 3), (1, 2, 6, 5)])
    # ---------------
    # Glass door
    # ---------------
    if doortype == "4" or doortype == "5" or doortype == "6" or doortype == "11":
        w = sx * gf  # calculate frame size W
        h = sz * gf  # calculate frame size V

        myvertex.extend([(w * f, 0, h), (w * f, -thickness, h), (w * f, -thickness, sz - h), (w * f, 0, sz - h),
                         ((sx - w) * f, 0, h),
                         ((sx - w) * f, -thickness, h), ((sx - w) * f, -thickness, sz - h), ((sx - w) * f, 0, sz - h)])
        myfaces.extend([(8, 9, 10, 11), (12, 13, 14, 15), (8, 11, 15, 12), (10, 11, 15, 14), (8, 12, 13, 9),
                        (1, 9, 10, 2), (5, 13, 14, 6), (6, 2, 10, 14), (5, 1, 9, 13),
                        (0, 3, 11, 8), (12, 15, 7, 4), (4, 0, 8, 12), (11, 3, 7, 15)])

    mymesh = bpy.data.meshes.new(objname)
    mydoor = bpy.data.objects.new(objname, mymesh)
    if f == -1:
        mydoor.location[0] = sx
    else:
        mydoor.location[0] = 0

    mydoor.location[1] = 0
    mydoor.location[2] = 0
    bpy.context.scene.objects.link(mydoor)

    mymesh.from_pydata(myvertex, [], myfaces)
    mymesh.update(calc_edges=True)

    # ----------------------------------------------
    # Handles
    #    RT: Put handle in right side top
    #    LT: Put handle in left side top
    #    RB: Put handle in right side bottom
    #    LB: Put handle in left side bottom
    #    T: Put handle in top side middle
    #    B: Put handle in bottom side middle
    #
    # The position is reverse to the open direction
    # of the door
    # ----------------------------------------------
    hpos = "RT"  # Right by default
    if handle:
        # -----------------
        # Floor units
        # -----------------
        if type_cabinet == "1":
            if doortype == "1" or doortype == "4" or doortype == "9":  # Right
                hpos = "LT"
            if doortype == "2" or doortype == "5" or doortype == "10":  # Left
                hpos = "RT"
            if doortype == "3" or doortype == "6":
                hpos = "T"
        # -----------------
        # Wall units
        # -----------------
        if type_cabinet == "2":
            if doortype == "1" or doortype == "4" or doortype == "9":  # Right
                hpos = "LB"
            if doortype == "2" or doortype == "5" or doortype == "10":  # Left
                hpos = "RB"
            if doortype == "3" or doortype == "6":
                hpos = "B"

        create_handle(handle_model, mydoor, thickness, hpos, mat, handle_x, handle_z)

    if mat and bpy.context.scene.render.engine == 'CYCLES':
        # Door material
        mat = create_diffuse_material("Door_material", False, 0.8, 0.8, 0.8, 0.279, 0.337, 0.6, 0.2)
        set_material(mydoor, mat)
        # Add Glass
        if doortype == "4" or doortype == "5" or doortype == "6" or doortype == "11":
            mat = create_glass_material("DoorGlass_material", False)
            mydoor.data.materials.append(mat)
            select_faces(mydoor, 6, True)
            set_material_faces(mydoor, 1)

    # Limit rotation axis
    if hpos != "T" and hpos != "TM" and hpos != "B":
        mydoor.lock_rotation = (True, True, False)

    return mydoor


# ------------------------------------------------------------------------------
# Create drawers
#
# thickness: wood thickness
# sX: Size in X axis
# sY: Size in Y axis
# sZ: Size in Z axis
# mat: Flag for creating materials
# handle: handle visibility flag
# handle_model: Type of handle
# handle_z: Position of handle in Z axis
# ------------------------------------------------------------------------------
def create_drawer(objname, thickness, sx, sy, sz, mat, handle, handle_model, handle_z):
    myvertex = []
    myfaces = []
    # Front face
    myvertex.extend([(0, 0, 0), (0, -thickness, 0), (0, -thickness, sz), (0, 0, sz), (sx, 0, 0), (sx, -thickness, 0),
                     (sx, -thickness, sz), (sx, 0, sz)])
    myfaces.extend([(0, 1, 2, 3), (4, 5, 6, 7), (0, 4, 7, 3), (0, 1, 5, 4), (3, 2, 6, 7), (1, 2, 6, 5)])

    # internal faces (thickness cm gap)
    myvertex.extend([(thickness, 0, thickness),
                     (thickness, sy - thickness, thickness),
                     (sx - thickness, sy - thickness, thickness),
                     (sx - thickness, 0, thickness),
                     (thickness * 2, 0, thickness),
                     (thickness * 2, sy - thickness * 2, thickness),
                     (sx - thickness * 2, sy - thickness * 2, thickness),
                     (sx - thickness * 2, 0, thickness)])

    myfaces.extend([(8, 9, 13, 12), (13, 9, 10, 14), (14, 10, 11, 15), (12, 13, 14, 15)])
    h = sz * 0.7
    myvertex.extend([(thickness, 0, h),
                     (thickness, sy - thickness, h),
                     (sx - thickness, sy - thickness, h),
                     (sx - thickness, 0, h),
                     (thickness * 2, 0, h),
                     (thickness * 2, sy - thickness * 2, h),
                     (sx - thickness * 2, sy - thickness * 2, h),
                     (sx - thickness * 2, 0, h)])
    myfaces.extend(
        [(16, 17, 21, 20), (21, 17, 18, 22), (22, 18, 19, 23), (8, 9, 17, 16), (9, 10, 18, 17), (10, 11, 19, 18),
         (12, 13, 21, 20), (13, 14, 22, 21), (14, 15, 23, 22)])

    mymesh = bpy.data.meshes.new(objname)
    mydrawer = bpy.data.objects.new(objname, mymesh)

    mydrawer.location[0] = 0
    mydrawer.location[1] = 0
    mydrawer.location[2] = 0
    bpy.context.scene.objects.link(mydrawer)

    mymesh.from_pydata(myvertex, [], myfaces)
    mymesh.update(calc_edges=True)

    # Handle
    if handle:
        model = handle_model
        # Drawers always horizontal handle, so override values
        if model == "1":
            model = "3"

        if model == "4":
            model = "2"

        create_handle(model, mydrawer, thickness, "TM", mat, 0, handle_z)  # always in the top area/middle

    # Material
    if mat and bpy.context.scene.render.engine == 'CYCLES':
        mat = create_diffuse_material("Drawer_material", False, 0.8, 0.8, 0.8, 0.6, 0.6, 0.6, 0.2)
        set_material(mydrawer, mat)

    # Lock transformation
    mydrawer.lock_location = (True, False, True)  # only Y axis

    return mydrawer


# ------------------------------------------------------------------------------
# Create Handles
#
# model: handle model
# myDoor: Door that has the handle
# thickness: thickness of board
# handle_position: position of the handle
#    RT: Put handle in right side top
#    LT: Put handle in left side top
#    RB: Put handle in right side bottom
#    LB: Put handle in left side bottom
#    T: Put handle in top side middle
#    TM: Put handle in top side middle (drawers)
#    B: Put handle in bottom side middle
# mat: create default cycles material
# handle_x: Position of handle in X axis
# handle_z: Position of handle in Z axis
# ------------------------------------------------------------------------------
def create_handle(model, mydoor, thickness, handle_position, mat, handle_x, handle_z):
    if model == "9":
        return None

    # Retry mesh data
    if model == "1" or model == "3":
        mydata = handle_model_01()
    elif model == "2" or model == "4":
        mydata = handle_model_02()
    elif model == "5":
        mydata = handle_model_05()
    elif model == "6":
        mydata = handle_model_06()
    elif model == "7":
        mydata = handle_model_07()
    elif model == "8":
        mydata = handle_model_08()
    else:
        mydata = handle_model_01()  # default model

    # move data
    myvertex = mydata[0]
    myfaces = mydata[1]

    mymesh = bpy.data.meshes.new("Handle")
    myhandle = bpy.data.objects.new("Handle", mymesh)

    bpy.context.scene.objects.link(myhandle)

    mymesh.from_pydata(myvertex, [], myfaces)
    mymesh.update(calc_edges=True)

    # Position handle
    myhandle.location.y = -thickness
    # Calculate dimensions
    if model == "1" or model == "4" or model == "5" or model == "6":
        width = myhandle.dimensions.z / 2
        height = myhandle.dimensions.x / 2
    else:
        width = myhandle.dimensions.x / 2
        height = myhandle.dimensions.z / 2
    # Limit handle position to door dimensions
    if handle_x + width > mydoor.dimensions.x:
        handle_x = mydoor.dimensions.x - 0.01

    if handle_z + height > mydoor.dimensions.z:
        handle_z = mydoor.dimensions.z - 0.01

    # Position in X axis
    if handle_position == "LT" or handle_position == "LB":
        myhandle.location.x = -mydoor.dimensions.x + handle_x + width

    if handle_position == "RT" or handle_position == "RB":
        myhandle.location.x = mydoor.dimensions.x - handle_x - width

    # Position in Z axis
    if handle_position == "RT" or handle_position == "LT":
        if mydoor.dimensions.z - handle_z - height > 1.2:
            myhandle.location.z = 1.2
        else:
            myhandle.location.z = mydoor.dimensions.z - handle_z - height

    if handle_position == "RB" or handle_position == "LB":
        myhandle.location.z = handle_z + height

    # Position for Middle point
    if handle_position == "T" or handle_position == "B":
        myhandle.location.x = -mydoor.dimensions.x / 2

    if handle_position == "TM":
        myhandle.location.x = mydoor.dimensions.x / 2

    if handle_position == "T" or handle_position == "TM":
        myhandle.location.z = mydoor.dimensions.z - handle_z - height

    if handle_position == "B":
        myhandle.location.z = handle_z - height

    # rotate
    if handle_position != "T" and handle_position != "B" and handle_position != "TM":
        yrot = 0
        if model == "1":
            yrot = pi / 2

        if model == "4":
            if handle_position == "LT" or handle_position == "LB":
                yrot = -pi / 2
            else:
                yrot = pi / 2

        myhandle.rotation_euler = (0, yrot, 0.0)  # radians PI=180

    # parent
    myhandle.parent = mydoor
    # Materials
    if mat and bpy.context.scene.render.engine == 'CYCLES':
        mat = create_glossy_material("Handle_material", False, 0.733, 0.779, 0.8, 0.733, 0.779, 0.8, 0.02)
        set_material(myhandle, mat)

    # Smooth
    if model == "1" or model == "3":
        set_smooth(myhandle)
        set_modifier_subsurf(myhandle)

    if model == "5" or model == "6" or model == "7" or model == "8":
        set_smooth(myhandle)

    return myhandle


# ----------------------------------------------
# Handle model 01
# ----------------------------------------------
def handle_model_01():
    # ------------------------------------
    # Mesh data
    # ------------------------------------
    minx = -0.07222598791122437
    maxx = 0.07222597301006317
    maxy = 6.545917585754069e-08
    minz = -0.004081448074430227
    maxz = 0.004081418737769127

    # Vertex
    myvertex = [(maxx - 0.013172730803489685, -0.025110241025686264, maxz - 0.0003106782678514719),
                (maxx - 0.01216559112071991, -0.027320515364408493, maxz - 0.0011954230722039938),
                (maxx - 0.011492643505334854, -0.028797375038266182, maxz - 0.0025195349007844925),
                (maxx - 0.011256333440542221, -0.029315980151295662, maxz - 0.0040814326939546675),
                (maxx - 0.011492643505334854, -0.02879737690091133, minz + 0.0025195364141836762),
                (maxx - 0.01216559112071991, -0.02732051908969879, minz + 0.0011954230722039938),
                (maxx - 0.013172730803489685, -0.025110244750976562, minz + 0.0003106798976659775),
                (maxx - 0.014360729604959488, -0.022503048181533813, minz),
                (maxx - 0.01554873213171959, -0.019895851612091064, minz + 0.00031067943200469017),
                (maxx - 0.016555871814489365, -0.017685577273368835, minz + 0.001195424236357212),
                (maxx - 0.01722881942987442, -0.016208721324801445, minz + 0.0025195354828611016),
                (maxx - 0.017465125769376755, -0.015690118074417114, minz + 0.00408143286244389),
                (maxx - 0.01722881942987442, -0.016208721324801445, maxz - 0.0025195367634296417),
                (maxx - 0.016555871814489365, -0.017685577273368835, maxz - 0.0011954237706959248),
                (maxx - 0.01554873213171959, -0.019895853474736214, maxz - 0.00031068059615790844),
                (maxx - 0.014360729604959488, -0.022503050044178963, maxz),
                (maxx - 0.00908602774143219, -0.022446047514677048, maxz - 0.0003106782678514719),
                (maxx - 0.007382020354270935, -0.024176951497793198, maxz - 0.0011954226065427065),
                (maxx - 0.006243452429771423, -0.025333505123853683, maxz - 0.002519535133615136),
                (maxx - 0.005843624472618103, -0.025739632546901703, maxz - 0.004081432702012222),
                (maxx - 0.006243452429771423, -0.025333506986498833, minz + 0.0025195362977683544),
                (maxx - 0.007382020354270935, -0.024176953360438347, minz + 0.0011954230722039938),
                (maxx - 0.00908602774143219, -0.022446051239967346, minz + 0.0003106798976659775),
                (maxx - 0.011096026748418808, -0.020404310896992683, minz),
                (maxx - 0.013106036931276321, -0.01836257427930832, minz + 0.0003106796648353338),
                (maxx - 0.014810033142566681, -0.01663167029619217, minz + 0.001195424236357212),
                (maxx - 0.015948612242937088, -0.015475118532776833, minz + 0.0025195355992764235),
                (maxx - 0.016348421573638916, -0.015068991109728813, minz + 0.004081432861045897),
                (maxx - 0.015948612242937088, -0.015475118532776833, maxz - 0.00251953664701432),
                (maxx - 0.014810033142566681, -0.01663167029619217, maxz - 0.0011954233050346375),
                (maxx - 0.013106033205986023, -0.01836257241666317, maxz - 0.0003106803633272648),
                (maxx - 0.011096026748418808, -0.020404312759637833, maxz - 4.656612873077393e-10),
                (maxx - 0.004618480801582336, -0.01468262542039156, maxz - 0.0008190707303583622),
                (maxx - 0.002191290259361267, -0.014774298295378685, maxz - 0.001584529411047697),
                (maxx - 0.0005694925785064697, -0.014835557900369167, maxz - 0.002730117877945304),
                (maxx, -0.014857066795229912, maxz - 0.004081432337202706),
                (maxx - 0.0005694925785064697, -0.014835558831691742, minz + 0.002730119973421097),
                (maxx - 0.002191290259361267, -0.014774300158023834, minz + 0.001584530808031559),
                (maxx - 0.004618480801582336, -0.01468262542039156, minz + 0.0008190732914954424),
                (maxx - 0.0074815452098846436, -0.014574488624930382, minz + 0.000550281023606658),
                (maxx - 0.010344602167606354, -0.014466354623436928, minz + 0.0008190732914954424),
                (maxx - 0.012771788984537125, -0.01437467709183693, minz + 0.0015845317393541336),
                (maxx - 0.014393582940101624, -0.01431342400610447, minz + 0.002730119158513844),
                (maxx - 0.014963079243898392, -0.014291912317276001, maxz - 0.004081433403984924),
                (maxx - 0.014393582940101624, -0.01431342400610447, maxz - 0.0027301193913444877),
                (maxx - 0.012771788984537125, -0.014374678023159504, maxz - 0.0015845298767089844),
                (maxx - 0.010344602167606354, -0.014466352760791779, maxz - 0.0008190723601728678),
                (maxx - 0.0074815452098846436, -0.014574489556252956, maxz - 0.0005502800922840834),
                (maxx - 0.004618480801582336, maxy - 2.029310053330846e-11, maxz - 0.0008190718945115805),
                (maxx - 0.002191290259361267, maxy - 7.808864666003501e-11, maxz - 0.0015845305752009153),
                (maxx - 0.0005694925785064697, maxy - 1.645759084567544e-10, maxz - 0.002730119042098522),
                (maxx, maxy - 2.665956344571896e-10, minz + 0.004081433353314345),
                (maxx - 0.0005694925785064697, maxy - 3.686153604576248e-10, minz + 0.0027301188092678785),
                (maxx - 0.002191290259361267, maxy - 4.5510972768170177e-10, minz + 0.0015845296438783407),
                (maxx - 0.004618480801582336, maxy - 5.128981683810707e-10, minz + 0.0008190721273422241),
                (maxx - 0.0074815452098846436, maxy - 5.331912689143792e-10, minz + 0.0005502798594534397),
                (maxx - 0.010344602167606354, maxy - 5.128981683810707e-10, minz + 0.0008190721273422241),
                (maxx - 0.012771788984537125, maxy - 4.5510972768170177e-10, minz + 0.0015845305752009153),
                (maxx - 0.014393582940101624, maxy - 3.686153604576248e-10, minz + 0.0027301181107759476),
                (maxx - 0.014963079243898392, maxy - 2.665956344571896e-10, minz + 0.00408143232919933),
                (maxx - 0.014393582940101624, maxy - 1.645759084567544e-10, maxz - 0.002730120439082384),
                (maxx - 0.012771788984537125, maxy - 7.808864666003501e-11, maxz - 0.0015845310408622026),
                (maxx - 0.010344602167606354, maxy - 2.029310053330846e-11, maxz - 0.000819073524326086),
                (maxx - 0.0074815452098846436, maxy, maxz - 0.0005502812564373016),
                (minx + 0.013172738254070282, -0.025110241025686264, maxz - 0.0003106782678514719),
                (minx + 0.012165598571300507, -0.027320515364408493, maxz - 0.0011954230722039938),
                (minx + 0.011492650955915451, -0.028797375038266182, maxz - 0.0025195349007844925),
                (minx + 0.011256340891122818, -0.029315980151295662, maxz - 0.0040814326939546675),
                (minx + 0.011492650955915451, -0.02879737690091133, minz + 0.0025195364141836762),
                (minx + 0.012165598571300507, -0.02732051908969879, minz + 0.0011954230722039938),
                (minx + 0.013172738254070282, -0.025110244750976562, minz + 0.0003106798976659775),
                (minx + 0.014360737055540085, -0.022503048181533813, minz),
                (minx + 0.015548739582300186, -0.019895851612091064, minz + 0.00031067943200469017),
                (minx + 0.01655587926506996, -0.017685577273368835, minz + 0.001195424236357212),
                (minx + 0.017228826880455017, -0.016208721324801445, minz + 0.0025195354828611016),
                (minx + 0.01746513321995735, -0.015690118074417114, minz + 0.00408143286244389),
                (minx + 0.017228826880455017, -0.016208721324801445, maxz - 0.0025195367634296417),
                (minx + 0.01655587926506996, -0.017685577273368835, maxz - 0.0011954237706959248),
                (minx + 0.015548739582300186, -0.019895853474736214, maxz - 0.00031068059615790844),
                (minx + 0.014360737055540085, -0.022503050044178963, maxz),
                (maxx - 0.07222597673535347, -0.022503051906824112, maxz),
                (maxx - 0.07222597673535347, -0.019637949764728546, maxz - 0.00031068059615790844),
                (maxx - 0.07222597673535347, -0.01720903068780899, maxz - 0.0011954237706959248),
                (maxx - 0.07222597673535347, -0.015586081892251968, maxz - 0.0025195368798449636),
                (maxx - 0.07222597673535347, -0.015016178600490093, minz + 0.004081432688119335),
                (maxx - 0.07222597673535347, -0.015586081892251968, minz + 0.00251953536644578),
                (maxx - 0.07222597673535347, -0.01720903068780899, minz + 0.001195424236357212),
                (maxx - 0.07222597673535347, -0.019637947902083397, minz + 0.00031067943200469017),
                (maxx - 0.07222597673535347, -0.022503051906824112, minz),
                (maxx - 0.07222597673535347, -0.025368154048919678, minz + 0.0003106798976659775),
                (maxx - 0.07222597673535347, -0.027797073125839233, minz + 0.0011954230722039938),
                (maxx - 0.07222597673535347, -0.029420025646686554, minz + 0.0025195364141836762),
                (maxx - 0.07222597673535347, -0.029989928007125854, maxz - 0.004081432643072702),
                (maxx - 0.07222597673535347, -0.029420021921396255, maxz - 0.0025195349007844925),
                (maxx - 0.07222597673535347, -0.027797069400548935, maxz - 0.0011954230722039938),
                (maxx - 0.07222597673535347, -0.025368154048919678, maxz - 0.0003106782678514719),
                (minx + 0.00908602774143219, -0.022446047514677048, maxz - 0.0003106782678514719),
                (minx + 0.007382035255432129, -0.024176951497793198, maxz - 0.0011954226065427065),
                (minx + 0.006243467330932617, -0.025333505123853683, maxz - 0.002519535133615136),
                (minx + 0.005843639373779297, -0.025739632546901703, maxz - 0.004081432702012222),
                (minx + 0.006243467330932617, -0.025333506986498833, minz + 0.0025195362977683544),
                (minx + 0.007382035255432129, -0.024176953360438347, minz + 0.0011954230722039938),
                (minx + 0.00908602774143219, -0.022446051239967346, minz + 0.0003106798976659775),
                (minx + 0.011096034198999405, -0.020404310896992683, minz),
                (minx + 0.013106044381856918, -0.01836257427930832, minz + 0.0003106796648353338),
                (minx + 0.014810040593147278, -0.01663167029619217, minz + 0.001195424236357212),
                (minx + 0.015948619693517685, -0.015475118532776833, minz + 0.0025195355992764235),
                (minx + 0.016348429024219513, -0.015068991109728813, minz + 0.004081432861045897),
                (minx + 0.015948619693517685, -0.015475118532776833, maxz - 0.00251953664701432),
                (minx + 0.014810040593147278, -0.01663167029619217, maxz - 0.0011954233050346375),
                (minx + 0.01310604065656662, -0.01836257241666317, maxz - 0.0003106803633272648),
                (minx + 0.011096034198999405, -0.020404312759637833, maxz - 4.656612873077393e-10),
                (minx + 0.004618480801582336, -0.01468262542039156, maxz - 0.0008190707303583622),
                (minx + 0.002191305160522461, -0.014774298295378685, maxz - 0.001584529411047697),
                (minx + 0.0005695074796676636, -0.014835557900369167, maxz - 0.002730117877945304),
                (minx, -0.014857066795229912, maxz - 0.004081432337202706),
                (minx + 0.0005694925785064697, -0.014835558831691742, minz + 0.002730119973421097),
                (minx + 0.002191290259361267, -0.014774300158023834, minz + 0.001584530808031559),
                (minx + 0.004618480801582336, -0.01468262542039156, minz + 0.0008190732914954424),
                (minx + 0.0074815452098846436, -0.014574488624930382, minz + 0.000550281023606658),
                (minx + 0.01034460961818695, -0.014466354623436928, minz + 0.0008190732914954424),
                (minx + 0.012771796435117722, -0.01437467709183693, minz + 0.0015845317393541336),
                (minx + 0.01439359039068222, -0.01431342400610447, minz + 0.002730119158513844),
                (minx + 0.014963086694478989, -0.014291912317276001, maxz - 0.004081433403984924),
                (minx + 0.01439359039068222, -0.01431342400610447, maxz - 0.0027301193913444877),
                (minx + 0.012771796435117722, -0.014374678023159504, maxz - 0.0015845298767089844),
                (minx + 0.01034460961818695, -0.014466352760791779, maxz - 0.0008190723601728678),
                (minx + 0.0074815452098846436, -0.014574489556252956, maxz - 0.0005502800922840834),
                (minx + 0.004618480801582336, maxy - 2.029310053330846e-11, maxz - 0.0008190718945115805),
                (minx + 0.002191305160522461, maxy - 7.808864666003501e-11, maxz - 0.0015845305752009153),
                (minx + 0.0005695074796676636, maxy - 1.645759084567544e-10, maxz - 0.002730119042098522),
                (minx, maxy - 2.665956344571896e-10, minz + 0.004081433353314345),
                (minx + 0.0005694925785064697, maxy - 3.686153604576248e-10, minz + 0.0027301188092678785),
                (minx + 0.002191290259361267, maxy - 4.5510972768170177e-10, minz + 0.0015845296438783407),
                (minx + 0.004618480801582336, maxy - 5.128981683810707e-10, minz + 0.0008190721273422241),
                (minx + 0.0074815452098846436, maxy - 5.331912689143792e-10, minz + 0.0005502798594534397),
                (minx + 0.01034460961818695, maxy - 5.128981683810707e-10, minz + 0.0008190721273422241),
                (minx + 0.012771796435117722, maxy - 4.5510972768170177e-10, minz + 0.0015845305752009153),
                (minx + 0.01439359039068222, maxy - 3.686153604576248e-10, minz + 0.0027301181107759476),
                (minx + 0.014963086694478989, maxy - 2.665956344571896e-10, minz + 0.00408143232919933),
                (minx + 0.01439359039068222, maxy - 1.645759084567544e-10, maxz - 0.002730120439082384),
                (minx + 0.012771796435117722, maxy - 7.808864666003501e-11, maxz - 0.0015845310408622026),
                (minx + 0.01034460961818695, maxy - 2.029310053330846e-11, maxz - 0.000819073524326086),
                (minx + 0.0074815452098846436, maxy, maxz - 0.0005502812564373016)]

    # Faces
    myfaces = [(90, 89, 6, 5), (88, 87, 8, 7), (86, 85, 10, 9), (84, 83, 12, 11), (80, 95, 0, 15),
               (82, 81, 14, 13), (93, 92, 3, 2), (91, 90, 5, 4), (89, 88, 7, 6), (87, 86, 9, 8),
               (85, 84, 11, 10), (95, 94, 1, 0), (83, 82, 13, 12), (94, 93, 2, 1), (81, 80, 15, 14),
               (92, 91, 4, 3), (2, 3, 19, 18), (13, 14, 30, 29), (15, 0, 16, 31), (11, 12, 28, 27),
               (9, 10, 26, 25), (7, 8, 24, 23), (5, 6, 22, 21), (3, 4, 20, 19), (14, 15, 31, 30),
               (1, 2, 18, 17), (12, 13, 29, 28), (0, 1, 17, 16), (10, 11, 27, 26), (8, 9, 25, 24),
               (6, 7, 23, 22), (4, 5, 21, 20), (19, 20, 36, 35), (30, 31, 47, 46), (17, 18, 34, 33),
               (28, 29, 45, 44), (16, 17, 33, 32), (26, 27, 43, 42), (24, 25, 41, 40), (22, 23, 39, 38),
               (20, 21, 37, 36), (18, 19, 35, 34), (29, 30, 46, 45), (31, 16, 32, 47), (27, 28, 44, 43),
               (25, 26, 42, 41), (23, 24, 40, 39), (21, 22, 38, 37), (36, 37, 53, 52), (34, 35, 51, 50),
               (45, 46, 62, 61), (47, 32, 48, 63), (43, 44, 60, 59), (41, 42, 58, 57), (39, 40, 56, 55),
               (37, 38, 54, 53), (35, 36, 52, 51), (46, 47, 63, 62), (33, 34, 50, 49), (44, 45, 61, 60),
               (32, 33, 49, 48), (42, 43, 59, 58), (40, 41, 57, 56), (38, 39, 55, 54), (90, 69, 70, 89),
               (88, 71, 72, 87), (86, 73, 74, 85), (84, 75, 76, 83), (80, 79, 64, 95), (82, 77, 78, 81),
               (93, 66, 67, 92), (91, 68, 69, 90), (89, 70, 71, 88), (87, 72, 73, 86), (85, 74, 75, 84),
               (95, 64, 65, 94), (83, 76, 77, 82), (94, 65, 66, 93), (81, 78, 79, 80), (92, 67, 68, 91),
               (66, 98, 99, 67), (77, 109, 110, 78), (79, 111, 96, 64), (75, 107, 108, 76), (73, 105, 106, 74),
               (71, 103, 104, 72), (69, 101, 102, 70), (67, 99, 100, 68), (78, 110, 111, 79), (65, 97, 98, 66),
               (76, 108, 109, 77), (64, 96, 97, 65), (74, 106, 107, 75), (72, 104, 105, 73), (70, 102, 103, 71),
               (68, 100, 101, 69), (99, 115, 116, 100), (110, 126, 127, 111), (97, 113, 114, 98), (108, 124, 125, 109),
               (96, 112, 113, 97), (106, 122, 123, 107), (104, 120, 121, 105), (102, 118, 119, 103),
               (100, 116, 117, 101),
               (98, 114, 115, 99), (109, 125, 126, 110), (111, 127, 112, 96), (107, 123, 124, 108),
               (105, 121, 122, 106),
               (103, 119, 120, 104), (101, 117, 118, 102), (116, 132, 133, 117), (114, 130, 131, 115),
               (125, 141, 142, 126),
               (127, 143, 128, 112), (123, 139, 140, 124), (121, 137, 138, 122), (119, 135, 136, 120),
               (117, 133, 134, 118),
               (115, 131, 132, 116), (126, 142, 143, 127), (113, 129, 130, 114), (124, 140, 141, 125),
               (112, 128, 129, 113),
               (122, 138, 139, 123), (120, 136, 137, 121), (118, 134, 135, 119)]

    return myvertex, myfaces


# ----------------------------------------------
# Handle model 02
# ----------------------------------------------
def handle_model_02():
    # ------------------------------------
    # Mesh data
    # ------------------------------------
    minx = -0.09079331159591675
    maxx = 0.09079315513372421
    maxy = 0
    minz = -0.018363870680332184
    maxz = 0.0015741242095828056

    # Vertex
    myvertex = [(maxx, maxy, maxz - 9.313225746154785e-10),
                (maxx, maxy, maxz - 0.0031482474878430367),
                (maxx, -0.02426009438931942, maxz - 0.0031482460908591747),
                (maxx, -0.02426009438931942, maxz),
                (maxx, -0.02727462910115719, maxz),
                (maxx, -0.02727462910115719, maxz - 0.0031482460908591747),
                (maxx, -0.02426009625196457, minz + 0.002603583037853241),
                (maxx, -0.027274630963802338, minz + 0.002603583037853241),
                (maxx, -0.02426009625196457, minz),
                (maxx, -0.027274630963802338, minz),
                (maxx, -0.021415365859866142, minz + 0.002603583037853241),
                (maxx, -0.02141536772251129, minz),
                (maxx - 0.0907932324437013, -0.02426009438931942, maxz - 0.0031482460908591747),
                (maxx - 0.0907932324437013, -0.02426009438931942, maxz),
                (minx, maxy, maxz - 9.313225746154785e-10),
                (minx, maxy, maxz - 0.0031482474878430367),
                (minx, -0.02426009438931942, maxz - 0.0031482460908591747),
                (minx, -0.02426009438931942, maxz),
                (minx, -0.02727462910115719, maxz),
                (minx, -0.02727462910115719, maxz - 0.0031482460908591747),
                (maxx - 0.0907932324437013, -0.02727462910115719, maxz),
                (maxx - 0.0907932324437013, maxy, maxz - 9.313225746154785e-10),
                (maxx - 0.0907932324437013, -0.02727462910115719, maxz - 0.0031482460908591747),
                (maxx - 0.0907932324437013, maxy, maxz - 0.0031482474878430367),
                (maxx - 0.0907932324437013, -0.02426009625196457, minz + 0.002603583037853241),
                (minx, -0.02426009625196457, minz + 0.002603583037853241),
                (minx, -0.027274630963802338, minz + 0.002603583037853241),
                (maxx - 0.0907932324437013, -0.027274630963802338, minz + 0.002603583037853241),
                (maxx - 0.0907932324437013, -0.02426009625196457, minz),
                (minx, -0.02426009625196457, minz),
                (minx, -0.027274630963802338, minz),
                (maxx - 0.0907932324437013, -0.027274630963802338, minz),
                (maxx - 0.0907932324437013, -0.021415365859866142, minz + 0.002603583037853241),
                (minx, -0.021415365859866142, minz + 0.002603583037853241),
                (maxx - 0.0907932324437013, -0.02141536772251129, minz),
                (minx, -0.02141536772251129, minz)]

    # Faces
    myfaces = [(2, 5, 7, 6), (13, 3, 0, 21), (3, 2, 1, 0), (7, 27, 31, 9), (23, 21, 0, 1),
               (5, 22, 27, 7), (4, 5, 2, 3), (2, 12, 23, 1), (20, 4, 3, 13), (5, 4, 20, 22),
               (12, 2, 6, 24), (9, 31, 28, 8), (6, 7, 9, 8), (32, 10, 11, 34), (6, 8, 11, 10),
               (8, 28, 34, 11), (24, 6, 10, 32), (16, 25, 26, 19), (13, 21, 14, 17), (17, 14, 15, 16),
               (26, 30, 31, 27), (23, 15, 14, 21), (19, 26, 27, 22), (18, 17, 16, 19), (16, 15, 23, 12),
               (20, 13, 17, 18), (19, 22, 20, 18), (22, 27, 24, 12), (12, 24, 25, 16), (30, 29, 28, 31),
               (25, 29, 30, 26), (27, 31, 28, 24), (28, 34, 32, 24), (32, 34, 35, 33), (25, 33, 35, 29),
               (29, 35, 34, 28), (24, 32, 33, 25)]

    return myvertex, myfaces


# ----------------------------------------------
# Handle model 05
# ----------------------------------------------
def handle_model_05():
    # ------------------------------------
    # Mesh data
    # ------------------------------------
    minx = -0.012873317115008831
    maxx = 0.012873315252363682
    maxy = 6.581399869531879e-10
    minz = -0.012873317115008831
    maxz = 0.012873315252363682

    # Vertex
    myvertex = [(maxx - 0.01287331552838386, maxy, maxz - 0.008879524189978838),
                (maxx - 0.01287331552838386, -0.004451401997357607, maxz - 0.008879524189978838),
                (maxx - 0.012094165373127908, maxy, maxz - 0.008956264238804579),
                (maxx - 0.012094165373127908, -0.004451401997357607, maxz - 0.008956263773143291),
                (maxx - 0.011344957514666021, maxy, maxz - 0.00918353395536542),
                (maxx - 0.011344957514666021, -0.004451401997357607, maxz - 0.009183533489704132),
                (maxx - 0.010654483688995242, maxy, maxz - 0.00955259962938726),
                (maxx - 0.010654483688995242, -0.004451401997357607, maxz - 0.009552599163725972),
                (maxx - 0.010049278382211924, maxy, maxz - 0.010049279080703855),
                (maxx - 0.010049278382211924, -0.004451401997357607, maxz - 0.010049278382211924),
                (maxx - 0.009552599163725972, maxy - 6.581399869531879e-10, maxz - 0.01065448485314846),
                (maxx - 0.009552599163725972, -0.004451401997357607, maxz - 0.01065448415465653),
                (maxx - 0.009183533489704132, maxy - 6.581399869531879e-10, maxz - 0.011344958445988595),
                (maxx - 0.009183533489704132, -0.004451401997357607, maxz - 0.011344957863911986),
                (maxx - 0.008956263773143291, maxy - 6.581399869531879e-10, maxz - 0.0120941661298275),
                (maxx - 0.008956263773143291, -0.004451401997357607, maxz - 0.012094165605958551),
                (maxx - 0.008879524189978838, maxy - 6.581399869531879e-10, maxz - 0.012873315995101497),
                (maxx - 0.008879524189978838, -0.004451401997357607, maxz - 0.012873315519886824),
                (maxx - 0.008956263307482004, maxy - 6.581399869531879e-10, minz + 0.012094166420865804),
                (maxx - 0.008956263307482004, -0.004451401997357607, minz + 0.01209416682831943),
                (maxx - 0.009183533024042845, maxy - 6.581399869531879e-10, minz + 0.011344958795234561),
                (maxx - 0.009183533024042845, -0.004451401997357607, minz + 0.011344959260895848),
                (maxx - 0.009552599163725972, maxy - 6.581399869531879e-10, minz + 0.01065448415465653),
                (maxx - 0.009552599163725972, -0.004451401997357607, minz + 0.01065448485314846),
                (maxx - 0.010049278382211924, -6.581399869531879e-10, minz + 0.010049278847873211),
                (maxx - 0.010049278382211924, -0.004451401997357607, minz + 0.010049279546365142),
                (maxx - 0.010654483921825886, -6.581399869531879e-10, minz + 0.00955259962938726),
                (maxx - 0.010654483921825886, -0.004451401997357607, minz + 0.009552600095048547),
                (maxx - 0.011344958213157952, -6.581399869531879e-10, minz + 0.009183533256873488),
                (maxx - 0.011344958213157952, -0.004451401997357607, minz + 0.009183533489704132),
                (maxx - 0.012094166362658143, -6.581399869531879e-10, minz + 0.008956264238804579),
                (maxx - 0.012094166362658143, -0.004451401997357607, minz + 0.008956264238804579),
                (minx + 0.012873315537646146, -6.581399869531879e-10, minz + 0.008879524655640125),
                (minx + 0.012873315537646146, -0.004451401997357607, minz + 0.008879525121301413),
                (minx + 0.012094165082089603, -6.581399869531879e-10, minz + 0.008956264238804579),
                (minx + 0.012094165082089603, -0.004451401997357607, minz + 0.008956264704465866),
                (minx + 0.011344957165420055, -6.581399869531879e-10, minz + 0.009183534886687994),
                (minx + 0.011344957165420055, -0.004451401997357607, minz + 0.009183535352349281),
                (minx + 0.010654483223333955, -6.581399869531879e-10, minz + 0.009552601026371121),
                (minx + 0.010654483223333955, -0.004451401997357607, minz + 0.009552601724863052),
                (minx + 0.010049277916550636, -6.581399869531879e-10, minz + 0.010049280477687716),
                (minx + 0.010049277916550636, -0.004451401997357607, minz + 0.010049281176179647),
                (minx + 0.009552598698064685, maxy - 6.581399869531879e-10, minz + 0.010654486482962966),
                (minx + 0.009552598698064685, -0.004451401997357607, minz + 0.010654486948624253),
                (minx + 0.009183533024042845, maxy - 6.581399869531879e-10, minz + 0.011344961123540998),
                (minx + 0.009183533024042845, -0.004451401997357607, minz + 0.011344961589202285),
                (minx + 0.008956264238804579, maxy - 6.581399869531879e-10, minz + 0.01209416938945651),
                (minx + 0.008956264238804579, -0.004451401997357607, minz + 0.012094169855117798),
                (minx + 0.008879525121301413, maxy - 6.581399869531879e-10, maxz - 0.012873312440222273),
                (minx + 0.008879525121301413, -0.004451401997357607, maxz - 0.012873311965007517),
                (minx + 0.008956265170127153, maxy - 6.581399869531879e-10, maxz - 0.012094162055291235),
                (minx + 0.008956265170127153, -0.004451401997357607, maxz - 0.012094161589629948),
                (minx + 0.009183535818010569, maxy - 6.581399869531879e-10, maxz - 0.01134495425503701),
                (minx + 0.009183535818010569, -0.004451401997357607, maxz - 0.011344953789375722),
                (minx + 0.009552602656185627, maxy - 6.581399869531879e-10, maxz - 0.010654480429366231),
                (minx + 0.009552602656185627, -0.004451401997357607, maxz - 0.010654479963704944),
                (minx + 0.01004928327165544, maxy, maxz - 0.010049275355413556),
                (minx + 0.01004928327165544, -0.004451401997357607, maxz - 0.010049275122582912),
                (minx + 0.010654489509761333, maxy, maxz - 0.009552596602588892),
                (minx + 0.010654489509761333, -0.004451401997357607, maxz - 0.009552596136927605),
                (minx + 0.011344964150339365, maxy, maxz - 0.00918353139422834),
                (minx + 0.011344964150339365, -0.004451401997357607, maxz - 0.009183531161397696),
                (minx + 0.012094172765500844, maxy, maxz - 0.008956263307482004),
                (minx + 0.012094172765500844, -0.004451401997357607, maxz - 0.00895626237615943),
                (minx + 0.01287331571475725, -0.01000460609793663, maxz),
                (maxx - 0.010361857246607542, -0.01000460609793663, maxz - 0.0002473592758178711),
                (maxx - 0.00794691126793623, -0.01000460609793663, maxz - 0.0009799236431717873),
                (maxx - 0.005721285007894039, -0.01000460609793663, maxz - 0.002169545739889145),
                (maxx - 0.0037705078721046448, -0.01000460609793663, maxz - 0.0037705088034272194),
                (maxx - 0.002169545739889145, -0.01000460609793663, maxz - 0.005721286870539188),
                (maxx - 0.0009799227118492126, -0.010004607029259205, maxz - 0.007946912664920092),
                (maxx - 0.00024735741317272186, -0.010004607029259205, maxz - 0.010361858177930117),
                (maxx, -0.010004607029259205, minz + 0.012873315524888684),
                (maxx - 0.00024735648185014725, -0.010004607029259205, minz + 0.010361856315284967),
                (maxx - 0.0009799227118492126, -0.010004607029259205, minz + 0.007946911733597517),
                (maxx - 0.002169545739889145, -0.01000460796058178, minz + 0.005721283610910177),
                (maxx - 0.0037705078721046448, -0.01000460796058178, minz + 0.003770505078136921),
                (maxx - 0.005721286404877901, -0.01000460796058178, minz + 0.002169542945921421),
                (maxx - 0.007946913596242666, -0.01000460796058178, minz + 0.0009799208492040634),
                (maxx - 0.010361860506236553, -0.01000460796058178, minz + 0.00024735648185014725),
                (minx + 0.012873311520098518, -0.01000460796058178, minz),
                (minx + 0.010361851193010807, -0.01000460796058178, minz + 0.00024735648185014725),
                (minx + 0.007946905214339495, -0.01000460796058178, minz + 0.0009799255058169365),
                (minx + 0.005721278488636017, -0.01000460796058178, minz + 0.0021695485338568687),
                (minx + 0.003770500421524048, -0.01000460796058178, minz + 0.0037705106660723686),
                (minx + 0.002169538289308548, -0.01000460796058178, minz + 0.005721290595829487),
                (minx + 0.0009799189865589142, -0.010004607029259205, minz + 0.007946919184178114),
                (minx + 0.000247354619204998, -0.010004607029259205, minz + 0.010361866094172001),
                (minx, -0.010004607029259205, maxz - 0.012873305383929612),
                (minx + 0.0002473592758178711, -0.010004607029259205, maxz - 0.010361845139414072),
                (minx + 0.0009799282997846603, -0.010004607029259205, maxz - 0.007946899626404047),
                (minx + 0.0021695513278245926, -0.01000460609793663, maxz - 0.005721272900700569),
                (minx + 0.003770517185330391, -0.01000460609793663, maxz - 0.003770497627556324),
                (minx + 0.005721298512071371, -0.01000460609793663, maxz - 0.0021695364266633987),
                (minx + 0.00794692849740386, -0.01000460609793663, maxz - 0.0009799161925911903),
                (minx + 0.010361875407397747, -0.01000460609793663, maxz - 0.000247354619204998),
                (minx + 0.01287331571475725, -0.012252332642674446, maxz),
                (maxx - 0.010361857246607542, -0.012252332642674446, maxz - 0.0002473592758178711),
                (maxx - 0.00794691126793623, -0.012252332642674446, maxz - 0.0009799236431717873),
                (maxx - 0.005721285007894039, -0.012252332642674446, maxz - 0.002169545739889145),
                (maxx - 0.0037705078721046448, -0.012252332642674446, maxz - 0.0037705088034272194),
                (maxx - 0.002169545739889145, -0.012252332642674446, maxz - 0.005721286870539188),
                (maxx - 0.0009799227118492126, -0.012252334505319595, maxz - 0.007946912664920092),
                (maxx - 0.00024735741317272186, -0.012252334505319595, maxz - 0.010361858177930117),
                (maxx, -0.012252334505319595, minz + 0.012873315638136429),
                (maxx - 0.00024735648185014725, -0.012252334505319595, minz + 0.010361856315284967),
                (maxx - 0.0009799227118492126, -0.012252334505319595, minz + 0.007946911733597517),
                (maxx - 0.002169545739889145, -0.01225233543664217, minz + 0.005721283610910177),
                (maxx - 0.0037705078721046448, -0.01225233543664217, minz + 0.003770505078136921),
                (maxx - 0.005721286404877901, -0.01225233543664217, minz + 0.002169542945921421),
                (maxx - 0.007946913596242666, -0.01225233543664217, minz + 0.0009799208492040634),
                (maxx - 0.010361860506236553, -0.01225233543664217, minz + 0.00024735648185014725),
                (minx + 0.012873311520098518, -0.01225233543664217, minz),
                (minx + 0.010361851193010807, -0.01225233543664217, minz + 0.00024735648185014725),
                (minx + 0.007946905214339495, -0.01225233543664217, minz + 0.0009799255058169365),
                (minx + 0.005721278488636017, -0.01225233543664217, minz + 0.0021695485338568687),
                (minx + 0.003770500421524048, -0.01225233543664217, minz + 0.0037705106660723686),
                (minx + 0.002169538289308548, -0.01225233543664217, minz + 0.005721290595829487),
                (minx + 0.0009799189865589142, -0.012252334505319595, minz + 0.007946919184178114),
                (minx + 0.000247354619204998, -0.012252334505319595, minz + 0.010361866094172001),
                (minx, -0.012252334505319595, maxz - 0.012873305270680646),
                (minx + 0.0002473592758178711, -0.012252334505319595, maxz - 0.010361845139414072),
                (minx + 0.0009799282997846603, -0.012252334505319595, maxz - 0.007946899626404047),
                (minx + 0.0021695513278245926, -0.012252332642674446, maxz - 0.005721272900700569),
                (minx + 0.003770517185330391, -0.012252332642674446, maxz - 0.003770497627556324),
                (minx + 0.005721298512071371, -0.012252332642674446, maxz - 0.0021695364266633987),
                (minx + 0.00794692849740386, -0.012252332642674446, maxz - 0.0009799161925911903),
                (minx + 0.010361875407397747, -0.012252332642674446, maxz - 0.000247354619204998),
                (minx + 0.01287331597587027, -0.012252331711351871, maxz - 0.006033936515450478),
                (maxx - 0.011539019644260406, -0.012252331711351871, maxz - 0.006165354512631893),
                (maxx - 0.010255999164655805, -0.012252331711351871, maxz - 0.006554554216563702),
                (maxx - 0.009073560824617743, -0.012252332642674446, maxz - 0.007186579518020153),
                (maxx - 0.008037144783884287, -0.012252332642674446, maxz - 0.008037144318223),
                (maxx - 0.007186580915004015, -0.012252332642674446, maxz - 0.009073559893295169),
                (maxx - 0.006554554216563702, -0.012252332642674446, maxz - 0.010255998698994517),
                (maxx - 0.006165354512631893, -0.012252332642674446, maxz - 0.011539018712937832),
                (maxx - 0.006033937446773052, -0.012252332642674446, maxz - 0.012873314963572108),
                (maxx - 0.0061653535813093185, -0.012252332642674446, minz + 0.011539021041244268),
                (maxx - 0.006554554216563702, -0.012252332642674446, minz + 0.01025600079447031),
                (maxx - 0.007186580915004015, -0.012252332642674446, minz + 0.009073561755940318),
                (maxx - 0.008037144783884287, -0.012252332642674446, minz + 0.008037145715206861),
                (maxx - 0.009073561057448387, -0.012252332642674446, minz + 0.007186580449342728),
                (maxx - 0.010256000561639667, -0.012252334505319595, minz + 0.006554553750902414),
                (maxx - 0.011539021274074912, -0.012252334505319595, minz + 0.0061653549782931805),
                (minx + 0.012873313747317816, -0.012252334505319595, minz + 0.006033938378095627),
                (minx + 0.01153901673387736, -0.012252334505319595, minz + 0.0061653549782931805),
                (minx + 0.01025599567219615, -0.012252334505319595, minz + 0.0065545570105314255),
                (minx + 0.009073557797819376, -0.012252332642674446, minz + 0.007186583708971739),
                (minx + 0.008037141524255276, -0.012252332642674446, minz + 0.008037148043513298),
                (minx + 0.007186576724052429, -0.012252332642674446, minz + 0.00907356571406126),
                (minx + 0.006554551888257265, -0.012252332642674446, minz + 0.010256004752591252),
                (minx + 0.006165352184325457, -0.012252332642674446, minz + 0.011539026163518429),
                (minx + 0.006033936981111765, -0.012252332642674446, maxz - 0.012873308875832823),
                (minx + 0.006165355443954468, -0.012252332642674446, maxz - 0.011539011728018522),
                (minx + 0.006554556544870138, -0.012252332642674446, maxz - 0.010255991481244564),
                (minx + 0.007186584174633026, -0.012252332642674446, maxz - 0.00907355290837586),
                (minx + 0.008037150837481022, -0.012252332642674446, maxz - 0.008037138264626265),
                (minx + 0.009073568508028984, -0.012252332642674446, maxz - 0.007186574395745993),
                (minx + 0.010256008245050907, -0.012252331711351871, maxz - 0.006554548628628254),
                (minx + 0.011539029655978084, -0.012252331711351871, maxz - 0.006165351718664169),
                (maxx - 0.01237887132447213, -0.012252329848706722, maxz - 0.010387574089691043),
                (maxx - 0.011465257033705711, -0.012252329848706722, maxz - 0.01076600537635386),
                (maxx - 0.01108119694981724, -0.012252329848706722, maxz - 0.011081195320002735),
                (maxx - 0.010766007238999009, -0.012252329848706722, maxz - 0.011465255171060562),
                (maxx - 0.010531799867749214, -0.012252329848706722, maxz - 0.01190342620247975),
                (maxx - 0.01108119694981724, -0.012252329848706722, minz + 0.01108119951095432),
                (maxx - 0.011903428356163204, -0.012252329848706722, minz + 0.010531801730394363),
                (minx + 0.012378871033433825, -0.012252329848706722, minz + 0.010387577582150698),
                (minx + 0.011465256451629102, -0.012252329848706722, minz + 0.01076600980013609),
                (minx + 0.01076600607484579, -0.012252329848706722, minz + 0.011465260875411332),
                (minx + 0.010531799402087927, -0.012252329848706722, minz + 0.011903432430699468),
                (minx + 0.010338877560570836, -0.012252329848706722, maxz - 0.01287331168983985),
                (minx + 0.010531801264733076, -0.012252329848706722, maxz - 0.01190342364134267),
                (minx + 0.011081199743784964, -0.012252329848706722, maxz - 0.011081192875280976),
                (minx + 0.011465260293334723, -0.012252329848706722, maxz - 0.010766003280878067),
                (maxx - 0.01287331586396423, -0.012252329848706722, maxz - 0.010338874999433756),
                (maxx - 0.011903427948709577, -0.012252329848706722, maxz - 0.010531798237934709),
                (maxx - 0.010387575486674905, -0.012252329848706722, maxz - 0.012378869636449963),
                (maxx - 0.010338877094909549, -0.012252329848706722, maxz - 0.012873313945746867),
                (maxx - 0.010387575486674905, -0.012252329848706722, minz + 0.012378874002024531),
                (maxx - 0.010531799867749214, -0.012252329848706722, minz + 0.011903430917300284),
                (maxx - 0.010766007238999009, -0.012252329848706722, minz + 0.011465259245596826),
                (maxx - 0.011465257382951677, -0.012252329848706722, minz + 0.010766008868813515),
                (maxx - 0.01237887202296406, -0.012252329848706722, minz + 0.010387577582150698),
                (minx + 0.01287331567758343, -0.012252329848706722, minz + 0.010338879656046629),
                (minx + 0.011903427541255951, -0.012252329848706722, minz + 0.010531802894547582),
                (minx + 0.011081196367740631, -0.012252329848706722, minz + 0.011081200325861573),
                (minx + 0.010387575021013618, -0.012252329848706722, minz + 0.01237887586466968),
                (minx + 0.01038757641799748, -0.012252329848706722, maxz - 0.012378867017105222),
                (minx + 0.010766008868813515, -0.012252329848706722, maxz - 0.011465252609923482),
                (minx + 0.011903432314284146, -0.012252329848706722, maxz - 0.01053179637528956),
                (minx + 0.01237887580646202, -0.012252329848706722, maxz - 0.010387573391199112)]

    # Faces
    myfaces = [(0, 1, 3, 2), (2, 3, 5, 4), (4, 5, 7, 6), (6, 7, 9, 8), (8, 9, 11, 10),
               (10, 11, 13, 12), (12, 13, 15, 14), (14, 15, 17, 16), (16, 17, 19, 18), (18, 19, 21, 20),
               (20, 21, 23, 22), (22, 23, 25, 24), (24, 25, 27, 26), (26, 27, 29, 28), (28, 29, 31, 30),
               (30, 31, 33, 32), (32, 33, 35, 34), (34, 35, 37, 36), (36, 37, 39, 38), (38, 39, 41, 40),
               (40, 41, 43, 42), (42, 43, 45, 44), (44, 45, 47, 46), (46, 47, 49, 48), (48, 49, 51, 50),
               (50, 51, 53, 52), (52, 53, 55, 54), (54, 55, 57, 56), (56, 57, 59, 58), (58, 59, 61, 60),
               (60, 61, 63, 62), (62, 63, 1, 0), (45, 43, 85, 86), (23, 21, 74, 75), (51, 49, 88, 89),
               (7, 5, 66, 67), (29, 27, 77, 78), (57, 55, 91, 92), (35, 33, 80, 81), (13, 11, 69, 70),
               (63, 61, 94, 95), (41, 39, 83, 84), (19, 17, 72, 73), (47, 45, 86, 87), (3, 1, 64, 65),
               (25, 23, 75, 76), (53, 51, 89, 90), (9, 7, 67, 68), (31, 29, 78, 79), (59, 57, 92, 93),
               (37, 35, 81, 82), (15, 13, 70, 71), (1, 63, 95, 64), (43, 41, 84, 85), (21, 19, 73, 74),
               (49, 47, 87, 88), (5, 3, 65, 66), (27, 25, 76, 77), (55, 53, 90, 91), (11, 9, 68, 69),
               (33, 31, 79, 80), (61, 59, 93, 94), (39, 37, 82, 83), (17, 15, 71, 72), (89, 88, 120, 121),
               (67, 66, 98, 99), (78, 77, 109, 110), (87, 86, 118, 119), (65, 64, 96, 97), (76, 75, 107, 108),
               (64, 95, 127, 96), (85, 84, 116, 117), (74, 73, 105, 106), (94, 93, 125, 126), (83, 82, 114, 115),
               (72, 71, 103, 104), (92, 91, 123, 124), (81, 80, 112, 113), (70, 69, 101, 102), (90, 89, 121, 122),
               (68, 67, 99, 100), (79, 78, 110, 111), (88, 87, 119, 120), (66, 65, 97, 98), (77, 76, 108, 109),
               (86, 85, 117, 118), (75, 74, 106, 107), (95, 94, 126, 127), (84, 83, 115, 116), (73, 72, 104, 105),
               (93, 92, 124, 125), (82, 81, 113, 114), (71, 70, 102, 103), (91, 90, 122, 123), (69, 68, 100, 101),
               (80, 79, 111, 112), (123, 122, 154, 155), (101, 100, 132, 133), (112, 111, 143, 144),
               (121, 120, 152, 153),
               (99, 98, 130, 131), (110, 109, 141, 142), (119, 118, 150, 151), (97, 96, 128, 129), (108, 107, 139, 140),
               (96, 127, 159, 128), (117, 116, 148, 149), (106, 105, 137, 138), (126, 125, 157, 158),
               (115, 114, 146, 147),
               (104, 103, 135, 136), (124, 123, 155, 156), (113, 112, 144, 145), (102, 101, 133, 134),
               (122, 121, 153, 154),
               (100, 99, 131, 132), (111, 110, 142, 143), (120, 119, 151, 152), (98, 97, 129, 130),
               (109, 108, 140, 141),
               (118, 117, 149, 150), (107, 106, 138, 139), (127, 126, 158, 159), (116, 115, 147, 148),
               (105, 104, 136, 137),
               (125, 124, 156, 157), (114, 113, 145, 146), (103, 102, 134, 135), (157, 156, 173, 174),
               (133, 132, 162, 163),
               (134, 133, 163, 164), (132, 131, 161, 162), (150, 149, 169, 170), (146, 145, 167, 185),
               (135, 134, 164, 177),
               (155, 154, 172, 189), (144, 143, 183, 184), (153, 152, 171, 188), (131, 130, 176, 161),
               (142, 141, 182, 166),
               (151, 150, 170, 187), (129, 128, 175, 160), (140, 139, 181, 165), (128, 159, 191, 175),
               (149, 148, 186, 169),
               (138, 137, 179, 180), (158, 157, 174, 190), (147, 146, 185, 168), (136, 135, 177, 178),
               (156, 155, 189, 173),
               (145, 144, 184, 167), (154, 153, 188, 172), (143, 142, 166, 183), (152, 151, 187, 171),
               (130, 129, 160, 176),
               (141, 140, 165, 182), (139, 138, 180, 181), (159, 158, 190, 191), (148, 147, 168, 186),
               (137, 136, 178, 179),
               (175, 191, 190, 174, 173, 189, 172, 188, 171, 187, 170, 169, 186, 168, 185, 167, 184, 183, 166, 182, 165,
                181, 180, 179, 178, 177, 164, 163, 162, 161, 176, 160)]

    return myvertex, myfaces


# ----------------------------------------------
# Handle model 06
# ----------------------------------------------
def handle_model_06():
    # ------------------------------------
    # Mesh data
    # ------------------------------------
    minx = -0.021158458665013313
    maxx = 0.021158456802368164
    maxy = 6.581399869531879e-10
    minz = -0.021158462390303612
    maxz = 0.021158454939723015

    # Vertex
    myvertex = [(maxx - 0.021158457078388343, maxy, maxz - 0.01716466387733817),
                (maxx - 0.021158457078388343, -0.004451401997357607, maxz - 0.01716466387733817),
                (maxx - 0.02037930692313239, maxy, maxz - 0.017241403926163912),
                (maxx - 0.02037930692313239, -0.004451401997357607, maxz - 0.017241403460502625),
                (maxx - 0.019630099064670503, maxy, maxz - 0.017468673642724752),
                (maxx - 0.019630099064670503, -0.004451401997357607, maxz - 0.017468673177063465),
                (maxx - 0.018939625238999724, maxy, maxz - 0.017837739316746593),
                (maxx - 0.018939625238999724, -0.004451401997357607, maxz - 0.017837738851085305),
                (maxx - 0.018334419932216406, maxy, maxz - 0.018334418768063188),
                (maxx - 0.018334419932216406, -0.004451401997357607, maxz - 0.018334418069571257),
                (maxx - 0.017837740713730454, maxy - 6.581399869531879e-10, maxz - 0.018939624540507793),
                (maxx - 0.017837740713730454, -0.004451401997357607, maxz - 0.018939623842015862),
                (maxx - 0.017468675039708614, maxy - 6.581399869531879e-10, maxz - 0.01963009813334793),
                (maxx - 0.017468675039708614, -0.004451401997357607, maxz - 0.01963009755127132),
                (maxx - 0.017241405323147774, maxy - 6.581399869531879e-10, maxz - 0.020379305817186832),
                (maxx - 0.017241405323147774, -0.004451401997357607, maxz - 0.020379305293317884),
                (maxx - 0.01716466573998332, maxy - 6.581399869531879e-10, maxz - 0.02115845568246083),
                (maxx - 0.01716466573998332, -0.004451401997357607, maxz - 0.021158455207246157),
                (maxx - 0.017241404857486486, maxy - 6.581399869531879e-10, minz + 0.020379311696160585),
                (maxx - 0.017241404857486486, -0.004451401997357607, minz + 0.02037931210361421),
                (maxx - 0.017468674574047327, maxy - 6.581399869531879e-10, minz + 0.01963010407052934),
                (maxx - 0.017468674574047327, -0.004451401997357607, minz + 0.01963010453619063),
                (maxx - 0.017837740713730454, maxy - 6.581399869531879e-10, minz + 0.01893962942995131),
                (maxx - 0.017837740713730454, -0.004451401997357607, minz + 0.01893963012844324),
                (maxx - 0.018334419932216406, -6.581399869531879e-10, minz + 0.01833442412316799),
                (maxx - 0.018334419932216406, -0.004451401997357607, minz + 0.018334424821659923),
                (maxx - 0.018939625471830368, -6.581399869531879e-10, minz + 0.01783774490468204),
                (maxx - 0.018939625471830368, -0.004451401997357607, minz + 0.017837745370343328),
                (maxx - 0.019630099763162434, -6.581399869531879e-10, minz + 0.01746867853216827),
                (maxx - 0.019630099763162434, -0.004451401997357607, minz + 0.017468678764998913),
                (maxx - 0.020379307912662625, -6.581399869531879e-10, minz + 0.01724140951409936),
                (maxx - 0.020379307912662625, -0.004451401997357607, minz + 0.01724140951409936),
                (minx + 0.02115845708765063, -6.581399869531879e-10, minz + 0.017164669930934906),
                (minx + 0.02115845708765063, -0.004451401997357607, minz + 0.017164670396596193),
                (minx + 0.020379306632094085, -6.581399869531879e-10, minz + 0.01724140951409936),
                (minx + 0.020379306632094085, -0.004451401997357607, minz + 0.017241409979760647),
                (minx + 0.019630098715424538, -6.581399869531879e-10, minz + 0.017468680161982775),
                (minx + 0.019630098715424538, -0.004451401997357607, minz + 0.017468680627644062),
                (minx + 0.018939624773338437, -6.581399869531879e-10, minz + 0.017837746301665902),
                (minx + 0.018939624773338437, -0.004451401997357607, minz + 0.017837747000157833),
                (minx + 0.01833441946655512, -6.581399869531879e-10, minz + 0.018334425752982497),
                (minx + 0.01833441946655512, -0.004451401997357607, minz + 0.018334426451474428),
                (minx + 0.017837740248069167, maxy - 6.581399869531879e-10, minz + 0.018939631758257747),
                (minx + 0.017837740248069167, -0.004451401997357607, minz + 0.018939632223919034),
                (minx + 0.017468674574047327, maxy - 6.581399869531879e-10, minz + 0.019630106398835778),
                (minx + 0.017468674574047327, -0.004451401997357607, minz + 0.019630106864497066),
                (minx + 0.01724140578880906, maxy - 6.581399869531879e-10, minz + 0.02037931466475129),
                (minx + 0.01724140578880906, -0.004451401997357607, minz + 0.02037931513041258),
                (minx + 0.017164666671305895, maxy - 6.581399869531879e-10, maxz - 0.021158452127581606),
                (minx + 0.017164666671305895, -0.004451401997357607, maxz - 0.02115845165236685),
                (minx + 0.017241406720131636, maxy - 6.581399869531879e-10, maxz - 0.02037930174265057),
                (minx + 0.017241406720131636, -0.004451401997357607, maxz - 0.02037930127698928),
                (minx + 0.01746867736801505, maxy - 6.581399869531879e-10, maxz - 0.019630093942396343),
                (minx + 0.01746867736801505, -0.004451401997357607, maxz - 0.019630093476735055),
                (minx + 0.01783774420619011, maxy - 6.581399869531879e-10, maxz - 0.018939620116725564),
                (minx + 0.01783774420619011, -0.004451401997357607, maxz - 0.018939619651064277),
                (minx + 0.018334424821659923, maxy, maxz - 0.01833441504277289),
                (minx + 0.018334424821659923, -0.004451401997357607, maxz - 0.018334414809942245),
                (minx + 0.018939631059765816, maxy, maxz - 0.017837736289948225),
                (minx + 0.018939631059765816, -0.004451401997357607, maxz - 0.017837735824286938),
                (minx + 0.019630105700343847, maxy, maxz - 0.017468671081587672),
                (minx + 0.019630105700343847, -0.004451401997357607, maxz - 0.01746867084875703),
                (minx + 0.020379314315505326, maxy, maxz - 0.017241402994841337),
                (minx + 0.020379314315505326, -0.004451401997357607, maxz - 0.017241402063518763),
                (minx + 0.02115845651317172, -0.01480177417397499, maxz),
                (maxx - 0.017030648421496153, -0.01480177417397499, maxz - 0.00040655583143234253),
                (maxx - 0.013061466626822948, -0.01480177417397499, maxz - 0.0016105938702821732),
                (maxx - 0.00940344762057066, -0.01480177417397499, maxz - 0.0035658441483974457),
                (maxx - 0.006197170354425907, -0.01480177417397499, maxz - 0.006197171285748482),
                (maxx - 0.0035658441483974457, -0.01480177417397499, maxz - 0.009403450414538383),
                (maxx - 0.0016105901449918747, -0.014801775105297565, maxz - 0.013061468489468098),
                (maxx - 0.0004065539687871933, -0.014801775105297565, maxz - 0.017030649818480015),
                (maxx, -0.014801775105297565, minz + 0.0211584585064859),
                (maxx - 0.0004065539687871933, -0.014801775105297565, minz + 0.017030648421496153),
                (maxx - 0.0016105901449918747, -0.014801775105297565, minz + 0.013061468489468098),
                (maxx - 0.0035658441483974457, -0.01480177603662014, minz + 0.00940344762057066),
                (maxx - 0.006197170354425907, -0.01480177603662014, minz + 0.006197166629135609),
                (maxx - 0.009403450414538383, -0.01480177603662014, minz + 0.0035658422857522964),
                (maxx - 0.013061470352113247, -0.01480177603662014, minz + 0.0016105901449918747),
                (maxx - 0.017030653543770313, -0.01480177603662014, minz + 0.0004065539687871933),
                (minx + 0.02115844961887081, -0.01480177603662014, minz),
                (minx + 0.017030637711286545, -0.01480177603662014, minz + 0.0004065539687871933),
                (minx + 0.013061455450952053, -0.01480177603662014, minz + 0.0016105975955724716),
                (minx + 0.009403438307344913, -0.01480177603662014, minz + 0.0035658497363328934),
                (minx + 0.006197156384587288, -0.01480177603662014, minz + 0.006197175942361355),
                (minx + 0.003565831109881401, -0.01480177603662014, minz + 0.00940345972776413),
                (minx + 0.001610584557056427, -0.014801775105297565, minz + 0.013061481527984142),
                (minx + 0.0004065483808517456, -0.014801775105297565, minz + 0.01703066425397992),
                (minx, -0.014801775105297565, maxz - 0.021158439990372813),
                (minx + 0.00040655583143234253, -0.014801775105297565, maxz - 0.01703062793239951),
                (minx + 0.0016105994582176208, -0.014801775105297565, maxz - 0.013061447069048882),
                (minx + 0.0035658515989780426, -0.01480177417397499, maxz - 0.009403428062796593),
                (minx + 0.006197184324264526, -0.01480177417397499, maxz - 0.006197153590619564),
                (minx + 0.00940346997231245, -0.01480177417397499, maxz - 0.003565829247236252),
                (minx + 0.013061493635177612, -0.01480177417397499, maxz - 0.0016105808317661285),
                (minx + 0.017030677758157253, -0.01480177417397499, maxz - 0.00040655024349689484),
                (minx + 0.02115845651317172, -0.017049500718712807, maxz),
                (maxx - 0.017030648421496153, -0.017049500718712807, maxz - 0.00040655583143234253),
                (maxx - 0.013061466626822948, -0.017049500718712807, maxz - 0.0016105938702821732),
                (maxx - 0.00940344762057066, -0.017049500718712807, maxz - 0.0035658441483974457),
                (maxx - 0.006197170354425907, -0.017049500718712807, maxz - 0.006197171285748482),
                (maxx - 0.0035658441483974457, -0.017049500718712807, maxz - 0.009403450414538383),
                (maxx - 0.0016105901449918747, -0.017049502581357956, maxz - 0.013061468489468098),
                (maxx - 0.0004065539687871933, -0.017049502581357956, maxz - 0.017030649818480015),
                (maxx, -0.017049502581357956, maxz - 0.021158458637408728),
                (maxx - 0.0004065539687871933, -0.017049502581357956, minz + 0.017030648421496153),
                (maxx - 0.0016105901449918747, -0.017049502581357956, minz + 0.013061468489468098),
                (maxx - 0.0035658441483974457, -0.017049502581357956, minz + 0.00940344762057066),
                (maxx - 0.006197170354425907, -0.017049502581357956, minz + 0.006197166629135609),
                (maxx - 0.009403450414538383, -0.017049502581357956, minz + 0.0035658422857522964),
                (maxx - 0.013061470352113247, -0.017049502581357956, minz + 0.0016105901449918747),
                (maxx - 0.017030653543770313, -0.017049502581357956, minz + 0.0004065539687871933),
                (minx + 0.02115844961887081, -0.017049502581357956, minz),
                (minx + 0.017030637711286545, -0.017049502581357956, minz + 0.0004065539687871933),
                (minx + 0.013061455450952053, -0.017049502581357956, minz + 0.0016105975955724716),
                (minx + 0.009403438307344913, -0.017049502581357956, minz + 0.0035658497363328934),
                (minx + 0.006197156384587288, -0.017049502581357956, minz + 0.006197175942361355),
                (minx + 0.003565831109881401, -0.017049502581357956, minz + 0.00940345972776413),
                (minx + 0.001610584557056427, -0.017049502581357956, minz + 0.013061481527984142),
                (minx + 0.0004065483808517456, -0.017049502581357956, minz + 0.01703066425397992),
                (minx, -0.017049502581357956, maxz - 0.02115843980423726),
                (minx + 0.00040655583143234253, -0.017049502581357956, maxz - 0.01703062793239951),
                (minx + 0.0016105994582176208, -0.017049502581357956, maxz - 0.013061447069048882),
                (minx + 0.0035658515989780426, -0.017049500718712807, maxz - 0.009403428062796593),
                (minx + 0.006197184324264526, -0.017049500718712807, maxz - 0.006197153590619564),
                (minx + 0.00940346997231245, -0.017049500718712807, maxz - 0.003565829247236252),
                (minx + 0.013061493635177612, -0.017049500718712807, maxz - 0.0016105808317661285),
                (minx + 0.017030677758157253, -0.017049500718712807, maxz - 0.00040655024349689484),
                (minx + 0.021158456942334758, -0.017049498856067657, maxz - 0.00991731882095337),
                (maxx - 0.01896542147733271, -0.017049498856067657, maxz - 0.010133316740393639),
                (maxx - 0.016856661066412926, -0.017049498856067657, maxz - 0.010773001238703728),
                (maxx - 0.014913217630237341, -0.017049500718712807, maxz - 0.01181179191917181),
                (maxx - 0.013209773227572441, -0.017049500718712807, maxz - 0.013209772296249866),
                (maxx - 0.011811794713139534, -0.017049500718712807, maxz - 0.014913215301930904),
                (maxx - 0.010773001238703728, -0.017049500718712807, maxz - 0.01685666013509035),
                (maxx - 0.010133316740393639, -0.017049500718712807, maxz - 0.01896541938185692),
                (maxx - 0.009917320683598518, -0.017049500718712807, maxz - 0.02115845573538011),
                (maxx - 0.01013331487774849, -0.017049500718712807, minz + 0.018965424969792366),
                (maxx - 0.010773001238703728, -0.017049500718712807, minz + 0.01685666525736451),
                (maxx - 0.011811794713139534, -0.017049500718712807, minz + 0.01491321949288249),
                (maxx - 0.013209773227572441, -0.017049500718712807, minz + 0.01320977695286274),
                (maxx - 0.014913217630237341, -0.017049500718712807, minz + 0.011811795644462109),
                (maxx - 0.016856663394719362, -0.017049502581357956, minz + 0.010773002170026302),
                (maxx - 0.01896542403846979, -0.017049502581357956, minz + 0.010133319534361362),
                (minx + 0.021158453279507494, -0.017049502581357956, minz + 0.009917323477566242),
                (minx + 0.018965415423735976, -0.017049502581357956, minz + 0.010133319534361362),
                (minx + 0.016856654547154903, -0.017049502581357956, minz + 0.01077300775796175),
                (minx + 0.014913210645318031, -0.017049500718712807, minz + 0.011811801232397556),
                (minx + 0.013209767639636993, -0.017049500718712807, minz + 0.013209780678153038),
                (minx + 0.011811788193881512, -0.017049500718712807, minz + 0.014913226012140512),
                (minx + 0.01077299751341343, -0.017049500718712807, minz + 0.016856671776622534),
                (minx + 0.010133313946425915, -0.017049500718712807, minz + 0.018965433351695538),
                (minx + 0.009917320683598518, -0.017049500718712807, maxz - 0.02115844572963077),
                (minx + 0.010133318603038788, -0.017049500718712807, maxz - 0.01896540797315538),
                (minx + 0.0107730058953166, -0.017049500718712807, maxz - 0.01685664849355817),
                (minx + 0.011811800301074982, -0.017049500718712807, maxz - 0.014913204126060009),
                (minx + 0.013209782540798187, -0.017049500718712807, maxz - 0.013209762051701546),
                (minx + 0.014913228340446949, -0.017049500718712807, maxz - 0.011811783537268639),
                (minx + 0.016856675501912832, -0.017049498856067657, maxz - 0.010772991925477982),
                (minx + 0.01896543661132455, -0.017049498856067657, maxz - 0.010133312083780766),
                (maxx - 0.020345793396700174, -0.017049498856067657, maxz - 0.01707291603088379),
                (maxx - 0.018844185629859567, -0.017049498856067657, maxz - 0.017694902140647173),
                (maxx - 0.01821294822730124, -0.017049498856067657, maxz - 0.01821294496767223),
                (maxx - 0.017694905400276184, -0.017049498856067657, maxz - 0.018844182137399912),
                (maxx - 0.017309964634478092, -0.017049498856067657, maxz - 0.0195643559563905),
                (maxx - 0.01821294822730124, -0.017049498856067657, minz + 0.01821295404806733),
                (maxx - 0.019564359914511442, -0.017049498856067657, minz + 0.017309968825429678),
                (minx + 0.02034579199971631, -0.017049498856067657, minz + 0.017072923481464386),
                (minx + 0.018844183767214417, -0.017049498856067657, minz + 0.017694910988211632),
                (minx + 0.01769490260630846, -0.017049498856067657, minz + 0.01884419354610145),
                (minx + 0.017309962771832943, -0.017049498856067657, minz + 0.01956436806358397),
                (minx + 0.016992878634482622, -0.017049498856067657, maxz - 0.021158450354705316),
                (minx + 0.017309966031461954, -0.017049498856067657, maxz - 0.019564351649023592),
                (minx + 0.01821295195259154, -0.017049498856067657, maxz - 0.018212941009551287),
                (minx + 0.01884419028647244, -0.017049498856067657, maxz - 0.017694898648187518),
                (maxx - 0.021158457657990293, -0.017049498856067657, maxz - 0.016992874443531036),
                (maxx - 0.01956435921601951, -0.017049498856067657, maxz - 0.017309961607679725),
                (maxx - 0.017072918359190226, -0.017049498856067657, maxz - 0.020345790137071162),
                (maxx - 0.016992878168821335, -0.017049498856067657, maxz - 0.021158454062492393),
                (maxx - 0.017072918359190226, -0.017049498856067657, minz + 0.020345799159258604),
                (maxx - 0.017309964634478092, -0.017049498856067657, minz + 0.01956436550244689),
                (maxx - 0.017694905400276184, -0.017049498856067657, minz + 0.01884419098496437),
                (maxx - 0.018844186328351498, -0.017049498856067657, minz + 0.01769490959122777),
                (maxx - 0.020345794560853392, -0.017049498856067657, minz + 0.017072923481464386),
                (minx + 0.021158456452073482, -0.017049498856067657, minz + 0.01699288422241807),
                (minx + 0.019564357702620327, -0.017049498856067657, minz + 0.01730997092090547),
                (minx + 0.01821294636465609, -0.017049498856067657, minz + 0.018212955445051193),
                (minx + 0.01707291742786765, -0.017049498856067657, minz + 0.020345802302472293),
                (minx + 0.017072919756174088, -0.017049498856067657, maxz - 0.020345785829704255),
                (minx + 0.017694907495751977, -0.017049498856067657, maxz - 0.018844177946448326),
                (minx + 0.01956436550244689, -0.017049498856067657, maxz - 0.017309958348050714),
                (minx + 0.020345799799542874, -0.017049498856067657, maxz - 0.017072914633899927)]

    # Faces
    myfaces = [(0, 1, 3, 2), (2, 3, 5, 4), (4, 5, 7, 6), (6, 7, 9, 8), (8, 9, 11, 10),
               (10, 11, 13, 12), (12, 13, 15, 14), (14, 15, 17, 16), (16, 17, 19, 18), (18, 19, 21, 20),
               (20, 21, 23, 22), (22, 23, 25, 24), (24, 25, 27, 26), (26, 27, 29, 28), (28, 29, 31, 30),
               (30, 31, 33, 32), (32, 33, 35, 34), (34, 35, 37, 36), (36, 37, 39, 38), (38, 39, 41, 40),
               (40, 41, 43, 42), (42, 43, 45, 44), (44, 45, 47, 46), (46, 47, 49, 48), (48, 49, 51, 50),
               (50, 51, 53, 52), (52, 53, 55, 54), (54, 55, 57, 56), (56, 57, 59, 58), (58, 59, 61, 60),
               (60, 61, 63, 62), (62, 63, 1, 0), (45, 43, 85, 86), (23, 21, 74, 75), (51, 49, 88, 89),
               (7, 5, 66, 67), (29, 27, 77, 78), (57, 55, 91, 92), (35, 33, 80, 81), (13, 11, 69, 70),
               (63, 61, 94, 95), (41, 39, 83, 84), (19, 17, 72, 73), (47, 45, 86, 87), (3, 1, 64, 65),
               (25, 23, 75, 76), (53, 51, 89, 90), (9, 7, 67, 68), (31, 29, 78, 79), (59, 57, 92, 93),
               (37, 35, 81, 82), (15, 13, 70, 71), (1, 63, 95, 64), (43, 41, 84, 85), (21, 19, 73, 74),
               (49, 47, 87, 88), (5, 3, 65, 66), (27, 25, 76, 77), (55, 53, 90, 91), (11, 9, 68, 69),
               (33, 31, 79, 80), (61, 59, 93, 94), (39, 37, 82, 83), (17, 15, 71, 72), (89, 88, 120, 121),
               (67, 66, 98, 99), (78, 77, 109, 110), (87, 86, 118, 119), (65, 64, 96, 97), (76, 75, 107, 108),
               (64, 95, 127, 96), (85, 84, 116, 117), (74, 73, 105, 106), (94, 93, 125, 126), (83, 82, 114, 115),
               (72, 71, 103, 104), (92, 91, 123, 124), (81, 80, 112, 113), (70, 69, 101, 102), (90, 89, 121, 122),
               (68, 67, 99, 100), (79, 78, 110, 111), (88, 87, 119, 120), (66, 65, 97, 98), (77, 76, 108, 109),
               (86, 85, 117, 118), (75, 74, 106, 107), (95, 94, 126, 127), (84, 83, 115, 116), (73, 72, 104, 105),
               (93, 92, 124, 125), (82, 81, 113, 114), (71, 70, 102, 103), (91, 90, 122, 123), (69, 68, 100, 101),
               (80, 79, 111, 112), (123, 122, 154, 155), (101, 100, 132, 133), (112, 111, 143, 144),
               (121, 120, 152, 153),
               (99, 98, 130, 131), (110, 109, 141, 142), (119, 118, 150, 151), (97, 96, 128, 129), (108, 107, 139, 140),
               (96, 127, 159, 128), (117, 116, 148, 149), (106, 105, 137, 138), (126, 125, 157, 158),
               (115, 114, 146, 147),
               (104, 103, 135, 136), (124, 123, 155, 156), (113, 112, 144, 145), (102, 101, 133, 134),
               (122, 121, 153, 154),
               (100, 99, 131, 132), (111, 110, 142, 143), (120, 119, 151, 152), (98, 97, 129, 130),
               (109, 108, 140, 141),
               (118, 117, 149, 150), (107, 106, 138, 139), (127, 126, 158, 159), (116, 115, 147, 148),
               (105, 104, 136, 137),
               (125, 124, 156, 157), (114, 113, 145, 146), (103, 102, 134, 135), (157, 156, 173, 174),
               (133, 132, 162, 163),
               (134, 133, 163, 164), (132, 131, 161, 162), (150, 149, 169, 170), (146, 145, 167, 185),
               (135, 134, 164, 177),
               (155, 154, 172, 189), (144, 143, 183, 184), (153, 152, 171, 188), (131, 130, 176, 161),
               (142, 141, 182, 166),
               (151, 150, 170, 187), (129, 128, 175, 160), (140, 139, 181, 165), (128, 159, 191, 175),
               (149, 148, 186, 169),
               (138, 137, 179, 180), (158, 157, 174, 190), (147, 146, 185, 168), (136, 135, 177, 178),
               (156, 155, 189, 173),
               (145, 144, 184, 167), (154, 153, 188, 172), (143, 142, 166, 183), (152, 151, 187, 171),
               (130, 129, 160, 176),
               (141, 140, 165, 182), (139, 138, 180, 181), (159, 158, 190, 191), (148, 147, 168, 186),
               (137, 136, 178, 179),
               (175, 191, 190, 174, 173, 189, 172, 188, 171, 187, 170, 169, 186, 168, 185, 167, 184, 183, 166, 182, 165,
                181, 180, 179, 178, 177, 164, 163, 162, 161, 176, 160)]

    return myvertex, myfaces


# ----------------------------------------------
# Handle model 07
# ----------------------------------------------
def handle_model_07():
    # ------------------------------------
    # Mesh data
    # ------------------------------------
    minx = -0.10910986363887787
    maxx = 0.10910986363887787
    maxy = 0
    minz = -0.0039262366481125355
    maxz = 0.0039262366481125355

    # Vertex
    myvertex = [(maxx, -0.017620893195271492, maxz),
                (maxx, -0.01611838862299919, maxz - 0.00029886653646826744),
                (maxx, -0.014844624325633049, maxz - 0.0011499673128128052),
                (maxx, -0.013993524014949799, maxz - 0.002423731260932982),
                (maxx, -0.013694657012820244, minz + 0.003926236289926277),
                (maxx, -0.013993524014949799, minz + 0.002423729980364442),
                (maxx, -0.014844624325633049, minz + 0.001149968709796667),
                (maxx, -0.016118386760354042, minz + 0.0002988663036376238),
                (maxx, -0.017620891332626343, minz),
                (maxx, -0.019123397767543793, minz + 0.00029886653646826744),
                (maxx, -0.020397160202264786, minz + 0.0011499675456434488),
                (maxx, -0.021248264238238335, minz + 0.0024237307952716947),
                (maxx, -0.02154713124036789, maxz - 0.003926236195012284),
                (maxx, -0.021248262375593185, maxz - 0.0024237297475337982),
                (maxx, -0.020397160202264786, maxz - 0.0011499666143208742),
                (maxx, -0.019123397767543793, maxz - 0.0002988646738231182),
                (maxx - 0.02949388325214386, -0.01396019384264946, maxz - 0.0024807279696688056),
                (maxx - 0.030047059059143066, -0.01396019384264946, maxz - 0.0025907604722306132),
                (maxx - 0.030516013503074646, -0.01396019384264946, maxz - 0.002904107444919646),
                (maxx - 0.030829355120658875, -0.01396019384264946, maxz - 0.0033730645664036274),
                (maxx - 0.030939392745494843, -0.01396019198000431, minz + 0.003926236608184253),
                (maxx - 0.030829355120658875, -0.01396019384264946, minz + 0.0033730643335729837),
                (maxx - 0.030516013503074646, -0.01396019384264946, minz + 0.0029041077941656113),
                (maxx - 0.030047059059143066, -0.01396019384264946, minz + 0.002590760588645935),
                (maxx - 0.02949388325214386, -0.01396019384264946, minz + 0.0024807280860841274),
                (maxx - 0.02894071489572525, -0.01396019384264946, minz + 0.002590760588645935),
                (maxx - 0.028471753001213074, -0.01396019384264946, minz + 0.002904107444919646),
                (maxx - 0.028158411383628845, -0.01396019384264946, minz + 0.0033730644499883056),
                (maxx - 0.028048373758792877, -0.01396019384264946, maxz - 0.0039262363893523555),
                (maxx - 0.028158411383628845, -0.01396019384264946, maxz - 0.0033730638679116964),
                (maxx - 0.028471753001213074, -0.01396019384264946, maxz - 0.0029041070956736803),
                (maxx - 0.02894071489572525, -0.01396019384264946, maxz - 0.0025907597737386823),
                (maxx - 0.02949388325214386, -1.862645149230957e-09, maxz - 0.0024807279696688056),
                (maxx - 0.030047059059143066, -1.862645149230957e-09, maxz - 0.0025907604722306132),
                (maxx - 0.030516013503074646, -1.862645149230957e-09, maxz - 0.002904107444919646),
                (maxx - 0.030829355120658875, maxy, maxz - 0.0033730645664036274),
                (maxx - 0.030939392745494843, maxy, minz + 0.003926236608184253),
                (maxx - 0.030829355120658875, maxy, minz + 0.0033730643335729837),
                (maxx - 0.030516013503074646, -1.862645149230957e-09, minz + 0.0029041077941656113),
                (maxx - 0.030047059059143066, -1.862645149230957e-09, minz + 0.002590760588645935),
                (maxx - 0.02949388325214386, -1.862645149230957e-09, minz + 0.0024807280860841274),
                (maxx - 0.02894071489572525, -1.862645149230957e-09, minz + 0.002590760588645935),
                (maxx - 0.028471753001213074, -1.862645149230957e-09, minz + 0.002904107444919646),
                (maxx - 0.028158411383628845, -1.862645149230957e-09, minz + 0.0033730644499883056),
                (maxx - 0.028048373758792877, -1.862645149230957e-09, maxz - 0.0039262363893523555),
                (maxx - 0.028158411383628845, -1.862645149230957e-09, maxz - 0.0033730638679116964),
                (maxx - 0.028471753001213074, -1.862645149230957e-09, maxz - 0.0029041070956736803),
                (maxx - 0.02894071489572525, -1.862645149230957e-09, maxz - 0.0025907597737386823),
                (minx + 0.10910986037924886, -0.017620893195271492, maxz),
                (minx + 0.10910986037924886, -0.01611838862299919, maxz - 0.00029886653646826744),
                (minx + 0.10910986037924886, -0.014844624325633049, maxz - 0.0011499673128128052),
                (minx + 0.10910986037924886, -0.013993524014949799, maxz - 0.002423731260932982),
                (minx + 0.10910986037924886, -0.013694657012820244, minz + 0.003926236289926277),
                (minx + 0.10910986037924886, -0.013993524014949799, minz + 0.002423729980364442),
                (minx + 0.10910986037924886, -0.014844624325633049, minz + 0.001149968709796667),
                (minx + 0.10910986037924886, -0.016118386760354042, minz + 0.0002988663036376238),
                (minx + 0.10910986037924886, -0.017620891332626343, minz),
                (minx + 0.10910986037924886, -0.019123397767543793, minz + 0.00029886653646826744),
                (minx + 0.10910986037924886, -0.020397160202264786, minz + 0.0011499675456434488),
                (minx + 0.10910986037924886, -0.021248264238238335, minz + 0.0024237307952716947),
                (minx + 0.10910986037924886, -0.02154713124036789, maxz - 0.003926236195012284),
                (minx + 0.10910986037924886, -0.021248262375593185, maxz - 0.0024237297475337982),
                (minx + 0.10910986037924886, -0.020397160202264786, maxz - 0.0011499666143208742),
                (minx + 0.10910986037924886, -0.019123397767543793, maxz - 0.0002988646738231182),
                (minx, -0.017620893195271492, maxz),
                (minx, -0.01611838862299919, maxz - 0.00029886653646826744),
                (minx, -0.014844624325633049, maxz - 0.0011499673128128052),
                (minx, -0.013993524014949799, maxz - 0.002423731260932982),
                (minx, -0.013694657012820244, minz + 0.003926236289926277),
                (minx, -0.013993524014949799, minz + 0.002423729980364442),
                (minx, -0.014844624325633049, minz + 0.001149968709796667),
                (minx, -0.016118386760354042, minz + 0.0002988663036376238),
                (minx, -0.017620891332626343, minz),
                (minx, -0.019123397767543793, minz + 0.00029886653646826744),
                (minx, -0.020397160202264786, minz + 0.0011499675456434488),
                (minx, -0.021248264238238335, minz + 0.0024237307952716947),
                (minx, -0.02154713124036789, maxz - 0.003926236195012284),
                (minx, -0.021248262375593185, maxz - 0.0024237297475337982),
                (minx, -0.020397160202264786, maxz - 0.0011499666143208742),
                (minx, -0.019123397767543793, maxz - 0.0002988646738231182),
                (minx + 0.02949388325214386, -0.01396019384264946, maxz - 0.0024807279696688056),
                (minx + 0.030047059059143066, -0.01396019384264946, maxz - 0.0025907604722306132),
                (minx + 0.030516013503074646, -0.01396019384264946, maxz - 0.002904107444919646),
                (minx + 0.030829355120658875, -0.01396019384264946, maxz - 0.0033730645664036274),
                (minx + 0.030939392745494843, -0.01396019198000431, minz + 0.003926236608184253),
                (minx + 0.030829355120658875, -0.01396019384264946, minz + 0.0033730643335729837),
                (minx + 0.030516013503074646, -0.01396019384264946, minz + 0.0029041077941656113),
                (minx + 0.030047059059143066, -0.01396019384264946, minz + 0.002590760588645935),
                (minx + 0.02949388325214386, -0.01396019384264946, minz + 0.0024807280860841274),
                (minx + 0.02894071489572525, -0.01396019384264946, minz + 0.002590760588645935),
                (minx + 0.028471753001213074, -0.01396019384264946, minz + 0.002904107444919646),
                (minx + 0.028158411383628845, -0.01396019384264946, minz + 0.0033730644499883056),
                (minx + 0.028048373758792877, -0.01396019384264946, maxz - 0.0039262363893523555),
                (minx + 0.028158411383628845, -0.01396019384264946, maxz - 0.0033730638679116964),
                (minx + 0.028471753001213074, -0.01396019384264946, maxz - 0.0029041070956736803),
                (minx + 0.02894071489572525, -0.01396019384264946, maxz - 0.0025907597737386823),
                (minx + 0.02949388325214386, -1.862645149230957e-09, maxz - 0.0024807279696688056),
                (minx + 0.030047059059143066, -1.862645149230957e-09, maxz - 0.0025907604722306132),
                (minx + 0.030516013503074646, -1.862645149230957e-09, maxz - 0.002904107444919646),
                (minx + 0.030829355120658875, maxy, maxz - 0.0033730645664036274),
                (minx + 0.030939392745494843, maxy, minz + 0.003926236608184253),
                (minx + 0.030829355120658875, maxy, minz + 0.0033730643335729837),
                (minx + 0.030516013503074646, -1.862645149230957e-09, minz + 0.0029041077941656113),
                (minx + 0.030047059059143066, -1.862645149230957e-09, minz + 0.002590760588645935),
                (minx + 0.02949388325214386, -1.862645149230957e-09, minz + 0.0024807280860841274),
                (minx + 0.02894071489572525, -1.862645149230957e-09, minz + 0.002590760588645935),
                (minx + 0.028471753001213074, -1.862645149230957e-09, minz + 0.002904107444919646),
                (minx + 0.028158411383628845, -1.862645149230957e-09, minz + 0.0033730644499883056),
                (minx + 0.028048373758792877, -1.862645149230957e-09, maxz - 0.0039262363893523555),
                (minx + 0.028158411383628845, -1.862645149230957e-09, maxz - 0.0033730638679116964),
                (minx + 0.028471753001213074, -1.862645149230957e-09, maxz - 0.0029041070956736803),
                (minx + 0.02894071489572525, -1.862645149230957e-09, maxz - 0.0025907597737386823)]

    # Faces
    myfaces = [(49, 48, 0, 1), (60, 59, 11, 12), (58, 57, 9, 10), (56, 55, 7, 8), (54, 53, 5, 6),
               (52, 51, 3, 4), (48, 63, 15, 0), (50, 49, 1, 2), (61, 60, 12, 13), (59, 58, 10, 11),
               (57, 56, 8, 9), (55, 54, 6, 7), (53, 52, 4, 5), (63, 62, 14, 15), (51, 50, 2, 3),
               (62, 61, 13, 14), (17, 16, 32, 33), (32, 47, 46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33),
               (28, 27, 43, 44), (26, 25, 41, 42),
               (24, 23, 39, 40), (22, 21, 37, 38), (20, 19, 35, 36), (16, 31, 47, 32), (18, 17, 33, 34),
               (29, 28, 44, 45), (27, 26, 42, 43), (25, 24, 40, 41), (23, 22, 38, 39), (21, 20, 36, 37),
               (31, 30, 46, 47), (19, 18, 34, 35), (30, 29, 45, 46), (49, 65, 64, 48), (60, 76, 75, 59),
               (58, 74, 73, 57), (56, 72, 71, 55), (54, 70, 69, 53), (52, 68, 67, 51), (48, 64, 79, 63),
               (50, 66, 65, 49), (61, 77, 76, 60), (59, 75, 74, 58), (57, 73, 72, 56), (55, 71, 70, 54),
               (53, 69, 68, 52), (63, 79, 78, 62), (51, 67, 66, 50), (62, 78, 77, 61), (81, 97, 96, 80),
               (96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111), (92, 108, 107, 91),
               (90, 106, 105, 89), (88, 104, 103, 87), (86, 102, 101, 85),
               (84, 100, 99, 83), (80, 96, 111, 95), (82, 98, 97, 81), (93, 109, 108, 92), (91, 107, 106, 90),
               (89, 105, 104, 88), (87, 103, 102, 86), (85, 101, 100, 84), (95, 111, 110, 94), (83, 99, 98, 82),
               (94, 110, 109, 93), (0, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1),
               (64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79)]

    return myvertex, myfaces


# ----------------------------------------------
# Handle model 08
# ----------------------------------------------
def handle_model_08():
    # ------------------------------------
    # Mesh data
    # ------------------------------------
    minx = -0.05910986289381981
    maxx = 0.05910986289381981
    maxy = 0
    minz = -0.0039262366481125355
    maxz = 0.0039262366481125355

    # Vertex
    myvertex = [(maxx, -0.017620893195271492, maxz),
                (maxx, -0.01611838862299919, maxz - 0.00029886653646826744),
                (maxx, -0.014844624325633049, maxz - 0.0011499673128128052),
                (maxx, -0.013993524014949799, maxz - 0.002423731260932982),
                (maxx, -0.013694657012820244, minz + 0.003926236289926277),
                (maxx, -0.013993524014949799, minz + 0.002423729980364442),
                (maxx, -0.014844624325633049, minz + 0.001149968709796667),
                (maxx, -0.016118386760354042, minz + 0.0002988663036376238),
                (maxx, -0.017620891332626343, minz),
                (maxx, -0.019123397767543793, minz + 0.00029886653646826744),
                (maxx, -0.020397160202264786, minz + 0.0011499675456434488),
                (maxx, -0.021248264238238335, minz + 0.0024237307952716947),
                (maxx, -0.02154713124036789, maxz - 0.003926236195012284),
                (maxx, -0.021248262375593185, maxz - 0.0024237297475337982),
                (maxx, -0.020397160202264786, maxz - 0.0011499666143208742),
                (maxx, -0.019123397767543793, maxz - 0.0002988646738231182),
                (maxx - 0.010583892464637756, -0.01396019384264946, maxz - 0.0024807279696688056),
                (maxx - 0.011137068271636963, -0.01396019384264946, maxz - 0.0025907604722306132),
                (maxx - 0.011606022715568542, -0.01396019384264946, maxz - 0.002904107444919646),
                (maxx - 0.011919364333152771, -0.01396019384264946, maxz - 0.0033730645664036274),
                (maxx - 0.012029401957988739, -0.01396019198000431, minz + 0.003926236608184253),
                (maxx - 0.011919364333152771, -0.01396019384264946, minz + 0.0033730643335729837),
                (maxx - 0.011606022715568542, -0.01396019384264946, minz + 0.0029041077941656113),
                (maxx - 0.011137068271636963, -0.01396019384264946, minz + 0.002590760588645935),
                (maxx - 0.010583892464637756, -0.01396019384264946, minz + 0.0024807280860841274),
                (maxx - 0.010030724108219147, -0.01396019384264946, minz + 0.002590760588645935),
                (maxx - 0.00956176221370697, -0.01396019384264946, minz + 0.002904107444919646),
                (maxx - 0.009248420596122742, -0.01396019384264946, minz + 0.0033730644499883056),
                (maxx - 0.009138382971286774, -0.01396019384264946, maxz - 0.0039262363893523555),
                (maxx - 0.009248420596122742, -0.01396019384264946, maxz - 0.0033730638679116964),
                (maxx - 0.00956176221370697, -0.01396019384264946, maxz - 0.0029041070956736803),
                (maxx - 0.010030724108219147, -0.01396019384264946, maxz - 0.0025907597737386823),
                (maxx - 0.010583892464637756, -1.862645149230957e-09, maxz - 0.0024807279696688056),
                (maxx - 0.011137068271636963, -1.862645149230957e-09, maxz - 0.0025907604722306132),
                (maxx - 0.011606022715568542, -1.862645149230957e-09, maxz - 0.002904107444919646),
                (maxx - 0.011919364333152771, maxy, maxz - 0.0033730645664036274),
                (maxx - 0.012029401957988739, maxy, minz + 0.003926236608184253),
                (maxx - 0.011919364333152771, maxy, minz + 0.0033730643335729837),
                (maxx - 0.011606022715568542, -1.862645149230957e-09, minz + 0.0029041077941656113),
                (maxx - 0.011137068271636963, -1.862645149230957e-09, minz + 0.002590760588645935),
                (maxx - 0.010583892464637756, -1.862645149230957e-09, minz + 0.0024807280860841274),
                (maxx - 0.010030724108219147, -1.862645149230957e-09, minz + 0.002590760588645935),
                (maxx - 0.00956176221370697, -1.862645149230957e-09, minz + 0.002904107444919646),
                (maxx - 0.009248420596122742, -1.862645149230957e-09, minz + 0.0033730644499883056),
                (maxx - 0.009138382971286774, -1.862645149230957e-09, maxz - 0.0039262363893523555),
                (maxx - 0.009248420596122742, -1.862645149230957e-09, maxz - 0.0033730638679116964),
                (maxx - 0.00956176221370697, -1.862645149230957e-09, maxz - 0.0029041070956736803),
                (maxx - 0.010030724108219147, -1.862645149230957e-09, maxz - 0.0025907597737386823),
                (minx, -0.017620893195271492, maxz),
                (minx, -0.01611838862299919, maxz - 0.00029886653646826744),
                (minx, -0.014844624325633049, maxz - 0.0011499673128128052),
                (minx, -0.013993524014949799, maxz - 0.002423731260932982),
                (minx, -0.013694657012820244, minz + 0.003926236289926277),
                (minx, -0.013993524014949799, minz + 0.002423729980364442),
                (minx, -0.014844624325633049, minz + 0.001149968709796667),
                (minx, -0.016118386760354042, minz + 0.0002988663036376238),
                (minx, -0.017620891332626343, minz),
                (minx, -0.019123397767543793, minz + 0.00029886653646826744),
                (minx, -0.020397160202264786, minz + 0.0011499675456434488),
                (minx, -0.021248264238238335, minz + 0.0024237307952716947),
                (minx, -0.02154713124036789, maxz - 0.003926236195012284),
                (minx, -0.021248262375593185, maxz - 0.0024237297475337982),
                (minx, -0.020397160202264786, maxz - 0.0011499666143208742),
                (minx, -0.019123397767543793, maxz - 0.0002988646738231182),
                (minx + 0.010583892464637756, -0.01396019384264946, maxz - 0.0024807279696688056),
                (minx + 0.011137068271636963, -0.01396019384264946, maxz - 0.0025907604722306132),
                (minx + 0.011606022715568542, -0.01396019384264946, maxz - 0.002904107444919646),
                (minx + 0.011919364333152771, -0.01396019384264946, maxz - 0.0033730645664036274),
                (minx + 0.012029401957988739, -0.01396019198000431, minz + 0.003926236608184253),
                (minx + 0.011919364333152771, -0.01396019384264946, minz + 0.0033730643335729837),
                (minx + 0.011606022715568542, -0.01396019384264946, minz + 0.0029041077941656113),
                (minx + 0.011137068271636963, -0.01396019384264946, minz + 0.002590760588645935),
                (minx + 0.010583892464637756, -0.01396019384264946, minz + 0.0024807280860841274),
                (minx + 0.010030724108219147, -0.01396019384264946, minz + 0.002590760588645935),
                (minx + 0.00956176221370697, -0.01396019384264946, minz + 0.002904107444919646),
                (minx + 0.009248420596122742, -0.01396019384264946, minz + 0.0033730644499883056),
                (minx + 0.009138382971286774, -0.01396019384264946, maxz - 0.0039262363893523555),
                (minx + 0.009248420596122742, -0.01396019384264946, maxz - 0.0033730638679116964),
                (minx + 0.00956176221370697, -0.01396019384264946, maxz - 0.0029041070956736803),
                (minx + 0.010030724108219147, -0.01396019384264946, maxz - 0.0025907597737386823),
                (minx + 0.010583892464637756, -1.862645149230957e-09, maxz - 0.0024807279696688056),
                (minx + 0.011137068271636963, -1.862645149230957e-09, maxz - 0.0025907604722306132),
                (minx + 0.011606022715568542, -1.862645149230957e-09, maxz - 0.002904107444919646),
                (minx + 0.011919364333152771, maxy, maxz - 0.0033730645664036274),
                (minx + 0.012029401957988739, maxy, minz + 0.003926236608184253),
                (minx + 0.011919364333152771, maxy, minz + 0.0033730643335729837),
                (minx + 0.011606022715568542, -1.862645149230957e-09, minz + 0.0029041077941656113),
                (minx + 0.011137068271636963, -1.862645149230957e-09, minz + 0.002590760588645935),
                (minx + 0.010583892464637756, -1.862645149230957e-09, minz + 0.0024807280860841274),
                (minx + 0.010030724108219147, -1.862645149230957e-09, minz + 0.002590760588645935),
                (minx + 0.00956176221370697, -1.862645149230957e-09, minz + 0.002904107444919646),
                (minx + 0.009248420596122742, -1.862645149230957e-09, minz + 0.0033730644499883056),
                (minx + 0.009138382971286774, -1.862645149230957e-09, maxz - 0.0039262363893523555),
                (minx + 0.009248420596122742, -1.862645149230957e-09, maxz - 0.0033730638679116964),
                (minx + 0.00956176221370697, -1.862645149230957e-09, maxz - 0.0029041070956736803),
                (minx + 0.010030724108219147, -1.862645149230957e-09, maxz - 0.0025907597737386823),
                (maxx - 0.0591098596341908, -0.017620893195271492, maxz),
                (maxx - 0.0591098596341908, -0.01611838862299919, maxz - 0.00029886653646826744),
                (maxx - 0.0591098596341908, -0.014844624325633049, maxz - 0.0011499673128128052),
                (maxx - 0.0591098596341908, -0.013993524014949799, maxz - 0.002423731260932982),
                (maxx - 0.0591098596341908, -0.013694657012820244, minz + 0.003926236289926277),
                (maxx - 0.0591098596341908, -0.013993524014949799, minz + 0.002423729980364442),
                (maxx - 0.0591098596341908, -0.014844624325633049, minz + 0.001149968709796667),
                (maxx - 0.0591098596341908, -0.016118386760354042, minz + 0.0002988663036376238),
                (maxx - 0.0591098596341908, -0.017620891332626343, minz),
                (maxx - 0.0591098596341908, -0.019123397767543793, minz + 0.00029886653646826744),
                (maxx - 0.0591098596341908, -0.020397160202264786, minz + 0.0011499675456434488),
                (maxx - 0.0591098596341908, -0.021248264238238335, minz + 0.0024237307952716947),
                (maxx - 0.0591098596341908, -0.02154713124036789, maxz - 0.003926236195012284),
                (maxx - 0.0591098596341908, -0.021248262375593185, maxz - 0.0024237297475337982),
                (maxx - 0.0591098596341908, -0.020397160202264786, maxz - 0.0011499666143208742),
                (maxx - 0.0591098596341908, -0.019123397767543793, maxz - 0.0002988646738231182)]

    # Faces
    myfaces = [(97, 96, 0, 1), (108, 107, 11, 12), (106, 105, 9, 10), (104, 103, 7, 8), (102, 101, 5, 6),
               (100, 99, 3, 4), (96, 111, 15, 0), (98, 97, 1, 2), (109, 108, 12, 13), (107, 106, 10, 11),
               (105, 104, 8, 9), (103, 102, 6, 7), (101, 100, 4, 5), (111, 110, 14, 15), (99, 98, 2, 3),
               (110, 109, 13, 14), (17, 16, 32, 33), (32, 47, 46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33),
               (28, 27, 43, 44), (26, 25, 41, 42),
               (24, 23, 39, 40), (22, 21, 37, 38), (20, 19, 35, 36), (16, 31, 47, 32), (18, 17, 33, 34),
               (29, 28, 44, 45), (27, 26, 42, 43), (25, 24, 40, 41), (23, 22, 38, 39), (21, 20, 36, 37),
               (31, 30, 46, 47), (19, 18, 34, 35), (30, 29, 45, 46),
               (0, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1),
               (97, 49, 48, 96),
               (108, 60, 59, 107), (106, 58, 57, 105), (104, 56, 55, 103), (102, 54, 53, 101), (100, 52, 51, 99),
               (96, 48, 63, 111), (98, 50, 49, 97), (109, 61, 60, 108), (107, 59, 58, 106), (105, 57, 56, 104),
               (103, 55, 54, 102), (101, 53, 52, 100), (111, 63, 62, 110), (99, 51, 50, 98), (110, 62, 61, 109),
               (65, 81, 80, 64), (80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95), (76, 92, 91, 75),
               (74, 90, 89, 73), (72, 88, 87, 71),
               (70, 86, 85, 69), (68, 84, 83, 67), (64, 80, 95, 79), (66, 82, 81, 65), (77, 93, 92, 76),
               (75, 91, 90, 74), (73, 89, 88, 72), (71, 87, 86, 70), (69, 85, 84, 68), (79, 95, 94, 78),
               (67, 83, 82, 66), (78, 94, 93, 77), (48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63)]

    return myvertex, myfaces


# ----------------------------------------------
# Creaate SKU code for inventory
# ----------------------------------------------
def createunitsku(self, cabinet):
    # ------------------
    # Wall or Floor
    # ------------------
    if self.type_cabinet == "1":
        p1 = "F"
    else:
        p1 = "W"
        # ------------------
    # Front type
    # ------------------
    if cabinet.dType == "1" or cabinet.dType == "2" or cabinet.dType == "3" or cabinet.dType == "8":
        p2 = "D"  # door
    elif cabinet.dType == "9" or cabinet.dType == "10":
        p2 = "L"  # door
    elif cabinet.dType == "4" or cabinet.dType == "5" or cabinet.dType == "6" or cabinet.dType == "11":
        p2 = "G"  # glass
    elif cabinet.dType == "7":
        p2 = "W"  # drawers
    else:
        p2 = "N"  # none
    # ------------------
    # Door number
    # ------------------
    if cabinet.dType == "1" or cabinet.dType == "2" or cabinet.dType == "3" or cabinet.dType == "4" \
            or cabinet.dType == "5" or cabinet.dType == "6" or cabinet.dType == "9" or cabinet.dType == "10":
        p3 = "01"
    elif cabinet.dType == "7":
        p3 = "%02d" % cabinet.dNum
    elif cabinet.dType == "8" or cabinet.dType == "11":
        p3 = "02"
    else:
        p3 = "00"
        # ------------------
    # Handles
    # ------------------
    if cabinet.hand is True:
        p4 = 1
    else:
        p4 = 0
        # ------------------
    # Shelves
    # ------------------
    # noinspection PyBroadException
    try:
        if cabinet.dType == "7":
            p5 = "00"  # drawers is always 0
        else:
            p5 = "%02d" % cabinet.sNum
    except:
        p5 = "00"
        # ------------------
    # Size
    # ------------------
    x = cabinet.sX
    y = self.depth + cabinet.wY
    z = self.height + cabinet.wZ

    p6 = "%06.3fx%06.3fx%06.3f-%06.3f" % (x, y, z, self.thickness)

    # ------------------
    # Door Size
    # ------------------
    if cabinet.dType == "1" or cabinet.dType == "2" or cabinet.dType == "3" \
            or cabinet.dType == "4" or cabinet.dType == "5" or cabinet.dType == "6":
        p7 = "%06.3f" % cabinet.sX
    elif cabinet.dType == "8" or cabinet.dType == "11":
        p7 = "%06.3f" % (cabinet.sX / 2)
    elif cabinet.dType == "9" or cabinet.dType == "10":  # corners
        dwidth = cabinet.sX - self.depth - self.thickness - 0.001
        p7 = "%06.3f" % dwidth
    else:
        p7 = "%06.3f" % 0

    sku = "%s%s%s%s%s-%s-%s" % (p1, p2, p3, p4, p5, p6, p7)

    return sku
