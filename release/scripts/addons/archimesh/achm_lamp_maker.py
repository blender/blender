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
# Automatic generation of lamps
# Author: Antonio Vazquez (antonioya)
#
# ----------------------------------------------------------
import bpy
from math import cos, sin, radians
from copy import copy
from bpy.types import Operator
from bpy.props import EnumProperty, FloatProperty, IntProperty, BoolProperty, FloatVectorProperty
from .achm_tools import *


# ------------------------------------------------------
# set predefined designs
#
# self: self container
# ------------------------------------------------------
def set_preset(self):
    # -----------------------
    # Sphere
    # -----------------------
    if self.preset == "1":
        self.base_height = 0.22
        self.base_segments = 16
        self.base_rings = 6
        self.smooth = True
        self.subdivide = True
        self.br01 = 0.05
        self.br02 = 0.07
        self.br03 = 0.11
        self.br04 = 0.11
        self.br05 = 0.07
        self.br06 = 0.03

        self.bz01 = 0
        self.bz02 = -1
        self.bz03 = -0.456
        self.bz04 = 0.089
        self.bz05 = -0.038
        self.bz06 = -0.165
    # -----------------------
    # Pear
    # -----------------------
    if self.preset == "2":
        self.base_height = 0.20
        self.base_segments = 16
        self.base_rings = 6
        self.smooth = True
        self.subdivide = True
        self.br01 = 0.056
        self.br02 = 0.062
        self.br03 = 0.072
        self.br04 = 0.090
        self.br05 = 0.074
        self.br06 = 0.03

        self.bz01 = 0
        self.bz02 = 0
        self.bz03 = 0
        self.bz04 = 0
        self.bz05 = 0
        self.bz06 = 0
    # -----------------------
    # Vase
    # -----------------------
    if self.preset == "3":
        self.base_height = 0.20
        self.base_segments = 8
        self.base_rings = 6
        self.smooth = True
        self.subdivide = True
        self.br01 = 0.05
        self.br02 = 0.11
        self.br03 = 0.15
        self.br04 = 0.07
        self.br05 = 0.05
        self.br06 = 0.03

        self.bz01 = 0
        self.bz02 = 0
        self.bz03 = 0
        self.bz04 = 0
        self.bz05 = 0
        self.bz06 = 0
    # -----------------------
    # Rectangular
    # -----------------------
    if self.preset == "4":
        self.base_height = 0.15
        self.base_segments = 4
        self.base_rings = 5
        self.smooth = False
        self.subdivide = False
        self.br01 = 0.08
        self.br02 = 0.08
        self.br03 = 0.08
        self.br04 = 0.08
        self.br05 = 0.03

        self.bz01 = 0
        self.bz02 = 0
        self.bz03 = 0
        self.bz04 = 0.25
        self.bz05 = 0


# ------------------------------------------------------------------
# Define UI class
# Lamps
# ------------------------------------------------------------------
class AchmLamp(Operator):
    bl_idname = "mesh.archimesh_lamp"
    bl_label = "Lamp"
    bl_description = "Lamp Generator"
    bl_category = 'Archimesh'
    bl_options = {'REGISTER', 'UNDO'}
    # preset
    preset = EnumProperty(
            items=(
                ('0', "None", ""),
                ('1', "Sphere", ""),
                ('2', "Pear", ""),
                ('3', "Vase", ""),
                ('4', "Rectangular", ""),
                ),
            name="Predefined",
            description="Apply predefined design",
            )
    oldpreset = preset

    base_height = FloatProperty(
            name='Height',
            min=0.01, max=10, default=0.20, precision=3,
            description='lamp base height',
            )
    base_segments = IntProperty(
            name='Segments',
            min=3, max=128, default=16,
            description='Number of segments (vertical)',
            )
    base_rings = IntProperty(
            name='Rings',
            min=2, max=12, default=6,
            description='Number of rings (horizontal)',
            )
    holder = FloatProperty(
            name='Lampholder',
            min=0.001, max=10, default=0.02, precision=3,
            description='Lampholder height',
            )
    smooth = BoolProperty(
            name="Smooth",
            description="Use smooth shader",
            default=True,
            )
    subdivide = BoolProperty(
            name="Subdivide",
            description="Add subdivision modifier",
            default=True,
            )

    bz01 = FloatProperty(name='S1', min=-1, max=1, default=0, precision=3, description='Z shift factor')
    bz02 = FloatProperty(name='S2', min=-1, max=1, default=0, precision=3, description='Z shift factor')
    bz03 = FloatProperty(name='S3', min=-1, max=1, default=0, precision=3, description='Z shift factor')
    bz04 = FloatProperty(name='S4', min=-1, max=1, default=0, precision=3, description='Z shift factor')
    bz05 = FloatProperty(name='S5', min=-1, max=1, default=0, precision=3, description='Z shift factor')
    bz06 = FloatProperty(name='S6', min=-1, max=1, default=0, precision=3, description='Z shift factor')
    bz07 = FloatProperty(name='S7', min=-1, max=1, default=0, precision=3, description='Z shift factor')
    bz08 = FloatProperty(name='S8', min=-1, max=1, default=0, precision=3, description='Z shift factor')
    bz09 = FloatProperty(name='S9', min=-1, max=1, default=0, precision=3, description='Z shift factor')
    bz10 = FloatProperty(name='S10', min=-1, max=1, default=0, precision=3, description='Z shift factor')
    bz11 = FloatProperty(name='S11', min=-1, max=1, default=0, precision=3, description='Z shift factor')
    bz12 = FloatProperty(name='S12', min=-1, max=1, default=0, precision=3, description='Z shift factor')

    br01 = FloatProperty(name='R1', min=0.001, max=10, default=0.06, precision=3, description='Ring radio')
    br02 = FloatProperty(name='R2', min=0.001, max=10, default=0.08, precision=3, description='Ring radio')
    br03 = FloatProperty(name='R3', min=0.001, max=10, default=0.09, precision=3, description='Ring radio')
    br04 = FloatProperty(name='R4', min=0.001, max=10, default=0.08, precision=3, description='Ring radio')
    br05 = FloatProperty(name='R5', min=0.001, max=10, default=0.06, precision=3, description='Ring radio')
    br06 = FloatProperty(name='R6', min=0.001, max=10, default=0.03, precision=3, description='Ring radio')
    br07 = FloatProperty(name='R7', min=0.001, max=10, default=0.10, precision=3, description='Ring radio')
    br08 = FloatProperty(name='R8', min=0.001, max=10, default=0.10, precision=3, description='Ring radio')
    br09 = FloatProperty(name='R9', min=0.001, max=10, default=0.10, precision=3, description='Ring radio')
    br10 = FloatProperty(name='R10', min=0.001, max=10, default=0.10, precision=3, description='Ring radio')
    br11 = FloatProperty(name='R11', min=0.001, max=10, default=0.10, precision=3, description='Ring radio')
    br12 = FloatProperty(name='R12', min=0.001, max=10, default=0.10, precision=3, description='Ring radio')

    top_height = FloatProperty(
            name='Height', min=0.01, max=10,
            default=0.20, precision=3,
            description='lampshade height',
            )
    top_segments = IntProperty(
            name='Segments', min=3, max=128,
            default=32,
            description='Number of segments (vertical)',
            )
    tr01 = FloatProperty(
            name='R1', min=0.001, max=10,
            default=0.16, precision=3,
            description='lampshade bottom radio',
            )
    tr02 = FloatProperty(name='R2', min=0.001, max=10,
                         default=0.08, precision=3,
                         description='lampshade top radio')
    pleats = BoolProperty(
            name="Pleats", description="Create pleats in the lampshade",
            default=False,
            )
    tr03 = FloatProperty(
            name='R3', min=0.001, max=1,
            default=0.01, precision=3, description='Pleats size',
            )
    energy = FloatProperty(
            name='Light', min=0.00, max=1000,
            default=15, precision=3,
            description='Light intensity',
            )
    opacity = FloatProperty(
            name='Translucency', min=0.00, max=1,
            default=0.3, precision=3,
            description='Lampshade translucency factor (1 completely translucent)',
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
            box.label("Lamp base")
            row = box.row()
            row.prop(self, 'preset')
            row = box.row()
            row.prop(self, 'base_height')
            row.prop(self, 'base_segments')
            row.prop(self, 'base_rings')
            row = box.row()
            row.prop(self, 'smooth')
            row.prop(self, 'subdivide')
            row = box.row()
            row.prop(self, 'holder')

            if self.base_rings >= 1:
                row = box.row()
                row.prop(self, 'br01')
                row.prop(self, 'bz01', slider=True)
            if self.base_rings >= 2:
                row = box.row()
                row.prop(self, 'br02')
                row.prop(self, 'bz02', slider=True)
            if self.base_rings >= 3:
                row = box.row()
                row.prop(self, 'br03')
                row.prop(self, 'bz03', slider=True)

            if self.base_rings >= 4:
                row = box.row()
                row.prop(self, 'br04')
                row.prop(self, 'bz04', slider=True)
            if self.base_rings >= 5:
                row = box.row()
                row.prop(self, 'br05')
                row.prop(self, 'bz05', slider=True)
            if self.base_rings >= 6:
                row = box.row()
                row.prop(self, 'br06')
                row.prop(self, 'bz06', slider=True)

            if self.base_rings >= 7:
                row = box.row()
                row.prop(self, 'br07')
                row.prop(self, 'bz07', slider=True)
            if self.base_rings >= 8:
                row = box.row()
                row.prop(self, 'br08')
                row.prop(self, 'bz08', slider=True)
            if self.base_rings >= 9:
                row = box.row()
                row.prop(self, 'br09')
                row.prop(self, 'bz09', slider=True)

            if self.base_rings >= 10:
                row = box.row()
                row.prop(self, 'br10')
                row.prop(self, 'bz10', slider=True)
            if self.base_rings >= 11:
                row = box.row()
                row.prop(self, 'br11')
                row.prop(self, 'bz11', slider=True)
            if self.base_rings >= 12:
                row = box.row()
                row.prop(self, 'br12')
                row.prop(self, 'bz12', slider=True)

            box = layout.box()
            box.label("Lampshade")
            row = box.row()
            row.prop(self, 'top_height')
            row.prop(self, 'top_segments')
            row = box.row()
            row.prop(self, 'tr01')
            row.prop(self, 'tr02')
            row = box.row()
            row.prop(self, 'energy')
            row.prop(self, 'opacity', slider=True)
            row = box.row()
            row.prop(self, 'pleats')
            if self.pleats:
                row.prop(self, 'tr03')

            box = layout.box()
            if not context.scene.render.engine == 'CYCLES':
                box.enabled = False
            box.prop(self, 'crt_mat')
            if self.crt_mat:
                row = box.row()
                row.prop(self, 'objcol')
        else:
            row = layout.row()
            row.label("Warning: Operator does not work in local view mode", icon='ERROR')

    # -----------------------------------------------------
    # Execute
    # -----------------------------------------------------
    # noinspection PyUnusedLocal
    def execute(self, context):
        if bpy.context.mode == "OBJECT":
            if self.oldpreset != self.preset:
                set_preset(self)
                self.oldpreset = self.preset

            # Create lamp
            create_lamp_mesh(self)
            return {'FINISHED'}
        else:
            self.report({'WARNING'}, "Archimesh: Option only valid in Object mode")
            return {'CANCELLED'}


# ------------------------------------------------------------------------------
# Generate mesh data
# All custom values are passed using self container (self.myvariable)
# ------------------------------------------------------------------------------
def create_lamp_mesh(self):
    # deactivate others
    for o in bpy.data.objects:
        if o.select is True:
            o.select = False
    bpy.ops.object.select_all(False)
    generate_lamp(self)

    return


# ------------------------------------------------------------------------------
# Generate lamps
# All custom values are passed using self container (self.myvariable)
# ------------------------------------------------------------------------------
def generate_lamp(self):
    location = bpy.context.scene.cursor_location
    myloc = copy(location)  # copy location to keep 3D cursor position
    # ---------------------
    # Lamp base
    # ---------------------
    mydata = create_lamp_base("Lamp_base", self.base_height,
                              myloc.x, myloc.y, myloc.z,
                              self.base_segments, self.base_rings,
                              [self.br01, self.br02, self.br03, self.br04, self.br05, self.br06,
                               self.br07, self.br08, self.br09, self.br10, self.br11, self.br12],
                              (self.bz01, self.bz02, self.bz03, self.bz04, self.bz05, self.bz06,
                               self.bz07, self.bz08, self.bz09, self.bz10, self.bz11, self.bz12),
                              self.subdivide,
                              self.crt_mat, self.objcol)
    mybase = mydata[0]
    posz = mydata[1]
    # refine
    remove_doubles(mybase)
    set_normals(mybase)
    # Smooth
    if self.smooth:
        set_smooth(mybase)
    if self.subdivide:
        set_modifier_subsurf(mybase)
    # ---------------------
    # Lampholder
    # ---------------------
    myholder = create_lampholder("Lampholder", self.holder,
                                 myloc.x, myloc.y, myloc.z,
                                 self.crt_mat)
    # refine
    remove_doubles(myholder)
    set_normals(myholder)
    set_smooth(myholder)

    myholder.parent = mybase
    myholder.location.x = 0
    myholder.location.y = 0
    myholder.location.z = posz
    # ---------------------
    # Lamp strings
    # ---------------------
    mystrings = create_lampholder_strings("Lampstrings", self.holder,
                                          myloc.x, myloc.y, myloc.z,
                                          self.tr02,
                                          self.top_height,
                                          self.crt_mat)
    # refine
    remove_doubles(mystrings)
    set_normals(mystrings)

    mystrings.parent = myholder
    mystrings.location.x = 0
    mystrings.location.y = 0
    mystrings.location.z = 0.03
    # ---------------------
    # Lampshade
    # ---------------------
    mytop = create_lampshade("Lampshade", self.top_height,
                             myloc.x, myloc.y, myloc.z,
                             self.top_segments,
                             self.tr01, self.tr02,
                             self.pleats, self.tr03,
                             self.opacity,
                             self.crt_mat)
    # refine
    remove_doubles(mytop)
    set_normals(mytop)
    if self.pleats is False:
        set_smooth(mytop)

    mytop.parent = mybase
    mytop.location.x = 0
    mytop.location.y = 0
    mytop.location.z = posz + self.holder
    # ---------------------
    # Light bulb
    # ---------------------
    radbulb = 0.02
    bpy.ops.mesh.primitive_uv_sphere_add(segments=16, size=radbulb)
    mybulb = bpy.data.objects[bpy.context.active_object.name]
    mybulb.name = "Lamp_Bulb"
    mybulb.parent = myholder
    mybulb.location = (0, 0, radbulb + self.holder + 0.04)
    if self.crt_mat and bpy.context.scene.render.engine == 'CYCLES':
        mat = create_emission_material(mybulb.name, True, 0.8, 0.8, 0.8, self.energy)
        set_material(mybulb, mat)

    # deactivate others
    for o in bpy.data.objects:
        if o.select is True:
            o.select = False

    mybase.select = True
    bpy.context.scene.objects.active = mybase

    return


# ------------------------------------------------------------------------------
# Create lamp base
#
# objName: Name for the new object
# height: Size in Z axis
# pX: position X axis
# pY: position Y axis
# pZ: position Z axis
# segments: number of segments
# rings: number of rings
# radios: ring radios
# ratios: Z shift ratios
# subdivide: Subdivision flag
# mat: Flag for creating materials
# objcol: Color
# ------------------------------------------------------------------------------
def create_lamp_base(objname, height, px, py, pz, segments, rings, radios, ratios, subdivide, mat, objcol):
    # Calculate heights
    h = height / (rings - 1)
    listheight = []
    z = 0
    for f in range(0, rings):
        listheight.extend([z + (z * ratios[f])])
        z += h

    mydata = create_cylinder_data(segments, listheight,
                                  radios,
                                  True, True, False, 0, subdivide)
    myvertex = mydata[0]
    myfaces = mydata[1]

    mymesh = bpy.data.meshes.new(objname)
    mycylinder = bpy.data.objects.new(objname, mymesh)
    bpy.context.scene.objects.link(mycylinder)

    mymesh.from_pydata(myvertex, [], myfaces)
    mymesh.update(calc_edges=True)
    # Position
    mycylinder.location.x = px
    mycylinder.location.y = py
    mycylinder.location.z = pz
    # Materials
    if mat and bpy.context.scene.render.engine == 'CYCLES':
        rgb = objcol
        mymat = create_diffuse_material(mycylinder.name + "_material", True, rgb[0], rgb[1], rgb[2], rgb[0], rgb[1],
                                        rgb[2], 0.1)
        set_material(mycylinder, mymat)

    return mycylinder, listheight[len(listheight) - 1]


# ------------------------------------------------------------------------------
# Create lampholder
#
# objName: Name for the new object
# height: Size in Z axis
# pX: position X axis
# pY: position Y axis
# pZ: position Z axis
# mat: Flag for creating materials
# ------------------------------------------------------------------------------
def create_lampholder(objname, height, px, py, pz, mat):
    mydata = create_cylinder_data(16, [0, height, height + 0.005, height + 0.008, height + 0.05],
                                  [0.005, 0.005, 0.010, 0.018, 0.018],
                                  False, False, False, 0, False)
    myvertex = mydata[0]
    myfaces = mydata[1]

    mymesh = bpy.data.meshes.new(objname)
    mycylinder = bpy.data.objects.new(objname, mymesh)
    bpy.context.scene.objects.link(mycylinder)

    mymesh.from_pydata(myvertex, [], myfaces)
    mymesh.update(calc_edges=True)
    # Position
    mycylinder.location.x = px
    mycylinder.location.y = py
    mycylinder.location.z = pz

    # Materials
    if mat and bpy.context.scene.render.engine == 'CYCLES':
        mat = create_diffuse_material(mycylinder.name + "_material", True, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.1)
        set_material(mycylinder, mat)

    return mycylinder


# ------------------------------------------------------------------------------
# Create lampholder strings
#
# objName: Name for the new object
# height: Size in Z axis
# pX: position X axis
# pY: position Y axis
# pZ: position Z axis
# radio: radio of lampshade
# shadeh: height of lampshader
# mat: Flag for creating materials
# ------------------------------------------------------------------------------
def create_lampholder_strings(objname, height, px, py, pz, radio, shadeh, mat):
    mydata = create_cylinder_data(32, [height + 0.005, height + 0.005, height + 0.006, height + 0.006],
                                  [0.018, 0.025, 0.025, 0.018],
                                  False, False, False, 0, False)
    myvertex = mydata[0]
    myfaces = mydata[1]

    mymesh = bpy.data.meshes.new(objname)
    mycylinder = bpy.data.objects.new(objname, mymesh)
    bpy.context.scene.objects.link(mycylinder)

    mymesh.from_pydata(myvertex, [], myfaces)
    mymesh.update(calc_edges=True)
    # Position
    mycylinder.location.x = px
    mycylinder.location.y = py
    mycylinder.location.z = pz
    # Box1
    box1 = create_box_segments("Lamp_B1", shadeh - 0.036, radio - 0.023)
    box1.parent = mycylinder
    box1.location = (0.021, 0, height + 0.004)
    # Box2
    box2 = create_box_segments("Lamp_B2", shadeh - 0.036, -radio + 0.023)
    box2.parent = mycylinder
    box2.location = (-0.021, 0, height + 0.004)

    # Materials
    if mat and bpy.context.scene.render.engine == 'CYCLES':
        mat = create_diffuse_material(mycylinder.name + "_material", True, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.1)
        set_material(mycylinder, mat)
        set_material(box1, mat)
        set_material(box2, mat)

    return mycylinder


# ------------------------------------------------------------------------------
# Create lampshade
#
# objName: Name for the new object
# height: Size in Z axis
# pX: position X axis
# pY: position Y axis
# pZ: position Z axis
# segments: number of segments
# radio1: ring radio 1
# radio2: ring radio 2
# pleats: flag for pleats
# pleatsize: difference in radios (less)
# opacity: opacity factor
# mat: Flag for creating materials
# ------------------------------------------------------------------------------
def create_lampshade(objname, height, px, py, pz, segments, radio1, radio2, pleats, pleatsize, opacity, mat):
    gap = 0.002
    radios = [radio1 - gap, radio1 - gap, radio1, radio2, radio2 - gap, radio2 - gap]
    heights = [gap * 2, 0, 0, height, height, height - (gap * 2)]
    mydata = create_cylinder_data(segments, heights,
                                  radios,
                                  False, False, pleats, pleatsize, False)
    myvertex = mydata[0]
    myfaces = mydata[1]

    mymesh = bpy.data.meshes.new(objname)
    mycylinder = bpy.data.objects.new(objname, mymesh)
    bpy.context.scene.objects.link(mycylinder)

    mymesh.from_pydata(myvertex, [], myfaces)
    mymesh.update(calc_edges=True)
    # Position
    mycylinder.location.x = px
    mycylinder.location.y = py
    mycylinder.location.z = pz
    # materials
    if mat and bpy.context.scene.render.engine == 'CYCLES':
        mymat = create_translucent_material(mycylinder.name + "_material", True, 0.8, 0.65, 0.45, 0.8, 0.65, 0.45,
                                            opacity)
        set_material(mycylinder, mymat)

    return mycylinder


# ------------------------------------------------------------------------------
# Create box segments
#
# objName: Name for the new object
# height: Size in Z axis
# shift: Shift movement
# ------------------------------------------------------------------------------
def create_box_segments(objname, height, shift):
    gap = 0.001
    myvertex = [(0, 0, 0), (0, gap, 0), (gap, gap, 0), (gap, 0, 0),
                (shift, 0, height),
                (shift, gap, height),
                (shift + gap, gap, height),
                (shift + gap, 0, height)]
    myfaces = [(6, 5, 1, 2), (7, 6, 2, 3), (4, 7, 3, 0), (1, 5, 4, 0)]

    mymesh = bpy.data.meshes.new(objname)
    mysegment = bpy.data.objects.new(objname, mymesh)
    bpy.context.scene.objects.link(mysegment)

    mymesh.from_pydata(myvertex, [], myfaces)
    mymesh.update(calc_edges=True)
    # Position
    mysegment.location.x = 0
    mysegment.location.y = 0
    mysegment.location.z = 0

    return mysegment


# ------------------------------------------------------------------------------
# Create cylinders data
#
# segments: Number of pies
# listHeight: list of heights
# listRadio: list of radios
# top: top face flag
# bottom: bottom face flag
# pleats: flag for pleats
# pleatsize: difference in radios (less)
# subdiv: fix subdivision problem
# ------------------------------------------------------------------------------
def create_cylinder_data(segments, listheight, listradio, bottom, top, pleats, pleatsize, subdiv):
    myvertex = []
    myfaces = []
    if subdiv:
        # Add at element 0 to fix subdivision problems
        listheight.insert(0, listheight[0] + 0.001)
        listradio.insert(0, listradio[0])
        # Add at last element to fix subdivision problems
        e = len(listheight) - 1
        listheight.insert(e, listheight[e] + 0.001)
        listradio.insert(e, listradio[e])
    # -------------------------------------
    # Vertices
    # -------------------------------------
    idx = 0
    rp = 0
    for z in listheight:
        seg = 0
        for i in range(segments):
            x = cos(radians(seg)) * (listradio[idx] + rp)
            y = sin(radians(seg)) * (listradio[idx] + rp)
            mypoint = [(x, y, z)]
            myvertex.extend(mypoint)
            seg += 360 / segments
            # pleats
            if pleats is True and rp == 0:
                rp = -pleatsize
            else:
                rp = 0

        idx += 1
    # -------------------------------------
    # Faces
    # -------------------------------------
    for r in range(0, len(listheight) - 1):
        s = r * segments
        t = 1
        for n in range(0, segments):
            t += 1
            if t > segments:
                t = 1
                myface = [(n + s, n + s - segments + 1, n + s + 1, n + s + segments)]
                myfaces.extend(myface)
            else:
                myface = [(n + s, n + s + 1, n + s + segments + 1, n + s + segments)]
                myfaces.extend(myface)

    # -----------------
    # bottom face
    # -----------------
    if bottom:
        fa = []
        for f in range(0, segments):
            fa.extend([f])
        myfaces.extend([fa])
    # -----------------
    # top face
    # -----------------
    if top:
        fa = []
        for f in range(len(myvertex) - segments, len(myvertex)):
            fa.extend([f])
        myfaces.extend([fa])

    return myvertex, myfaces
