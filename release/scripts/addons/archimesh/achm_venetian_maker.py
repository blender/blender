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
# Author: Antonio Vazquez (antonioya)
#
# ----------------------------------------------------------
# noinspection PyUnresolvedReferences
import bpy
from math import atan, sin, cos, radians
# noinspection PyUnresolvedReferences
from bpy.types import Operator, PropertyGroup, Object, Panel
from bpy.props import FloatProperty, BoolProperty, IntProperty, FloatVectorProperty, CollectionProperty
from .achm_tools import *


# ------------------------------------------------------------------
# Define operator class to create object
# ------------------------------------------------------------------
class AchmVenetian(Operator):
    bl_idname = "mesh.archimesh_venetian"
    bl_label = "Venetian blind"
    bl_description = "Venetian"
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

    # -----------------------------------------------------
    # Execute
    # -----------------------------------------------------
    def execute(self, context):
        if bpy.context.mode == "OBJECT":
            create_object(self, context)
            return {'FINISHED'}
        else:
            self.report({'WARNING'}, "Archimesh: Option only valid in Object mode")
            return {'CANCELLED'}


# ------------------------------------------------------------------------------
#
# Create main object. The other objects will be children of this.
#
# ------------------------------------------------------------------------------
# noinspection PyUnusedLocal
def create_object(self, context):
    # deselect all objects
    for o in bpy.data.objects:
        o.select = False

    # we create main object and mesh
    mainmesh = bpy.data.meshes.new("VenetianFrane")
    mainobject = bpy.data.objects.new("VenetianFrame", mainmesh)
    mainobject.location = bpy.context.scene.cursor_location
    bpy.context.scene.objects.link(mainobject)
    mainobject.VenetianObjectGenerator.add()

    # we shape the main object and create other objects as children
    shape_mesh_and_create_children(mainobject, mainmesh)

    # we select, and activate, main object
    mainobject.select = True
    bpy.context.scene.objects.active = mainobject


# ------------------------------------------------------------------------------
#
# Update main mesh and children objects
#
# ------------------------------------------------------------------------------
# noinspection PyUnusedLocal
def update_object(self, context):
    # When we update, the active object is the main object
    o = bpy.context.active_object
    oldmesh = o.data
    oldname = o.data.name
    # Now we deselect that object to not delete it.
    o.select = False
    # and we create a new mesh
    tmp_mesh = bpy.data.meshes.new("temp")
    # deselect all objects
    for obj in bpy.data.objects:
        obj.select = False

    # -----------------------
    # remove all children
    # -----------------------
    # first granchild
    for child in o.children:
        remove_children(child)
    # now children of main object
    remove_children(o)

    # Finally we create all that again (except main object),
    shape_mesh_and_create_children(o, tmp_mesh, True)
    o.data = tmp_mesh
    # Remove data (mesh of active object),
    bpy.data.meshes.remove(oldmesh)
    tmp_mesh.name = oldname
    # and select, and activate, the main object
    o.select = True
    bpy.context.scene.objects.active = o


# ------------------------------------------------------------------------------
# Generate all objects
# For main, it only shapes mesh and creates modifiers (the modifier, only the first time).
# And, for the others, it creates object and mesh.
# ------------------------------------------------------------------------------
# noinspection PyUnusedLocal
def shape_mesh_and_create_children(mainobject, tmp_mesh, update=False):
    mp = mainobject.VenetianObjectGenerator[0]
    mat = None
    plastic = None

    if mp.crt_mat and bpy.context.scene.render.engine == 'CYCLES':
        rgb = mp.objcol
        plastic = create_diffuse_material("Plastic_venetian_material", True, rgb[0], rgb[1], rgb[2], rgb[0], rgb[1],
                                          rgb[2], 0.2)

    # ------------------
    # Top
    # ------------------
    create_venetian_top(tmp_mesh, mp.width + 0.002, mp.depth + 0.002, -0.06)

    # materials
    if mp.crt_mat and bpy.context.scene.render.engine == 'CYCLES':
        set_material(mainobject, plastic)
    # --------------------------------------------------------------------------------
    # segments
    # --------------------------------------------------------------------------------
    margin = mp.depth
    mydata = create_slat_mesh("Venetian_slats", mp.width, mp.depth, mp.height - 0.06, mp.angle, mp.ratio)
    myslats = mydata[0]
    myslats.parent = mainobject
    myslats.location.x = 0
    myslats.location.y = 0
    myslats.location.z = -margin - 0.04

    mypoints = mydata[1]
    angleused = mydata[2]
    # refine
    remove_doubles(myslats)
    set_normals(myslats)
    set_smooth(myslats)

    if mp.crt_mat and bpy.context.scene.render.engine == 'CYCLES':
        set_material(myslats, plastic)
    # ------------------------
    # Strings (Middle)
    # ------------------------
    myp = [((0, 0, mypoints[0] + margin), (0, 0, 0), (0, 0, 0)),
           ((0, 0, mypoints[len(mypoints) - 1]), (0, 0, 0), (0, 0, 0))]

    mycurvel = create_bezier("String.L", myp, (0, 0, 0))
    mycurvel.parent = myslats
    mycurvec = create_bezier("String.C", myp, (0, 0, 0))
    mycurvec.parent = myslats
    mycurver = create_bezier("String.R", myp, (0, 0, 0))
    mycurver.parent = myslats

    if mp.width < 0.60:
        sep = 0.058
    else:
        sep = 0.148

    mycurvel.location.x = (mp.width / 2) - sep
    mycurvel.location.y = 0
    mycurvel.location.z = 0

    mycurvec.location.x = 0
    mycurvec.location.y = 0
    mycurvec.location.z = 0

    mycurver.location.x = -(mp.width / 2) + sep
    mycurver.location.y = 0
    mycurver.location.z = 0

    if mp.crt_mat and bpy.context.scene.render.engine == 'CYCLES':
        mat = create_diffuse_material("String_material", False, 0.674, 0.617, 0.496, 0.1, 0.1, 0.1, 0.01)
        set_material(mycurvel, mat)
        set_material(mycurvec, mat)
        set_material(mycurver, mat)
    # ------------------------
    # Strings (Front)
    # ------------------------
    myp = [((0, 0, margin), (0, 0, 0), (0, 0, 0)),
           ((0, 0, mypoints[len(mypoints) - 1] - 0.003 - sin(radians(angleused)) * mp.depth / 2), (0, 0, 0),
            (0, 0, 0))]

    mycurvelf = create_bezier("String.f.L", myp, (0, 0, 0), 0.001, 'FRONT')
    mycurvelf.parent = myslats
    mycurvecf = create_bezier("String.f.C", myp, (0, 0, 0), 0.001, 'FRONT')
    mycurvecf.parent = myslats
    mycurverf = create_bezier("String.f.R", myp, (0, 0, 0), 0.001, 'FRONT')
    mycurverf.parent = myslats

    if mp.width < 0.60:
        sep = 0.058
    else:
        sep = 0.148

    mycurvelf.location.x = (mp.width / 2) - sep
    mycurvelf.location.y = ((-mp.depth / 2) * cos(radians(mp.angle))) - 0.001
    mycurvelf.location.z = 0

    mycurvecf.location.x = 0
    mycurvecf.location.y = ((-mp.depth / 2) * cos(radians(mp.angle))) - 0.001
    mycurvecf.location.z = 0

    mycurverf.location.x = -(mp.width / 2) + sep
    mycurverf.location.y = ((-mp.depth / 2) * cos(radians(mp.angle))) - 0.001
    mycurverf.location.z = 0

    if mp.crt_mat and bpy.context.scene.render.engine == 'CYCLES':
        set_material(mycurvelf, mat)
        set_material(mycurvecf, mat)
        set_material(mycurverf, mat)

    # ------------------------
    # Strings (Back)
    # ------------------------
    myp = [((0, 0, margin), (0, 0, 0), (0, 0, 0)),
           ((0, 0, mypoints[len(mypoints) - 1] - 0.003 + sin(radians(angleused)) * mp.depth / 2),
            (0, 0, 0),
            (0, 0, 0))]

    mycurvelb = create_bezier("String.b.L", myp, (0, 0, 0), 0.001, 'BACK')
    mycurvelb.parent = myslats
    mycurvecb = create_bezier("String.b.C", myp, (0, 0, 0), 0.001, 'BACK')
    mycurvecb.parent = myslats
    mycurverb = create_bezier("String.b.R", myp, (0, 0, 0), 0.001, 'BACK')
    mycurverb.parent = myslats

    if mp.width < 0.60:
        sep = 0.058
    else:
        sep = 0.148

    mycurvelb.location.x = (mp.width / 2) - sep
    mycurvelb.location.y = ((mp.depth / 2) * cos(radians(mp.angle))) + 0.001
    mycurvelb.location.z = 0

    mycurvecb.location.x = 0
    mycurvecb.location.y = ((mp.depth / 2) * cos(radians(mp.angle))) + 0.001
    mycurvecb.location.z = 0

    mycurverb.location.x = -(mp.width / 2) + sep
    mycurverb.location.y = ((mp.depth / 2) * cos(radians(mp.angle))) + 0.001
    mycurverb.location.z = 0

    if mp.crt_mat and bpy.context.scene.render.engine == 'CYCLES':
        set_material(mycurvelb, mat)
        set_material(mycurvecb, mat)
        set_material(mycurverb, mat)

    # ------------------
    # Bottom
    # ------------------
    mybase = create_venetian_base("Venetian_base", mp.width + 0.002, mp.depth + 0.002, -0.006)
    mybase.parent = myslats
    mybase.location.x = 0
    mybase.location.y = 0
    mybase.location.z = mypoints[len(mypoints) - 1]
    mybase.rotation_euler = (radians(angleused), 0, 0)

    # materials
    if mp.crt_mat and bpy.context.scene.render.engine == 'CYCLES':
        set_material(mybase, plastic)
    # ------------------
    # Stick
    # ------------------
    mystick = get_venetian_stick("Venetian_stick", mp.height * 0.6)
    mystick.parent = mainobject
    mystick.location.x = -mp.width / 2 + 0.03
    mystick.location.y = -mp.depth / 2 - 0.003
    mystick.location.z = -0.03
    # materials
    if mp.crt_mat and bpy.context.scene.render.engine == 'CYCLES':
        matstick = create_diffuse_material("Stick_material", False, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.04)
        set_material(mybase, matstick)

    # ------------------
    # Strings up/down
    # ------------------
    mystring = get_venetian_strings("Venetian_updown", mp.height * 0.75)
    mystring.parent = mainobject
    mystring.location.x = mp.width / 2 - 0.03
    mystring.location.y = -mp.depth / 2 - 0.003
    mystring.location.z = -0.03

    if mp.crt_mat and bpy.context.scene.render.engine == 'CYCLES':
        set_material(mystring, mat)
    # deactivate others
    for o in bpy.data.objects:
        if o.select is True and o.name != mainobject.name:
            o.select = False

    return


# ------------------------------------------------------------------
# Define property group class to create or modify
# ------------------------------------------------------------------
class ObjectProperties(PropertyGroup):
    width = FloatProperty(
            name='Width',
            min=0.30, max=4, default=1, precision=3,
            description='Total width', update=update_object,
            )
    height = FloatProperty(
            name='Height',
            min=0.20, max=10, default=1.7, precision=3,
            description='Total height',
            update=update_object,
            )
    depth = FloatProperty(
            name='Slat depth', min=0.02, max=0.30, default=0.04,
            precision=3,
            description='Slat depth', update=update_object,
            )
    angle = FloatProperty(
            name='Angle', min=0, max=85, default=0, precision=1,
            description='Angle of the slats', update=update_object,
            )
    ratio = IntProperty(
            name='Extend', min=0, max=100, default=100,
            description='% of extension (100 full extend)', update=update_object,
            )

    # Materials
    crt_mat = BoolProperty(
            name="Create default Cycles materials",
            description="Create default materials for Cycles render",
            default=True, update=update_object,
            )
    objcol = FloatVectorProperty(
            name="Color",
            description="Color for material",
            default=(0.616, 0.435, 1.0, 1.0),
            min=0.1, max=1,
            subtype='COLOR',
            size=4, update=update_object,
            )

# Register
bpy.utils.register_class(ObjectProperties)
Object.VenetianObjectGenerator = CollectionProperty(type=ObjectProperties)


# ------------------------------------------------------------------
# Define panel class to modify object
# ------------------------------------------------------------------
class AchmVenetianObjectgeneratorpanel(Panel):
    bl_idname = "OBJECT_PT_venetian_generator"
    bl_label = "Venetian"
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
        if 'VenetianObjectGenerator' not in o:
            return False
        else:
            return True

# -----------------------------------------------------
    # Draw (create UI interface)
    # -----------------------------------------------------
    def draw(self, context):
        o = context.object
        # noinspection PyBroadException
        try:
            if 'VenetianObjectGenerator' not in o:
                return
        except:
            return

        layout = self.layout
        if bpy.context.mode == 'EDIT_MESH':
            layout.label('Warning: Operator does not work in edit mode.', icon='ERROR')
        else:
            myobjdat = o.VenetianObjectGenerator[0]
            space = bpy.context.space_data
            if not space.local_view:
                # Imperial units warning
                if bpy.context.scene.unit_settings.system == "IMPERIAL":
                    row = layout.row()
                    row.label("Warning: Imperial units not supported", icon='COLOR_RED')

            box = layout.box()
            row = box.row()
            row.prop(myobjdat, 'width')
            row.prop(myobjdat, 'height')
            row.prop(myobjdat, 'depth')
            row = box.row()
            row.prop(myobjdat, 'angle', slider=True)
            row.prop(myobjdat, 'ratio', slider=True)

            box = layout.box()
            if not context.scene.render.engine == 'CYCLES':
                box.enabled = False
            box.prop(myobjdat, 'crt_mat')
            if myobjdat.crt_mat:
                row = box.row()
                row.prop(myobjdat, 'objcol')
            else:
                row = layout.row()
                row.label("Warning: Operator does not work in local view mode", icon='ERROR')


# ------------------------------------------------------------------------------
# Create rectangular base
#
# x: size x axis
# y: size y axis
# z: size z axis
# ------------------------------------------------------------------------------
def create_venetian_top(mymesh, x, y, z):
    myvertex = [(-x / 2, -y / 2, 0.0),
                (-x / 2, y / 2, 0.0),
                (x / 2, y / 2, 0.0),
                (x / 2, -y / 2, 0.0),
                (-x / 2, -y / 2, z),
                (-x / 2, y / 2, z),
                (x / 2, y / 2, z),
                (x / 2, -y / 2, z)]

    myfaces = [(0, 1, 2, 3), (0, 1, 5, 4), (1, 2, 6, 5), (2, 6, 7, 3), (5, 6, 7, 4), (0, 4, 7, 3)]

    mymesh.from_pydata(myvertex, [], myfaces)
    mymesh.update(calc_edges=True)

    return


# ------------------------------------------------------------
# Create venetian slats
#
# width: total width of the slat
# depth: depth of the slat
# height: Total height (extended)
# ratio: factor of extension 0-collapsed 100-extended
# angle: angle
# ------------------------------------------------------------
def create_slat_mesh(objname, width, depth, height, angle, ratio):
    # Vertex
    v = 0
    gap = 0.001
    angleused = 0
    myvertex = []
    myfaces = []
    mypoints = []
    # Calculate total slats
    separation = (depth * 0.75)  # posZ is % of depth
    numslats = int(height / separation)
    collapsedslats = numslats - int((height * ((100 - ratio) / 100)) / separation)
    # --------------------------------
    # Generate slats
    # --------------------------------
    posz = 0
    for x in range(numslats):
        if x < collapsedslats:
            angleused = angle
        elif x == collapsedslats:
            angleused = angle / 2
        else:
            angleused = 0
        # if the slat is collapsed, the angle is 0

        mydata = get_slat_data(v, angleused, width, depth, posz)
        mypoints.extend([posz])  # saves all Z points
        myvertex.extend(mydata[0])
        myfaces.extend(mydata[1])
        v = mydata[2]
        if x < collapsedslats:
            posz -= separation
        else:
            posz -= gap
            # Transition to horizontal
        if angleused == angle / 2:
            sinheight = sin(radians(angle / 2)) * depth / 2
            posz -= sinheight

    mesh = bpy.data.meshes.new(objname)
    myobject = bpy.data.objects.new(objname, mesh)

    myobject.location = bpy.context.scene.cursor_location
    bpy.context.scene.objects.link(myobject)

    mesh.from_pydata(myvertex, [], myfaces)
    mesh.update(calc_edges=True)

    return myobject, mypoints, angleused


# ------------------------------------------------------------
# Generate slat data for venetian slat mesh
#
# v: last vertex index
# angle: angle
# width: total width of the slat
# depth: depth of the slat
# posZ: position in Z axis
# ------------------------------------------------------------
def get_slat_data(v, angle, width, depth, posz):
    # ------------------------------------
    # Mesh data
    # ------------------------------------
    maxx = width / 2
    miny = -depth / 2
    maxy = depth / 2
    maxz = 0.0028
    gap = 0.0025
    radio = 0.00195
    sinv = sin(atan(maxz / (maxy - gap)))
    cos_value = cos(radians(angle))
    sin_value = sin(radians(angle))

    if width < 0.60:
        sep = 0.06
    else:
        sep = 0.15

    sep2 = sep - 0.005

    # Vertex
    myvertex = []

    myvertex.extend(
        [(maxx - 0.0017, (miny + 0.00195) * cos_value, posz + (-maxz + (radio * sinv)) + ((miny + 0.00195) * sin_value)),
         (maxx - 0.0017, (maxy - 0.00195) * cos_value, posz + (-maxz + (radio * sinv)) + ((maxy - 0.00195) * sin_value)),
         (maxx - 0.0045, miny * cos_value, posz + -maxz + (miny * sin_value)),
         (maxx - 0.0045, maxy * cos_value, posz + -maxz + (maxy * sin_value)),
         (maxx, -gap * cos_value, posz + (-gap * sin_value)),
         (maxx, gap * cos_value, posz + (gap * sin_value)),
         (maxx - 0.0045, -gap * cos_value, posz + (-gap * sin_value)),
         (maxx - 0.0045, gap * cos_value, posz + (gap * sin_value)),
         (0.001172, miny * cos_value, posz + -maxz + (miny * sin_value)),
         (0.001172, maxy * cos_value, posz + -maxz + (maxy * sin_value)),
         (0.001172, -gap * cos_value, posz + (-gap * sin_value)),
         (0.001172, gap * cos_value, posz + (gap * sin_value)),
         (maxx - sep, miny * cos_value, posz + -maxz + (miny * sin_value)),
         (maxx - sep, maxy * cos_value, posz + -maxz + (maxy * sin_value)),
         (maxx - sep, -gap * cos_value, posz + (-gap * sin_value)),
         (maxx - sep, gap * cos_value, posz + (gap * sin_value)),
         (maxx - sep2, miny * cos_value, posz + -maxz + (miny * sin_value)),
         (maxx - sep2, maxy * cos_value, posz + -maxz + (maxy * sin_value)),
         (maxx - sep2, -gap * cos_value, posz + (-gap * sin_value)),
         (maxx - sep2, gap * cos_value, posz + (gap * sin_value))])

    myvertex.extend(
        [(-maxx + 0.0017, (miny + 0.00195) * cos_value, posz + (-maxz + (radio * sinv)) + ((miny + 0.00195) * sin_value)),
         (-maxx + 0.0017, (maxy - 0.00195) * cos_value, posz + (-maxz + (radio * sinv)) + ((maxy - 0.00195) * sin_value)),
         (-maxx + 0.0045, miny * cos_value, posz + -maxz + (miny * sin_value)),
         (-maxx + 0.0045, maxy * cos_value, posz + -maxz + (maxy * sin_value)),
         (-maxx, -gap * cos_value, posz + (-gap * sin_value)),
         (-maxx, gap * cos_value, posz + (gap * sin_value)),
         (-maxx + 0.0045, -gap * cos_value, posz + (-gap * sin_value)),
         (-maxx + 0.0045, gap * cos_value, posz + (gap * sin_value)),
         (-0.001172, miny * cos_value, posz + -maxz + (miny * sin_value)),
         (-0.001172, maxy * cos_value, posz + -maxz + (maxy * sin_value)),
         (-0.001172, -gap * cos_value, posz + (-gap * sin_value)),
         (-0.001172, gap * cos_value, posz + (gap * sin_value)),
         (-maxx + sep, miny * cos_value, posz + -maxz + (miny * sin_value)),
         (-maxx + sep, maxy * cos_value, posz + -maxz + (maxy * sin_value)),
         (-maxx + sep, -gap * cos_value, posz + (-gap * sin_value)),
         (-maxx + sep, gap * cos_value, posz + (gap * sin_value)),
         (-maxx + sep2, miny * cos_value, posz + -maxz + (miny * sin_value)),
         (-maxx + sep2, maxy * cos_value, posz + -maxz + (maxy * sin_value)),
         (-maxx + sep2, -gap * cos_value, posz + (-gap * sin_value)),
         (-maxx + sep2, gap * cos_value, posz + (gap * sin_value))])

    # Faces
    myfaces = [(v + 7, v + 5, v + 1, v + 3), (v + 19, v + 7, v + 3, v + 17), (v + 2, v + 0, v + 4, v + 6),
               (v + 6, v + 4, v + 5, v + 7), (v + 16, v + 2, v + 6, v + 18),
               (v + 18, v + 6, v + 7, v + 19), (v + 11, v + 15, v + 13, v + 9), (v + 8, v + 12, v + 14, v + 10),
               (v + 10, v + 14, v + 15, v + 11), (v + 15, v + 19, v + 17, v + 13),
               (v + 12, v + 16, v + 18, v + 14), (v + 39, v + 35, v + 33, v + 37), (v + 34, v + 38, v + 36, v + 32),
               (v + 27, v + 25, v + 21, v + 23), (v + 27, v + 26, v + 24, v + 25),
               (v + 24, v + 26, v + 22, v + 20), (v + 39, v + 37, v + 23, v + 27), (v + 39, v + 27, v + 26, v + 38),
               (v + 38, v + 26, v + 22, v + 36), (v + 30, v + 34, v + 32, v + 28),
               (v + 34, v + 30, v + 31, v + 35), (v + 35, v + 31, v + 29, v + 33), (v + 11, v + 9, v + 29, v + 31),
               (v + 8, v + 10, v + 30, v + 28)]

    v += len(myvertex)

    return myvertex, myfaces, v


# ------------------------------------------------------------------------------
# Create rectangular base
#
# objName: Object name
# x: size x axis
# y: size y axis
# z: size z axis
# ------------------------------------------------------------------------------
def create_venetian_base(objname, x, y, z):
    myvertex = [(-x / 2, -y / 2, 0.0),
                (-x / 2, y / 2, 0.0),
                (x / 2, y / 2, 0.0),
                (x / 2, -y / 2, 0.0),
                (-x / 2, -y / 2, z),
                (-x / 2, y / 2, z),
                (x / 2, y / 2, z),
                (x / 2, -y / 2, z)]

    myfaces = [(0, 1, 2, 3), (0, 1, 5, 4), (1, 2, 6, 5), (2, 6, 7, 3), (5, 6, 7, 4), (0, 4, 7, 3)]

    mesh = bpy.data.meshes.new(objname)
    myobject = bpy.data.objects.new(objname, mesh)

    myobject.location = bpy.context.scene.cursor_location
    bpy.context.scene.objects.link(myobject)

    mesh.from_pydata(myvertex, [], myfaces)
    mesh.update(calc_edges=True)

    return myobject


# ------------------------------------------------------------
# Generate stick for venetian
#
# objName: Object name
# height: height
# ------------------------------------------------------------
def get_venetian_stick(objname, height):
    # ------------------------------------
    # Mesh data
    # ------------------------------------
    minx = -0.005686083808541298
    maxx = 0.005686083808541298
    # minZ = -0.2236524224281311
    minz = -height
    maxz = 0

    # Vertex
    myvertex = [(minx + 0.0031912513077259064, -0.012805024161934853, maxz - 0.02722930908203125),
                (minx + 0.0030460257548838854, -0.007894348353147507, maxz - 0.008386015892028809),
                (minx + 0.003936204360798001, -0.014603500254452229, maxz - 0.02722930908203125),
                (minx + 0.003812092007137835, -0.006685533560812473, maxz - 0.009785711765289307),
                (maxx - 0.005637487949570641, -0.01534845307469368, maxz - 0.02722930908203125),
                (minx + 0.005661540681103361, -0.006184821017086506, maxz - 0.010365545749664307),
                (maxx - 0.0038390127010643482, -0.014603500254452229, maxz - 0.02722930908203125),
                (maxx - 0.0038611787604168057, -0.006685533560812473, maxz - 0.009785711765289307),
                (maxx - 0.0030940594151616096, -0.012805024161934853, maxz - 0.02722930908203125),
                (maxx - 0.0030951115768402815, -0.007894348353147507, maxz - 0.008386015892028809),
                (maxx - 0.0038390120025724173, -0.011006549000740051, maxz - 0.02722930908203125),
                (maxx - 0.0038611781783401966, -0.009103144519031048, maxz - 0.0069863200187683105),
                (maxx - 0.005637487425701693, -0.010261595249176025, maxz - 0.02722930908203125),
                (minx + 0.005661541004883475, -0.00960385799407959, maxz - 0.0064064860343933105),
                (minx + 0.003936204593628645, -0.011006549000740051, maxz - 0.02722930908203125),
                (minx + 0.003812092822045088, -0.009103145450353622, maxz - 0.0069863200187683105),
                (minx + 0.004708557506091893, -0.0009992714039981365, maxz - 0.002431390807032585),
                (minx + 0.004987679829355329, -0.0009992715204134583, maxz - 0.0031052385456860065),
                (minx + 0.005661541195877362, -0.0009992715204134583, maxz - 0.0033843691926449537),
                (maxx - 0.00503676530206576, -0.0009992715204134583, maxz - 0.0031052385456860065),
                (maxx - 0.004757642629556358, -0.0009992716368287802, maxz - 0.002431390807032585),
                (maxx - 0.005036764952819794, -0.0009992715204134583, maxz - 0.0017575474921613932),
                (minx + 0.005661541457811836, -0.0009992715204134583, maxz - 0.0014784171944484115),
                (minx + 0.004987680295016617, -0.0009992715204134583, maxz - 0.0017575474921613932),
                (minx + 0.0038341645849868655, -0.010904508642852306, maxz - 0.015928268432617188),
                (maxx - 0.005637487367494032, -0.01011728961020708, maxz - 0.015928268432617188),
                (maxx - 0.003736971877515316, -0.010904508642852306, maxz - 0.015928268432617188),
                (maxx - 0.002949752612039447, -0.012805024161934853, maxz - 0.015928268432617188),
                (maxx - 0.0037369721103459597, -0.014705540612339973, maxz - 0.015928268432617188),
                (maxx - 0.00563748789136298, -0.015492759644985199, maxz - 0.015928268432617188),
                (minx + 0.0038341644685715437, -0.014705540612339973, maxz - 0.015928268432617188),
                (minx + 0.003046944970265031, -0.012805024161934853, maxz - 0.015928268432617188),
                (minx + 0.0043586865067481995, -0.012638782151043415, maxz - 0.013130486011505127),
                (minx + 0.004740283475257456, -0.0120366420596838, maxz - 0.013827741146087646),
                (minx + 0.0056615397916175425, -0.011787224560976028, maxz - 0.014116525650024414),
                (maxx - 0.004789371509104967, -0.0120366420596838, maxz - 0.013827741146087646),
                (maxx - 0.0044077744241803885, -0.012638782151043415, maxz - 0.013130486011505127),
                (maxx - 0.0047893712762743235, -0.013240913860499859, maxz - 0.012433230876922607),
                (minx + 0.005661539897118928, -0.01349033135920763, maxz - 0.01214444637298584),
                (minx + 0.004740283824503422, -0.013240914791822433, maxz - 0.012433230876922607),
                (minx + 0.005661537383275572, -0.012638770043849945, maxz - 0.017504926770925522),
                (maxx - 0.0039202586049214005, -0.010174507275223732, maxz - 0.015622403472661972),
                (maxx - 0.0028137227054685354, -0.013580016791820526, maxz - 0.015622403472661972),
                (minx + 0.005661537154082907, -0.015684761106967926, maxz - 0.015622403472661972),
                (minx + 0.0027646280359476805, -0.013580016791820526, maxz - 0.015622403472661972),
                (minx + 0.003871166962198913, -0.010174507275223732, maxz - 0.015622403472661972),
                (maxx - 0.0028137224726378918, -0.011697524227201939, maxz - 0.01257637981325388),
                (maxx - 0.003920258954167366, -0.015103034675121307, maxz - 0.01257637981325388),
                (minx + 0.0038711666129529476, -0.015103034675121307, maxz - 0.01257637981325388),
                (minx + 0.002764628268778324, -0.011697524227201939, maxz - 0.01257637981325388),
                (minx + 0.0056615376142872265, -0.00959277804940939, maxz - 0.01257637981325388),
                (minx + 0.005661537383275572, -0.012638770043849945, maxz - 0.010693902149796486),
                (maxx - 0.004007883137091994, -0.013192017562687397, maxz - 0.016996320337057114),
                (maxx - 0.004658283665776253, -0.011190323159098625, maxz - 0.016996320337057114),
                (maxx - 0.002955520059913397, -0.011743564158678055, maxz - 0.015889829024672508),
                (minx + 0.00566153760337329, -0.009741866029798985, maxz - 0.015889829024672508),
                (minx + 0.0046091912081465125, -0.011190323159098625, maxz - 0.016996320337057114),
                (minx + 0.005661537248670356, -0.014429156668484211, maxz - 0.016996320337057114),
                (maxx - 0.004007878364063799, -0.014982416294515133, maxz - 0.015889829024672508),
                (minx + 0.003958789980970323, -0.013192017562687397, maxz - 0.016996320337057114),
                (minx + 0.00395878404378891, -0.014982416294515133, maxz - 0.015889829024672508),
                (minx + 0.002906427951529622, -0.011743564158678055, maxz - 0.015889829024672508),
                (maxx - 0.004658278776332736, -0.009399918839335442, maxz - 0.014099414460361004),
                (minx + 0.004609186435118318, -0.009399918839335442, maxz - 0.014099414460361004),
                (maxx - 0.0023051041644066572, -0.012638770043849945, maxz - 0.014099414460361004),
                (maxx - 0.0029555035289376974, -0.010637051425874233, maxz - 0.014099414460361004),
                (maxx - 0.004658279241994023, -0.015877623111009598, maxz - 0.014099414460361004),
                (maxx - 0.0029555039945989847, -0.014640489593148232, maxz - 0.014099414460361004),
                (minx + 0.0029064100235700607, -0.014640489593148232, maxz - 0.014099414460361004),
                (minx + 0.00460918596945703, -0.015877623111009598, maxz - 0.014099414460361004),
                (minx + 0.002906410489231348, -0.010637051425874233, maxz - 0.014099414460361004),
                (minx + 0.0022560113575309515, -0.012638770043849945, maxz - 0.014099414460361004),
                (maxx - 0.004007878014817834, -0.010295123793184757, maxz - 0.012308954261243343),
                (maxx - 0.002955520059913397, -0.013533977791666985, maxz - 0.012308954261243343),
                (minx + 0.005661537164996844, -0.015535674057900906, maxz - 0.012308954261243343),
                (minx + 0.0029064277186989784, -0.013533977791666985, maxz - 0.012308954261243343),
                (minx + 0.003958784393034875, -0.010295123793184757, maxz - 0.012308954261243343),
                (maxx - 0.004007883137091994, -0.012085524387657642, maxz - 0.011202462017536163),
                (minx + 0.0056615375196997775, -0.01084838341921568, maxz - 0.011202462017536163),
                (maxx - 0.004658283898606896, -0.01408721785992384, maxz - 0.011202462017536163),
                (minx + 0.004609190975315869, -0.01408721785992384, maxz - 0.011202462017536163),
                (minx + 0.003958789980970323, -0.012085524387657642, maxz - 0.011202462017536163),
                (minx + 0.003936204593628645, -0.011006549000740051, minz),
                (maxx - 0.005637487425701693, -0.010261595249176025, minz),
                (maxx - 0.0038390120025724173, -0.011006549000740051, minz),
                (maxx - 0.0030940594151616096, -0.012805024161934853, minz),
                (maxx - 0.0038390127010643482, -0.014603500254452229, minz),
                (maxx - 0.005637487949570641, -0.01534845307469368, minz),
                (minx + 0.003936204360798001, -0.014603500254452229, minz),
                (minx + 0.0031912513077259064, -0.012805024161934853, minz),
                (minx, -0.001379312016069889, maxz - 0.005334946792572737),
                (minx, -4.6566128730773926e-09, maxz - 0.005334946792572737),
                (maxx, -4.6566128730773926e-09, maxz - 0.005334946792572737),
                (maxx, -0.001379312016069889, maxz - 0.005334946792572737),
                (minx, -0.001379312016069889, maxz - 1.5133991837501526e-08),
                (minx, -4.6566128730773926e-09, maxz - 1.5133991837501526e-08),
                (maxx, -4.6566128730773926e-09, maxz - 1.5133991837501526e-08),
                (maxx, -0.001379312016069889, maxz - 1.5133991837501526e-08),
                (minx + 0.0009754756465554237, -0.001379312016069889, maxz - 0.00447732862085104),
                (minx + 0.0009754756465554237, -0.001379312016069889, maxz - 0.0008576327236369252),
                (maxx - 0.0009754756465554237, -0.001379312016069889, maxz - 0.0008576327236369252),
                (maxx - 0.0009754756465554237, -0.001379312016069889, maxz - 0.00447732862085104),
                (minx + 0.0009754756465554237, -0.0007799165323376656, maxz - 0.00447732862085104),
                (minx + 0.0009754756465554237, -0.0007799165323376656, maxz - 0.0008576327236369252),
                (maxx - 0.0009754756465554237, -0.0007799165323376656, maxz - 0.0008576327236369252),
                (maxx - 0.0009754756465554237, -0.0007799165323376656, maxz - 0.00447732862085104)]

    # Faces
    myfaces = [(1, 15, 23, 16), (17, 16, 23, 22, 21, 20, 19, 18),
               (15, 13, 22, 23), (13, 11, 21, 22), (11, 9, 20, 21),
               (9, 7, 19, 20), (7, 5, 18, 19), (5, 3, 17, 18), (3, 1, 16, 17), (0, 31, 24, 14),
               (14, 24, 25, 12), (12, 25, 26, 10), (10, 26, 27, 8), (8, 27, 28, 6), (6, 28, 29, 4),
               (2, 30, 31, 0), (4, 29, 30, 2), (15, 1, 32, 39), (13, 15, 39, 38), (11, 13, 38, 37),
               (9, 11, 37, 36), (7, 9, 36, 35), (5, 7, 35, 34), (3, 5, 34, 33), (1, 3, 33, 32),
               (40, 53, 52), (41, 53, 55), (40, 52, 57), (40, 57, 59), (40, 59, 56),
               (41, 55, 62), (42, 54, 64), (43, 58, 66), (44, 60, 68), (45, 61, 70),
               (41, 62, 65), (42, 64, 67), (43, 66, 69), (44, 68, 71), (45, 70, 63),
               (46, 72, 77), (47, 73, 79), (48, 74, 80), (49, 75, 81), (50, 76, 78),
               (52, 54, 42), (52, 53, 54), (53, 41, 54), (55, 56, 45), (55, 53, 56),
               (53, 40, 56), (57, 58, 43), (57, 52, 58), (52, 42, 58), (59, 60, 44),
               (59, 57, 60), (57, 43, 60), (56, 61, 45), (56, 59, 61), (59, 44, 61),
               (62, 63, 50), (62, 55, 63), (55, 45, 63), (64, 65, 46), (64, 54, 65),
               (54, 41, 65), (66, 67, 47), (66, 58, 67), (58, 42, 67), (68, 69, 48),
               (68, 60, 69), (60, 43, 69), (70, 71, 49), (70, 61, 71), (61, 44, 71),
               (65, 72, 46), (65, 62, 72), (62, 50, 72), (67, 73, 47), (67, 64, 73),
               (64, 46, 73), (69, 74, 48), (69, 66, 74), (66, 47, 74), (71, 75, 49),
               (71, 68, 75), (68, 48, 75), (63, 76, 50), (63, 70, 76), (70, 49, 76),
               (77, 78, 51), (77, 72, 78), (72, 50, 78), (79, 77, 51), (79, 73, 77),
               (73, 46, 77), (80, 79, 51), (80, 74, 79), (74, 47, 79), (81, 80, 51),
               (81, 75, 80), (75, 48, 80), (78, 81, 51), (78, 76, 81), (76, 49, 81),
               (87, 4, 2, 88), (88, 2, 0, 89), (86, 6, 4, 87), (85, 8, 6, 86), (84, 10, 8, 85),
               (83, 12, 10, 84), (82, 14, 12, 83), (89, 0, 14, 82), (94, 95, 91, 90), (95, 96, 92, 91),
               (96, 97, 93, 92), (98, 101, 105, 102), (90, 91, 92, 93), (97, 96, 95, 94), (98, 99, 94, 90),
               (100, 101, 93, 97), (99, 100, 97, 94), (101, 98, 90, 93), (104, 103, 102, 105), (100, 99, 103, 104),
               (101, 100, 104, 105), (99, 98, 102, 103)]

    mesh = bpy.data.meshes.new(objname)
    myobject = bpy.data.objects.new(objname, mesh)

    myobject.location = bpy.context.scene.cursor_location
    bpy.context.scene.objects.link(myobject)

    mesh.from_pydata(myvertex, [], myfaces)
    mesh.update(calc_edges=True)

    return myobject


# ------------------------------------------------------------
# Generate strings for venetian
#
# objName: Object name
# height: height
# ------------------------------------------------------------
def get_venetian_strings(objname, height):
    # ------------------------------------
    # Mesh data
    # ------------------------------------
    minx = -0.006897219456732273
    maxx = 0.006897197104990482
    maxy = 1.57160684466362e-08
    minz = -height
    maxz = 0

    # Vertex
    myvertex = [(maxx - 0.004941887455061078, -0.005041633266955614, minz + 3.1925737857818604e-05),
                (maxx - 0.005070153623819351, -0.004731120076030493, minz + 4.9524009227752686e-05),
                (maxx - 0.005380495102144778, -0.004602177534252405, minz + 5.389750003814697e-05),
                (maxx - 0.005691118887625635, -0.00473033869639039, minz + 4.2498111724853516e-05),
                (maxx - 0.005820065038278699, -0.005040528252720833, minz + 2.1979212760925293e-05),
                (maxx - 0.005691798985935748, -0.005351040977984667, minz + 4.380941390991211e-06),
                (maxx - 0.005381457391194999, -0.0054799835197627544, minz),
                (maxx - 0.005070833722129464, -0.005351822357624769, minz + 1.1406838893890381e-05),
                (maxx - 0.005004765349440277, -0.005394590552896261, minz + 0.029493853449821472),
                (maxx - 0.005133012658916414, -0.005083992145955563, minz + 0.02950144186615944),
                (maxx - 0.005443348782137036, -0.004955026786774397, minz + 0.029502809047698975),
                (maxx - 0.005753983394242823, -0.005083239171653986, minz + 0.02949715033173561),
                (maxx - 0.0058829509653151035, -0.005393524654209614, minz + 0.02948778122663498),
                (maxx - 0.005754703655838966, -0.005704122595489025, minz + 0.029480192810297012),
                (maxx - 0.005444367183372378, -0.005833088420331478, minz + 0.029478825628757477),
                (maxx - 0.005133732687681913, -0.005704876501113176, minz + 0.02948448434472084),
                (maxx - 0.005029367166571319, -0.005440863780677319, maxz - 0.029401594772934914),
                (maxx - 0.005157589330337942, -0.005130413919687271, maxz - 0.029417745769023895),
                (maxx - 0.005467916373163462, -0.005001509562134743, maxz - 0.029425399377942085),
                (maxx - 0.005778562976047397, -0.0051296609453856945, maxz - 0.029420070350170135),
                (maxx - 0.005907556158490479, -0.00543979974463582, maxz - 0.029404880478978157),
                (maxx - 0.005779333529062569, -0.005750249605625868, maxz - 0.029388729482889175),
                (maxx - 0.005469006602652371, -0.005879154894500971, maxz - 0.029381075873970985),
                (maxx - 0.005158359999768436, -0.0057510025799274445, maxz - 0.029386404901742935),
                (maxx - 0.00503770902287215, -0.005161902867257595, maxz - 0.015295670367777348),
                (maxx - 0.00516585458535701, -0.004854197148233652, maxz - 0.015373632311820984),
                (maxx - 0.005476148682646453, -0.00472648162394762, maxz - 0.01540757529437542),
                (maxx - 0.0057868254370987415, -0.004853568039834499, maxz - 0.015377615578472614),
                (maxx - 0.005915894289501011, -0.005161012522876263, maxz - 0.015301303938031197),
                (maxx - 0.0057877483777701855, -0.005468717776238918, maxz - 0.015223342925310135),
                (maxx - 0.005477453931234777, -0.005596434231847525, maxz - 0.01518939808011055),
                (maxx - 0.005166777293197811, -0.005469346884638071, maxz - 0.01521935872733593),
                (maxx - 0.0050518905045464635, -0.004537198692560196, maxz - 0.007016266230493784),
                (maxx - 0.0051788275595754385, -0.004260011948645115, maxz - 0.007274628151208162),
                (maxx - 0.005488568218424916, -0.004146246705204248, maxz - 0.007390326354652643),
                (maxx - 0.005799670936539769, -0.004262544680386782, maxz - 0.007295588497072458),
                (maxx - 0.005929895443841815, -0.004540780559182167, maxz - 0.007045908365398645),
                (maxx - 0.0058029580395668745, -0.004817967768758535, maxz - 0.006787544582039118),
                (maxx - 0.005493217264302075, -0.004931733012199402, maxz - 0.006671848241239786),
                (maxx - 0.005182114546187222, -0.004815434105694294, maxz - 0.006766586098819971),
                (maxx - 0.005094519350677729, -0.0035386907402426004, maxz - 0.003334574867039919),
                (maxx - 0.005213141208514571, -0.0034136706963181496, maxz - 0.0038649537600576878),
                (maxx - 0.005518985213711858, -0.003371135564520955, maxz - 0.004107045475393534),
                (maxx - 0.0058328923769295216, -0.0034360019490122795, maxz - 0.003919033799320459),
                (maxx - 0.005970980157144368, -0.003570271423086524, maxz - 0.003411055076867342),
                (maxx - 0.005852358415722847, -0.003695291467010975, maxz - 0.002880676183849573),
                (maxx - 0.005546514177694917, -0.0037378265988081694, maxz - 0.002638584468513727),
                (maxx - 0.005232607247307897, -0.003672960214316845, maxz - 0.0028265961445868015),
                (maxx - 0.005944175529293716, -0.0036394987255334854, maxz - 0.003126396331936121),
                (maxx - 0.005875613423995674, -0.00560829509049654, maxz - 0.02939625270664692),
                (maxx - 0.005034194211475551, -0.005343204364180565, maxz - 0.021542727947235107),
                (maxx - 0.005612890352495015, -0.00580073706805706, minz + 0.029478538781404495),
                (maxx - 0.0052128632087260485, -0.005448057781904936, minz + 3.8817524909973145e-06),
                (maxx - 0.005320212687365711, -0.004174317233264446, maxz - 0.007358333561569452),
                (maxx - 0.005857669049873948, -0.005254213232547045, minz + 0.01586836576461792),
                (maxx - 0.005270682042464614, -0.002958775730803609, maxz - 0.0018540903693065047),
                (maxx - 0.004973854636773467, -0.004873105324804783, minz + 4.190206527709961e-05),
                (maxx - 0.005896858056075871, -0.0043898820877075195, maxz - 0.007182727102190256),
                (maxx - 0.005644757067784667, -0.004758160561323166, maxz - 0.015400669537484646),
                (maxx - 0.0050204748986288905, -0.0054572150111198425, maxz - 0.03902619704604149),
                (maxx - 0.005107744364067912, -0.004944739863276482, minz + 0.01588994264602661),
                (maxx - 0.005821454804390669, -0.004324669484049082, maxz - 0.004334418568760157),
                (maxx - 0.0058509946102276444, -0.00556210009381175, minz + 0.029483404010534286),
                (maxx - 0.004974223440513015, -0.005210080184042454, minz + 2.121180295944214e-05),
                (maxx - 0.005512951058335602, -0.004414208233356476, maxz - 0.004156060982495546),
                (maxx - 0.005657264497131109, -0.004175691865384579, maxz - 0.007369710598140955),
                (maxx - 0.005083143012598157, -0.004386562388390303, maxz - 0.007155258674174547),
                (maxx - 0.005898642586544156, -0.004691417329013348, maxz - 0.006906915921717882),
                (maxx - 0.005482866894453764, -0.005316956900060177, maxz - 0.010308705270290375),
                (maxx - 0.005200946354307234, -0.004315671045333147, maxz - 0.004297847393900156),
                (maxx - 0.005883492063730955, -0.004994065500795841, maxz - 0.015342974103987217),
                (maxx - 0.0055490892846137285, -0.004634103272110224, minz + 5.002319812774658e-05),
                (maxx - 0.005586991785094142, -0.002982017118483782, maxz - 0.0016202632104977965),
                (maxx - 0.005500998347997665, -0.0037720794789493084, maxz - 0.005249096546322107),
                (maxx - 0.0056365174241364, -0.005033436696976423, maxz - 0.029424406588077545),
                (maxx - 0.0051623902982100844, -0.0050335354171693325, maxz - 0.021582692861557007),
                (maxx - 0.005850603571161628, -0.005225026980042458, minz + 0.029492609202861786),
                (maxx - 0.005787728703580797, -0.0048720804043114185, minz + 3.269314765930176e-05),
                (maxx - 0.005192494718357921, -0.003861617762595415, maxz - 0.005070738960057497),
                (maxx - 0.005163213470950723, -0.005652572028338909, maxz - 0.021503763273358345),
                (maxx - 0.005460095009766519, -0.005895696114748716, maxz - 0.039026711136102676),
                (maxx - 0.005661572678945959, -0.004903662484139204, maxz - 0.006703841034322977),
                (maxx - 0.005856060888618231, -0.002980045508593321, maxz - 0.003230756614357233),
                (maxx - 0.005036721588112414, -0.005226014647632837, minz + 0.029498230665922165),
                (maxx - 0.005933607462793589, -0.0034987321123480797, maxz - 0.0036901147104799747),
                (maxx - 0.005068209487944841, -0.0040867808274924755, maxz - 0.004676718730479479),
                (maxx - 0.005645966622978449, -0.005564413033425808, maxz - 0.015198467299342155),
                (maxx - 0.005149454576894641, -0.005767486989498138, maxz - 0.03902563825249672),
                (maxx - 0.0050617282977327704, -0.005609281826764345, maxz - 0.029393207281827927),
                (maxx - 0.005539751029573381, -0.0029568036552518606, maxz - 0.0034645837731659412),
                (maxx - 0.005688222707249224, -0.003390622790902853, maxz - 0.00406796345487237),
                (maxx - 0.005419071996584535, -0.005693721119314432, minz + 0.0158514603972435),
                (maxx - 0.005788097972981632, -0.005209055729210377, minz + 1.1995434761047363e-05),
                (maxx - 0.005069609847851098, -0.004994889721274376, maxz - 0.015337754040956497),
                (maxx - 0.005299395183101296, -0.0050338455475866795, maxz - 0.029423145577311516),
                (maxx - 0.005729411728680134, -0.005564768332988024, minz + 0.015854761004447937),
                (maxx - 0.005889465333893895, -0.002997873816639185, maxz - 0.0019266236340627074),
                (maxx - 0.00523727759718895, -0.002940947189927101, maxz - 0.0031582233496010303),
                (maxx - 0.005883992882445455, -0.0053280252031981945, maxz - 0.015259221196174622),
                (maxx - 0.005637527909129858, -0.0058468179777264595, maxz - 0.029383329674601555),
                (maxx - 0.0060009173466823995, -0.002997057046741247, maxz - 0.0025937133468687534),
                (maxx - 0.00581300281919539, -0.0038706157356500626, maxz - 0.005107311997562647),
                (maxx - 0.005784186767414212, -0.005651846993714571, maxz - 0.02150617726147175),
                (maxx - 0.005061309668235481, -0.005272367969155312, maxz - 0.02941022254526615),
                (maxx - 0.00535176380071789, -0.0033784990664571524, maxz - 0.004038602579385042),
                (maxx - 0.005131891928613186, -0.0036102300509810448, maxz - 0.0030555170960724354),
                (maxx - 0.0059457405004650354, -0.004099506419152021, maxz - 0.004728438798338175),
                (maxx - 0.005791432806290686, -0.004595593549311161, maxz - 0.010660232976078987),
                (maxx - 0.005473870667628944, -0.005780417006462812, maxz - 0.021488623693585396),
                (maxx - 0.005108443321660161, -0.005565532948821783, minz + 0.01586039364337921),
                (maxx - 0.005042683565989137, -0.00489416578784585, maxz - 0.01050524227321148),
                (maxx - 0.0053004054352641106, -0.005847226828336716, maxz - 0.029382066801190376),
                (maxx - 0.005324521102011204, -0.004902287386357784, maxz - 0.006692463997751474),
                (maxx - 0.0053076359909027815, -0.00475850235670805, maxz - 0.015398506075143814),
                (maxx - 0.005770427291281521, -0.005766738206148148, maxz - 0.03902877867221832),
                (maxx - 0.005377276800572872, -0.0037183398380875587, maxz - 0.0026776683516800404),
                (maxx - 0.005898662726394832, -0.005456155631691217, maxz - 0.03903063386678696),
                (maxx - 0.005084927543066442, -0.004688097629696131, maxz - 0.006879445631057024),
                (maxx - 0.005037112743593752, -0.0055630882270634174, minz + 0.029489029198884964),
                (maxx - 0.0050701110158115625, -0.0053288498893380165, maxz - 0.015254000201821327),
                (maxx - 0.005418083746917546, -0.004815786611288786, minz + 0.01589323580265045),
                (maxx - 0.005308845662511885, -0.0055647543631494045, maxz - 0.015196305699646473),
                (maxx - 0.0054806380067020655, -0.0044716945849359035, maxz - 0.010715048760175705),
                (maxx - 0.005459042498841882, -0.005017674993723631, maxz - 0.03903011977672577),
                (maxx - 0.00571373593993485, -0.0037304633297026157, maxz - 0.0027070273645222187),
                (maxx - 0.005125825526192784, -0.0029417641926556826, maxz - 0.002491133753210306),
                (maxx - 0.005783363711088896, -0.005032809916883707, maxz - 0.02158510498702526),
                (maxx - 0.005121324211359024, -0.003469463437795639, maxz - 0.00361923361197114),
                (maxx - 0.005170495598576963, -0.0045953672379255295, maxz - 0.010650848969817162),
                (maxx - 0.005611946457065642, -0.004986968822777271, minz + 0.02950076386332512),
                (maxx - 0.005769683048129082, -0.005145883187651634, maxz - 0.03903118893504143),
                (maxx - 0.004979487042874098, -0.005255294498056173, minz + 0.01587633788585663),
                (maxx - 0.005172071512788534, -0.005193057470023632, maxz - 0.010363521054387093),
                (maxx - 0.005793009069748223, -0.005193284247070551, maxz - 0.010372905060648918),
                (maxx - 0.005875195027329028, -0.005271381698548794, maxz - 0.029413267970085144),
                (maxx - 0.005472706281580031, -0.004904965870082378, maxz - 0.02160024456679821),
                (maxx - 0.0052757697412744164, -0.0058011459186673164, minz + 0.029480870813131332),
                (maxx - 0.0057287130039185286, -0.004943975247442722, minz + 0.01588430255651474),
                (maxx - 0.0051487102173268795, -0.0051466329023242, maxz - 0.03902805224061012),
                (maxx - 0.005920821567997336, -0.004894486162811518, maxz - 0.0105185117572546),
                (maxx - 0.005912382970564067, -0.005342178046703339, maxz - 0.021546142175793648),
                (maxx - 0.005211971350945532, -0.004634527489542961, minz + 5.383789539337158e-05),
                (maxx - 0.005274825729429722, -0.004987378139048815, minz + 0.029503092169761658),
                (maxx - 0.005549981025978923, -0.0054476335644721985, minz + 6.705522537231445e-08),
                (maxx - 0.005011449102312326, -0.005086742807179689, minz + 0.01588405668735504),
                (maxx - 0.005249560461379588, -0.004848137032240629, minz + 0.01589324325323105),
                (maxx - 0.005586679908446968, -0.004847722128033638, minz + 0.015890181064605713),
                (maxx - 0.005825327709317207, -0.005085740704089403, minz + 0.01587667316198349),
                (maxx - 0.005825707106851041, -0.005422765389084816, minz + 0.015860632061958313),
                (maxx - 0.005587595398537815, -0.005661370232701302, minz + 0.0158514603972435),
                (maxx - 0.005250476184301078, -0.005661786068230867, minz + 0.015854522585868835),
                (maxx - 0.005011828150600195, -0.005423767026513815, minz + 0.01586802303791046),
                (maxx - 0.0050524246180430055, -0.00528864748775959, maxz - 0.03902701288461685),
                (maxx - 0.005290519911795855, -0.005050024017691612, maxz - 0.03902914375066757),
                (maxx - 0.005627641920000315, -0.005049617029726505, maxz - 0.03903084620833397),
                (maxx - 0.005866308696568012, -0.005287665408104658, maxz - 0.03903112933039665),
                (maxx - 0.005866712890565395, -0.005624723620712757, maxz - 0.039029818028211594),
                (maxx - 0.005628617363981903, -0.005863346625119448, maxz - 0.03902768716216087),
                (maxx - 0.0052914953557774425, -0.005863754078745842, maxz - 0.03902598097920418),
                (maxx - 0.005052828579209745, -0.005625705700367689, maxz - 0.03902570158243179),
                (maxx - 0.005066122743301094, -0.005175130441784859, maxz - 0.02156427875161171),
                (maxx - 0.005304188118316233, -0.004937214311212301, maxz - 0.021595504134893417),
                (maxx - 0.0056413100101053715, -0.004936820361763239, maxz - 0.021596813574433327),
                (maxx - 0.005880007753148675, -0.005174180027097464, maxz - 0.021567441523075104),
                (maxx - 0.005880454205907881, -0.005510250572115183, maxz - 0.021524591371417046),
                (maxx - 0.0056423889473080635, -0.005748167168349028, maxz - 0.021493365988135338),
                (maxx - 0.005305267055518925, -0.005748561583459377, maxz - 0.02149205468595028),
                (maxx - 0.005066569661721587, -0.005511201918125153, maxz - 0.02152142859995365),
                (maxx - 0.005074405577033758, -0.004731936380267143, maxz - 0.010583722963929176),
                (maxx - 0.005312168272212148, -0.004502579569816589, maxz - 0.01069762371480465),
                (maxx - 0.005649270839057863, -0.004502702970057726, maxz - 0.010702718049287796),
                (maxx - 0.0058882435550913215, -0.004732233472168446, maxz - 0.010596020147204399),
                (maxx - 0.005889098974876106, -0.0050567155703902245, maxz - 0.010440031066536903),
                (maxx - 0.005651336396113038, -0.005286071915179491, maxz - 0.010326128453016281),
                (maxx - 0.005314233829267323, -0.005285948980599642, maxz - 0.010321034118533134),
                (maxx - 0.0050752609968185425, -0.005056418012827635, maxz - 0.01042773388326168),
                (maxx - 0.005098042776808143, -0.003963995724916458, maxz - 0.004888410214334726),
                (maxx - 0.005333001143299043, -0.0037931459955871105, maxz - 0.005199151579290628),
                (maxx - 0.005669870879501104, -0.003798031248152256, maxz - 0.0052190073765814304),
                (maxx - 0.005911318236030638, -0.003975789062678814, maxz - 0.004936345387250185),
                (maxx - 0.005915906862355769, -0.004222291521728039, maxz - 0.004516747314482927),
                (maxx - 0.0056809482630342245, -0.004393140785396099, maxz - 0.004206005949527025),
                (maxx - 0.0053440784104168415, -0.004388255998492241, maxz - 0.0041861520148813725),
                (maxx - 0.005102631403133273, -0.004210498183965683, maxz - 0.004468812141567469),
                (maxx - 0.005148796364665031, -0.0029389490373432636, maxz - 0.002848892007023096),
                (maxx - 0.005373513558879495, -0.0029471139423549175, maxz - 0.0033773710019886494),
                (maxx - 0.005709447083063424, -0.0029683399479836226, maxz - 0.003416749183088541),
                (maxx - 0.005959811387583613, -0.002990193199366331, maxz - 0.002943959552794695),
                (maxx - 0.0059779464500024915, -0.002999872202053666, maxz - 0.0022359550930559635),
                (maxx - 0.005753229022957385, -0.0029917070642113686, maxz - 0.0017074759816750884),
                (maxx - 0.005417295498773456, -0.0029704810585826635, maxz - 0.0016680978005751967),
                (maxx - 0.0051669314270839095, -0.002948627807199955, maxz - 0.0021408875472843647),
                (minx + 0.006793352426029742, -0.005108049139380455, minz + 0.00023909658193588257),
                (minx + 0.0066346460953354836, -0.004723844584077597, minz + 0.00025669485330581665),
                (minx + 0.006250653299503028, -0.004564301110804081, minz + 0.00026106834411621094),
                (minx + 0.00586631172336638, -0.0047228774055838585, minz + 0.0002496689558029175),
                (minx + 0.005706763477064669, -0.005106681492179632, minz + 0.00022915005683898926),
                (minx + 0.005865469924174249, -0.005490886978805065, minz + 0.00021155178546905518),
                (minx + 0.006249462720006704, -0.005650430452078581, minz + 0.00020717084407806396),
                (minx + 0.006633804412558675, -0.005491853225976229, minz + 0.00021857768297195435),
                (minx + 0.006715552299283445, -0.005544771905988455, minz + 0.029701028019189835),
                (minx + 0.00655686913523823, -0.005160461645573378, minz + 0.0297086164355278),
                (minx + 0.006172882916871458, -0.005000889301300049, minz + 0.029709983617067337),
                (minx + 0.005788527661934495, -0.0051595289260149, minz + 0.029704324901103973),
                (minx + 0.005628953338600695, -0.005543452687561512, minz + 0.02969495579600334),
                (minx + 0.005787636619061232, -0.005927762482315302, minz + 0.029687367379665375),
                (minx + 0.0061716228374280035, -0.006087335292249918, minz + 0.02968600019812584),
                (minx + 0.006555978092364967, -0.005928694736212492, minz + 0.029691658914089203),
                (minx + 0.006685112020932138, -0.005602027289569378, maxz - 0.0291944220662117),
                (minx + 0.006526459823362529, -0.005217899568378925, maxz - 0.029210573062300682),
                (minx + 0.0061424848972819746, -0.005058403126895428, maxz - 0.029218226671218872),
                (minx + 0.005758114974014461, -0.00521696824580431, maxz - 0.029212897643446922),
                (minx + 0.005598508752882481, -0.005600709468126297, maxz - 0.029197707772254944),
                (minx + 0.005757161183282733, -0.005984836723655462, maxz - 0.029181556776165962),
                (minx + 0.006141135876532644, -0.0061443340964615345, maxz - 0.029173903167247772),
                (minx + 0.006525505916215479, -0.005985768511891365, maxz - 0.029179232195019722),
                (minx + 0.00667479052208364, -0.005256861448287964, maxz - 0.015088497661054134),
                (minx + 0.006516232970170677, -0.0048761311918497086, maxz - 0.01516645960509777),
                (minx + 0.006132298905868083, -0.0047181048430502415, maxz - 0.015200402587652206),
                (minx + 0.005747891729697585, -0.004875352140516043, maxz - 0.015170442871749401),
                (minx + 0.005588191794231534, -0.005255760159343481, maxz - 0.015094131231307983),
                (minx + 0.005746749578975141, -0.005636490881443024, maxz - 0.015016170218586922),
                (minx + 0.006130683759693056, -0.005794517230242491, maxz - 0.014982225373387337),
                (minx + 0.006515091052278876, -0.005637269467115402, maxz - 0.015012186020612717),
                (minx + 0.006657243531662971, -0.004483901429921389, maxz - 0.0068090930581092834),
                (minx + 0.0065001812763512135, -0.004140931181609631, maxz - 0.007067454978823662),
                (minx + 0.006116931792348623, -0.004000166896730661, maxz - 0.007183153182268143),
                (minx + 0.005731997545808554, -0.004144065547734499, maxz - 0.007088415324687958),
                (minx + 0.005570868030190468, -0.004488333128392696, maxz - 0.006838735193014145),
                (minx + 0.005727930460125208, -0.004831302911043167, maxz - 0.006580371409654617),
                (minx + 0.006111179769504815, -0.004972067661583424, maxz - 0.006464675068855286),
                (minx + 0.006496114016044885, -0.004828169010579586, maxz - 0.006559412926435471),
                (minx + 0.006604497611988336, -0.0032484245020896196, maxz - 0.0031274016946554184),
                (minx + 0.006457724317442626, -0.003093734150752425, maxz - 0.0036577805876731873),
                (minx + 0.006079296523239464, -0.003041104646399617, maxz - 0.003899872303009033),
                (minx + 0.005690891877748072, -0.003121365327388048, maxz - 0.003711860626935959),
                (minx + 0.0055200329516083, -0.003287500236183405, maxz - 0.0032038819044828415),
                (minx + 0.005666806362569332, -0.003442190121859312, maxz - 0.0026735030114650726),
                (minx + 0.006045234156772494, -0.0034948198590427637, maxz - 0.0024314112961292267),
                (minx + 0.006433638744056225, -0.0034145594108849764, maxz - 0.002619422972202301),
                (minx + 0.005553199094720185, -0.003373156301677227, maxz - 0.0029192231595516205),
                (minx + 0.005638031987473369, -0.005809193942695856, maxz - 0.029189079999923706),
                (minx + 0.006679139332845807, -0.005481190048158169, maxz - 0.021335555240511894),
                (minx + 0.005963105417322367, -0.0060473051853477955, minz + 0.029685713350772858),
                (minx + 0.0064580682083033025, -0.005610927473753691, minz + 0.00021105259656906128),
                (minx + 0.006325242109596729, -0.004034899175167084, maxz - 0.007151160389184952),
                (minx + 0.005660235299728811, -0.0053710793145000935, minz + 0.016075536608695984),
                (minx + 0.00638652773341164, -0.002530882600694895, maxz - 0.001646917313337326),
                (minx + 0.006753799156285822, -0.004899526014924049, minz + 0.0002490729093551636),
                (minx + 0.005611745989881456, -0.004301622975617647, maxz - 0.006975553929805756),
                (minx + 0.005923676071688533, -0.004757302813231945, maxz - 0.015193496830761433),
                (minx + 0.006696114782243967, -0.00562225840985775, maxz - 0.038819022476673126),
                (minx + 0.006588134099729359, -0.004988160915672779, minz + 0.016097113490104675),
                (minx + 0.0057050439063459635, -0.004220933653414249, maxz - 0.004127245396375656),
                (minx + 0.005668493686243892, -0.005752034951001406, minz + 0.02969057857990265),
                (minx + 0.006753342226147652, -0.005316472612321377, minz + 0.0002283826470375061),
                (minx + 0.00608676258707419, -0.0043317219242453575, maxz - 0.003948887810111046),
                (minx + 0.005908200400881469, -0.004036600701510906, maxz - 0.0071625374257564545),
                (minx + 0.006618573679588735, -0.004297514911741018, maxz - 0.006948085501790047),
                (minx + 0.005609537824057043, -0.004674719180911779, maxz - 0.006699742749333382),
                (minx + 0.0061239866190589964, -0.005448713432997465, maxz - 0.010101532563567162),
                (minx + 0.006472813023719937, -0.004209799692034721, maxz - 0.0040906742215156555),
                (minx + 0.00562828395050019, -0.005049192346632481, maxz - 0.015135801397264004),
                (minx + 0.006042047869414091, -0.004603803623467684, minz + 0.00025719404220581055),
                (minx + 0.005995150306262076, -0.002559639746323228, maxz - 0.0014130901545286179),
                (minx + 0.006101552047766745, -0.0035372015554457903, maxz - 0.005041923373937607),
                (minx + 0.005933871027082205, -0.005097907967865467, maxz - 0.029217233881354332),
                (minx + 0.006520519149489701, -0.005098029971122742, maxz - 0.021375520154833794),
                (minx + 0.005668977275490761, -0.005334966816008091, minz + 0.02969978377223015),
                (minx + 0.005746773676946759, -0.004898258484899998, minz + 0.00023986399173736572),
                (minx + 0.006483270728494972, -0.0036479895934462547, maxz - 0.0048635657876729965),
                (minx + 0.006519501097500324, -0.005863978061825037, maxz - 0.021296590566635132),
                (minx + 0.00615216267760843, -0.006164800841361284, maxz - 0.038819536566734314),
                (minx + 0.00590286951046437, -0.004937335848808289, maxz - 0.0064966678619384766),
                (minx + 0.005662225186824799, -0.0025571994483470917, maxz - 0.0030235834419727325),
                (minx + 0.00667601206805557, -0.0053361887112259865, minz + 0.029705405235290527),
                (minx + 0.005566274747252464, -0.0031989826820790768, maxz - 0.0034829415380954742),
                (minx + 0.006637051934376359, -0.0039265891537070274, maxz - 0.004469545558094978),
                (minx + 0.005922179203480482, -0.005754896439611912, maxz - 0.014991294592618942),
                (minx + 0.006536525208503008, -0.006006165407598019, maxz - 0.03881846368312836),
                (minx + 0.006645070854574442, -0.005810413975268602, maxz - 0.029186034575104713),
                (minx + 0.006053602788597345, -0.0025284423027187586, maxz - 0.0032574106007814407),
                (minx + 0.005869895336218178, -0.0030652163550257683, maxz - 0.0038607902824878693),
                (minx + 0.0062029213877394795, -0.005914892535656691, minz + 0.016058631241321564),
                (minx + 0.005746316979639232, -0.005315205082297325, minz + 0.00021916627883911133),
                (minx + 0.006635318568442017, -0.005050212610512972, maxz - 0.015130581334233284),
                (minx + 0.006350999989081174, -0.005098414141684771, maxz - 0.029215972870588303),
                (minx + 0.005818931153044105, -0.00575533602386713, minz + 0.016061931848526),
                (minx + 0.005620893090963364, -0.002579259453341365, maxz - 0.0017194505780935287),
                (minx + 0.0064278600621037185, -0.0025088228285312653, maxz - 0.00295105017721653),
                (minx + 0.005627664038911462, -0.00546240946277976, maxz - 0.015052048489451408),
                (minx + 0.005932620842941105, -0.006104323081672192, maxz - 0.02917615696787834),
                (minx + 0.0054829909931868315, -0.0025782485026866198, maxz - 0.002386540174484253),
                (minx + 0.005715501611120999, -0.003659122856333852, maxz - 0.004900138825178146),
                (minx + 0.005751156946644187, -0.005863080266863108, maxz - 0.021299004554748535),
                (minx + 0.006645588902756572, -0.005393543280661106, maxz - 0.029203049838542938),
                (minx + 0.006286203453782946, -0.0030502157751470804, maxz - 0.0038314294070005417),
                (minx + 0.00655825569992885, -0.0033369415905326605, maxz - 0.002848343923687935),
                (minx + 0.005551262525841594, -0.003942334558814764, maxz - 0.004521265625953674),
                (minx + 0.0057421906385570765, -0.004556154832243919, maxz - 0.010453060269355774),
                (minx + 0.00613511772826314, -0.006022163201123476, maxz - 0.021281450986862183),
                (minx + 0.0065872694831341505, -0.005756282713264227, minz + 0.016067564487457275),
                (minx + 0.006668635527603328, -0.0049255844205617905, maxz - 0.010298069566488266),
                (minx + 0.006349750037770718, -0.006104828789830208, maxz - 0.029174894094467163),
                (minx + 0.006319911277387291, -0.0049356333911418915, maxz - 0.006485290825366974),
                (minx + 0.0063408034620806575, -0.004757725168019533, maxz - 0.015191333368420601),
                (minx + 0.00576818163972348, -0.006005238275974989, maxz - 0.03882160410284996),
                (minx + 0.0062546354020014405, -0.0034707081504166126, maxz - 0.00247049517929554),
                (minx + 0.00560951279476285, -0.005620947107672691, maxz - 0.038823459297418594),
                (minx + 0.006616365630179644, -0.004670611582696438, maxz - 0.0066722724586725235),
                (minx + 0.0066755281295627356, -0.005753257777541876, minz + 0.029696203768253326),
                (minx + 0.006634698482230306, -0.005463429726660252, maxz - 0.015046827495098114),
                (minx + 0.006204143981449306, -0.004828604869544506, minz + 0.016100406646728516),
                (minx + 0.006339306535664946, -0.005755319260060787, maxz - 0.01498913299292326),
                (minx + 0.006126744614448398, -0.004402851220220327, maxz - 0.010507876053452492),
                (minx + 0.006153465015813708, -0.005078405141830444, maxz - 0.03882294520735741),
                (minx + 0.005838327226229012, -0.003485708963125944, maxz - 0.002499854192137718),
                (minx + 0.00656576210167259, -0.0025098335463553667, maxz - 0.0022839605808258057),
                (minx + 0.005752174882218242, -0.005097132176160812, maxz - 0.021377932280302048),
                (minx + 0.006571331556187943, -0.003162768203765154, maxz - 0.0034120604395866394),
                (minx + 0.006510490493383259, -0.00455587450414896, maxz - 0.010443676263093948),
                (minx + 0.005964273179415613, -0.005040412303060293, minz + 0.02970793843269348),
                (minx + 0.005769102484919131, -0.005237040109932423, maxz - 0.038824014365673065),
                (minx + 0.006746829953044653, -0.005372417625039816, minz + 0.016083508729934692),
                (minx + 0.0065085404785349965, -0.0052954102866351604, maxz - 0.01015634834766388),
                (minx + 0.005740240449085832, -0.005295691080391407, maxz - 0.010165732353925705),
                (minx + 0.0056385500356554985, -0.005392322316765785, maxz - 0.02920609526336193),
                (minx + 0.006136558135040104, -0.004938947968184948, maxz - 0.021393071860074997),
                (minx + 0.006380232633091509, -0.006047811824828386, minz + 0.029688045382499695),
                (minx + 0.005819795769639313, -0.0049872142262756824, minz + 0.016091473400592804),
                (minx + 0.006537446053698659, -0.00523796770721674, maxz - 0.03882087767124176),
                (minx + 0.005582095473073423, -0.004925980698317289, maxz - 0.010311339050531387),
                (minx + 0.005592536414042115, -0.005479920189827681, maxz - 0.021338969469070435),
                (minx + 0.0064591713598929346, -0.00460432842373848, minz + 0.00026100873947143555),
                (minx + 0.006381400395184755, -0.005040918942540884, minz + 0.02971026673913002),
                (minx + 0.006040944659616798, -0.005610402673482895, minz + 0.00020723789930343628),
                (minx + 0.006707282620482147, -0.005163864698261023, minz + 0.016091227531433105),
                (minx + 0.006412662158254534, -0.004868632182478905, minz + 0.016100414097309113),
                (minx + 0.005995536455884576, -0.004868118558079004, minz + 0.016097351908683777),
                (minx + 0.0057002517860382795, -0.005162625107914209, minz + 0.016083844006061554),
                (minx + 0.005699782632291317, -0.005579632706940174, minz + 0.016067802906036377),
                (minx + 0.0059944032109342515, -0.005874865222722292, minz + 0.016058631241321564),
                (minx + 0.00641152891330421, -0.005875378847122192, minz + 0.0160616934299469),
                (minx + 0.0067068132339045405, -0.005580872762948275, minz + 0.016075193881988525),
                (minx + 0.006656582350842655, -0.00541368592530489, maxz - 0.03881983831524849),
                (minx + 0.00636198150459677, -0.005118431523442268, maxz - 0.03882196918129921),
                (minx + 0.005944853066466749, -0.005117928143590689, maxz - 0.03882367163896561),
                (minx + 0.005649544997140765, -0.005412471015006304, maxz - 0.03882395476102829),
                (minx + 0.005649045226164162, -0.005829520057886839, maxz - 0.03882264345884323),
                (minx + 0.005943646072410047, -0.006124773994088173, maxz - 0.03882051259279251),
                (minx + 0.00636077462695539, -0.00612527783960104, maxz - 0.038818806409835815),
                (minx + 0.00665608246345073, -0.005830735433846712, maxz - 0.03881852701306343),
                (minx + 0.006639633560553193, -0.005273229442536831, maxz - 0.021357106044888496),
                (minx + 0.006345069617964327, -0.004978850018233061, maxz - 0.021388331428170204),
                (minx + 0.00592794077238068, -0.00497836247086525, maxz - 0.021389640867710114),
                (minx + 0.005632595275528729, -0.005272052250802517, maxz - 0.02136026881635189),
                (minx + 0.005632042535580695, -0.0056878807954490185, maxz - 0.021317418664693832),
                (minx + 0.005926606128923595, -0.005982260685414076, maxz - 0.021286193281412125),
                (minx + 0.0063437349162995815, -0.005982748232781887, maxz - 0.021284881979227066),
                (minx + 0.006639080471359193, -0.0056890579871833324, maxz - 0.021314255893230438),
                (minx + 0.006629384937696159, -0.004724854603409767, maxz - 0.010376550257205963),
                (minx + 0.006335195794235915, -0.004441066179424524, maxz - 0.010490451008081436),
                (minx + 0.0059180911630392075, -0.004441218450665474, maxz - 0.010495545342564583),
                (minx + 0.005622404860332608, -0.004725221544504166, maxz - 0.010388847440481186),
                (minx + 0.005621346062980592, -0.005126711446791887, maxz - 0.01023285835981369),
                (minx + 0.0059155350318178535, -0.005410499405115843, maxz - 0.010118955746293068),
                (minx + 0.0063326399540528655, -0.005410346668213606, maxz - 0.010113861411809921),
                (minx + 0.006628326023928821, -0.005126343574374914, maxz - 0.010220561176538467),
                (minx + 0.006600138149224222, -0.0037746636662632227, maxz - 0.004681237041950226),
                (minx + 0.006309418939054012, -0.0035632678773254156, maxz - 0.004991978406906128),
                (minx + 0.005892602261155844, -0.0035693119280040264, maxz - 0.00501183420419693),
                (minx + 0.005593853886239231, -0.0037892560940235853, maxz - 0.0047291722148656845),
                (minx + 0.005588176427409053, -0.004094259347766638, maxz - 0.004309574142098427),
                (minx + 0.005878895753994584, -0.004305655136704445, maxz - 0.003998832777142525),
                (minx + 0.006295712431892753, -0.00429961085319519, maxz - 0.003978978842496872),
                (minx + 0.006594460690394044, -0.004079666920006275, maxz - 0.004261638969182968),
                (minx + 0.006537339504575357, -0.002506350167095661, maxz - 0.0026417188346385956),
                (minx + 0.006259291782043874, -0.0025164526887238026, maxz - 0.003170197829604149),
                (minx + 0.005843633785843849, -0.0025427164509892464, maxz - 0.0032095760107040405),
                (minx + 0.005533852614462376, -0.0025697555392980576, maxz - 0.0027367863804101944),
                (minx + 0.005511413444764912, -0.0025817318819463253, maxz - 0.002028781920671463),
                (minx + 0.005789461429230869, -0.0025716288946568966, maxz - 0.0015003029257059097),
                (minx + 0.006205119309015572, -0.0025453658308833838, maxz - 0.001460924744606018),
                (minx + 0.0065149005386047065, -0.0025183262769132853, maxz - 0.0019337143748998642),
                (minx, -0.0013792915269732475, maxz - 0.005334946792572737),
                (minx + 4.656612873077393e-10, maxy, maxz - 0.005334946792572737),
                (maxx, maxy - 4.423782229423523e-09, maxz - 0.005334946792572737),
                (maxx, -0.0013792961835861206, maxz - 0.005334946792572737),
                (minx, -0.0013792915269732475, maxz - 1.5133991837501526e-08),
                (minx + 4.656612873077393e-10, maxy, maxz - 1.5133991837501526e-08),
                (maxx, maxy - 4.423782229423523e-09, maxz - 1.5133991837501526e-08),
                (maxx, -0.0013792961835861206, maxz - 1.5133991837501526e-08),
                (minx + 0.0011832499876618385, -0.0013792921090498567, maxz - 0.00447732862085104),
                (minx + 0.0011832499876618385, -0.0013792921090498567, maxz - 0.0008576327236369252),
                (maxx - 0.0011832499876618385, -0.0013792957179248333, maxz - 0.0008576327236369252),
                (maxx - 0.0011832499876618385, -0.0013792957179248333, maxz - 0.00447732862085104),
                (minx + 0.0011832504533231258, -0.0007798965089023113, maxz - 0.00447732862085104),
                (minx + 0.0011832504533231258, -0.0007798965089023113, maxz - 0.0008576327236369252),
                (maxx - 0.0011832495220005512, -0.0007799001759849489, maxz - 0.0008576327236369252),
                (maxx - 0.0011832495220005512, -0.0007799001759849489, maxz - 0.00447732862085104),
                (minx + 0.004529597237706184, -0.0007973873289301991, maxz - 0.0044512152671813965),
                (minx + 0.004529597237706184, -0.0007973873289301991, maxz - 0.0008894965285435319),
                (minx + 0.004144799197092652, -0.001563245547004044, maxz - 0.0044512152671813965),
                (minx + 0.004144799197092652, -0.001563245547004044, maxz - 0.0008894965285435319),
                (minx + 0.0032158144749701023, -0.0018804739229381084, maxz - 0.0044512152671813965),
                (minx + 0.0032158144749701023, -0.0018804739229381084, maxz - 0.0008894965285435319),
                (minx + 0.0022868295200169086, -0.0015632447320967913, maxz - 0.0044512152671813965),
                (minx + 0.0022868295200169086, -0.0015632447320967913, maxz - 0.0008894965285435319),
                (minx + 0.0019020326435565948, -0.0007973865140229464, maxz - 0.0044512152671813965),
                (minx + 0.0019020326435565948, -0.0007973865140229464, maxz - 0.0008894965285435319),
                (maxx - 0.001917288638651371, -0.0007973890169523656, maxz - 0.0044512152671813965),
                (maxx - 0.001917288638651371, -0.0007973890169523656, maxz - 0.0008894965285435319),
                (maxx - 0.0023020864464342594, -0.0015632474096491933, maxz - 0.0044512152671813965),
                (maxx - 0.0023020864464342594, -0.0015632474096491933, maxz - 0.0008894965285435319),
                (maxx - 0.0032310718670487404, -0.0018804756691679358, maxz - 0.0044512152671813965),
                (maxx - 0.0032310718670487404, -0.0018804756691679358, maxz - 0.0008894965285435319),
                (maxx - 0.00416005658917129, -0.0015632465947419405, maxz - 0.0044512152671813965),
                (maxx - 0.00416005658917129, -0.0015632465947419405, maxz - 0.0008894965285435319),
                (maxx - 0.0045448546297848225, -0.0007973880274221301, maxz - 0.0044512152671813965),
                (maxx - 0.0045448546297848225, -0.0007973880274221301, maxz - 0.0008894965285435319)]

    # Faces
    myfaces = [(0, 56, 144, 131), (0, 131, 151, 63), (1, 141, 145, 60), (1, 60, 144, 56), (2, 71, 146, 120),
               (2, 120, 145, 141), (3, 77, 147, 137), (3, 137, 146, 71), (4, 92, 148, 54), (4, 54, 147, 77),
               (5, 143, 149, 95), (5, 95, 148, 92), (6, 52, 150, 91), (6, 91, 149, 143), (7, 63, 151, 109),
               (7, 109, 150, 52), (8, 131, 144, 83), (8, 83, 152, 59), (8, 59, 159, 118), (8, 118, 151, 131),
               (9, 60, 145, 142), (9, 142, 153, 138), (9, 138, 152, 83), (9, 83, 144, 60), (10, 120, 146, 129),
               (10, 129, 154, 123), (10, 123, 153, 142), (10, 142, 145, 120), (11, 137, 147, 76), (11, 76, 155, 130),
               (11, 130, 154, 129), (11, 129, 146, 137), (12, 54, 148, 62), (12, 62, 156, 116), (12, 116, 155, 76),
               (12, 76, 147, 54), (13, 95, 149, 51), (13, 51, 157, 114), (13, 114, 156, 62), (13, 62, 148, 95),
               (14, 91, 150, 136), (14, 136, 158, 80), (14, 80, 157, 51), (14, 51, 149, 91), (15, 109, 151, 118),
               (15, 118, 159, 87), (15, 87, 158, 136), (15, 136, 150, 109), (16, 59, 152, 103), (16, 103, 160, 50),
               (16, 50, 167, 88), (16, 88, 159, 59), (17, 138, 153, 94), (17, 94, 161, 75), (17, 75, 160, 103),
               (17, 103, 152, 138), (18, 123, 154, 74), (18, 74, 162, 135), (18, 135, 161, 94), (18, 94, 153, 123),
               (19, 130, 155, 134), (19, 134, 163, 126), (19, 126, 162, 74), (19, 74, 154, 130), (20, 116, 156, 49),
               (20, 49, 164, 140), (20, 140, 163, 134), (20, 134, 155, 116), (21, 114, 157, 99), (21, 99, 165, 102),
               (21, 102, 164, 49), (21, 49, 156, 114), (22, 80, 158, 111), (22, 111, 166, 108), (22, 108, 165, 99),
               (22, 99, 157, 80), (23, 87, 159, 88), (23, 88, 167, 79), (23, 79, 166, 111), (23, 111, 158, 87),
               (24, 50, 160, 93), (24, 93, 168, 110), (24, 110, 175, 119), (24, 119, 167, 50), (25, 75, 161, 113),
               (25, 113, 169, 128), (25, 128, 168, 93), (25, 93, 160, 75), (26, 135, 162, 58), (26, 58, 170, 122),
               (26, 122, 169, 113), (26, 113, 161, 135), (27, 126, 163, 70), (27, 70, 171, 107), (27, 107, 170, 58),
               (27, 58, 162, 126), (28, 140, 164, 98), (28, 98, 172, 139), (28, 139, 171, 70), (28, 70, 163, 140),
               (29, 102, 165, 86), (29, 86, 173, 133), (29, 133, 172, 98), (29, 98, 164, 102), (30, 108, 166, 121),
               (30, 121, 174, 68), (30, 68, 173, 86), (30, 86, 165, 108), (31, 79, 167, 119), (31, 119, 175, 132),
               (31, 132, 174, 121), (31, 121, 166, 79), (32, 110, 168, 66), (32, 66, 176, 85), (32, 85, 183, 117),
               (32, 117, 175, 110), (33, 128, 169, 53), (33, 53, 177, 78), (33, 78, 176, 66), (33, 66, 168, 128),
               (34, 122, 170, 65), (34, 65, 178, 73), (34, 73, 177, 53), (34, 53, 169, 122), (35, 107, 171, 57),
               (35, 57, 179, 101), (35, 101, 178, 65), (35, 65, 170, 107), (36, 139, 172, 67), (36, 67, 180, 106),
               (36, 106, 179, 57), (36, 57, 171, 139), (37, 133, 173, 81), (37, 81, 181, 61), (37, 61, 180, 67),
               (37, 67, 172, 133), (38, 68, 174, 112), (38, 112, 182, 64), (38, 64, 181, 81), (38, 81, 173, 68),
               (39, 132, 175, 117), (39, 117, 183, 69), (39, 69, 182, 112), (39, 112, 174, 132), (40, 85, 176, 127),
               (40, 127, 184, 125), (40, 125, 191, 105), (40, 105, 183, 85), (41, 78, 177, 104), (41, 104, 185, 97),
               (41, 97, 184, 127), (41, 127, 176, 78), (42, 73, 178, 90), (42, 90, 186, 89), (42, 89, 185, 104),
               (42, 104, 177, 73), (43, 101, 179, 84), (43, 84, 187, 82), (43, 82, 186, 90), (43, 90, 178, 101),
               (44, 106, 180, 48), (44, 48, 188, 100), (44, 100, 187, 84), (44, 84, 179, 106), (45, 61, 181, 124),
               (45, 124, 189, 96), (45, 96, 188, 48), (45, 48, 180, 61), (46, 64, 182, 115), (46, 115, 190, 72),
               (46, 72, 189, 124), (46, 124, 181, 64), (47, 69, 183, 105), (47, 105, 191, 55), (47, 55, 190, 115),
               (47, 115, 182, 69), (192, 248, 336, 323), (192, 323, 343, 255),
               (193, 333, 337, 252), (193, 252, 336, 248),
               (194, 263, 338, 312), (194, 312, 337, 333), (195, 269, 339, 329),
               (195, 329, 338, 263), (196, 284, 340, 246),
               (196, 246, 339, 269), (197, 335, 341, 287), (197, 287, 340, 284),
               (198, 244, 342, 283), (198, 283, 341, 335),
               (199, 255, 343, 301), (199, 301, 342, 244), (200, 323, 336, 275),
               (200, 275, 344, 251), (200, 251, 351, 310),
               (200, 310, 343, 323), (201, 252, 337, 334), (201, 334, 345, 330),
               (201, 330, 344, 275), (201, 275, 336, 252),
               (202, 312, 338, 321), (202, 321, 346, 315), (202, 315, 345, 334),
               (202, 334, 337, 312), (203, 329, 339, 268),
               (203, 268, 347, 322), (203, 322, 346, 321), (203, 321, 338, 329),
               (204, 246, 340, 254), (204, 254, 348, 308),
               (204, 308, 347, 268), (204, 268, 339, 246), (205, 287, 341, 243),
               (205, 243, 349, 306), (205, 306, 348, 254),
               (205, 254, 340, 287), (206, 283, 342, 328), (206, 328, 350, 272),
               (206, 272, 349, 243), (206, 243, 341, 283),
               (207, 301, 343, 310), (207, 310, 351, 279), (207, 279, 350, 328),
               (207, 328, 342, 301), (208, 251, 344, 295),
               (208, 295, 352, 242), (208, 242, 359, 280), (208, 280, 351, 251),
               (209, 330, 345, 286), (209, 286, 353, 267),
               (209, 267, 352, 295), (209, 295, 344, 330), (210, 315, 346, 266),
               (210, 266, 354, 327), (210, 327, 353, 286),
               (210, 286, 345, 315), (211, 322, 347, 326), (211, 326, 355, 318),
               (211, 318, 354, 266), (211, 266, 346, 322),
               (212, 308, 348, 241), (212, 241, 356, 332), (212, 332, 355, 326),
               (212, 326, 347, 308), (213, 306, 349, 291),
               (213, 291, 357, 294), (213, 294, 356, 241), (213, 241, 348, 306),
               (214, 272, 350, 303), (214, 303, 358, 300),
               (214, 300, 357, 291), (214, 291, 349, 272), (215, 279, 351, 280),
               (215, 280, 359, 271), (215, 271, 358, 303),
               (215, 303, 350, 279), (216, 242, 352, 285), (216, 285, 360, 302),
               (216, 302, 367, 311), (216, 311, 359, 242),
               (217, 267, 353, 305), (217, 305, 361, 320), (217, 320, 360, 285),
               (217, 285, 352, 267), (218, 327, 354, 250),
               (218, 250, 362, 314), (218, 314, 361, 305), (218, 305, 353, 327),
               (219, 318, 355, 262), (219, 262, 363, 299),
               (219, 299, 362, 250), (219, 250, 354, 318), (220, 332, 356, 290),
               (220, 290, 364, 331), (220, 331, 363, 262),
               (220, 262, 355, 332), (221, 294, 357, 278), (221, 278, 365, 325),
               (221, 325, 364, 290), (221, 290, 356, 294),
               (222, 300, 358, 313), (222, 313, 366, 260), (222, 260, 365, 278),
               (222, 278, 357, 300), (223, 271, 359, 311),
               (223, 311, 367, 324), (223, 324, 366, 313), (223, 313, 358, 271),
               (224, 302, 360, 258), (224, 258, 368, 277),
               (224, 277, 375, 309), (224, 309, 367, 302), (225, 320, 361, 245),
               (225, 245, 369, 270), (225, 270, 368, 258),
               (225, 258, 360, 320), (226, 314, 362, 257), (226, 257, 370, 265),
               (226, 265, 369, 245), (226, 245, 361, 314),
               (227, 299, 363, 249), (227, 249, 371, 293), (227, 293, 370, 257),
               (227, 257, 362, 299), (228, 331, 364, 259),
               (228, 259, 372, 298), (228, 298, 371, 249), (228, 249, 363, 331),
               (229, 325, 365, 273), (229, 273, 373, 253),
               (229, 253, 372, 259), (229, 259, 364, 325), (230, 260, 366, 304),
               (230, 304, 374, 256), (230, 256, 373, 273),
               (230, 273, 365, 260), (231, 324, 367, 309), (231, 309, 375, 261),
               (231, 261, 374, 304), (231, 304, 366, 324),
               (232, 277, 368, 319), (232, 319, 376, 317), (232, 317, 383, 297),
               (232, 297, 375, 277), (233, 270, 369, 296),
               (233, 296, 377, 289), (233, 289, 376, 319), (233, 319, 368, 270),
               (234, 265, 370, 282), (234, 282, 378, 281),
               (234, 281, 377, 296), (234, 296, 369, 265), (235, 293, 371, 276),
               (235, 276, 379, 274), (235, 274, 378, 282),
               (235, 282, 370, 293), (236, 298, 372, 240), (236, 240, 380, 292),
               (236, 292, 379, 276), (236, 276, 371, 298),
               (237, 253, 373, 316), (237, 316, 381, 288), (237, 288, 380, 240),
               (237, 240, 372, 253), (238, 256, 374, 307),
               (238, 307, 382, 264), (238, 264, 381, 316), (238, 316, 373, 256),
               (239, 261, 375, 297), (239, 297, 383, 247),
               (239, 247, 382, 307), (239, 307, 374, 261), (388, 389, 385, 384),
               (389, 390, 386, 385), (390, 391, 387, 386),
               (392, 395, 399, 396), (384, 385, 386, 387), (391, 390, 389, 388),
               (392, 393, 388, 384), (394, 395, 387, 391),
               (393, 394, 391, 388), (395, 392, 384, 387), (398, 397, 396, 399),
               (394, 393, 397, 398), (395, 394, 398, 399),
               (393, 392, 396, 397), (400, 401, 403, 402), (402, 403, 405, 404),
               (404, 405, 407, 406), (406, 407, 409, 408),
               (410, 411, 413, 412), (412, 413, 415, 414), (414, 415, 417, 416), (416, 417, 419, 418)]

    mesh = bpy.data.meshes.new(objname)
    myobject = bpy.data.objects.new(objname, mesh)

    myobject.location = bpy.context.scene.cursor_location
    bpy.context.scene.objects.link(myobject)

    mesh.from_pydata(myvertex, [], myfaces)
    mesh.update(calc_edges=True)

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
