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
# Automatic generation of books
# Author: Antonio Vazquez (antonioya)
#
# ----------------------------------------------------------
# noinspection PyUnresolvedReferences
import bpy
from math import cos, sin, radians
from random import randint
from copy import copy
from colorsys import rgb_to_hsv, hsv_to_rgb
from bpy.types import Operator
from bpy.props import BoolProperty, IntProperty, FloatProperty, FloatVectorProperty
from .achm_tools import *


# ------------------------------------------------------------------
# Define UI class
# Books
# ------------------------------------------------------------------
class AchmBooks(Operator):
    bl_idname = "mesh.archimesh_books"
    bl_label = "Books"
    bl_description = "Books Generator"
    bl_category = 'Archimesh'
    bl_options = {'REGISTER', 'UNDO'}

    width = FloatProperty(
            name='Width', min=0.001, max=1, default=0.045, precision=3,
            description='Bounding book width',
            )
    depth = FloatProperty(
            name='Depth', min=0.001, max=1, default=0.22, precision=3,
            description='Bounding book depth',
            )
    height = FloatProperty(
            name='Height', min=0.001, max=1, default=0.30, precision=3,
            description='Bounding book height',
            )
    num = IntProperty(
            name='Number of books', min=1, max=100, default=20,
            description='Number total of books',
            )

    rX = FloatProperty(
            name='X', min=0.000, max=0.999, default=0, precision=3,
            description='Randomness for X axis',
            )
    rY = FloatProperty(
            name='Y', min=0.000, max=0.999, default=0, precision=3,
            description='Randomness for Y axis',
            )
    rZ = FloatProperty(
            name='Z', min=0.000, max=0.999, default=0, precision=3,
            description='Randomness for Z axis',
            )

    rot = FloatProperty(
            name='Rotation', min=0.000, max=1, default=0, precision=3,
            description='Randomness for vertical position (0-> All straight)',
            )
    afn = IntProperty(
            name='Affinity', min=0, max=10, default=5,
            description='Number of books with same rotation angle',
            )

    # Materials
    crt_mat = BoolProperty(
            name="Create default Cycles materials",
            description="Create default materials for Cycles render",
            default=True,
            )
    objcol = FloatVectorProperty(
            name="Color",
            description="Color for material",
            default=(1.0, 1.0, 1.0, 1.0),
            min=0.1, max=1,
            subtype='COLOR',
            size=4,
            )
    rC = FloatProperty(
            name='Randomness',
            min=0.000, max=1, default=0, precision=3,
            description='Randomness for color ',
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
            box.label("Book size")
            row = box.row()
            row.prop(self, 'width')
            row.prop(self, 'depth')
            row.prop(self, 'height')
            row = box.row()
            row.prop(self, 'num', slider=True)

            box = layout.box()
            box.label("Randomness")
            row = box.row()
            row.prop(self, 'rX', slider=True)
            row.prop(self, 'rY', slider=True)
            row.prop(self, 'rZ', slider=True)
            row = box.row()
            row.prop(self, 'rot', slider=True)
            row.prop(self, 'afn', slider=True)

            box = layout.box()
            if not context.scene.render.engine == 'CYCLES':
                box.enabled = False
            box.prop(self, 'crt_mat')
            if self.crt_mat:
                row = box.row()
                row.prop(self, 'objcol')
                row = box.row()
                row.prop(self, 'rC', slider=True)
        else:
            row = layout.row()
            row.label("Warning: Operator does not work in local view mode", icon='ERROR')

    # -----------------------------------------------------
    # Execute
    # -----------------------------------------------------
    # noinspection PyUnusedLocal
    def execute(self, context):
        if bpy.context.mode == "OBJECT":
            # Create shelves
            create_book_mesh(self)
            return {'FINISHED'}
        else:
            self.report({'WARNING'}, "Archimesh: Option only valid in Object mode")
            return {'CANCELLED'}


# ------------------------------------------------------------------------------
# Generate mesh data
# All custom values are passed using self container (self.myvariable)
# ------------------------------------------------------------------------------
def create_book_mesh(self):
    # deactivate others
    for o in bpy.data.objects:
        if o.select is True:
            o.select = False
    bpy.ops.object.select_all(False)
    generate_books(self)

    return


# ------------------------------------------------------------------------------
# Generate books
# All custom values are passed using self container (self.myvariable)
# ------------------------------------------------------------------------------
def generate_books(self):
    boxes = []
    location = bpy.context.scene.cursor_location
    myloc = copy(location)  # copy location to keep 3D cursor position

    # Create
    lastx = myloc.x
    ox = 0
    oy = 0
    oz = 0
    ot = 0
    i = 0
    for x in range(self.num):
        # reset rotation
        if i >= self.afn:
            i = 0
            ot = -1

        mydata = create_book("Book" + str(x),
                             self.width, self.depth, self.height,
                             lastx, myloc.y, myloc.z,
                             self.crt_mat if bpy.context.scene.render.engine == 'CYCLES' else False,
                             self.rX, self.rY, self.rZ, self.rot, ox, oy, oz, ot,
                             self.objcol, self.rC)
        boxes.extend([mydata[0]])
        bookdata = mydata[1]

        # calculate rotation using previous book
        ot = bookdata[3]
        i += 1
        oz = 0

        # calculate x size after rotation
        if i < self.afn:
            size = 0.0002
        else:
            size = 0.0003 + cos(radians(90 - bookdata[3])) * bookdata[2]  # the height is the radius
            oz = bookdata[2]

        lastx = lastx + bookdata[0] + size

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

    return


# ------------------------------------------------------------------------------
# Create books unit
#
# objName: Name for the new object
# thickness: wood thickness (sides)
# sX: Size in X axis
# sY: Size in Y axis
# sZ: Size in Z axis
# pX: position X axis
# pY: position Y axis
# pZ: position Z axis
# mat: Flag for creating materials
# frX: Random factor X
# frY: Random factor Y
# frZ: Random factor Z
# frR: Random factor Rotation
# oX: override x size
# oY: override y size
# oZ: override z size
# oR: override rotation
# objcol: color
# frC: color randomness factor
# ------------------------------------------------------------------------------
def create_book(objname, sx, sy, sz, px, py, pz, mat, frx,
                fry, frz, frr, ox, oy, oz, ot, objcol, frc):
    # gap Randomness
    ri = randint(10, 150)
    gap = ri / 100000
    # Randomness X
    if ox == 0:
        ri = randint(0, int(frx * 1000))
        factor = ri / 1000
        sx -= sx * factor
        if sx < (gap * 3):
            sx = gap * 3
    else:
        sx = ox

        # Randomness Y
    if oy == 0:
        ri = randint(0, int(fry * 1000))
        factor = ri / 1000
        sy -= sy * factor
        if sy < (gap * 3):
            sy = gap * 3
    else:
        sy = oy

        # Randomness Z
    if oz == 0:
        ri = randint(0, int(frz * 1000))
        factor = ri / 1000
        sz -= sz * factor
        if sz < (gap * 3):
            sz = gap * 3
    else:
        sz = oz

        # Randomness rotation
    rot = 0
    if frr > 0 and ot != -1:
        if ot == 0:
            ri = randint(0, int(frr * 1000))
            factor = ri / 1000
            rot = 30 * factor
        else:
            rot = ot

    # Randomness color (only hue)
    hsv = rgb_to_hsv(objcol[0], objcol[1], objcol[2])
    hue = hsv[0]
    if frc > 0:
        rc1 = randint(0, int(hue * 1000))  # 0 to hue
        rc2 = randint(int(hue * 1000), 1000)  # hue to maximum
        rc3 = randint(0, 1000)  # sign

        if rc3 >= hue * 1000:
            hue += (rc2 * frc) / 1000
        else:
            hue -= (rc1 * frc) / 1000
        # Convert random color
        objcol = hsv_to_rgb(hue, hsv[1], hsv[2])

    myvertex = []
    myfaces = []
    x = 0
    # Left side
    myvertex.extend([(x, -sy, 0), (0, 0, 0), (x, 0, sz), (x, -sy, sz)])
    myfaces.extend([(0, 1, 2, 3)])

    myvertex.extend([(x + gap, -sy + gap, 0), (x + gap, 0, 0), (x + gap, 0, sz),
                     (x + gap, -sy + gap, sz)])
    myfaces.extend([(4, 5, 6, 7)])

    # Right side
    x = sx - gap
    myvertex.extend([(x, -sy + gap, 0), (x, 0, 0), (x, 0, sz), (x, -sy + gap, sz)])
    myfaces.extend([(8, 9, 10, 11)])

    myvertex.extend([(x + gap, -sy, 0), (x + gap, 0, 0), (x + gap, 0, sz), (x + gap, -sy, sz)])
    myfaces.extend([(12, 13, 14, 15)])

    myfaces.extend(
        [(0, 12, 15, 3), (4, 8, 11, 7), (3, 15, 11, 7), (0, 12, 8, 4), (0, 1, 5, 4),
         (8, 9, 13, 12), (3, 2, 6, 7),
         (11, 10, 14, 15), (1, 2, 6, 5), (9, 10, 14, 13)])

    # Top inside
    myvertex.extend([(gap, -sy + gap, sz - gap), (gap, -gap, sz - gap), (sx - gap, -gap, sz - gap),
                     (sx - gap, -sy + gap, sz - gap)])
    myfaces.extend([(16, 17, 18, 19)])

    # bottom inside and front face
    myvertex.extend([(gap, -sy + gap, gap), (gap, -gap, gap), (sx - gap, -gap, gap), (sx - gap, -sy + gap, gap)])
    myfaces.extend([(20, 21, 22, 23), (17, 18, 22, 21)])

    mymesh = bpy.data.meshes.new(objname)
    mybook = bpy.data.objects.new(objname, mymesh)

    mybook.location[0] = px
    mybook.location[1] = py
    mybook.location[2] = pz + sin(radians(rot)) * sx
    bpy.context.scene.objects.link(mybook)

    mymesh.from_pydata(myvertex, [], myfaces)
    mymesh.update(calc_edges=True)

    # ---------------------------------
    # Materials and UV Maps
    # ---------------------------------
    if mat and bpy.context.scene.render.engine == 'CYCLES':
        rgb = objcol
        # External
        mat = create_diffuse_material(objname + "_material", True,
                                      rgb[0], rgb[1], rgb[2], rgb[0], rgb[1], rgb[2], 0.05)
        set_material(mybook, mat)
        # UV unwrap external
        select_faces(mybook, 0, True)
        select_faces(mybook, 3, False)
        select_faces(mybook, 4, False)
        unwrap_mesh(mybook, False)
        # Add Internal
        mat = create_diffuse_material(objname + "_side_material", True, 0.5, 0.5, 0.5, 0.5, 0.5, 0.3, 0.03)
        mybook.data.materials.append(mat)
        select_faces(mybook, 14, True)
        select_faces(mybook, 15, False)
        select_faces(mybook, 16, False)
        set_material_faces(mybook, 1)
        # UV unwrap
        bpy.ops.object.mode_set(mode='EDIT', toggle=False)
        bpy.ops.mesh.select_all(action='DESELECT')
        bpy.ops.object.mode_set(mode='OBJECT')
        select_faces(mybook, 14, True)
        select_faces(mybook, 15, False)
        select_faces(mybook, 16, False)
        unwrap_mesh(mybook, False)

    # ---------------------------------
    # Rotation on Y axis
    # ---------------------------------
    mybook.rotation_euler = (0.0, radians(rot), 0.0)  # radians

    # add some gap to the size between books
    return mybook, (sx, sy, sz, rot)
