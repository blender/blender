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
# Automatic generation of curtains
# Author: Antonio Vazquez (antonioya)
#
# ----------------------------------------------------------
# noinspection PyUnresolvedReferences
import bpy
from copy import copy
from math import cos, sin, radians
from bpy.types import Operator
from .achm_tools import *


# ------------------------------------------------------------------
# Define UI class
# Japanese curtains
# ------------------------------------------------------------------
class AchmJapan(Operator):
    bl_idname = "mesh.archimesh_japan"
    bl_label = "Japanese curtains"
    bl_description = "Japanese curtains Generator"
    bl_category = 'Archimesh'
    bl_options = {'REGISTER', 'UNDO'}

    width = bpy.props.FloatProperty(
            name='Width',
            min=0.30, max=4, default=1, precision=3,
            description='Total width',
            )
    height = bpy.props.FloatProperty(
            name='Height',
            min=0.20, max=50, default=1.8, precision=3,
            description='Total height',
            )
    num = bpy.props.IntProperty(
            name='Rails',
            min=2, max=5, default=2,
            description='Number total of rails',
            )
    palnum = bpy.props.IntProperty(
            name='Panels',
            min=1, max=2, default=1,
            description='Panels by rail',
            )

    open01 = bpy.props.FloatProperty(
            name='Position 01',
            min=0, max=1, default=0, precision=3,
            description='Position of the panel',
            )
    open02 = bpy.props.FloatProperty(
            name='Position 02',
            min=0, max=1, default=0, precision=3,
            description='Position of the panel',
            )
    open03 = bpy.props.FloatProperty(
            name='Position 03',
            min=0, max=1, default=0, precision=3,
            description='Position of the panel',
            )
    open04 = bpy.props.FloatProperty(
            name='Position 04',
            min=0, max=1, default=0, precision=3,
            description='Position of the panel',
            )
    open05 = bpy.props.FloatProperty(
            name='Position 05',
            min=0, max=1, default=0, precision=3,
            description='Position of the panel',
            )

    # Materials
    crt_mat = bpy.props.BoolProperty(
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
            row.prop(self, 'width')
            row.prop(self, 'height')
            row = box.row()
            row.prop(self, 'num')
            row.prop(self, 'palnum')

            if self.num >= 1:
                row = box.row()
                row.prop(self, 'open01', slider=True)
            if self.num >= 2:
                row = box.row()
                row.prop(self, 'open02', slider=True)
            if self.num >= 3:
                row = box.row()
                row.prop(self, 'open03', slider=True)
            if self.num >= 4:
                row = box.row()
                row.prop(self, 'open04', slider=True)
            if self.num >= 5:
                row = box.row()
                row.prop(self, 'open05', slider=True)

            box = layout.box()
            if not context.scene.render.engine == 'CYCLES':
                box.enabled = False
            box.prop(self, 'crt_mat')
            if self.crt_mat:
                box.label("* Remember to verify fabric texture folder")
        else:
            row = layout.row()
            row.label("Warning: Operator does not work in local view mode", icon='ERROR')

    # -----------------------------------------------------
    # Execute
    # -----------------------------------------------------
    # noinspection PyUnusedLocal
    def execute(self, context):
        if bpy.context.mode == "OBJECT":
            create_japan_mesh(self)
            return {'FINISHED'}
        else:
            self.report({'WARNING'}, "Archimesh: Option only valid in Object mode")
            return {'CANCELLED'}


# ------------------------------------------------------------------------------
# Generate mesh data
# All custom values are passed using self container (self.myvariable)
# ------------------------------------------------------------------------------
def create_japan_mesh(self):
    # deactivate others
    for o in bpy.data.objects:
        if o.select is True:
            o.select = False
    bpy.ops.object.select_all(False)
    # Create units
    generate_japan(self)

    return


# ------------------------------------------------------------------
# Define UI class
# Roller curtains
# ------------------------------------------------------------------
class AchmRoller(Operator):
    bl_idname = "mesh.archimesh_roller"
    bl_label = "Roller curtains"
    bl_description = "Roller_curtains Generator"
    bl_category = 'Archimesh'
    bl_options = {'REGISTER', 'UNDO'}

    width = bpy.props.FloatProperty(
            name='Width',
            min=0.30, max=4, default=1, precision=3,
            description='Total width',
            )
    height = bpy.props.FloatProperty(
            name='Height',
            min=0.01, max=50, default=1.7, precision=3,
            description='Total height',
            )

    # Materials
    crt_mat = bpy.props.BoolProperty(
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
            row.prop(self, 'width')
            row.prop(self, 'height')

            box = layout.box()
            if not context.scene.render.engine == 'CYCLES':
                box.enabled = False
            box.prop(self, 'crt_mat')
            if self.crt_mat:
                box.label("* Remember to verify fabric texture folder")
        else:
            row = layout.row()
            row.label("Warning: Operator does not work in local view mode", icon='ERROR')

    # -----------------------------------------------------
    # Execute
    # -----------------------------------------------------
    # noinspection PyUnusedLocal
    def execute(self, context):
        if bpy.context.mode == "OBJECT":
            create_roller_mesh(self)
            return {'FINISHED'}
        else:
            self.report({'WARNING'}, "Archimesh: Option only valid in Object mode")
            return {'CANCELLED'}


# ------------------------------------------------------------------------------
# Generate mesh data
# All custom values are passed using self container (self.myvariable)
# ------------------------------------------------------------------------------
def create_roller_mesh(self):
    # deactivate others
    for o in bpy.data.objects:
        if o.select is True:
            o.select = False
    bpy.ops.object.select_all(False)
    generate_roller(self)

    return


# ------------------------------------------------------------------------------
# Generate japanese curtains
# All custom values are passed using self container (self.myvariable)
# ------------------------------------------------------------------------------
def generate_japan(self):
    support = []
    panel = []

    location = bpy.context.scene.cursor_location
    myloc = copy(location)  # copy location to keep 3D cursor position

    # ------------------
    # Rail
    # ------------------
    myrail = create_japan_rail("Rail",
                               self.width - 0.02, self.num,
                               myloc.x, myloc.y, myloc.z,
                               self.crt_mat)
    # refine
    remove_doubles(myrail)
    set_normals(myrail)

    # --------------------------------------------------------------------------------
    # Supports
    # --------------------------------------------------------------------------------
    width = (self.width / self.num) / self.palnum
    # left
    posx = 0.01 + 0.01
    posy = 0.006
    posz = 0.006
    for x in range(self.num):
        mysup = create_japan_support("Support_" + str(x) + ".L",
                                     width - 0.02,    # subtract 2 cm
                                     0, 0, 0,
                                     self.crt_mat)
        support.extend([mysup])
        mysup.parent = myrail

        if x == 0:
            f = self.open01
        if x == 1:
            f = self.open02
        if x == 2:
            f = self.open03
        if x == 3:
            f = self.open04
        if x == 4:
            f = self.open05

        if self.palnum == 1:
            maxpos = ((self.width / self.palnum) - width - 0.02) * f
        else:
            maxpos = ((self.width / self.palnum) - width) * f

        mysup.location.x = posx + maxpos
        mysup.location.y = -posy
        mysup.location.z = posz

        posy += 0.015
    # Right
    if self.palnum > 1:
        posx = self.width - width  # + 0.01
        posy = 0.006
        posz = 0.006
        for x in range(self.num):
            mysup = create_japan_support("Support_" + str(x) + ".R",
                                         width - 0.02,   # subtract 2 cm
                                         0, 0, 0,
                                         self.crt_mat)
            support.extend([mysup])
            mysup.parent = myrail

            if x == 0:
                f = self.open01
            if x == 1:
                f = self.open02
            if x == 2:
                f = self.open03
            if x == 3:
                f = self.open04
            if x == 4:
                f = self.open05

            maxpos = ((self.width / self.palnum) - width) * f

            mysup.location.x = posx - maxpos
            mysup.location.y = -posy
            mysup.location.z = posz

            posy += 0.015
    # --------------------------------------------------------------------------------
    # Panels
    # --------------------------------------------------------------------------------
    width = ((self.width / self.num) / self.palnum) + 0.01
    posx = -0.01
    posy = -0.006
    posz = -0.008
    x = 1
    fabricmat = None
    if self.crt_mat and bpy.context.scene.render.engine == 'CYCLES':
        fabricmat = create_fabric_material("Fabric_material", False, 0.653, 0.485, 0.265,
                                           0.653, 0.485, 0.265)

    for sup in support:
        mypanel = create_japan_panel("Panel_" + str(x),
                                     width, self.height,
                                     0, 0, 0,
                                     self.crt_mat, fabricmat)
        panel.extend([mypanel])
        mypanel.parent = sup
        mypanel.location.x = posx
        mypanel.location.y = posy
        mypanel.location.z = posz
        x += 1
    # ------------------------
    # Strings
    # ------------------------
    x = myrail.location.x
    y = myrail.location.y
    z = myrail.location.z

    long = -1
    if self.height < 1:
        long = -self.height

    myp = [((0, 0, 0), (- 0.25, 0, 0), (0.0, 0, 0)),
           ((0, 0, long), (- 0.01, 0, long), (0.25, 0, long))]  # double element
    mycurve1 = create_bezier("String_1", myp, (x, y, z))
    mycurve1.parent = myrail
    mycurve1.location.x = self.width
    mycurve1.location.y = -0.004
    mycurve1.location.z = 0.005

    mycurve2 = create_bezier("String_2", myp, (x, y, z))
    mycurve2.parent = myrail
    mycurve2.location.x = self.width
    mycurve2.location.y = -0.01
    mycurve2.location.z = 0.005

    if self.crt_mat and bpy.context.scene.render.engine == 'CYCLES':
        mat = create_diffuse_material("String_material", False, 0.1, 0.1, 0.1,
                                      0.1, 0.1, 0.1, 0.01)
        set_material(mycurve1, mat)
        set_material(mycurve2, mat)

    # refine
    for obj in support:
        remove_doubles(obj)
        set_normals(obj)

    # deactivate others
    for o in bpy.data.objects:
        if o.select is True:
            o.select = False

    myrail.select = True
    bpy.context.scene.objects.active = myrail

    return


# ------------------------------------------------------------------------------
# Create japan rail
#
# objName: Name for the new object
# sX: Size in X axis
# ways: Number of ways
# pX: position X axis
# pY: position Y axis
# pZ: position Z axis
# mat: Flag for creating materials
# ------------------------------------------------------------------------------
def create_japan_rail(objname, sx, ways, px, py, pz, mat):
    myvertex = []
    myfaces = []

    waysize = 0.005  # gap
    sz = 0.0145
    size = 0.005
    sizeint = 0.01
    tap = 0.01
    sy = (size * 2) + (waysize * ways) + (sizeint * (ways - 1))
    v = 0
    # left extension
    myvertex.extend([(0, 0, 0), (0, 0, sz), (0, -sy, sz), (0, -sy, 0),
                     (tap, 0, 0), (tap, 0, sz), (tap, -sy, sz), (tap, -sy, 0)])
    myfaces.extend([(0, 1, 2, 3), (4, 5, 6, 7), (0, 1, 5, 4), (3, 2, 6, 7), (2, 1, 5, 6), (3, 0, 4, 7)])
    v += 8
    # Center
    myvertex.extend([(tap, -size, size), (tap, -size, 0), (tap, 0, 0), (tap, 0, sz), (tap, -sy, sz), (tap, -sy, 0),
                     (tap, -sy + size, 0), (tap, -sy + size, sz - 0.002)])
    myvertex.extend([(sx + tap, -size, size), (sx + tap, -size, 0), (sx + tap, 0, 0),
                     (sx + tap, 0, sz), (sx + tap, -sy, sz),
                     (sx + tap, -sy, 0), (sx + tap, -sy + size, 0), (sx + tap, -sy + size, sz - 0.002)])
    myfaces.extend([(v, v + 8, v + 9, v + 1), (v + 1, v + 9, v + 10, v + 2), (v + 2, v + 10, v + 11, v + 3),
                    (v + 3, v + 11, v + 12, v + 4),
                    (v + 4, v + 12, v + 13, v + 5), (v + 5, v + 13, v + 14, v + 6), (v + 7, v + 15, v + 14, v + 6)])
    v += 16
    # Right extension
    myvertex.extend([(sx + tap, 0, 0), (sx + tap, 0, sz), (sx + tap, -sy, sz),
                     (sx + tap, -sy, 0), (sx + tap + tap, 0, 0),
                     (sx + tap + tap, 0, sz), (sx + tap + tap, -sy, sz), (sx + tap + tap, -sy, 0)])
    myfaces.extend([(v, v + 1, v + 2, v + 3), (v + 4, v + 5, v + 6, v + 7),
                    (v, v + 1, v + 5, v + 4), (v + 3, v + 2, v + 6, v + 7),
                    (v + 2, v + 1, v + 5, v + 6), (v + 3, v, v + 4, v + 7)])
    v += 8

    # Internal
    space = waysize + size
    if ways > 1:
        for x in range(ways - 1):
            myvertex.extend([(tap, -space, sz), (tap, -space, 0), (tap, -space - sizeint, 0),
                             (tap, -space - sizeint, size)])
            myvertex.extend([(sx + tap, -space, sz), (sx + tap, -space, 0), (sx + tap, -space - sizeint, 0),
                             (sx + tap, -space - sizeint, size)])
            myfaces.extend([(v, v + 4, v + 5, v + 1), (v + 1, v + 5, v + 6, v + 2), (v + 2, v + 6, v + 7, v + 3)])
            v += 8
            space = space + waysize + sizeint

    mymesh = bpy.data.meshes.new(objname)
    myobject = bpy.data.objects.new(objname, mymesh)

    myobject.location[0] = px
    myobject.location[1] = py
    myobject.location[2] = pz
    bpy.context.scene.objects.link(myobject)

    mymesh.from_pydata(myvertex, [], myfaces)
    mymesh.update(calc_edges=True)

    # ---------------------------------
    # Materials
    # ---------------------------------
    if mat and bpy.context.scene.render.engine == 'CYCLES':
        # External
        mat = create_diffuse_material(objname + "_material", False, 0.8, 0.8, 0.8, 0.6, 0.6, 0.6, 0.15)
        set_material(myobject, mat)

    return myobject


# ------------------------------------------------------------------------------
# Create japan support
#
# objName: Name for the new object
# sX: Size in X axis
# pX: position X axis
# pY: position Y axis
# pZ: position Z axis
# mat: Flag for creating materials
# ------------------------------------------------------------------------------
def create_japan_support(objname, sx, px, py, pz, mat):
    myvertex = []
    myfaces = []

    waysize = 0.008
    sz = 0.015
    sy = 0.006

    myvertex.extend([(0, 0, 0), (0, 0, -sz), (0, -sy, -sz), (0, -sy, -waysize), (0, -0.003, -waysize), (0, -0.003, 0)])
    myvertex.extend(
        [(sx, 0, 0), (sx, 0, -sz), (sx, -sy, -sz), (sx, -sy, - waysize), (sx, -0.003, -waysize), (sx, -0.003, 0)])
    myfaces.extend(
        [(0, 1, 7, 6), (2, 3, 9, 8), (1, 7, 8, 2), (3, 4, 10, 9), (4, 5, 11, 10), (0, 6, 11, 5), (0, 1, 2, 3, 4, 5),
         (6, 7, 8, 9, 10, 11)])

    mymesh = bpy.data.meshes.new(objname)
    myobject = bpy.data.objects.new(objname, mymesh)

    myobject.location[0] = px
    myobject.location[1] = py
    myobject.location[2] = pz
    bpy.context.scene.objects.link(myobject)

    mymesh.from_pydata(myvertex, [], myfaces)
    mymesh.update(calc_edges=True)

    # ---------------------------------
    # Materials
    # ---------------------------------
    if mat and bpy.context.scene.render.engine == 'CYCLES':
        # External
        mat = create_diffuse_material(objname + "_material", False, 0.8, 0.8, 0.8, 0.6, 0.6, 0.6, 0.15)
        set_material(myobject, mat)

    return myobject


# ------------------------------------------------------------------------------
# Create japan panel
#
# objName: Name for the new object
# sX: Size in X axis
# sZ: Size in Z axis
# pX: position X axis
# pY: position Y axis
# pZ: position Z axis
# mat: Flag for creating materials
# fabricMat: Fabric material
# ------------------------------------------------------------------------------
def create_japan_panel(objname, sx, sz, px, py, pz, mat, fabricmat):
    myvertex = []
    myfaces = []

    myvertex.extend([(0, 0, 0), (0, 0, -sz), (sx, 0, -sz), (sx, 0, 0)])
    myfaces.extend([(0, 1, 2, 3)])

    mymesh = bpy.data.meshes.new(objname)
    myobject = bpy.data.objects.new(objname, mymesh)

    myobject.location[0] = px
    myobject.location[1] = py
    myobject.location[2] = pz
    bpy.context.scene.objects.link(myobject)

    mymesh.from_pydata(myvertex, [], myfaces)
    mymesh.update(calc_edges=True)

    # ---------------------------------
    # Materials
    # ---------------------------------
    if mat and bpy.context.scene.render.engine == 'CYCLES':
        unwrap_mesh(myobject, True)
        # remap UV to use all texture
        for uv_loop in myobject.data.uv_layers.active.data:
            myvector = uv_loop.uv
            if myvector.x > 0.0001:
                myvector.x = 1

        set_material(myobject, fabricmat)
    return myobject


# ------------------------------------------------------------------------------
# Create bezier curve
# ------------------------------------------------------------------------------
def create_bezier(objname, points, origin, depth=0.001, fill='FULL'):
    curvedata = bpy.data.curves.new(name=objname, type='CURVE')
    curvedata.dimensions = '3D'
    curvedata.fill_mode = fill
    curvedata.bevel_resolution = 5
    curvedata.bevel_depth = depth

    myobject = bpy.data.objects.new(objname, curvedata)
    myobject.location = origin

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


# ------------------------------------------------------------------------------
# Generate Roller curtains
# All custom values are passed using self container (self.myvariable)
# ------------------------------------------------------------------------------
def generate_roller(self):
    location = bpy.context.scene.cursor_location
    myloc = copy(location)  # copy location to keep 3D cursor position

    # ------------------
    # Roller Top
    # ------------------
    fabricsolid = None
    if self.crt_mat and bpy.context.scene.render.engine == 'CYCLES':
        fabricsolid = create_diffuse_material("Fabric_solid_material", False, 0.653, 0.485, 0.265)

    myroller = create_roller_rail("Roller",
                                  self.width,
                                  0.035,
                                  myloc.x, myloc.y, myloc.z,
                                  self.crt_mat, fabricsolid)
    # refine
    remove_doubles(myroller)
    set_normals(myroller)

    # --------------------------------------------------------------------------------
    # Sides
    # --------------------------------------------------------------------------------
    plastic = None
    if self.crt_mat and bpy.context.scene.render.engine == 'CYCLES':
        plastic = create_diffuse_material("Plastic_roller_material", False, 0.653, 0.485, 0.265, 0.653, 0.485, 0.265,
                                          0.2)

    myside_l = create_roller_sides(myroller, "L",
                                   0.026, 0, 0,
                                   self.crt_mat, plastic)
    # refine
    remove_doubles(myside_l)
    set_normals(myside_l)

    myside_r = create_roller_sides(myroller, "R",
                                   self.width - 0.026, 0, 0,
                                   self.crt_mat, plastic)
    # refine
    remove_doubles(myside_r)
    set_normals(myside_r)

    # --------------------------------------------------------------------------------
    # Panel
    # --------------------------------------------------------------------------------
    fabricmat = None
    if self.crt_mat and bpy.context.scene.render.engine == 'CYCLES':
        fabricmat = create_fabric_material("Fabric_translucent_material", False, 0.653, 0.485, 0.265, 0.653, 0.485,
                                           0.265)

    mypanel = create_japan_panel("Panel",
                                 self.width, self.height,
                                 0, 0, 0,
                                 self.crt_mat, fabricmat)
    mypanel.parent = myroller
    mypanel.location.x = 0
    mypanel.location.y = 0.035
    mypanel.location.z = 0
    # ------------------
    # Roller Bottom
    # ------------------
    mybottom = create_roller_rail("Roller_bottom",
                                  self.width,
                                  0.001,
                                  0, 0, -self.height,
                                  self.crt_mat, plastic)
    mybottom.parent = mypanel
    # refine
    remove_doubles(myroller)
    set_normals(myroller)

    # ------------------------
    # Strings
    # ------------------------
    myp = [((0.0000, -0.0328, -0.0000), (0.0000, -0.0403, -0.3327), (0.0000, -0.0293, 0.1528)),
           ((0.0000, 0.0000, 0.3900), (0.0000, -0.0264, 0.3900), (-0.0000, 0.0226, 0.3900)),
           ((-0.0000, 0.0212, 0.0000), (-0.0000, 0.0189, 0.1525), (-0.0000, 0.0260, -0.3326)),
           ((-0.0000, -0.0000, -0.8518), (-0.0000, 0.0369, -0.8391), (0.0000, -0.0373, -0.8646))]  # double element
    mycurve = create_bezier("String", myp, (0, 0, 0))
    set_curve_cycle(mycurve)
    mycurve.parent = myroller
    mycurve.location.x = self.width + 0.015
    mycurve.location.y = 0
    mycurve.location.z = -0.38
    if self.crt_mat and bpy.context.scene.render.engine == 'CYCLES':
        mat = create_diffuse_material("String_material", False, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.01)
        set_material(mycurve, mat)

    # deactivate others
    for o in bpy.data.objects:
        if o.select is True:
            o.select = False

    myroller.select = True
    bpy.context.scene.objects.active = myroller

    return


# ------------------------------------------------------------------------------
# Create roller
#
# objName: Object name
# width: Total width of roller
# radio: Roll radio
# pX: position X axis
# pY: position Y axis
# pZ: position Z axis
# mat: create default cycles material
# mymaterial: plastic material or fabric
# ------------------------------------------------------------------------------
def create_roller_rail(objname, width, radio, px, py, pz, mat, mymaterial):
    myvertex = []
    myfaces = []
    pies = 16
    seg = 0

    # Add right circle
    for i in range(pies):
        x = cos(radians(seg)) * radio
        y = sin(radians(seg)) * radio
        mypoint = [(0.0, x, y)]
        myvertex.extend(mypoint)
        seg += 360 / pies
    # Add left circle
    seg = 0
    for i in range(pies):
        x = cos(radians(seg)) * radio
        y = sin(radians(seg)) * radio
        mypoint = [(width, x, y)]
        myvertex.extend(mypoint)
        seg += 360 / pies
    # -------------------------------------
    # Faces
    # -------------------------------------
    t = 1
    for n in range(0, pies):
        t += 1
        if t > pies:
            t = 1
            myface = [(n, n - pies + 1, n + 1, n + pies)]
            myfaces.extend(myface)
        else:
            myface = [(n, n + 1, n + pies + 1, n + pies)]
            myfaces.extend(myface)

    mymesh = bpy.data.meshes.new(objname)
    myroll = bpy.data.objects.new(objname, mymesh)
    bpy.context.scene.objects.link(myroll)

    mymesh.from_pydata(myvertex, [], myfaces)
    mymesh.update(calc_edges=True)
    # Position
    myroll.location.x = px
    myroll.location.y = py
    myroll.location.z = pz

    # Materials
    if mat and bpy.context.scene.render.engine == 'CYCLES':
        set_material(myroll, mymaterial)

    # Smooth
    set_smooth(myroll)

    return myroll


# ------------------------------------------------------------------------------
# Create roller sides
#
# myRoller: Roller to add sides
# side: Side of the cap R/L
# pX: position X axis
# pY: position Y axis
# pZ: position Z axis
# mat: create default cycles material
# plastic: plastic material
# ------------------------------------------------------------------------------
def create_roller_sides(myroller, side, px, py, pz, mat, plastic):
    # Retry mesh data
    mydata = roller_side()

    # move data
    myvertex = mydata[0]
    myfaces = mydata[1]

    mymesh = bpy.data.meshes.new("Side." + side)
    myside = bpy.data.objects.new("Side." + side, mymesh)
    bpy.context.scene.objects.link(myside)
    mymesh.from_pydata(myvertex, [], myfaces)
    mymesh.update(calc_edges=True)
    # Position
    myside.location.x = px
    myside.location.y = py
    myside.location.z = pz
    # rotate
    if side == "L":
        myside.rotation_euler = (0, 0, radians(180))
    # parent
    myside.parent = myroller

    # Materials
    if mat and bpy.context.scene.render.engine == 'CYCLES':
        set_material(myside, plastic)

    # Smooth
    set_smooth(myside)
    set_modifier_subsurf(myside)

    return myside


# ----------------------------------------------
# Roller side data
# ----------------------------------------------
def roller_side():
    # ------------------------------------
    # Mesh data
    # ------------------------------------
    minx = -7.54842304218073e-08
    maxx = 0.05209559202194214
    miny = -0.04486268758773804
    maxy = 0.04486268758773804
    minz = -0.04486268758773804
    maxz = 0.08202265202999115

    # Vertex
    myvertex = [(maxx - 0.004684023559093475, maxy, minz + 0.04486270064847542),
                (maxx - 0.004684023559093475, maxy - 0.0034149661660194397, minz + 0.027694489806890488),
                (maxx - 0.004684023559093475, maxy - 0.013139978051185608, minz + 0.013139985501766205),
                (maxx - 0.004684023559093475, maxy - 0.02769448049366474, minz + 0.0034149736166000366),
                (maxx - 0.004684023559093475, miny + 0.044862685327428764, minz),
                (maxx - 0.004684023559093475, miny + 0.027694476768374443, minz + 0.0034149736166000366),
                (maxx - 0.004684023559093475, miny + 0.013139978051185608, minz + 0.013139985501766205),
                (maxx - 0.004684023559093475, miny + 0.0034149661660194397, minz + 0.02769448794424534),
                (maxx - 0.004684023559093475, miny, minz + 0.04486269387439812),
                (maxx - 0.004684023559093475, miny + 0.0034149624407291412, minz + 0.06203090213239193),
                (maxx - 0.004684023559093475, miny + 0.013139966875314713, maxz - 0.050299935042858124),
                (maxx - 0.004684023559093475, miny + 0.027694474905729294, maxz - 0.04057491198182106),
                (maxx - 0.004684023559093475, maxy - 0.027694473043084145, maxz - 0.04057491570711136),
                (maxx - 0.004684023559093475, maxy - 0.013139966875314713, maxz - 0.05029993876814842),
                (maxx - 0.004684023559093475, maxy - 0.0034149587154388428, minz + 0.062030890956521034),
                (maxx - 0.0046574510633945465, miny + 0.028278490528464317, minz + 0.0048249028623104095),
                (maxx - 0.0046574510633945465, miny + 0.014219092205166817, minz + 0.014219097793102264),
                (maxx - 0.0046574510633945465, miny + 0.004824899137020111, minz + 0.028278499841690063),
                (maxx - 0.003122705966234207, maxy, minz + 0.04486270064847542),
                (maxx - 0.003122705966234207, maxy - 0.0034149661660194397, minz + 0.027694489806890488),
                (maxx - 0.003122705966234207, maxy - 0.013139978051185608, minz + 0.013139985501766205),
                (maxx - 0.003122705966234207, maxy - 0.02769448049366474, minz + 0.0034149736166000366),
                (maxx - 0.003149278461933136, maxy - 0.04486268735455812, maxz - 0.03868604078888893),
                (maxx - 0.003149278461933136, maxy - 0.02827848680317402, maxz - 0.04198484495282173),
                (maxx - 0.003149278461933136, maxy - 0.014219081029295921, maxz - 0.05137905105948448),
                (maxx - 0.003149278461933136, maxy - 0.004824887961149216, minz + 0.06144687905907631),
                (maxx - 0.02118653617799282, miny + 0.027694474905729294, maxz - 0.04057491570711136),
                (maxx - 0.02118653617799282, miny + 0.013139966875314713, maxz - 0.050299935042858124),
                (maxx - 0.02118653617799282, miny + 0.0034149624407291412, minz + 0.06203090213239193),
                (maxx - 0.02118653617799282, miny, minz + 0.04486269262849252),
                (maxx - 0.003122705966234207, miny, minz + 0.04486269387439812),
                (maxx - 0.003122705966234207, miny + 0.0034149624407291412, minz + 0.06203090213239193),
                (maxx - 0.003122705966234207, miny + 0.013139966875314713, maxz - 0.050299935042858124),
                (maxx - 0.003122705966234207, miny + 0.027694474905729294, maxz - 0.04057491198182106),
                (maxx - 0.02118653617799282, maxy - 0.02769448049366474, minz + 0.0034149661660194397),
                (maxx - 0.02118653617799282, maxy - 0.013139978051185608, minz + 0.013139981776475906),
                (maxx - 0.02118653617799282, maxy - 0.0034149661660194397, minz + 0.02769448794424534),
                (maxx - 0.02118653617799282, maxy, minz + 0.044862699402576034),
                (maxx - 0.020517520606517792, miny + 0.01146744191646576, minz + 0.03102993033826351),
                (maxx - 0.020517520606517792, miny + 0.01930307224392891, minz + 0.019303075969219208),
                (maxx - 0.020517520606517792, miny + 0.031029919162392616, minz + 0.01146744191646576),
                (maxx - 0.020517520606517792, miny + 0.04486268576937835, minz + 0.008715935051441193),
                (maxx - 0.003122705966234207, maxy - 0.013139966875314713, maxz - 0.02605174481868744),
                (maxx, miny + 0.013139966875314713, maxz - 0.026319395750761032),
                (maxx, miny + 0.027694474905729294, maxz - 0.026230186223983765),
                (maxx, maxy - 0.013139966875314713, maxz - 0.02605174481868744),
                (maxx - 0.0046574510633945465, miny + 0.0015261024236679077, minz + 0.04486269394558251),
                (maxx - 0.0046574510633945465, miny + 0.004824895411729813, minz + 0.061446888372302055),
                (maxx - 0.0046574510633945465, miny + 0.014219081029295921, maxz - 0.05137904919683933),
                (maxx - 0.0046574510633945465, miny + 0.02827848680317402, maxz - 0.04198484495282173),
                (maxx, maxy - 0.027694473043084145, maxz - 0.026230186223983765),
                (maxx, maxy - 0.04486268735205459, maxz - 0.02629481628537178),
                (maxx - 0.003122705966234207, maxy - 0.027694473043084145, maxz - 0.04057491570711136),
                (maxx - 0.003122705966234207, maxy - 0.013139966875314713, maxz - 0.05029993876814842),
                (maxx - 0.003122705966234207, maxy - 0.0034149587154388428, minz + 0.062030890956521034),
                (maxx - 0.003149278461933136, maxy - 0.0015261024236679077, minz + 0.044862700489230356),
                (maxx - 0.003149278461933136, maxy - 0.004824899137020111, minz + 0.028278501704335213),
                (maxx - 0.003149278461933136, maxy - 0.014219092205166817, minz + 0.014219097793102264),
                (maxx - 0.003149278461933136, maxy - 0.028278492391109467, minz + 0.0048249028623104095),
                (maxx - 0.003149278461933136, miny + 0.0015261024236679077, minz + 0.04486269394558251),
                (maxx - 0.003149278461933136, miny + 0.004824895411729813, minz + 0.061446888372302055),
                (maxx - 0.003149278461933136, miny + 0.014219081029295921, maxz - 0.05137904919683933),
                (maxx - 0.003149278461933136, miny + 0.02827848680317402, maxz - 0.04198484495282173),
                (maxx - 0.02118653617799282, maxy - 0.0034149587154388428, minz + 0.062030889093875885),
                (maxx - 0.02118653617799282, maxy - 0.013139966875314713, maxz - 0.05029993876814842),
                (maxx - 0.02118653617799282, maxy - 0.027694473043084145, maxz - 0.04057491570711136),
                (maxx - 0.02118653617799282, maxy - 0.04486268735205459, maxz - 0.03715994209051132),
                (maxx - 0.020517520606517792, maxy - 0.011467430740594864, minz + 0.058695447631180286),
                (maxx - 0.020517520606517792, maxy - 0.019303061068058014, maxz - 0.05646303482353687),
                (maxx - 0.020517520606517792, maxy - 0.031029915437102318, maxz - 0.04862739145755768),
                (maxx - 0.020517520606517792, maxy - 0.044862687395027134, maxz - 0.045875877141952515),
                (maxx, miny + 0.0034149661660194397, minz + 0.02769448794424534),
                (maxx, miny + 0.013139978051185608, minz + 0.013139985501766205),
                (maxx, miny + 0.027694476768374443, minz + 0.0034149736166000366),
                (maxx, miny + 0.044862685327428764, minz),
                (maxx - 0.02118653617799282, miny + 0.0034149661660194397, minz + 0.02769448794424534),
                (maxx - 0.02118653617799282, miny + 0.013139978051185608, minz + 0.013139981776475906),
                (maxx - 0.02118653617799282, miny + 0.027694476768374443, minz + 0.0034149661660194397),
                (maxx - 0.02118653617799282, miny + 0.044862685327428764, minz),
                (maxx - 0.020517520606517792, maxy - 0.031029922887682915, minz + 0.01146744191646576),
                (maxx - 0.020517520606517792, maxy - 0.01930307224392891, minz + 0.019303075969219208),
                (maxx - 0.020517520606517792, maxy - 0.01146744191646576, minz + 0.03102993033826351),
                (maxx - 0.020517520606517792, maxy - 0.008715927600860596, minz + 0.04486269835125789),
                (maxx - 0.0046574510633945465, maxy - 0.04486268735455812, maxz - 0.03868604078888893),
                (maxx - 0.0046574510633945465, maxy - 0.02827848680317402, maxz - 0.04198484495282173),
                (maxx - 0.0046574510633945465, maxy - 0.014219081029295921, maxz - 0.05137905105948448),
                (maxx - 0.0046574510633945465, maxy - 0.004824887961149216, minz + 0.06144687905907631),
                (maxx - 0.0046574510633945465, maxy - 0.0015261024236679077, minz + 0.044862700489230356),
                (maxx - 0.0046574510633945465, maxy - 0.004824899137020111, minz + 0.028278501704335213),
                (maxx - 0.0046574510633945465, maxy - 0.014219092205166817, minz + 0.014219097793102264),
                (maxx - 0.0046574510633945465, maxy - 0.028278492391109467, minz + 0.0048249028623104095),
                (maxx - 0.003149278461933136, miny + 0.004824899137020111, minz + 0.028278499841690063),
                (maxx - 0.003149278461933136, miny + 0.014219092205166817, minz + 0.014219097793102264),
                (maxx - 0.003149278461933136, miny + 0.028278490528464317, minz + 0.0048249028623104095),
                (maxx, miny, minz + 0.04486269387439812),
                (maxx, miny + 0.0034149624407291412, minz + 0.06203090213239193),
                (maxx, miny + 0.013139966875314713, maxz - 0.050299935042858124),
                (maxx, miny + 0.027694474905729294, maxz - 0.04057491198182106),
                (maxx - 0.020517520606517792, miny + 0.031029917299747467, maxz - 0.04862739145755768),
                (maxx - 0.020517520606517792, miny + 0.019303061068058014, maxz - 0.056463029235601425),
                (maxx - 0.020517520606517792, miny + 0.011467434465885162, minz + 0.05869545880705118),
                (maxx - 0.020517520606517792, miny + 0.008715927600860596, minz + 0.04486269289324163),
                (maxx - 0.003122705966234207, miny + 0.0034149661660194397, minz + 0.02769448794424534),
                (maxx - 0.003122705966234207, miny + 0.013139978051185608, minz + 0.013139985501766205),
                (maxx - 0.003122705966234207, miny + 0.027694476768374443, minz + 0.0034149736166000366),
                (maxx - 0.003122705966234207, miny + 0.044862685327428764, minz),
                (maxx, maxy - 0.02769448049366474, minz + 0.0034149736166000366),
                (maxx, maxy - 0.013139978051185608, minz + 0.013139985501766205),
                (maxx, maxy - 0.0034149661660194397, minz + 0.027694489806890488),
                (maxx, maxy, minz + 0.04486270064847542),
                (maxx, maxy - 0.0034149587154388428, minz + 0.062030890956521034),
                (maxx, maxy - 0.013139966875314713, maxz - 0.05029993876814842),
                (maxx, maxy - 0.027694473043084145, maxz - 0.04057491570711136),
                (maxx, maxy - 0.04486268735205459, maxz - 0.03715994209051132),
                (maxx - 0.003122705966234207, maxy - 0.027694473043084145, maxz - 0.026230186223983765),
                (maxx - 0.003122705966234207, maxy - 0.04486268735205459, maxz - 0.02629481628537178),
                (maxx - 0.003122705966234207, miny + 0.027694474905729294, maxz - 0.026230186223983765),
                (maxx - 0.003122705966234207, miny + 0.013139966875314713, maxz - 0.026319395750761032),
                (maxx - 0.003122705966234207, miny + 0.013139966875314713, maxz - 0.0018796995282173157),
                (maxx - 0.01466318964958191, miny + 0.013139966875314713, maxz - 0.0018796995282173157),
                (maxx, miny + 0.027694474905729294, maxz - 0.0017904937267303467),
                (maxx, maxy - 0.013139966875314713, maxz - 0.001612052321434021),
                (maxx - 0.009187713265419006, maxy - 0.013139966875314713, maxz - 0.02605174481868744),
                (maxx - 0.009187713265419006, maxy - 0.027694473043084145, maxz - 0.026230186223983765),
                (maxx - 0.009187713265419006, maxy - 0.04486268735205459, maxz - 0.02629481628537178),
                (maxx - 0.009187713265419006, miny + 0.027694474905729294, maxz - 0.026230186223983765),
                (maxx - 0.009187713265419006, miny + 0.013139966875314713, maxz - 0.026319395750761032),
                (maxx - 0.003122705966234207, maxy - 0.013139966875314713, maxz - 0.001612052321434021),
                (maxx - 0.01466318964958191, miny + 0.027694474905729294, maxz - 0.0017904937267303467),
                (maxx, miny + 0.022660084068775177, minz + 0.03566607739776373),
                (maxx, miny + 0.02786955051124096, minz + 0.027869559824466705),
                (maxx, miny + 0.03566606715321541, minz + 0.022660093382000923),
                (maxx, miny + 0.04486268649608238, minz + 0.020830770954489708),
                (maxx, miny + 0.02083076350390911, minz + 0.044862694725740226),
                (maxx, miny + 0.022660082206130028, minz + 0.05405931267887354),
                (maxx, miny + 0.02786954492330551, minz + 0.061855828389525414),
                (maxx, miny + 0.035666066221892834, maxz - 0.059820037335157394),
                (maxx, maxy - 0.03566606715321541, minz + 0.022660093382000923),
                (maxx, maxy - 0.02786955051124096, minz + 0.027869559824466705),
                (maxx, maxy - 0.022660084068775177, minz + 0.035666078329086304),
                (maxx, maxy - 0.02083076350390911, minz + 0.044862698354463326),
                (maxx, maxy - 0.02266007848083973, minz + 0.05405930709093809),
                (maxx, maxy - 0.02786954492330551, minz + 0.061855828389525414),
                (maxx, maxy - 0.035666064359247684, maxz - 0.059820037335157394),
                (maxx, maxy - 0.04486268734234705, maxz - 0.05799071677029133),
                (maxx, miny + 0.04486268733843682, minz + 0.04486269544876009),
                (maxx - 0.009557131677865982, maxy - 0.04486268735205459, maxz - 0.02464577928185463),
                (maxx - 0.009557131677865982, miny + 0.027694474905729294, maxz - 0.024581149220466614),
                (maxx - 0.009557131677865982, maxy - 0.013139966875314713, maxz - 0.024402707815170288),
                (maxx - 0.009557131677865982, miny + 0.013139966875314713, maxz - 0.02467035874724388),
                (maxx - 0.009557131677865982, maxy - 0.027694473043084145, maxz - 0.024581149220466614),
                (maxx - 0.015024378895759583, miny + 0.027694474905729294, maxz - 0.00017844140529632568),
                (maxx - 0.015024378895759583, miny + 0.013139966875314713, maxz - 0.0002676546573638916),
                (maxx - 0.015024378895759583, maxy - 0.04486268735205459, maxz - 0.0002430751919746399),
                (maxx - 0.015024378895759583, maxy - 0.027694473043084145, maxz - 0.00017844140529632568),
                (maxx - 0.015024378895759583, maxy - 0.013139966875314713, maxz),
                (maxx, miny + 0.013139966875314713, maxz - 0.0018796995282173157),
                (maxx - 0.01466318964958191, maxy - 0.04486268735205459, maxz - 0.001855120062828064),
                (maxx, maxy - 0.04486268735205459, maxz - 0.001855120062828064),
                (maxx - 0.01466318964958191, maxy - 0.027694473043084145, maxz - 0.0017904937267303467),
                (maxx - 0.01466318964958191, maxy - 0.013139966875314713, maxz - 0.001612052321434021),
                (maxx, maxy - 0.027694473043084145, maxz - 0.0017904937267303467),
                (maxx - 0.020517520606517792, miny + 0.014739999547600746, minz + 0.03238546848297119),
                (maxx - 0.020517520606517792, miny + 0.021807780489325523, minz + 0.02180778607726097),
                (maxx - 0.020517520606517792, miny + 0.03238545823842287, minz + 0.014740003272891045),
                (maxx - 0.020517520606517792, miny + 0.044862685933359736, minz + 0.012258127331733704),
                (maxx - 0.020517520606517792, maxy - 0.014739990234375, minz + 0.05733991041779518),
                (maxx - 0.020517520606517792, maxy - 0.021807771176099777, maxz - 0.05896773934364319),
                (maxx - 0.020517520606517792, maxy - 0.03238545451313257, maxz - 0.051899950951337814),
                (maxx - 0.020517520606517792, maxy - 0.044862687428120204, maxz - 0.049418069422245026),
                (maxx - 0.020517520606517792, maxy - 0.03238546196371317, minz + 0.014740003272891045),
                (maxx - 0.020517520606517792, maxy - 0.021807780489325523, minz + 0.02180778607726097),
                (maxx - 0.020517520606517792, maxy - 0.014739999547600746, minz + 0.03238546848297119),
                (maxx - 0.020517520606517792, maxy - 0.012258119881153107, minz + 0.04486269794694575),
                (maxx - 0.020517520606517792, miny + 0.03238545544445515, maxz - 0.051899950951337814),
                (maxx - 0.020517520606517792, miny + 0.021807771176099777, maxz - 0.05896773561835289),
                (maxx - 0.020517520606517792, miny + 0.014739995822310448, minz + 0.05733991973102093),
                (maxx - 0.020517520606517792, miny + 0.012258119881153107, minz + 0.04486269302378876),
                (minx, miny + 0.014739999547600746, minz + 0.03238546848297119),
                (minx, miny + 0.021807780489325523, minz + 0.02180778607726097),
                (minx, miny + 0.03238545823842287, minz + 0.014740003272891045),
                (minx, miny + 0.044862685933359736, minz + 0.012258127331733704),
                (minx, maxy - 0.014739990234375, minz + 0.05733991041779518),
                (minx, maxy - 0.021807771176099777, maxz - 0.05896773934364319),
                (minx, maxy - 0.03238545451313257, maxz - 0.051899950951337814),
                (minx, maxy - 0.044862687428120204, maxz - 0.049418069422245026),
                (minx, maxy - 0.03238546196371317, minz + 0.014740003272891045),
                (minx, maxy - 0.021807780489325523, minz + 0.02180778607726097),
                (minx, maxy - 0.014739999547600746, minz + 0.03238546848297119),
                (minx, maxy - 0.012258119881153107, minz + 0.04486269794694575),
                (minx, miny + 0.03238545544445515, maxz - 0.051899950951337814),
                (minx, miny + 0.021807771176099777, maxz - 0.05896773561835289),
                (minx, miny + 0.014739995822310448, minz + 0.05733991973102093),
                (minx, miny + 0.012258119881153107, minz + 0.04486269302378876)]

    # Faces
    myfaces = [(37, 0, 1, 36), (36, 1, 2, 35), (35, 2, 3, 34), (34, 3, 4, 78), (78, 4, 5, 77),
               (77, 5, 6, 76), (76, 6, 7, 75), (75, 7, 8, 29), (29, 8, 9, 28), (28, 9, 10, 27),
               (27, 10, 11, 26), (65, 12, 13, 64), (8, 7, 17, 46), (63, 14, 0, 37), (64, 13, 14, 63),
               (34, 78, 41, 79), (64, 63, 67, 68), (76, 75, 38, 39), (65, 64, 68, 69), (27, 26, 98, 99),
               (78, 77, 40, 41), (28, 27, 99, 100), (35, 34, 79, 80), (63, 37, 82, 67), (29, 28, 100, 101),
               (26, 66, 70, 98), (36, 35, 80, 81), (66, 65, 69, 70), (77, 76, 39, 40), (37, 36, 81, 82),
               (75, 29, 101, 38), (19, 18, 109, 108), (31, 32, 61, 60), (2, 1, 88, 89), (103, 102, 91, 92),
               (7, 6, 16, 17), (54, 18, 55, 25), (32, 33, 62, 61), (18, 19, 56, 55), (6, 5, 15, 16),
               (11, 10, 48, 49), (52, 53, 24, 23), (0, 14, 86, 87), (94, 71, 129, 133), (97, 113, 51, 44),
               (33, 32, 117, 116), (18, 54, 110, 109), (32, 31, 95, 96), (96, 97, 44, 43), (102, 103, 72, 71),
               (53, 52, 114, 42), (21, 20, 107, 106), (103, 104, 73, 72), (31, 30, 94, 95), (20, 19, 108, 107),
               (30, 102, 71, 94), (105, 21, 106, 74), (54, 53, 111, 110), (104, 105, 74, 73), (47, 46, 59, 60),
               (90, 89, 57, 58), (87, 86, 25, 55), (48, 47, 60, 61), (49, 48, 61, 62), (83, 49, 62, 22),
               (16, 15, 93, 92), (88, 87, 55, 56), (84, 83, 22, 23), (17, 16, 92, 91), (85, 84, 23, 24),
               (46, 17, 91, 59), (89, 88, 56, 57), (86, 85, 24, 25), (104, 103, 92, 93), (3, 2, 89, 90),
               (20, 21, 58, 57), (13, 12, 84, 85), (9, 8, 46, 47), (102, 30, 59, 91), (30, 31, 60, 59),
               (19, 20, 57, 56), (14, 13, 85, 86), (53, 54, 25, 24), (10, 9, 47, 48), (1, 0, 87, 88),
               (111, 53, 42, 45), (112, 111, 45, 50), (32, 96, 43, 117), (113, 112, 50, 51), (115, 116, 125, 124),
               (42, 114, 123, 122), (116, 117, 126, 125), (114, 115, 124, 123), (112, 113, 144, 143),
               (95, 94, 133, 134),
               (110, 111, 142, 141), (96, 95, 134, 135), (74, 106, 137, 132), (97, 96, 135, 136), (73, 74, 132, 131),
               (107, 108, 139, 138), (113, 97, 136, 144), (72, 73, 131, 130), (108, 109, 140, 139),
               (109, 110, 141, 140),
               (71, 72, 130, 129), (106, 107, 138, 137), (111, 112, 143, 142), (135, 134, 145), (137, 138, 145),
               (142, 143, 145), (136, 135, 145), (131, 132, 145), (143, 144, 145), (144, 136, 145),
               (130, 131, 145), (141, 142, 145), (129, 130, 145), (132, 137, 145), (133, 129, 145),
               (138, 139, 145), (134, 133, 145), (139, 140, 145), (140, 141, 145), (26, 11, 83, 66),
               (66, 83, 12, 65), (12, 83, 84), (22, 52, 23), (83, 11, 49), (21, 105, 58),
               (4, 90, 58, 105), (15, 4, 105, 93), (33, 22, 62), (4, 3, 90), (105, 104, 93),
               (5, 4, 15), (52, 22, 115, 114), (22, 33, 116, 115), (124, 125, 147, 146), (122, 123, 150, 148),
               (125, 126, 149, 147), (123, 124, 146, 150), (157, 128, 151, 153), (159, 157, 153, 154),
               (160, 159, 154, 155),
               (128, 119, 152, 151), (146, 147, 128, 157), (150, 146, 157, 159), (148, 150, 159, 160),
               (147, 149, 119, 128),
               (69, 68, 167, 168), (101, 100, 176, 177), (39, 38, 162, 163), (100, 99, 175, 176), (41, 40, 164, 165),
               (80, 79, 170, 171), (82, 81, 172, 173), (67, 82, 173, 166), (70, 69, 168, 169), (38, 101, 177, 162),
               (98, 70, 169, 174), (40, 39, 163, 164), (79, 41, 165, 170), (99, 98, 174, 175), (81, 80, 171, 172),
               (68, 67, 166, 167), (169, 168, 184, 185), (170, 165, 181, 186), (172, 171, 187, 188),
               (162, 177, 193, 178),
               (164, 163, 179, 180), (167, 166, 182, 183), (174, 169, 185, 190), (168, 167, 183, 184),
               (175, 174, 190, 191),
               (171, 170, 186, 187), (173, 172, 188, 189), (163, 162, 178, 179), (165, 164, 180, 181),
               (177, 176, 192, 193),
               (166, 173, 189, 182), (176, 175, 191, 192), (51, 50, 161, 158), (127, 160, 155), (156, 120, 151, 152),
               (155, 154, 161, 121), (161, 154, 153, 158), (42, 122, 148), (117, 149, 126), (127, 42, 148, 160),
               (118, 152, 119), (158, 153, 151, 120), (50, 45, 121, 161), (117, 118, 119, 149), (43, 44, 120, 156),
               (44, 51, 158, 120), (117, 43, 156, 118), (45, 42, 127, 121)]

    return myvertex, myfaces


# --------------------------------------------------------------------
# Set curve cycle
#
# myObject: Curve obejct
# --------------------------------------------------------------------
def set_curve_cycle(myobject):
    bpy.context.scene.objects.active = myobject
    # go edit mode
    bpy.ops.object.mode_set(mode='EDIT')
    # select all faces
    bpy.ops.curve.select_all(action='SELECT')
    # St cyclic
    bpy.ops.curve.cyclic_toggle(direction='CYCLIC_U')
    # go object mode again
    bpy.ops.object.editmode_toggle()
