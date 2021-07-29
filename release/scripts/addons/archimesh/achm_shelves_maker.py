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
# Automatic generation of shelves
# Author: Antonio Vazquez (antonioya)
#
# ----------------------------------------------------------
import bpy
from copy import copy
from bpy.types import Operator, PropertyGroup
from bpy.props import FloatProperty, BoolProperty, IntProperty, CollectionProperty, EnumProperty
from .achm_tools import *


# ------------------------------------------------------------------
# Define property group class for shelves properties
# ------------------------------------------------------------------
class ShelvesProperties(PropertyGroup):
    sX = FloatProperty(name='width', min=0.001, max=10, default=1,
                       precision=3, description='Furniture width')
    wY = FloatProperty(name='', min=-10, max=10, default=0, precision=3, description='Modify y size')
    wZ = FloatProperty(name='', min=-10, max=10, default=0, precision=3, description='Modify z size')
    # Cabinet position shift
    pX = FloatProperty(name='', min=0, max=10, default=0, precision=3, description='Position x shift')
    pY = FloatProperty(name='', min=-10, max=10, default=0, precision=3, description='Position y shift')
    pZ = FloatProperty(name='', min=-10, max=10, default=0, precision=3, description='Position z shift')

    # Shelves
    sNum = IntProperty(name='Shelves', min=0, max=12, default=6, description='Number total of shelves')

    # 12 shelves (shelf)
    Z01 = FloatProperty(name='zS1', min=-10, max=10, default=0, precision=3, description='Position z shift')
    Z02 = FloatProperty(name='zS2', min=-10, max=10, default=0, precision=3, description='Position z shift')
    Z03 = FloatProperty(name='zS3', min=-10, max=10, default=0, precision=3, description='Position z shift')
    Z04 = FloatProperty(name='zS4', min=-10, max=10, default=0, precision=3, description='Position z shift')
    Z05 = FloatProperty(name='zS5', min=-10, max=10, default=0, precision=3, description='Position z shift')
    Z06 = FloatProperty(name='zS6', min=-10, max=10, default=0, precision=3, description='Position z shift')
    Z07 = FloatProperty(name='zS7', min=-10, max=10, default=0, precision=3, description='Position z shift')
    Z08 = FloatProperty(name='zS8', min=-10, max=10, default=0, precision=3, description='Position z shift')
    Z09 = FloatProperty(name='zS9', min=-10, max=10, default=0, precision=3, description='Position z shift')
    Z10 = FloatProperty(name='zS10', min=-10, max=10, default=0, precision=3, description='Position z shift')
    Z11 = FloatProperty(name='zS11', min=-10, max=10, default=0, precision=3, description='Position z shift')
    Z12 = FloatProperty(name='zS12', min=-10, max=10, default=0, precision=3, description='Position z shift')

    right = BoolProperty(name="Right", description="Create right side", default=True)
    left = BoolProperty(name="Left", description="Create left side", default=True)

bpy.utils.register_class(ShelvesProperties)


# ------------------------------------------------------------------
# Define UI class
# Shelves
# ------------------------------------------------------------------
class AchmShelves(Operator):
    bl_idname = "mesh.archimesh_shelves"
    bl_label = "Shelves"
    bl_description = "Shelves Generator"
    bl_category = 'Archimesh'
    bl_options = {'REGISTER', 'UNDO'}

    thickness = FloatProperty(
            name='Side Thickness', min=0.001, max=5,
            default=0.03, precision=3,
            description='Board thickness',
            )
    sthickness = FloatProperty(
            name='Shelves Thickness', min=0.001, max=5,
            default=0.03, precision=3,
            description='Board thickness',
            )
    depth = FloatProperty(
            name='Depth', min=0.001, max=50,
            default=0.28, precision=3,
            description='Default unit depth',
            )
    height = FloatProperty(
            name='Height', min=0.001, max=50,
            default=2, precision=3,
            description='Default unit height',
            )
    top = FloatProperty(
            name='Top', min=0, max=50,
            default=0.03, precision=3,
            description='Default top shelf position',
            )
    bottom = FloatProperty(
            name='Bottom', min=0, max=50,
            default=0.07, precision=3,
            description='Default bottom self position',
            )
    stype = EnumProperty(
            items=(
                ('1', "Full side", ""),
                ('4', "4 Legs", ""),
                ('99', "None", "")),
            name="Sides",
            description="Type of side construction",
            )

    fitZ = BoolProperty(
            name="Floor origin in Z=0",
            description="Use Z=0 axis as vertical origin floor position",
            default=True,
            )

    shelves_num = IntProperty(
            name='Number of Units',
            min=1, max=10,
            default=1,
            description='Number total of shelves units',
            )
    shelves = CollectionProperty(type=ShelvesProperties)

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
            row.prop(self, 'thickness')
            row.prop(self, 'sthickness')
            row = box.row()
            row.prop(self, 'depth')
            row.prop(self, 'height')
            row = box.row()
            row.prop(self, 'top')
            row.prop(self, 'bottom')
            row = box.row()
            row.prop(self, 'stype')
            row.prop(self, 'fitZ')

            # Furniture number
            row = layout.row()
            row.prop(self, 'shelves_num')
            # Add menu for shelves
            if self.shelves_num > 0:
                for idx in range(0, self.shelves_num):
                    box = layout.box()
                    add_shelves(self, box, idx + 1, self.shelves[idx])

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
            # Create all elements
            for i in range(len(self.shelves) - 1, self.shelves_num):
                self.shelves.add()

            # Create shelves
            create_shelves_mesh(self)
            return {'FINISHED'}
        else:
            self.report({'WARNING'}, "Archimesh: Option only valid in Object mode")
            return {'CANCELLED'}


# -----------------------------------------------------
# Add shelves parameters
# -----------------------------------------------------
def add_shelves(self, box, num, sh):
    row = box.row()
    row.label("Unit " + str(num))
    row.prop(sh, 'sX')

    row = box.row()
    row.prop(sh, 'wY')
    row.prop(sh, 'wZ')
    if self.stype != "99":
        row.prop(sh, 'left')
        row.prop(sh, 'right')

    row = box.row()
    row.prop(sh, 'pX')
    row.prop(sh, 'pY')
    row.prop(sh, 'pZ')

    row = box.row()
    row.prop(sh, 'sNum', slider=True)

    if sh.sNum >= 1:
        row = box.row()
        row.prop(sh, 'Z01')
    if sh.sNum >= 2:
        row.prop(sh, 'Z02')
    if sh.sNum >= 3:
        row.prop(sh, 'Z03')

    if sh.sNum >= 4:
        row = box.row()
        row.prop(sh, 'Z04')
    if sh.sNum >= 5:
        row.prop(sh, 'Z05')
    if sh.sNum >= 6:
        row.prop(sh, 'Z06')

    if sh.sNum >= 7:
        row = box.row()
        row.prop(sh, 'Z07')
    if sh.sNum >= 8:
        row.prop(sh, 'Z08')
    if sh.sNum >= 9:
        row.prop(sh, 'Z09')

    if sh.sNum >= 10:
        row = box.row()
        row.prop(sh, 'Z10')
    if sh.sNum >= 11:
        row.prop(sh, 'Z11')
    if sh.sNum >= 12:
        row.prop(sh, 'Z12')


# ------------------------------------------------------------------------------
# Generate mesh data
# All custom values are passed using self container (self.myvariable)
# ------------------------------------------------------------------------------
def create_shelves_mesh(self):
    # deactivate others
    for o in bpy.data.objects:
        if o.select is True:
            o.select = False
    bpy.ops.object.select_all(False)
    # Create units
    generate_shelves(self)

    return


# ------------------------------------------------------------------------------
# Generate Units
# All custom values are passed using self container (self.myvariable)
# ------------------------------------------------------------------------------
def generate_shelves(self):

    boxes = []
    location = bpy.context.scene.cursor_location
    myloc = copy(location)  # copy location to keep 3D cursor position
    # Fit to floor
    if self.fitZ:
        myloc[2] = 0

    # Create units
    lastx = myloc[0]
    # ------------------------------------------------------------------------------
    # Shelves
    # ------------------------------------------------------------------------------
    for i in range(0, self.shelves_num):
        mydata = create_unit(self.stype, "Shelves" + str(i + 1),
                             self.thickness, self.sthickness,
                             self.shelves[i].sX, self.depth + self.shelves[i].wY, self.height + self.shelves[i].wZ,
                             self.shelves[i].pX + lastx, myloc[1] + self.shelves[i].pY, myloc[2] + self.shelves[i].pZ,
                             self.shelves[i].left, self.shelves[i].right,
                             self.shelves[i].sNum,
                             (self.shelves[i].Z01, self.shelves[i].Z02, self.shelves[i].Z03,
                              self.shelves[i].Z04, self.shelves[i].Z05, self.shelves[i].Z06,
                              self.shelves[i].Z07, self.shelves[i].Z08, self.shelves[i].Z09,
                              self.shelves[i].Z10, self.shelves[i].Z11, self.shelves[i].Z12),
                             self.top, self.bottom)
        boxes.extend([mydata[0]])
        lastx = mydata[1]

    # refine units
    for box in boxes:
        remove_doubles(box)
        set_normals(box)

    # deactivate others
    for o in bpy.data.objects:
        if o.select is True:
            o.select = False

    boxes[0].select = True
    bpy.context.scene.objects.active = boxes[0]

    # Create materials
    if self.crt_mat and bpy.context.scene.render.engine == 'CYCLES':
        mat = create_diffuse_material("Shelves_material", False, 0.8, 0.8, 0.8)
        for box in boxes:
            set_material(box, mat)

    return


# ------------------------------------------------------------------------------
# Create shelves unit
#
# stype: type of sides
# objName: Name for the new object
# thickness: wood thickness (sides)
# sthickness: wood thickness (shelves)
# sX: Size in X axis
# sY: Size in Y axis
# sZ: Size in Z axis
# pX: position X axis
# pY: position Y axis
# pZ: position Z axis
# right: True-> create right side
# left: True-> create left side
# shelves: Number of shelves
# zPos: List with z shift for each self
# top: position of top shelf
# bottom: position of bottom shelf
# ------------------------------------------------------------------------------
def create_unit(stype, objname, thickness, sthickness, sx, sy, sz, px, py, pz, left, right, shelves, zpos,
                top, bottom):

    myvertex = []
    myfaces = []
    v = 0

    # no Sides, then no thickness
    if stype == "99":
        thickness = 0

    # ------------------------------
    # Left side
    # ------------------------------
    if left and stype != "99":
        # Full side
        if stype == "1":
            myvertex.extend([(0, 0, 0), (0, -sy, 0), (0, -sy, sz), (0, 0, sz),
                             (thickness, 0, 0), (thickness, -sy, 0), (thickness, -sy, sz), (thickness, 0, sz)])
            myfaces.extend([(v, v + 1, v + 2, v + 3), (v + 4, v + 5, v + 6, v + 7), (v, v + 4, v + 7, v + 3),
                            (v, v + 1, v + 5, v + 4),
                            (v + 3, v + 2, v + 6, v + 7), (v + 1, v + 2, v + 6, v + 5)])
            v += 8
        # Four legs
        if stype == "4":
            # back
            myvertex.extend([(0, 0, 0), (0, -thickness, 0), (0, -thickness, sz), (0, 0, sz),
                             (thickness, 0, 0), (thickness, -thickness, 0), (thickness, -thickness, sz),
                             (thickness, 0, sz)])
            myfaces.extend([(v, v + 1, v + 2, v + 3), (v + 4, v + 5, v + 6, v + 7), (v, v + 4, v + 7, v + 3),
                            (v, v + 1, v + 5, v + 4),
                            (v + 3, v + 2, v + 6, v + 7), (v + 1, v + 2, v + 6, v + 5)])
            v += 8
            # Front
            myvertex.extend([(0, -sy + thickness, 0), (0, -sy, 0), (0, -sy, sz), (0, -sy + thickness, sz),
                             (thickness, -sy + thickness, 0), (thickness, -sy, 0), (thickness, -sy, sz),
                             (thickness, -sy + thickness, sz)])
            myfaces.extend([(v, v + 1, v + 2, v + 3), (v + 4, v + 5, v + 6, v + 7), (v, v + 4, v + 7, v + 3),
                            (v, v + 1, v + 5, v + 4),
                            (v + 3, v + 2, v + 6, v + 7), (v + 1, v + 2, v + 6, v + 5)])
            v += 8

    # -----------------
    # Right side
    # -----------------
    if right and stype != "99":
        width = sx - thickness
        # Full side
        if stype == "1":
            myvertex.extend([(width, 0, 0), (width, -sy, 0), (width, -sy, sz), (width, 0, sz),
                             (width + thickness, 0, 0), (width + thickness, -sy, 0), (width + thickness, -sy, sz),
                             (width + thickness, 0, sz)])
            myfaces.extend([(v, v + 1, v + 2, v + 3), (v + 4, v + 5, v + 6, v + 7), (v, v + 4, v + 7, v + 3),
                            (v, v + 1, v + 5, v + 4), (v + 3, v + 2, v + 6, v + 7), (v + 1, v + 2, v + 6, v + 5)])
            v += 8
        # Four legs
        if stype == "4":
            # back
            myvertex.extend([(width, 0, 0), (width, -thickness, 0), (width, -thickness, sz), (width, 0, sz),
                             (width + thickness, 0, 0), (width + thickness, -thickness, 0),
                             (width + thickness, -thickness, sz), (width + thickness, 0, sz)])
            myfaces.extend([(v, v + 1, v + 2, v + 3), (v + 4, v + 5, v + 6, v + 7), (v, v + 4, v + 7, v + 3),
                            (v, v + 1, v + 5, v + 4), (v + 3, v + 2, v + 6, v + 7), (v + 1, v + 2, v + 6, v + 5)])
            v += 8
            # Front
            myvertex.extend(
                [(width, -sy + thickness, 0), (width, -sy, 0), (width, -sy, sz), (width, -sy + thickness, sz),
                 (width + thickness, -sy + thickness, 0), (width + thickness, -sy, 0), (width + thickness, -sy, sz),
                 (width + thickness, -sy + thickness, sz)])
            myfaces.extend([(v, v + 1, v + 2, v + 3), (v + 4, v + 5, v + 6, v + 7), (v, v + 4, v + 7, v + 3),
                            (v, v + 1, v + 5, v + 4), (v + 3, v + 2, v + 6, v + 7), (v + 1, v + 2, v + 6, v + 5)])
            v += 8
    # -----------------
    # shelves
    # -----------------
    posx = 0
    # calculate width
    width = sx - thickness
    posx = posx + thickness

    # calculate vertical spaces
    dist = sz - top - bottom - sthickness
    # if only top/bottom the space is not necessary
    if shelves > 2:
        space = dist / (shelves - 1)
    else:
        space = 0

    posz1 = bottom

    for x in range(shelves):
        # bottom
        if x == 0:
            posz1 = bottom
        # top
        if x == shelves - 1:
            posz1 = sz - top - sthickness

        posz2 = posz1 - sthickness
        myvertex.extend([(posx, 0, posz1 + zpos[x]), (posx, -sy, posz1 + zpos[x]),
                         (posx, -sy, posz2 + zpos[x]), (posx, 0, posz2 + zpos[x]),
                         (width, 0, posz1 + zpos[x]), (width, -sy, posz1 + zpos[x]),
                         (width, -sy, posz2 + zpos[x]), (width, 0, posz2 + zpos[x])])

        myfaces.extend(
            [(v, v + 1, v + 2, v + 3), (v + 4, v + 5, v + 6, v + 7), (v, v + 4, v + 7, v + 3), (v, v + 1, v + 5, v + 4),
             (v + 3, v + 2, v + 6, v + 7), (v + 1, v + 2, v + 6, v + 5)])
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

    return myobject, px + sx - thickness
