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
# Automatic generation of stairs
# Author: Antonio Vazquez (antonioya)
#
# ----------------------------------------------------------
# noinspection PyUnresolvedReferences
import bpy
from math import radians, sin, cos
from bpy.types import Operator
from bpy.props import FloatProperty, BoolProperty, IntProperty, EnumProperty
from .achm_tools import *


# ------------------------------------------------------------------
# Define UI class
# Stairs
# ------------------------------------------------------------------
class AchmStairs(Operator):
    bl_idname = "mesh.archimesh_stairs"
    bl_label = "Stairs"
    bl_description = "Stairs Generator"
    bl_category = 'Archimesh'
    bl_options = {'REGISTER', 'UNDO'}

    # Define properties
    model = EnumProperty(
            items=(
                ('1', "Rectangular", ""),
                ('2', "Rounded", ""),
                ),
            name="Model",
            description="Type of steps",
            )
    radio = FloatProperty(
            name='',
            min=0.001, max=0.500,
            default=0.20, precision=3,
            description='Radius factor for rounded',
            )
    curve = BoolProperty(
            name="Include deformation handles",
            description="Include a curve to modify the stairs curve",
            default=False,
            )

    step_num = IntProperty(
            name='Number of steps',
            min=1, max=1000,
            default=3,
            description='Number total of steps',
            )
    max_width = FloatProperty(
            name='Width',
            min=0.001, max=10,
            default=1, precision=3,
            description='Step maximum width',
            )
    depth = FloatProperty(
            name='Depth',
            min=0.001, max=10,
            default=0.30, precision=3,
            description='Depth of the step',
            )
    shift = FloatProperty(
            name='Shift',
            min=0.001, max=1,
            default=1, precision=3,
            description='Step shift in Y axis',
            )
    thickness = FloatProperty(
            name='Thickness',
            min=0.001, max=10,
            default=0.03, precision=3,
            description='Step thickness',
            )
    sizev = BoolProperty(
            name="Variable width",
            description="Steps are not equal in width",
            default=False,
            )
    back = BoolProperty(
            name="Close sides",
            description="Close all steps side to make a solid structure",
            default=False,
            )
    min_width = FloatProperty(
            name='',
            min=0.001, max=10,
            default=1, precision=3,
            description='Step minimum width',
            )

    height = FloatProperty(
            name='height',
            min=0.001, max=10,
            default=0.14, precision=3,
            description='Step height',
            )
    front_gap = FloatProperty(
            name='Front',
            min=0, max=10,
            default=0.03,
            precision=3,
            description='Front gap',
            )
    side_gap = FloatProperty(
            name='Side',
            min=0, max=10,
            default=0, precision=3,
            description='Side gap',
            )
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
            row.prop(self, 'model')
            if self.model == "2":
                row.prop(self, 'radio')

            box.prop(self, 'step_num')
            row = box.row()
            row.prop(self, 'max_width')
            row.prop(self, 'depth')
            row.prop(self, 'shift')
            row = box.row()
            row.prop(self, 'back')
            row.prop(self, 'sizev')
            row = box.row()
            row.prop(self, 'curve')
            # all equal
            if self.sizev is True:
                row.prop(self, 'min_width')

            box = layout.box()
            row = box.row()
            row.prop(self, 'thickness')
            row.prop(self, 'height')
            row = box.row()
            row.prop(self, 'front_gap')
            if self.model == "1":
                row.prop(self, 'side_gap')

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
            create_stairs_mesh(self)
            return {'FINISHED'}
        else:
            self.report({'WARNING'}, "Archimesh: Option only valid in Object mode")
            return {'CANCELLED'}


# ------------------------------------------------------------------------------
# Generate mesh data
# All custom values are passed using self container (self.myvariable)
# ------------------------------------------------------------------------------
def create_stairs_mesh(self):

    # deactivate others
    for o in bpy.data.objects:
        if o.select is True:
            o.select = False

    bpy.ops.object.select_all(False)

    # ------------------------
    # Create stairs
    # ------------------------
    mydata = create_stairs(self, "Stairs")
    mystairs = mydata[0]
    mystairs.select = True
    bpy.context.scene.objects.active = mystairs
    remove_doubles(mystairs)
    set_normals(mystairs)
    set_modifier_mirror(mystairs, "X")
    # ------------------------
    # Create curve handles
    # ------------------------
    if self.curve:
        x = mystairs.location.x
        y = mystairs.location.y
        z = mystairs.location.z

        last = mydata[1]
        x1 = last[1]  # use y

        myp = [((0, 0, 0), (- 0.25, 0, 0), (0.25, 0, 0)),
               ((x1, 0, 0), (x1 - 0.25, 0, 0), (x1 + 0.25, 0, 0))]  # double element
        mycurve = create_bezier("Stairs_handle", myp, (x, y, z))
        set_modifier_curve(mystairs, mycurve)

    # ------------------------
    # Create materials
    # ------------------------
    if self.crt_mat and bpy.context.scene.render.engine == 'CYCLES':
        # Stairs material
        mat = create_diffuse_material("Stairs_material", False, 0.8, 0.8, 0.8)
        set_material(mystairs, mat)

    bpy.ops.object.select_all(False)
    mystairs.select = True
    bpy.context.scene.objects.active = mystairs

    return


# ------------------------------------------------------------------------------
# Create rectangular Stairs
# ------------------------------------------------------------------------------
def create_stairs(self, objname):

    myvertex = []
    myfaces = []
    index = 0

    lastpoint = (0, 0, 0)
    for s in range(0, self.step_num):
        if self.model == "1":
            mydata = create_rect_step(self, lastpoint, myvertex, myfaces, index, s)
        if self.model == "2":
            mydata = create_round_step(self, lastpoint, myvertex, myfaces, index, s)
        index = mydata[0]
        lastpoint = mydata[1]

    mesh = bpy.data.meshes.new(objname)
    myobject = bpy.data.objects.new(objname, mesh)

    myobject.location = bpy.context.scene.cursor_location
    bpy.context.scene.objects.link(myobject)

    mesh.from_pydata(myvertex, [], myfaces)
    mesh.update(calc_edges=True)

    return myobject, lastpoint


# ------------------------------------------------------------------------------
# Create rectangular step
# ------------------------------------------------------------------------------
def create_rect_step(self, origin, myvertex, myfaces, index, step):
    x = origin[0]
    y = origin[1]
    z = origin[2]
    i = index
    max_depth = y + self.depth
    if self.back is True:
        max_depth = self.depth * self.step_num

    # calculate width (no side gap)
    if self.sizev is False:
        width = self.max_width / 2
    else:
        width = (self.max_width / 2) - (step * (((self.max_width - self.min_width) / 2) / self.step_num))

        # Vertical Rectangle
    myvertex.extend([(x, y, z), (x, y, z + self.height), (x + width, y, z + self.height), (x + width, y, z)])
    val = y + self.thickness
    myvertex.extend([(x, val, z), (x, val, z + self.height), (x + width, val, z + self.height), (x + width, val, z)])

    myfaces.extend([(i + 0, i + 1, i + 2, i + 3), (i + 4, i + 5, i + 6, i + 7), (i + 0, i + 3, i + 7, i + 4),
                    (i + 1, i + 2, i + 6, i + 5), (i + 0, i + 1, i + 5, i + 4), (i + 3, i + 2, i + 6, i + 7)])
    # Side plane
    myvertex.extend([(x + width, max_depth, z + self.height), (x + width, max_depth, z)])
    myfaces.extend([(i + 7, i + 6, i + 8, i + 9)])
    i += 10
    # calculate width (side gap)
    width = width + self.side_gap

    # Horizontal Rectangle
    z = z + self.height
    myvertex.extend([(x, y - self.front_gap, z), (x, max_depth, z), (x + width, max_depth, z),
                     (x + width, y - self.front_gap, z)])
    z = z + self.thickness
    myvertex.extend([(x, y - self.front_gap, z), (x, max_depth, z), (x + width, max_depth, z),
                     (x + width, y - self.front_gap, z)])
    myfaces.extend([(i + 0, i + 1, i + 2, i + 3), (i + 4, i + 5, i + 6, i + 7), (i + 0, i + 3, i + 7, i + 4),
                    (i + 1, i + 2, i + 6, i + 5), (i + 3, i + 2, i + 6, i + 7)])
    i += 8
    # remap origin
    y = y + (self.depth * self.shift)

    return i, (x, y, z)


# ------------------------------------------------------------------------------
# Create rounded step
# ------------------------------------------------------------------------------
def create_round_step(self, origin, myvertex, myfaces, index, step):
    x = origin[0]
    y = origin[1]
    z = origin[2]
    pos_x = None
    i = index
    li = [radians(270), radians(288), radians(306), radians(324), radians(342),
          radians(0)]

    max_width = self.max_width
    max_depth = y + self.depth
    if self.back is True:
        max_depth = self.depth * self.step_num

    # Resize for width
    if self.sizev is True:
        max_width = max_width - (step * ((self.max_width - self.min_width) / self.step_num))

    half = max_width / 2
    # ------------------------------------
    # Vertical
    # ------------------------------------
    # calculate width
    width = half - (half * self.radio)
    myradio = half - width

    myvertex.extend([(x, y, z), (x, y, z + self.height)])
    # Round
    for e in li:
        pos_x = (cos(e) * myradio) + x + width - myradio
        pos_y = (sin(e) * myradio) + y + myradio

        myvertex.extend([(pos_x, pos_y, z), (pos_x, pos_y, z + self.height)])

    # back point
    myvertex.extend([(x + width, max_depth, z), (x + width, max_depth, z + self.height)])

    myfaces.extend([(i, i + 1, i + 3, i + 2), (i + 2, i + 3, i + 5, i + 4), (i + 4, i + 5, i + 7, i + 6),
                    (i + 6, i + 7, i + 9, i + 8),
                    (i + 8, i + 9, i + 11, i + 10), (i + 10, i + 11, i + 13, i + 12), (i + 12, i + 13, i + 15, i + 14)])

    i += 16
    # ------------------------------------
    # Horizontal
    # ------------------------------------
    # calculate width gap
    width = half + self.front_gap - (half * self.radio)

    z = z + self.height
    # Vertical
    myvertex.extend([(x, y - self.front_gap, z), (x, y - self.front_gap, z + self.thickness)])
    # Round
    for e in li:
        pos_x = (cos(e) * myradio) + x + width - myradio
        pos_y = (sin(e) * myradio) + y + myradio - self.front_gap

        myvertex.extend([(pos_x, pos_y, z), (pos_x, pos_y, z + self.thickness)])

    # back points
    myvertex.extend([(pos_x, max_depth, z), (pos_x, max_depth, z + self.thickness),
                     (x, max_depth, z), (x, max_depth, z + self.thickness)])

    myfaces.extend([(i, i + 1, i + 3, i + 2), (i + 2, i + 3, i + 5, i + 4), (i + 4, i + 5, i + 7, i + 6),
                    (i + 6, i + 7, i + 9, i + 8),
                    (i + 8, i + 9, i + 11, i + 10), (i + 10, i + 11, i + 13, i + 12), (i + 12, i + 13, i + 15, i + 14),
                    (i, i + 2, i + 4, i + 6, i + 8, i + 10, i + 12, i + 14, i + 16),
                    (i + 1, i + 3, i + 5, i + 7, i + 9, i + 11, i + 13, i + 15, i + 17),
                    (i + 14, i + 15, i + 17, i + 16)])

    i += 18
    z = z + self.thickness

    # remap origin
    y = y + (self.depth * self.shift)

    return i, (x, y, z)


# ------------------------------------------------------------------------------
# Create bezier curve
# ------------------------------------------------------------------------------
def create_bezier(objname, points, origin):
    curvedata = bpy.data.curves.new(name=objname, type='CURVE')
    curvedata.dimensions = '3D'

    myobject = bpy.data.objects.new(objname, curvedata)
    myobject.location = origin
    myobject.rotation_euler[2] = radians(90)

    bpy.context.scene.objects.link(myobject)

    polyline = curvedata.splines.new('BEZIER')
    polyline.bezier_points.add(len(points) - 1)

    for idx, (knot, h1, h2) in enumerate(points):
        point = polyline.bezier_points[idx]
        point.co = knot
        point.handle_left = h1
        point.handle_right = h2
        point.handle_left_type = 'FREE'
        point.handle_right_type = 'FREE'

    return myobject
