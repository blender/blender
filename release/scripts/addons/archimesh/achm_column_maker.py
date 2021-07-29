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
# Automatic generation of columns
# Author: Antonio Vazquez (antonioya)
#
# ----------------------------------------------------------
# noinspection PyUnresolvedReferences
import bpy
from math import cos, sin, radians, atan, sqrt
from .achm_tools import *


# ------------------------------------------------------------------
# Define UI class
# Columns
# ------------------------------------------------------------------
class AchmColumn(bpy.types.Operator):
    bl_idname = "mesh.archimesh_column"
    bl_label = "Column"
    bl_description = "Columns Generator"
    bl_category = 'Archimesh'
    bl_options = {'REGISTER', 'UNDO'}

    # Define properties
    model = bpy.props.EnumProperty(
            name="Model",
            items=(
                ('1', "Circular", ""),
                ('2', "Rectangular", ""),
                ),
            description="Type of column",
            )
    keep_size = bpy.props.BoolProperty(
            name="Keep radius equal",
            description="Keep all radius (top, mid and bottom) to the same size",
            default=True,
            )

    rad_top = bpy.props.FloatProperty(
            name='Top radius',
            min=0.001, max=10, default=0.15, precision=3,
            description='Radius of the column in the top',
            )
    rad_mid = bpy.props.FloatProperty(
            name='Middle radius',
            min=0.001, max=10, default=0.15, precision=3,
            description='Radius of the column in the middle',
            )
    shift = bpy.props.FloatProperty(
            name='',
            min=-1, max=1, default=0, precision=3,
            description='Middle displacement',
            )

    rad_bottom = bpy.props.FloatProperty(
            name='Bottom radius',
            min=0.001, max=10, default=0.15, precision=3,
            description='Radius of the column in the bottom',
            )

    col_height = bpy.props.FloatProperty(
            name='Total height',
            min=0.001, max=10, default=2.4, precision=3,
            description='Total height of column, including bases and tops',
            )
    col_sx = bpy.props.FloatProperty(
            name='X size',
            min=0.001, max=10, default=0.30, precision=3,
            description='Column size for x axis',
            )
    col_sy = bpy.props.FloatProperty(
            name='Y size',
            min=0.001, max=10, default=0.30, precision=3,
            description='Column size for y axis',
            )

    cir_base = bpy.props.BoolProperty(
            name="Include circular base",
            description="Include a base with circular form",
            default=False,
            )
    cir_base_r = bpy.props.FloatProperty(
            name='Radio',
            min=0.001, max=10, default=0.08, precision=3,
            description='Rise up radio of base',
            )
    cir_base_z = bpy.props.FloatProperty(
            name='Height',
            min=0.001, max=10, default=0.05, precision=3,
            description='Size for z axis',
            )

    cir_top = bpy.props.BoolProperty(
            name="Include circular top",
            description="Include a top with circular form",
            default=False,
            )
    cir_top_r = bpy.props.FloatProperty(
            name='Radio',
            min=0.001, max=10, default=0.08, precision=3,
            description='Rise up radio of top',
            )
    cir_top_z = bpy.props.FloatProperty(
            name='Height',
            min=0.001, max=10, default=0.05, precision=3,
            description='Size for z axis',
            )

    box_base = bpy.props.BoolProperty(
            name="Include rectangular base",
            description="Include a base with rectangular form",
            default=True,
            )
    box_base_x = bpy.props.FloatProperty(
            name='X size',
            min=0.001, max=10, default=0.40, precision=3,
            description='Size for x axis',
            )
    box_base_y = bpy.props.FloatProperty(
            name='Y size',
            min=0.001, max=10, default=0.40, precision=3,
            description='Size for y axis',
            )
    box_base_z = bpy.props.FloatProperty(
            name='Height',
            min=0.001, max=10, default=0.05, precision=3,
            description='Size for z axis',
            )

    box_top = bpy.props.BoolProperty(
            name="Include rectangular top",
            description="Include a top with rectangular form",
            default=True,
            )
    box_top_x = bpy.props.FloatProperty(
            name='X size',
            min=0.001, max=10, default=0.40, precision=3,
            description='Size for x axis',
            )
    box_top_y = bpy.props.FloatProperty(
            name='Y size',
            min=0.001, max=10, default=0.40, precision=3,
            description='Size for y axis',
            )
    box_top_z = bpy.props.FloatProperty(
            name='Height',
            min=0.001, max=10, default=0.05, precision=3,
            description='Size for z axis',
            )

    arc_top = bpy.props.BoolProperty(
            name="Create top arch",
            description="Include an arch in the top of the column",
            default=False,
            )
    arc_radio = bpy.props.FloatProperty(
            name='Arc Radio',
            min=0.001, max=10, default=1, precision=1,
            description='Radio of the arch',
            )
    arc_width = bpy.props.FloatProperty(
            name='Thickness',
            min=0.01, max=10, default=0.15, precision=2,
            description='Thickness of the arch wall',
            )
    arc_gap = bpy.props.FloatProperty(
            name='Arc gap',
            min=0.01, max=10, default=0.25, precision=2,
            description='Size of the gap in the arch sides',
            )

    crt_mat = bpy.props.BoolProperty(
            name="Create default Cycles materials",
            description="Create default materials for Cycles render",
            default=True,
            )
    crt_array = bpy.props.BoolProperty(
            name="Create array of elements",
            description="Create a modifier array for all elemnst",
            default=False,
            )
    array_num_x = bpy.props.IntProperty(
            name='Count X',
            min=0, max=100, default=3,
            description='Number of elements in array',
            )
    array_space_x = bpy.props.FloatProperty(
            name='Distance X',
            min=0.000, max=10, default=1, precision=3,
            description='Distance between elements (only arc disabled)',
            )
    array_num_y = bpy.props.IntProperty(
            name='Count Y',
            min=0, max=100, default=0,
            description='Number of elements in array',
            )
    array_space_y = bpy.props.FloatProperty(
            name='Distance Y',
            min=0.000, max=10, default=1, precision=3,
            description='Distance between elements (only arc disabled)',
            )
    array_space_z = bpy.props.FloatProperty(
            name='Distance Z',
            min=-10, max=10, default=0, precision=3,
            description='Combined X/Z distance between elements (only arc disabled)',
            )
    ramp = bpy.props.BoolProperty(
            name="Deform",
            description="Deform top base with Z displacement", default=True,
            )
    array_space_factor = bpy.props.FloatProperty(
            name='Move Y center',
            min=0.00, max=1, default=0.0, precision=3,
            description='Move the center of the arch in Y axis. (0 centered)',
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
            box.prop(self, 'model')
            # Circular
            if self.model == "1":
                box.prop(self, 'keep_size')
                box.prop(self, 'rad_top')
                if self.keep_size is False:
                    row = box.row()
                    row.prop(self, 'rad_mid')
                    row.prop(self, 'shift')
                    box.prop(self, 'rad_bottom')

            # Rectangular
            if self.model == "2":
                box.prop(self, 'col_sx')
                box.prop(self, 'col_sy')

            box.prop(self, 'col_height')

            box = layout.box()
            box.prop(self, 'box_base')
            if self.box_base is True:
                row = box.row()
                row.prop(self, 'box_base_x')
                row.prop(self, 'box_base_y')
                row.prop(self, 'box_base_z')

            box = layout.box()
            box.prop(self, 'box_top')
            if self.box_top is True:
                row = box.row()
                row.prop(self, 'box_top_x')
                row.prop(self, 'box_top_y')
                row.prop(self, 'box_top_z')

            box = layout.box()
            box.prop(self, 'cir_base')
            if self.cir_base is True:
                row = box.row()
                row.prop(self, 'cir_base_r')
                row.prop(self, 'cir_base_z')

            box = layout.box()
            box.prop(self, 'cir_top')
            if self.cir_top is True:
                row = box.row()
                row.prop(self, 'cir_top_r')
                row.prop(self, 'cir_top_z')

            box = layout.box()
            box.prop(self, 'arc_top')
            if self.arc_top is True:
                row = box.row()
                row.prop(self, 'arc_radio')
                row.prop(self, 'arc_width')
                row = box.row()
                row.prop(self, 'arc_gap')
                row.prop(self, 'array_space_factor')

            box = layout.box()
            box.prop(self, 'crt_array')
            if self.crt_array is True:
                row = box.row()
                row.prop(self, 'array_num_x')
                row.prop(self, 'array_num_y')
                if self.arc_top is True:
                    box.label("Use arch radio and thickness to set distances")

                if self.arc_top is False:
                    row = box.row()
                    row.prop(self, 'array_space_x')
                    row.prop(self, 'array_space_y')
                    row = box.row()
                    row.prop(self, 'array_space_z')
                    row.prop(self, 'ramp')

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
            create_column_mesh(self)
            return {'FINISHED'}
        else:
            self.report({'WARNING'}, "Archimesh: Option only valid in Object mode")
            return {'CANCELLED'}


# ------------------------------------------------------------------------------
# Generate mesh data
# All custom values are passed using self container (self.myvariable)
# ------------------------------------------------------------------------------
def create_column_mesh(self):
    myarc = None
    cir_top = None
    cir_bottom = None
    box_top = None
    box_bottom = None
    mycolumn = None
    # deactivate others
    for o in bpy.data.objects:
        if o.select is True:
            o.select = False

    bpy.ops.object.select_all(False)

    radio_top = self.rad_top
    if self.keep_size is True:
        radio_mid = radio_top
        radio_bottom = radio_top
    else:
        radio_mid = self.rad_mid
        radio_bottom = self.rad_bottom

    # Calculate height
    height = self.col_height
    if self.box_base:
        height = height - self.box_base_z
    if self.box_top:
        height = height - self.box_top_z
    # ------------------------
    # Create circular column
    # ------------------------
    if self.model == "1":
        bpy.ops.object.select_all(False)
        mycolumn = create_circular_column(self, "Column", radio_top, radio_mid, radio_bottom, height)
        mycolumn.select = True
        bpy.context.scene.objects.active = mycolumn
        # Subsurf
        set_smooth(mycolumn)
        set_modifier_subsurf(mycolumn)
    # ------------------------
    # Create rectangular column
    # ------------------------
    if self.model == "2":
        mycolumn = create_rectangular_base(self, "Column", self.col_sx, self.col_sy, height)
        bpy.ops.object.select_all(False)
        mycolumn.select = True
        bpy.context.scene.objects.active = mycolumn
        set_normals(mycolumn)
    # ------------------------
    # Circular base
    # ------------------------
    if self.cir_base is True:
        cir_bottom = create_torus("Column_cir_bottom", radio_bottom, self.cir_base_r, self.cir_base_z)
        bpy.ops.object.select_all(False)
        cir_bottom.select = True
        bpy.context.scene.objects.active = cir_bottom
        set_modifier_subsurf(cir_bottom)
        set_smooth(cir_bottom)
        cir_bottom.location.x = 0.0
        cir_bottom.location.y = 0.0
        cir_bottom.location.z = self.cir_base_z / 2
        cir_bottom.parent = mycolumn

    # ------------------------
    # Rectangular base
    # ------------------------
    if self.box_base is True:
        box_bottom = create_rectangular_base(self, "Column_box_bottom", self.box_base_x, self.box_base_y,
                                             self.box_base_z)
        bpy.ops.object.select_all(False)
        box_bottom.select = True
        bpy.context.scene.objects.active = box_bottom
        box_bottom.parent = mycolumn
        set_normals(box_bottom)
        box_bottom.location.x = 0.0
        box_bottom.location.y = 0.0
        box_bottom.location.z = - self.box_base_z
        # move column
        mycolumn.location.z += self.box_base_z

    # ------------------------
    # Circular top
    # ------------------------
    if self.cir_top is True:
        cir_top = create_torus("Column_cir_top", radio_top, self.cir_top_r, self.cir_top_z)
        bpy.ops.object.select_all(False)
        cir_top.select = True
        bpy.context.scene.objects.active = cir_top
        set_modifier_subsurf(cir_top)
        set_smooth(cir_top)
        cir_top.parent = mycolumn
        cir_top.location.x = 0.0
        cir_top.location.y = 0.0
        cir_top.location.z = height - self.cir_top_z / 2

    # ------------------------
    # Rectangular top
    # ------------------------
    if self.box_top is True:
        box_top = create_rectangular_base(self, "Column_box_top", self.box_top_x, self.box_top_y,
                                          self.box_top_z, self.ramp)
        bpy.ops.object.select_all(False)
        box_top.select = True
        bpy.context.scene.objects.active = box_top
        set_normals(box_top)
        box_top.parent = mycolumn
        box_top.location.x = 0.0
        box_top.location.y = 0.0
        box_top.location.z = height

    # ------------------------
    # Create arc
    # ------------------------
    if self.arc_top:
        myarc = create_arc("Column_arch", self.arc_radio, self.arc_gap, self.arc_width,
                           self.array_space_factor)
        myarc.parent = mycolumn
        bpy.ops.object.select_all(False)
        myarc.select = True
        bpy.context.scene.objects.active = myarc
        set_normals(myarc)
        set_modifier_mirror(myarc, "X")
        myarc.location.x = self.arc_radio + self.arc_gap
        myarc.location.y = 0.0
        if self.box_top is True:
            myarc.location.z = height + self.box_top_z
        else:
            myarc.location.z = height
    # ------------------------
    # Create Array X
    # ------------------------
    if self.array_num_x > 0:
        if self.arc_top:
            distance = ((self.arc_radio + self.arc_gap) * 2)
            zmove = 0
        else:
            distance = self.array_space_x
            zmove = self.array_space_z

        if self.crt_array:
            set_modifier_array(mycolumn, "X", 0, self.array_num_x, True, distance, zmove)

            if self.box_base is True:
                set_modifier_array(box_bottom, "X", 0, self.array_num_x, True, distance, zmove)
            if self.box_top is True:
                set_modifier_array(box_top, "X", 0, self.array_num_x, True, distance, zmove)

            if self.cir_base is True:
                set_modifier_array(cir_bottom, "X", 0, self.array_num_x, True, distance, zmove)
            if self.cir_top is True:
                set_modifier_array(cir_top, "X", 0, self.array_num_x, True, distance, zmove)

            if self.arc_top:
                if self.array_num_x > 1:
                    set_modifier_array(myarc, "X", 1, self.array_num_x - 1)  # one arc minus
    # ------------------------
    # Create Array Y
    # ------------------------
    if self.array_num_y > 0:
        if self.arc_top:
            distance = self.arc_width
        else:
            distance = self.array_space_y

        if self.crt_array:
            set_modifier_array(mycolumn, "Y", 0, self.array_num_y, True, distance)

            if self.box_base is True:
                set_modifier_array(box_bottom, "Y", 0, self.array_num_y, True, distance)
            if self.box_top is True:
                set_modifier_array(box_top, "Y", 0, self.array_num_y, True, distance)

            if self.cir_base is True:
                set_modifier_array(cir_bottom, "Y", 0, self.array_num_y, True, distance)
            if self.cir_top is True:
                set_modifier_array(cir_top, "Y", 0, self.array_num_y, True, distance)

            if self.arc_top:
                if self.array_num_y > 1:
                    set_modifier_array(myarc, "Y", 1, self.array_num_y - 1)  # one less

    # ------------------------
    # Create materials
    # ------------------------
    if self.crt_mat and bpy.context.scene.render.engine == 'CYCLES':
        # Column material
        mat = create_diffuse_material("Column_material", False, 0.748, 0.734, 0.392, 0.573, 0.581, 0.318)
        set_material(mycolumn, mat)

        if self.box_base is True or self.box_top is True:
            mat = create_diffuse_material("Column_rect", False, 0.56, 0.56, 0.56, 0.56, 0.56, 0.56)
        if self.box_base is True:
            set_material(box_bottom, mat)
        if self.box_top is True:
            set_material(box_top, mat)

        if self.cir_base is True or self.cir_top is True:
            mat = create_diffuse_material("Column_cir", False, 0.65, 0.65, 0.65, 0.65, 0.65, 0.65)
        if self.cir_base is True:
            set_material(cir_bottom, mat)
        if self.cir_top is True:
            set_material(cir_top, mat)

        if self.arc_top:
            mat = create_diffuse_material("Column_arch", False, 0.8, 0.8, 0.8)
            set_material(myarc, mat)

    bpy.ops.object.select_all(False)
    mycolumn.select = True
    bpy.context.scene.objects.active = mycolumn

    return


# ------------------------------------------------------------------------------
# Create Column
# ------------------------------------------------------------------------------
def create_circular_column(self, objname, radio_top, radio_mid, radio_bottom, height):
    myvertex = []
    myfaces = []
    pies = [0, 30, 60, 90, 120, 150, 180, 210, 240, 270, 300, 330]  # circle

    # Add bottom circle
    for pie in pies:
        x = cos(radians(pie)) * radio_bottom
        y = sin(radians(pie)) * radio_bottom
        mypoint = [(x, y, 0.0)]
        myvertex.extend(mypoint)
    # Add middle circle
    for pie in pies:
        x = cos(radians(pie)) * radio_mid
        y = sin(radians(pie)) * radio_mid
        mypoint = [(x, y, (height / 2) + ((height / 2) * self.shift))]
        myvertex.extend(mypoint)
    # Add top circle
    for pie in pies:
        x = cos(radians(pie)) * radio_top
        y = sin(radians(pie)) * radio_top
        mypoint = [(x, y, height)]
        myvertex.extend(mypoint)
    # -------------------------------------
    # Faces
    # -------------------------------------
    t = 1
    for n in range(0, len(pies) * 2):
        t += 1
        if t > len(pies):
            t = 1
            myface = [(n, n - len(pies) + 1, n + 1, n + len(pies))]
            myfaces.extend(myface)
        else:
            myface = [(n, n + 1, n + len(pies) + 1, n + len(pies))]
            myfaces.extend(myface)

    mesh = bpy.data.meshes.new(objname)
    myobject = bpy.data.objects.new(objname, mesh)

    myobject.location = bpy.context.scene.cursor_location
    bpy.context.scene.objects.link(myobject)

    mesh.from_pydata(myvertex, [], myfaces)
    mesh.update(calc_edges=True)

    return myobject


# ------------------------------------------------------------------------------
# Create Torus
# ------------------------------------------------------------------------------
def create_torus(objname, radio_inside, radio_outside, height):
    myvertex = []
    myfaces = []
    pies = [0, 30, 60, 90, 120, 150, 180, 210, 240, 270, 300, 330]  # circle
    segments = [80, 60, 30, 0, 330, 300, 280]  # section

    radio_mid = radio_outside + radio_inside - (height / 2)
    # Add internal circles Top
    for pie in pies:
        x = cos(radians(pie)) * radio_inside
        y = sin(radians(pie)) * radio_inside
        mypoint = [(x, y, height / 2)]
        myvertex.extend(mypoint)
    # Add external circles Top
    for pie in pies:
        x = cos(radians(pie)) * radio_mid
        y = sin(radians(pie)) * radio_mid
        mypoint = [(x, y, height / 2)]
        myvertex.extend(mypoint)
    # Add Intermediate lines
    for segment in segments:
        for pie in pies:
            radio_externo = radio_mid + (height * cos(radians(segment)))
            x = cos(radians(pie)) * radio_externo
            y = sin(radians(pie)) * radio_externo
            z = sin(radians(segment)) * (height / 2)

            mypoint = [(x, y, z)]
            myvertex.extend(mypoint)

    # Add internal circles Bottom
    for pie in pies:
        x = cos(radians(pie)) * radio_inside
        y = sin(radians(pie)) * radio_inside
        mypoint = [(x, y, height / 2 * -1)]
        myvertex.extend(mypoint)
    # Add external circles bottom
    for pie in pies:
        x = cos(radians(pie)) * radio_mid
        y = sin(radians(pie)) * radio_mid
        mypoint = [(x, y, height / 2 * -1)]
        myvertex.extend(mypoint)

    # -------------------------------------
    # Faces
    # -------------------------------------
    t = 1
    for n in range(0, len(pies) * len(segments) + (len(pies) * 2)):
        t += 1
        if t > len(pies):
            t = 1
            myface = [(n, n - len(pies) + 1, n + 1, n + len(pies))]
            myfaces.extend(myface)
        else:
            myface = [(n, n + 1, n + len(pies) + 1, n + len(pies))]
            myfaces.extend(myface)

    mesh = bpy.data.meshes.new(objname)
    myobject = bpy.data.objects.new(objname, mesh)

    myobject.location = bpy.context.scene.cursor_location
    bpy.context.scene.objects.link(myobject)

    mesh.from_pydata(myvertex, [], myfaces)
    mesh.update(calc_edges=True)

    return myobject


# ------------------------------------------------------------------------------
# Create rectangular base
# ------------------------------------------------------------------------------
def create_rectangular_base(self, objname, x, y, z, ramp=False):
    elements = self.array_num_x - 1
    height = self.array_space_z * elements
    width = self.array_space_x * elements
    if width > 0:
        angle = atan(height / width)
    else:
        angle = 0

    radio = sqrt((x * x) + (self.array_space_z * self.array_space_z))
    disp = radio * sin(angle)

    if ramp is False or self.arc_top:
        addz1 = 0
        addz2 = 0
    else:
        if self.array_space_z >= 0:
            addz1 = 0
            addz2 = disp
        else:
            addz1 = disp * -1
            addz2 = 0

    myvertex = [(-x / 2, -y / 2, 0.0),
                (-x / 2, y / 2, 0.0),
                (x / 2, y / 2, 0.0),
                (x / 2, -y / 2, 0.0),
                (-x / 2, -y / 2, z + addz1),
                (-x / 2, y / 2, z + addz1),
                (x / 2, y / 2, z + addz2),
                (x / 2, -y / 2, z + addz2)]

    myfaces = [(0, 1, 2, 3), (0, 1, 5, 4), (1, 2, 6, 5), (2, 6, 7, 3), (5, 6, 7, 4), (0, 4, 7, 3)]

    mesh = bpy.data.meshes.new(objname)
    myobject = bpy.data.objects.new(objname, mesh)

    myobject.location = bpy.context.scene.cursor_location
    bpy.context.scene.objects.link(myobject)

    mesh.from_pydata(myvertex, [], myfaces)
    mesh.update(calc_edges=True)

    return myobject


# ------------------------------------------------------------------------------
# Create arc
# ------------------------------------------------------------------------------
def create_arc(objname, radio, gap, thickness, center):
    myvertex = []

    half = (thickness / 2)
    move = half * center

    listdata = [half + move, -half + move]
    for pos_y in listdata:
        # --------------------------------
        # First vertices
        # --------------------------------
        myvertex.extend([(-radio - gap, pos_y, radio + radio / 10)])
        # Flat cuts
        angle = 13 * (180 / 16)
        for i in range(1, 4):
            z = sin(radians(angle)) * radio
            mypoint = [(-radio - gap, pos_y, z)]
            myvertex.extend(mypoint)
            angle += 180 / 16

        myvertex.extend([(-radio - gap, pos_y, 0.0)])
        # --------------------------------
        # Arc points
        # --------------------------------
        angle = 180
        for i in range(0, 9):
            x = cos(radians(angle)) * radio
            z = sin(radians(angle)) * radio
            mypoint = [(x, pos_y, z)]
            myvertex.extend(mypoint)

            angle -= 180 / 16
        # --------------------------------
        # vertical cut points
        # --------------------------------
        angle = 8 * (180 / 16)
        for i in range(1, 5):
            x = cos(radians(angle)) * radio
            mypoint = [(x, pos_y, radio + radio / 10)]
            myvertex.extend(mypoint)

            angle += 180 / 16

    myfaces = [(23, 24, 21, 22), (24, 25, 20, 21), (25, 26, 19, 20), (27, 18, 19, 26), (18, 27, 28, 35),
               (28, 29, 34, 35), (29, 30, 33, 34), (30, 31, 32, 33), (12, 13, 31, 30), (29, 11, 12, 30),
               (11, 29, 28, 10), (10, 28, 27, 9), (9, 27, 26, 8), (25, 7, 8, 26), (24, 6, 7, 25),
               (23, 5, 6, 24), (22, 4, 5, 23), (5, 4, 3, 6), (6, 3, 2, 7), (7, 2, 1, 8),
               (8, 1, 0, 9), (9, 0, 17, 10), (10, 17, 16, 11), (11, 16, 15, 12), (14, 13, 12, 15),
               (21, 3, 4, 22), (20, 2, 3, 21), (19, 1, 2, 20), (1, 19, 18, 0), (0, 18, 35, 17),
               (17, 35, 34, 16), (33, 15, 16, 34), (32, 14, 15, 33)]

    mesh = bpy.data.meshes.new(objname)
    myobject = bpy.data.objects.new(objname, mesh)

    myobject.location = bpy.context.scene.cursor_location
    bpy.context.scene.objects.link(myobject)

    mesh.from_pydata(myvertex, [], myfaces)
    mesh.update(calc_edges=True)

    return myobject
