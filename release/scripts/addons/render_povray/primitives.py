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

############ To get POV-Ray specific objects In and Out of Blender ###########

import bpy
import os.path
from bpy_extras.io_utils import ImportHelper
from bpy_extras import object_utils
from math import atan, pi, degrees, sqrt, cos, sin


from bpy.props import (
        StringProperty,
        BoolProperty,
        IntProperty,
        FloatProperty,
        FloatVectorProperty,
        EnumProperty,
        PointerProperty,
        CollectionProperty,
        )

from mathutils import (
    Vector,
    Matrix,
)

#import collections

def pov_define_mesh(mesh, verts, edges, faces, name, hide_geometry=True):
    if mesh is None:
        mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(verts, edges, faces)
    mesh.update()
    mesh.validate(False)  # Set it to True to see debug messages (helps ensure you generate valid geometry).
    if hide_geometry:
        mesh.vertices.foreach_set("hide", [True] * len(mesh.vertices))
        mesh.edges.foreach_set("hide", [True] * len(mesh.edges))
        mesh.polygons.foreach_set("hide", [True] * len(mesh.polygons))
    return mesh


class POVRAY_OT_lathe_add(bpy.types.Operator):
    bl_idname = "pov.addlathe"
    bl_label = "Lathe"
    bl_options = {'REGISTER','UNDO'}
    bl_description = "adds lathe"


    def execute(self, context):
        layers=[False]*20
        layers[0]=True
        bpy.ops.curve.primitive_bezier_curve_add(
            location=context.scene.cursor_location,
            rotation=(0, 0, 0),
            layers=layers,
        )
        ob = context.scene.objects.active
        ob_data = ob.data
        ob.name = ob_data.name = "PovLathe"
        ob_data.dimensions = '2D'
        ob_data.transform(Matrix.Rotation(-pi / 2.0, 4, 'Z'))
        ob.pov.object_as='LATHE'
        self.report({'INFO'}, "This native POV-Ray primitive")
        ob.pov.curveshape = "lathe"
        bpy.ops.object.modifier_add(type='SCREW')
        mod = ob.modifiers[-1]
        mod.axis = 'Y'
        mod.show_render = False
        return {'FINISHED'}



def pov_superellipsoid_define(context, op, ob):

        if op:
            mesh = None

            u = op.se_u
            v = op.se_v
            n1 = op.se_n1
            n2 = op.se_n2
            edit = op.se_edit
            se_param1 = n2 # op.se_param1
            se_param2 = n1 # op.se_param2

        else:
            assert(ob)
            mesh = ob.data

            u = ob.pov.se_u
            v = ob.pov.se_v
            n1 = ob.pov.se_n1
            n2 = ob.pov.se_n2
            edit = ob.pov.se_edit
            se_param1 = ob.pov.se_param1
            se_param2 = ob.pov.se_param2

        verts = []
        r=1

        stepSegment=360/v*pi/180
        stepRing=pi/u
        angSegment=0
        angRing=-pi/2

        step=0
        for ring in range(0,u-1):
            angRing += stepRing
            for segment in range(0,v):
                step += 1
                angSegment += stepSegment
                x = r*(abs(cos(angRing))**n1)*(abs(cos(angSegment))**n2)
                if (cos(angRing) < 0 and cos(angSegment) > 0) or \
                        (cos(angRing) > 0 and cos(angSegment) < 0):
                    x = -x
                y = r*(abs(cos(angRing))**n1)*(abs(sin(angSegment))**n2)
                if (cos(angRing) < 0 and sin(angSegment) > 0) or \
                        (cos(angRing) > 0 and sin(angSegment) < 0):
                    y = -y
                z = r*(abs(sin(angRing))**n1)
                if sin(angRing) < 0:
                    z = -z
                x = round(x,4)
                y = round(y,4)
                z = round(z,4)
                verts.append((x,y,z))
        if edit == 'TRIANGLES':
            verts.append((0,0,1))
            verts.append((0,0,-1))

        faces = []

        for i in range(0,u-2):
            m=i*v
            for p in range(0,v):
                if p < v-1:
                    face=(m+p,1+m+p,v+1+m+p,v+m+p)
                if p == v-1:
                    face=(m+p,m,v+m,v+m+p)
                faces.append(face)
        if edit == 'TRIANGLES':
            indexUp=len(verts)-2
            indexDown=len(verts)-1
            indexStartDown=len(verts)-2-v
            for i in range(0,v):
                if i < v-1:
                    face=(indexDown,i,i+1)
                    faces.append(face)
                if i == v-1:
                    face=(indexDown,i,0)
                    faces.append(face)
            for i in range(0,v):
                if i < v-1:
                    face=(indexUp,i+indexStartDown,i+indexStartDown+1)
                    faces.append(face)
                if i == v-1:
                    face=(indexUp,i+indexStartDown,indexStartDown)
                    faces.append(face)
        if edit == 'NGONS':
            face=[]
            for i in range(0,v):
                face.append(i)
            faces.append(face)
            face=[]
            indexUp=len(verts)-1
            for i in range(0,v):
                face.append(indexUp-i)
            faces.append(face)
        mesh = pov_define_mesh(mesh, verts, [], faces, "SuperEllipsoid")

        if not ob:
            ob_base = object_utils.object_data_add(context, mesh, operator=None)
            ob = ob_base.object
            #engine = context.scene.render.engine what for?
            ob = context.object
            ob.name =  ob.data.name = "PovSuperellipsoid"
            ob.pov.object_as = 'SUPERELLIPSOID'
            ob.pov.se_param1 = n2
            ob.pov.se_param2 = n1

            ob.pov.se_u = u
            ob.pov.se_v = v
            ob.pov.se_n1 = n1
            ob.pov.se_n2 = n2
            ob.pov.se_edit = edit

            bpy.ops.object.mode_set(mode="EDIT")
            bpy.ops.mesh.hide(unselected=False)
            bpy.ops.object.mode_set(mode="OBJECT")

class POVRAY_OT_superellipsoid_add(bpy.types.Operator):
    bl_idname = "pov.addsuperellipsoid"
    bl_label = "Add SuperEllipsoid"
    bl_description = "Create a SuperEllipsoid"
    bl_options = {'REGISTER', 'UNDO'}
    COMPAT_ENGINES = {'POVRAY_RENDER'}

    # XXX Keep it in sync with __init__'s RenderPovSettingsConePrimitive
    #     If someone knows how to define operators' props from a func, I'd be delighted to learn it!
    se_param1 = FloatProperty(
            name="Parameter 1",
            description="",
            min=0.00, max=10.0, default=0.04)

    se_param2 = FloatProperty(
            name="Parameter 2",
            description="",
            min=0.00, max=10.0, default=0.04)

    se_u = IntProperty(name = "U-segments",
                    description = "radial segmentation",
                    default = 20, min = 4, max = 265)
    se_v = IntProperty(name = "V-segments",
                    description = "lateral segmentation",
                    default = 20, min = 4, max = 265)
    se_n1 = FloatProperty(name = "Ring manipulator",
                      description = "Manipulates the shape of the Ring",
                      default = 1.0, min = 0.01, max = 100.0)
    se_n2 = FloatProperty(name = "Cross manipulator",
                      description = "Manipulates the shape of the cross-section",
                      default = 1.0, min = 0.01, max = 100.0)
    se_edit = EnumProperty(items=[("NOTHING", "Nothing", ""),
                                ("NGONS", "N-Gons", ""),
                                ("TRIANGLES", "Triangles", "")],
                        name="Fill up and down",
                        description="",
                        default='TRIANGLES')

    @classmethod
    def poll(cls, context):
        engine = context.scene.render.engine
        return (engine in cls.COMPAT_ENGINES)

    def execute(self,context):
        pov_superellipsoid_define(context, self, None)

        self.report({'INFO'}, "This native POV-Ray primitive won't have any vertex to show in edit mode")

        return {'FINISHED'}

class POVRAY_OT_superellipsoid_update(bpy.types.Operator):
    bl_idname = "pov.superellipsoid_update"
    bl_label = "Update"
    bl_description = "Update Superellipsoid"
    bl_options = {'REGISTER', 'UNDO'}
    COMPAT_ENGINES = {'POVRAY_RENDER'}

    @classmethod
    def poll(cls, context):
        engine = context.scene.render.engine
        ob = context.object
        return (ob and ob.data and ob.type == 'MESH' and engine in cls.COMPAT_ENGINES)

    def execute(self, context):
        bpy.ops.object.mode_set(mode="EDIT")
        bpy.ops.mesh.reveal()
        bpy.ops.mesh.select_all(action='SELECT')
        bpy.ops.mesh.delete(type='VERT')
        bpy.ops.object.mode_set(mode="OBJECT")

        pov_superellipsoid_define(context, None, context.object)

        return {'FINISHED'}

def createFaces(vertIdx1, vertIdx2, closed=False, flipped=False):
    faces = []
    if not vertIdx1 or not vertIdx2:
        return None
    if len(vertIdx1) < 2 and len(vertIdx2) < 2:
        return None
    fan = False
    if (len(vertIdx1) != len(vertIdx2)):
        if (len(vertIdx1) == 1 and len(vertIdx2) > 1):
            fan = True
        else:
            return None
    total = len(vertIdx2)
    if closed:
        if flipped:
            face = [
                vertIdx1[0],
                vertIdx2[0],
                vertIdx2[total - 1]]
            if not fan:
                face.append(vertIdx1[total - 1])
            faces.append(face)

        else:
            face = [vertIdx2[0], vertIdx1[0]]
            if not fan:
                face.append(vertIdx1[total - 1])
            face.append(vertIdx2[total - 1])
            faces.append(face)
    for num in range(total - 1):
        if flipped:
            if fan:
                face = [vertIdx2[num], vertIdx1[0], vertIdx2[num + 1]]
            else:
                face = [vertIdx2[num], vertIdx1[num],
                    vertIdx1[num + 1], vertIdx2[num + 1]]
            faces.append(face)
        else:
            if fan:
                face = [vertIdx1[0], vertIdx2[num], vertIdx2[num + 1]]
            else:
                face = [vertIdx1[num], vertIdx2[num],
                    vertIdx2[num + 1], vertIdx1[num + 1]]
            faces.append(face)

    return faces

def power(a,b):
    if a < 0:
        return -((-a)**b)
    return a**b

def supertoroid(R,r,u,v,n1,n2):
    a = 2*pi/u
    b = 2*pi/v
    verts = []
    faces = []
    for i in range(u):
        s = power(sin(i*a),n1)
        c = power(cos(i*a),n1)
        for j in range(v):
            c2 = R+r*power(cos(j*b),n2)
            s2 = r*power(sin(j*b),n2)
            verts.append((c*c2,s*c2,s2))# type as a (mathutils.Vector(c*c2,s*c2,s2))?
        if i > 0:
            f = createFaces(range((i-1)*v,i*v),range(i*v,(i+1)*v),closed = True)
            faces.extend(f)
    f = createFaces(range((u-1)*v,u*v),range(v),closed=True)
    faces.extend(f)
    return verts, faces

def pov_supertorus_define(context, op, ob):
        if op:
            mesh = None
            st_R = op.st_R
            st_r = op.st_r
            st_u = op.st_u
            st_v = op.st_v
            st_n1 = op.st_n1
            st_n2 = op.st_n2
            st_ie = op.st_ie
            st_edit = op.st_edit

        else:
            assert(ob)
            mesh = ob.data
            st_R = ob.pov.st_major_radius
            st_r = ob.pov.st_minor_radius
            st_u = ob.pov.st_u
            st_v = ob.pov.st_v
            st_n1 = ob.pov.st_ring
            st_n2 = ob.pov.st_cross
            st_ie = ob.pov.st_ie
            st_edit = ob.pov.st_edit

        if st_ie:
            rad1 = (st_R+st_r)/2
            rad2 = (st_R-st_r)/2
            if rad2 > rad1:
                [rad1,rad2] = [rad2,rad1]
        else:
            rad1 = st_R
            rad2 = st_r
            if rad2 > rad1:
                rad1 = rad2
        verts,faces = supertoroid(rad1,
                                  rad2,
                                  st_u,
                                  st_v,
                                  st_n1,
                                  st_n2)
        mesh = pov_define_mesh(mesh, verts, [], faces, "PovSuperTorus", True)
        if not ob:
            ob_base = object_utils.object_data_add(context, mesh, operator=None)

            ob = ob_base.object
            ob.pov.object_as = 'SUPERTORUS'
            ob.pov.st_major_radius = st_R
            ob.pov.st_minor_radius = st_r
            ob.pov.st_u = st_u
            ob.pov.st_v = st_v
            ob.pov.st_ring = st_n1
            ob.pov.st_cross = st_n2
            ob.pov.st_ie = st_ie
            ob.pov.st_edit = st_edit

class POVRAY_OT_supertorus_add(bpy.types.Operator):
    bl_idname = "pov.addsupertorus"
    bl_label = "Add Supertorus"
    bl_description = "Create a SuperTorus"
    bl_options = {'REGISTER', 'UNDO'}
    COMPAT_ENGINES = {'POVRAY_RENDER'}

    st_R = FloatProperty(name = "big radius",
                      description = "The radius inside the tube",
                      default = 1.0, min = 0.01, max = 100.0)
    st_r = FloatProperty(name = "small radius",
                      description = "The radius of the tube",
                      default = 0.3, min = 0.01, max = 100.0)
    st_u = IntProperty(name = "U-segments",
                    description = "radial segmentation",
                    default = 16, min = 3, max = 265)
    st_v = IntProperty(name = "V-segments",
                    description = "lateral segmentation",
                    default = 8, min = 3, max = 265)
    st_n1 = FloatProperty(name = "Ring manipulator",
                      description = "Manipulates the shape of the Ring",
                      default = 1.0, min = 0.01, max = 100.0)
    st_n2 = FloatProperty(name = "Cross manipulator",
                      description = "Manipulates the shape of the cross-section",
                      default = 1.0, min = 0.01, max = 100.0)
    st_ie = BoolProperty(name = "Use Int.+Ext. radii",
                      description = "Use internal and external radii",
                      default = False)
    st_edit = BoolProperty(name="",
                        description="",
                        default=False,
                        options={'HIDDEN'})

    @classmethod
    def poll(cls, context):
        engine = context.scene.render.engine
        return (engine in cls.COMPAT_ENGINES)

    def execute(self, context):
        pov_supertorus_define(context, self, None)

        self.report({'INFO'}, "This native POV-Ray primitive won't have any vertex to show in edit mode")
        return {'FINISHED'}

class POVRAY_OT_supertorus_update(bpy.types.Operator):
    bl_idname = "pov.supertorus_update"
    bl_label = "Update"
    bl_description = "Update SuperTorus"
    bl_options = {'REGISTER', 'UNDO'}
    COMPAT_ENGINES = {'POVRAY_RENDER'}

    @classmethod
    def poll(cls, context):
        engine = context.scene.render.engine
        ob = context.object
        return (ob and ob.data and ob.type == 'MESH' and engine in cls.COMPAT_ENGINES)

    def execute(self, context):
        bpy.ops.object.mode_set(mode="EDIT")
        bpy.ops.mesh.reveal()
        bpy.ops.mesh.select_all(action='SELECT')
        bpy.ops.mesh.delete(type='VERT')
        bpy.ops.object.mode_set(mode="OBJECT")

        pov_supertorus_define(context, None, context.object)

        return {'FINISHED'}
#########################################################################################################
class POVRAY_OT_loft_add(bpy.types.Operator):
    bl_idname = "pov.addloft"
    bl_label = "Add Loft Data"
    bl_description = "Create a Curve data for Meshmaker"
    bl_options = {'REGISTER', 'UNDO'}
    COMPAT_ENGINES = {'POVRAY_RENDER'}

    loft_n = IntProperty(name = "Segments",
                    description = "Vertical segments",
                    default = 16, min = 3, max = 720)
    loft_rings_bottom = IntProperty(name = "Bottom",
                    description = "Bottom rings",
                    default = 5, min = 2, max = 100)
    loft_rings_side = IntProperty(name = "Side",
                    description = "Side rings",
                    default = 10, min = 2, max = 100)
    loft_thick = FloatProperty(name = "Thickness",
                      description = "Manipulates the shape of the Ring",
                      default = 0.3, min = 0.01, max = 1.0)
    loft_r = FloatProperty(name = "Radius",
                      description = "Radius",
                      default = 1, min = 0.01, max = 10)
    loft_height = FloatProperty(name = "Height",
                      description = "Manipulates the shape of the Ring",
                      default = 2, min = 0.01, max = 10.0)

    def execute(self,context):

        props = self.properties
        loftData = bpy.data.curves.new('Loft', type='CURVE')
        loftData.dimensions = '3D'
        loftData.resolution_u = 2
        loftData.show_normal_face = False
        n=props.loft_n
        thick = props.loft_thick
        side = props.loft_rings_side
        bottom = props.loft_rings_bottom
        h = props.loft_height
        r = props.loft_r
        distB = r/bottom
        r0 = 0.00001
        z = -h/2
        print("New")
        for i in range(bottom+1):
            coords = []
            angle = 0
            for p in range(n):
                x = r0*cos(angle)
                y = r0*sin(angle)
                coords.append((x,y,z))
                angle+=pi*2/n
            r0+=distB
            nurbs = loftData.splines.new('NURBS')
            nurbs.points.add(len(coords)-1)
            for i, coord in enumerate(coords):
                x,y,z = coord
                nurbs.points[i].co = (x, y, z, 1)
            nurbs.use_cyclic_u = True
        for i in range(side):
            z+=h/side
            coords = []
            angle = 0
            for p in range(n):
                x = r*cos(angle)
                y = r*sin(angle)
                coords.append((x,y,z))
                angle+=pi*2/n
            nurbs = loftData.splines.new('NURBS')
            nurbs.points.add(len(coords)-1)
            for i, coord in enumerate(coords):
                x,y,z = coord
                nurbs.points[i].co = (x, y, z, 1)
            nurbs.use_cyclic_u = True
        r-=thick
        for i in range(side):
            coords = []
            angle = 0
            for p in range(n):
                x = r*cos(angle)
                y = r*sin(angle)
                coords.append((x,y,z))
                angle+=pi*2/n
            nurbs = loftData.splines.new('NURBS')
            nurbs.points.add(len(coords)-1)
            for i, coord in enumerate(coords):
                x,y,z = coord
                nurbs.points[i].co = (x, y, z, 1)
            nurbs.use_cyclic_u = True
            z-=h/side
        z = (-h/2) + thick
        distB = (r-0.00001)/bottom
        for i in range(bottom+1):
            coords = []
            angle = 0
            for p in range(n):
                x = r*cos(angle)
                y = r*sin(angle)
                coords.append((x,y,z))
                angle+=pi*2/n
            r-=distB
            nurbs = loftData.splines.new('NURBS')
            nurbs.points.add(len(coords)-1)
            for i, coord in enumerate(coords):
                x,y,z = coord
                nurbs.points[i].co = (x, y, z, 1)
            nurbs.use_cyclic_u = True
        ob = bpy.data.objects.new('Loft_shape', loftData)
        scn = bpy.context.scene
        scn.objects.link(ob)
        scn.objects.active = ob
        ob.select = True
        ob.pov.curveshape = "loft"
        return {'FINISHED'}

class POVRAY_OT_plane_add(bpy.types.Operator):
    bl_idname = "pov.addplane"
    bl_label = "Plane"
    bl_description = "Add Plane"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self,context):
        layers = 20*[False]
        layers[0] = True
        bpy.ops.mesh.primitive_plane_add(radius = 100000,layers=layers)
        ob = context.object
        ob.name = ob.data.name = 'PovInfinitePlane'
        bpy.ops.object.mode_set(mode="EDIT")
        self.report({'INFO'}, "This native POV-Ray primitive "
                                 "won't have any vertex to show in edit mode")
        bpy.ops.mesh.hide(unselected=False)
        bpy.ops.object.mode_set(mode="OBJECT")
        bpy.ops.object.shade_smooth()
        ob.pov.object_as = "PLANE"
        return {'FINISHED'}

class POVRAY_OT_box_add(bpy.types.Operator):
    bl_idname = "pov.addbox"
    bl_label = "Box"
    bl_description = "Add Box"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self,context):
        layers = 20*[False]
        layers[0] = True
        bpy.ops.mesh.primitive_cube_add(layers=layers)
        ob = context.object
        ob.name = ob.data.name = 'PovBox'
        bpy.ops.object.mode_set(mode="EDIT")
        self.report({'INFO'}, "This native POV-Ray primitive "
                                 "won't have any vertex to show in edit mode")
        bpy.ops.mesh.hide(unselected=False)
        bpy.ops.object.mode_set(mode="OBJECT")
        ob.pov.object_as = "BOX"
        return {'FINISHED'}


def pov_cylinder_define(context, op, ob, radius, loc, loc_cap):
    if op:
        R = op.R
        loc = bpy.context.scene.cursor_location
        loc_cap[0] = loc[0]
        loc_cap[1] = loc[1]
        loc_cap[2] = (loc[2]+2)
    vec = Vector(loc_cap) - Vector(loc)
    depth = vec.length
    rot = Vector((0, 0, 1)).rotation_difference(vec)  # Rotation from Z axis.
    trans = rot * Vector((0, 0, depth / 2)) # Such that origin is at center of the base of the cylinder.
    roteuler = rot.to_euler()
    if not ob:
        bpy.ops.object.add(type='MESH', location=loc)
        ob = context.object
        ob.name = ob.data.name = "PovCylinder"
        ob.pov.cylinder_radius = radius
        ob.pov.cylinder_location_cap = vec
        ob.pov.object_as = "CYLINDER"
    else:
        ob.location = loc

    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.reveal()
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.mesh.delete(type='VERT')
    bpy.ops.mesh.primitive_cylinder_add(radius=radius, depth=depth, location=loc, rotation=roteuler, end_fill_type='NGON') #'NOTHING'
    bpy.ops.transform.translate(value=trans)

    bpy.ops.mesh.hide(unselected=False)
    bpy.ops.object.mode_set(mode="OBJECT")
    bpy.ops.object.shade_smooth()


class POVRAY_OT_cylinder_add(bpy.types.Operator):
    bl_idname = "pov.addcylinder"
    bl_label = "Cylinder"
    bl_description = "Add Cylinder"
    bl_options = {'REGISTER', 'UNDO'}

    # XXX Keep it in sync with __init__'s cylinder Primitive
    R = FloatProperty(name="Cylinder radius", min=0.00, max=10.0, default=1.0)

    imported_cyl_loc = FloatVectorProperty(
        name="Imported Pov base location",
        precision=6,
        default=(0.0, 0.0, 0.0))

    imported_cyl_loc_cap = FloatVectorProperty(
        name="Imported Pov cap location",
        precision=6,
        default=(0.0, 0.0, 2.0))

    def execute(self,context):
        props = self.properties
        R = props.R
        ob = context.object
        layers = 20*[False]
        layers[0] = True
        if ob:
            if ob.pov.imported_cyl_loc:
                LOC = ob.pov.imported_cyl_loc
            if ob.pov.imported_cyl_loc_cap:
                LOC_CAP = ob.pov.imported_cyl_loc_cap
        else:
            if not props.imported_cyl_loc:
                LOC_CAP = LOC = bpy.context.scene.cursor_location
                LOC_CAP[2] += 2.0
            else:
                LOC = props.imported_cyl_loc
                LOC_CAP = props.imported_cyl_loc_cap
            self.report({'INFO'}, "This native POV-Ray primitive "
                                     "won't have any vertex to show in edit mode")

        pov_cylinder_define(context, self, None, self.R, LOC, LOC_CAP)

        return {'FINISHED'}


class POVRAY_OT_cylinder_update(bpy.types.Operator):
    bl_idname = "pov.cylinder_update"
    bl_label = "Update"
    bl_description = "Update Cylinder"
    bl_options = {'REGISTER', 'UNDO'}
    COMPAT_ENGINES = {'POVRAY_RENDER'}

    @classmethod
    def poll(cls, context):
        engine = context.scene.render.engine
        ob = context.object
        return (ob and ob.data and ob.type == 'MESH' and ob.pov.object_as == "CYLINDER" and engine in cls.COMPAT_ENGINES)

    def execute(self, context):
        ob = context.object
        radius = ob.pov.cylinder_radius
        loc = ob.location
        loc_cap = loc + ob.pov.cylinder_location_cap

        pov_cylinder_define(context, None, ob, radius, loc, loc_cap)

        return {'FINISHED'}


################################SPHERE##########################################
def pov_sphere_define(context, op, ob, loc):
        if op:
            R = op.R
            loc = bpy.context.scene.cursor_location
        else:
            assert(ob)
            R = ob.pov.sphere_radius

            #keep object rotation and location for the add object operator
            obrot = ob.rotation_euler
            #obloc = ob.location
            obscale = ob.scale

            bpy.ops.object.mode_set(mode="EDIT")
            bpy.ops.mesh.reveal()
            bpy.ops.mesh.select_all(action='SELECT')
            bpy.ops.mesh.delete(type='VERT')
            bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=4, size=ob.pov.sphere_radius, location=loc, rotation=obrot)
            #bpy.ops.transform.rotate(axis=obrot,constraint_orientation='GLOBAL')
            bpy.ops.transform.resize(value=obscale)
            #bpy.ops.transform.rotate(axis=obrot, proportional_size=1)


            bpy.ops.mesh.hide(unselected=False)
            bpy.ops.object.mode_set(mode="OBJECT")
            bpy.ops.object.shade_smooth()
            #bpy.ops.transform.rotate(axis=obrot,constraint_orientation='GLOBAL')

        if not ob:
            bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=4, size=R, location=loc)
            ob = context.object
            ob.name =  ob.data.name = "PovSphere"
            ob.pov.object_as = "SPHERE"
            ob.pov.sphere_radius = R
            bpy.ops.object.mode_set(mode="EDIT")
            bpy.ops.mesh.hide(unselected=False)
            bpy.ops.object.mode_set(mode="OBJECT")

class POVRAY_OT_sphere_add(bpy.types.Operator):
    bl_idname = "pov.addsphere"
    bl_label = "Sphere"
    bl_description = "Add Sphere Shape"
    bl_options = {'REGISTER', 'UNDO'}

    # XXX Keep it in sync with __init__'s torus Primitive
    R = FloatProperty(name="Sphere radius",min=0.00, max=10.0, default=0.5)

    imported_loc = FloatVectorProperty(
        name="Imported Pov location",
        precision=6,
        default=(0.0, 0.0, 0.0))

    def execute(self,context):
        props = self.properties
        R = props.R
        ob = context.object



        if ob:
            if ob.pov.imported_loc:
                LOC = ob.pov.imported_loc
        else:
            if not props.imported_loc:
                LOC = bpy.context.scene.cursor_location

            else:
                LOC = props.imported_loc
                self.report({'INFO'}, "This native POV-Ray primitive "
                                         "won't have any vertex to show in edit mode")
        pov_sphere_define(context, self, None, LOC)

        return {'FINISHED'}

    # def execute(self,context):
        # layers = 20*[False]
        # layers[0] = True

        # bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=4, radius=ob.pov.sphere_radius, layers=layers)
        # ob = context.object
        # bpy.ops.object.mode_set(mode="EDIT")
        # self.report({'INFO'}, "This native POV-Ray primitive "
                                 # "won't have any vertex to show in edit mode")
        # bpy.ops.mesh.hide(unselected=False)
        # bpy.ops.object.mode_set(mode="OBJECT")
        # bpy.ops.object.shade_smooth()
        # ob.pov.object_as = "SPHERE"
        # ob.name = ob.data.name = 'PovSphere'
        # return {'FINISHED'}
class POVRAY_OT_sphere_update(bpy.types.Operator):
    bl_idname = "pov.sphere_update"
    bl_label = "Update"
    bl_description = "Update Sphere"
    bl_options = {'REGISTER', 'UNDO'}
    COMPAT_ENGINES = {'POVRAY_RENDER'}

    @classmethod
    def poll(cls, context):
        engine = context.scene.render.engine
        ob = context.object
        return (ob and ob.data and ob.type == 'MESH' and engine in cls.COMPAT_ENGINES)

    def execute(self, context):

        pov_sphere_define(context, None, context.object,context.object.location)

        return {'FINISHED'}


####################################CONE#######################################
def pov_cone_define(context, op, ob):
    verts = []
    faces = []
    if op:
        mesh = None
        base = op.base
        cap = op.cap
        seg = op.seg
        height = op.height
    else:
        assert(ob)
        mesh = ob.data
        base = ob.pov.cone_base_radius
        cap = ob.pov.cone_cap_radius
        seg = ob.pov.cone_segments
        height = ob.pov.cone_height

    zc = height / 2
    zb = -zc
    angle = 2 * pi / seg
    t = 0
    for i in range(seg):
        xb = base * cos(t)
        yb = base * sin(t)
        xc = cap * cos(t)
        yc = cap * sin(t)
        verts.append((xb, yb, zb))
        verts.append((xc, yc, zc))
        t += angle
    for i in range(seg):
        f = i * 2
        if i == seg - 1:
            faces.append([0, 1, f + 1, f])
        else:
            faces.append([f + 2, f + 3, f + 1, f])
    if base != 0:
        base_face = []
        for i in range(seg - 1, -1, -1):
            p = i * 2
            base_face.append(p)
        faces.append(base_face)
    if cap != 0:
        cap_face = []
        for i in range(seg):
            p = i * 2 + 1
            cap_face.append(p)
        faces.append(cap_face)

    mesh = pov_define_mesh(mesh, verts, [], faces, "PovCone", True)
    if not ob:
        ob_base = object_utils.object_data_add(context, mesh, operator=None)
        ob = ob_base.object
        ob.pov.object_as = "CONE"
        ob.pov.cone_base_radius = base
        ob.pov.cone_cap_radius = cap
        ob.pov.cone_height = height
        ob.pov.cone_base_z = zb
        ob.pov.cone_cap_z = zc


class POVRAY_OT_cone_add(bpy.types.Operator):
    bl_idname = "pov.cone_add"
    bl_label = "Cone"
    bl_description = "Add Cone"
    bl_options = {'REGISTER', 'UNDO'}
    COMPAT_ENGINES = {'POVRAY_RENDER'}

    # XXX Keep it in sync with __init__'s RenderPovSettingsConePrimitive
    #     If someone knows how to define operators' props from a func, I'd be delighted to learn it!
    base = FloatProperty(
        name = "Base radius", description = "The first radius of the cone",
        default = 1.0, min = 0.01, max = 100.0)
    cap = FloatProperty(
        name = "Cap radius", description = "The second radius of the cone",
        default = 0.3, min = 0.0, max = 100.0)
    seg = IntProperty(
        name = "Segments", description = "Radial segmentation of the proxy mesh",
        default = 16, min = 3, max = 265)
    height = FloatProperty(
        name = "Height", description = "Height of the cone",
        default = 2.0, min = 0.01, max = 100.0)

    @classmethod
    def poll(cls, context):
        engine = context.scene.render.engine
        return (engine in cls.COMPAT_ENGINES)

    def execute(self, context):
        pov_cone_define(context, self, None)

        self.report({'INFO'}, "This native POV-Ray primitive won't have any vertex to show in edit mode")
        return {'FINISHED'}


class POVRAY_OT_cone_update(bpy.types.Operator):
    bl_idname = "pov.cone_update"
    bl_label = "Update"
    bl_description = "Update Cone"
    bl_options = {'REGISTER', 'UNDO'}
    COMPAT_ENGINES = {'POVRAY_RENDER'}

    @classmethod
    def poll(cls, context):
        engine = context.scene.render.engine
        ob = context.object
        return (ob and ob.data and ob.type == 'MESH' and engine in cls.COMPAT_ENGINES)

    def execute(self, context):
        bpy.ops.object.mode_set(mode="EDIT")
        bpy.ops.mesh.reveal()
        bpy.ops.mesh.select_all(action='SELECT')
        bpy.ops.mesh.delete(type='VERT')
        bpy.ops.object.mode_set(mode="OBJECT")

        pov_cone_define(context, None, context.object)

        return {'FINISHED'}
#########################################################################################################

class POVRAY_OT_isosurface_box_add(bpy.types.Operator):
    bl_idname = "pov.addisosurfacebox"
    bl_label = "Isosurface Box"
    bl_description = "Add Isosurface contained by Box"
    bl_options = {'REGISTER', 'UNDO'}


    def execute(self,context):
        layers = 20*[False]
        layers[0] = True
        bpy.ops.mesh.primitive_cube_add(layers = layers)
        ob = context.object
        bpy.ops.object.mode_set(mode="EDIT")
        self.report({'INFO'}, "This native POV-Ray primitive "
                                 "won't have any vertex to show in edit mode")
        bpy.ops.mesh.hide(unselected=False)
        bpy.ops.object.mode_set(mode="OBJECT")
        ob.pov.object_as = "ISOSURFACE"
        ob.pov.contained_by = 'box'
        ob.name = 'PovIsosurfaceBox'
        return {'FINISHED'}

class POVRAY_OT_isosurface_sphere_add(bpy.types.Operator):
    bl_idname = "pov.addisosurfacesphere"
    bl_label = "Isosurface Sphere"
    bl_description = "Add Isosurface contained by Sphere"
    bl_options = {'REGISTER', 'UNDO'}


    def execute(self,context):
        layers = 20*[False]
        layers[0] = True
        bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=4,layers=layers)
        ob = context.object
        bpy.ops.object.mode_set(mode="EDIT")
        self.report({'INFO'}, "This native POV-Ray primitive "
                                 "won't have any vertex to show in edit mode")
        bpy.ops.mesh.hide(unselected=False)
        bpy.ops.object.mode_set(mode="OBJECT")
        bpy.ops.object.shade_smooth()
        ob.pov.object_as = "ISOSURFACE"
        ob.pov.contained_by = 'sphere'
        ob.name = 'PovIsosurfaceSphere'
        return {'FINISHED'}

class POVRAY_OT_sphere_sweep_add(bpy.types.Operator):
    bl_idname = "pov.addspheresweep"
    bl_label = "Sphere Sweep"
    bl_description = "Create Sphere Sweep along curve"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self,context):
        layers = 20*[False]
        layers[0] = True
        bpy.ops.curve.primitive_nurbs_curve_add(layers = layers)
        ob = context.object
        ob.name = ob.data.name = "PovSphereSweep"
        ob.pov.curveshape = "sphere_sweep"
        ob.data.bevel_depth = 0.02
        ob.data.bevel_resolution = 4
        ob.data.fill_mode = 'FULL'
        #ob.data.splines[0].order_u = 4

        return {'FINISHED'}

class POVRAY_OT_blob_add(bpy.types.Operator):
    bl_idname = "pov.addblobsphere"
    bl_label = "Blob Sphere"
    bl_description = "Add Blob Sphere"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self,context):
        layers = 20*[False]
        layers[0] = True
        bpy.ops.object.metaball_add(type = 'BALL',layers = layers)
        ob = context.object
        ob.name = "PovBlob"
        return {'FINISHED'}


class POVRAY_OT_rainbow_add(bpy.types.Operator):
    bl_idname = "pov.addrainbow"
    bl_label = "Rainbow"
    bl_description = "Add Rainbow"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self,context):
        cam = context.scene.camera
        bpy.ops.object.lamp_add(type='SPOT', radius=1)
        ob = context.object
        ob.data.show_cone = False
        ob.data.spot_blend = 0.5
        ob.data.shadow_buffer_clip_end = 0
        ob.data.shadow_buffer_clip_start = 4*cam.location.length
        ob.data.distance = cam.location.length
        ob.data.energy = 0
        ob.name = ob.data.name = "PovRainbow"
        ob.pov.object_as = "RAINBOW"

        #obj = context.object
        bpy.ops.object.constraint_add(type='DAMPED_TRACK')



        ob.constraints["Damped Track"].target = cam
        ob.constraints["Damped Track"].track_axis = 'TRACK_NEGATIVE_Z'
        ob.location = -cam.location

        #refocus on the actual rainbow
        bpy.context.scene.objects.active = ob
        ob.select=True

        return {'FINISHED'}

class POVRAY_OT_height_field_add(bpy.types.Operator, ImportHelper):
    bl_idname = "pov.addheightfield"
    bl_label = "Height Field"
    bl_description = "Add Height Field "
    bl_options = {'REGISTER', 'UNDO'}

    # XXX Keep it in sync with __init__'s hf Primitive
    # filename_ext = ".png"

    # filter_glob = StringProperty(
            # default="*.exr;*.gif;*.hdr;*.iff;*.jpeg;*.jpg;*.pgm;*.png;*.pot;*.ppm;*.sys;*.tga;*.tiff;*.EXR;*.GIF;*.HDR;*.IFF;*.JPEG;*.JPG;*.PGM;*.PNG;*.POT;*.PPM;*.SYS;*.TGA;*.TIFF",
            # options={'HIDDEN'},
            # )
    quality = IntProperty(name = "Quality",
                      description = "",
                      default = 100, min = 1, max = 100)
    hf_filename = StringProperty(maxlen = 1024)

    hf_gamma = FloatProperty(
            name="Gamma",
            description="Gamma",
            min=0.0001, max=20.0, default=1.0)

    hf_premultiplied = BoolProperty(
            name="Premultiplied",
            description="Premultiplied",
            default=True)

    hf_smooth = BoolProperty(
            name="Smooth",
            description="Smooth",
            default=False)

    hf_water = FloatProperty(
            name="Water Level",
            description="Wather Level",
            min=0.00, max=1.00, default=0.0)

    hf_hierarchy = BoolProperty(
            name="Hierarchy",
            description="Height field hierarchy",
            default=True)
    def execute(self,context):
        props = self.properties
        impath = bpy.path.abspath(self.filepath)
        img = bpy.data.images.load(impath)
        im_name = img.name
        im_name, file_extension = os.path.splitext(im_name)
        hf_tex = bpy.data.textures.new('%s_hf_image'%im_name, type = 'IMAGE')
        hf_tex.image = img
        mat = bpy.data.materials.new('Tex_%s_hf'%im_name)
        hf_slot = mat.texture_slots.create(-1)
        hf_slot.texture = hf_tex
        layers = 20*[False]
        layers[0] = True
        quality = props.quality
        res = 100/quality
        w,h = hf_tex.image.size[:]
        w = int(w/res)
        h = int(h/res)
        bpy.ops.mesh.primitive_grid_add(x_subdivisions=w, y_subdivisions=h,radius = 0.5,layers=layers)
        ob = context.object
        ob.name = ob.data.name = '%s'%im_name
        ob.data.materials.append(mat)
        bpy.ops.object.mode_set(mode="EDIT")
        bpy.ops.mesh.noise(factor=1)
        bpy.ops.object.mode_set(mode="OBJECT")

        #needs a loop to select by index?
        #bpy.ops.object.material_slot_remove()
        #material just left there for now


        mat.texture_slots.clear(-1)
        bpy.ops.object.mode_set(mode="EDIT")
        bpy.ops.mesh.hide(unselected=False)
        bpy.ops.object.mode_set(mode="OBJECT")
        ob.pov.object_as = 'HEIGHT_FIELD'
        ob.pov.hf_filename = impath
        return {'FINISHED'}


############################TORUS############################################
def pov_torus_define(context, op, ob):
        if op:
            mas = op.mas
            mis = op.mis
            mar = op.mar
            mir = op.mir
        else:
            assert(ob)
            mas = ob.pov.torus_major_segments
            mis = ob.pov.torus_minor_segments
            mar = ob.pov.torus_major_radius
            mir = ob.pov.torus_minor_radius

            #keep object rotation and location for the add object operator
            obrot = ob.rotation_euler
            obloc = ob.location

            bpy.ops.object.mode_set(mode="EDIT")
            bpy.ops.mesh.reveal()
            bpy.ops.mesh.select_all(action='SELECT')
            bpy.ops.mesh.delete(type='VERT')
            bpy.ops.mesh.primitive_torus_add(rotation = obrot, location = obloc, major_segments=mas, minor_segments=mis,major_radius=mar, minor_radius=mir)


            bpy.ops.mesh.hide(unselected=False)
            bpy.ops.object.mode_set(mode="OBJECT")


        if not ob:
            bpy.ops.mesh.primitive_torus_add(major_segments=mas, minor_segments=mis,major_radius=mar, minor_radius=mir)
            ob = context.object
            ob.name =  ob.data.name = "PovTorus"
            ob.pov.object_as = "TORUS"
            ob.pov.torus_major_segments = mas
            ob.pov.torus_minor_segments = mis
            ob.pov.torus_major_radius = mar
            ob.pov.torus_minor_radius = mir
            bpy.ops.object.mode_set(mode="EDIT")
            bpy.ops.mesh.hide(unselected=False)
            bpy.ops.object.mode_set(mode="OBJECT")

class POVRAY_OT_torus_add(bpy.types.Operator):
    bl_idname = "pov.addtorus"
    bl_label = "Torus"
    bl_description = "Add Torus"
    bl_options = {'REGISTER', 'UNDO'}

    # XXX Keep it in sync with __init__'s torus Primitive
    mas = IntProperty(name = "Major Segments",
                    description = "",
                    default = 48, min = 3, max = 720)
    mis = IntProperty(name = "Minor Segments",
                    description = "",
                    default = 12, min = 3, max = 720)
    mar = FloatProperty(name = "Major Radius",
                    description = "",
                    default = 1.0)
    mir = FloatProperty(name = "Minor Radius",
                    description = "",
                    default = 0.25)
    def execute(self,context):
        props = self.properties
        mar = props.mar
        mir = props.mir
        mas = props.mas
        mis = props.mis
        pov_torus_define(context, self, None)
        self.report({'INFO'}, "This native POV-Ray primitive "
                                 "won't have any vertex to show in edit mode")
        return {'FINISHED'}


class POVRAY_OT_torus_update(bpy.types.Operator):
    bl_idname = "pov.torus_update"
    bl_label = "Update"
    bl_description = "Update Torus"
    bl_options = {'REGISTER', 'UNDO'}
    COMPAT_ENGINES = {'POVRAY_RENDER'}

    @classmethod
    def poll(cls, context):
        engine = context.scene.render.engine
        ob = context.object
        return (ob and ob.data and ob.type == 'MESH' and engine in cls.COMPAT_ENGINES)

    def execute(self, context):

        pov_torus_define(context, None, context.object)

        return {'FINISHED'}

###################################################################################


class POVRAY_OT_prism_add(bpy.types.Operator):
    bl_idname = "pov.addprism"
    bl_label = "Prism"
    bl_description = "Create Prism"
    bl_options = {'REGISTER', 'UNDO'}

    prism_n = IntProperty(name = "Sides",
                description = "Number of sides",
                default = 5, min = 3, max = 720)
    prism_r = FloatProperty(name = "Radius",
                    description = "Radius",
                    default = 1.0)
    def execute(self,context):

        props = self.properties
        loftData = bpy.data.curves.new('Prism', type='CURVE')
        loftData.dimensions = '2D'
        loftData.resolution_u = 2
        loftData.show_normal_face = False
        loftData.extrude = 2
        n=props.prism_n
        r=props.prism_r
        coords = []
        z = 0
        angle = 0
        for p in range(n):
            x = r*cos(angle)
            y = r*sin(angle)
            coords.append((x,y,z))
            angle+=pi*2/n
        poly = loftData.splines.new('POLY')
        poly.points.add(len(coords)-1)
        for i, coord in enumerate(coords):
            x,y,z = coord
            poly.points[i].co = (x, y, z, 1)
        poly.use_cyclic_u = True

        ob = bpy.data.objects.new('Prism_shape', loftData)
        scn = bpy.context.scene
        scn.objects.link(ob)
        scn.objects.active = ob
        ob.select = True
        ob.pov.curveshape = "prism"
        ob.name = ob.data.name = "Prism"
        return {'FINISHED'}

##############################PARAMETRIC######################################
def pov_parametric_define(context, op, ob):
        if op:
            u_min = op.u_min
            u_max = op.u_max
            v_min = op.v_min
            v_max = op.v_max
            x_eq = op.x_eq
            y_eq = op.y_eq
            z_eq = op.z_eq

        else:
            assert(ob)
            u_min = ob.pov.u_min
            u_max = ob.pov.u_max
            v_min = ob.pov.v_min
            v_max = ob.pov.v_max
            x_eq = ob.pov.x_eq
            y_eq = ob.pov.y_eq
            z_eq = ob.pov.z_eq

            #keep object rotation and location for the updated object
            obloc = ob.location
            obrot = ob.rotation_euler # In radians
            #Parametric addon has no loc rot, some extra work is needed
            #in case cursor has moved
            curloc = bpy.context.scene.cursor_location


            bpy.ops.object.mode_set(mode="EDIT")
            bpy.ops.mesh.reveal()
            bpy.ops.mesh.select_all(action='SELECT')
            bpy.ops.mesh.delete(type='VERT')
            bpy.ops.mesh.primitive_xyz_function_surface(x_eq=x_eq, y_eq=y_eq, z_eq=z_eq, range_u_min=u_min, range_u_max=u_max, range_v_min=v_min, range_v_max=v_max)
            bpy.ops.mesh.select_all(action='SELECT')
            #extra work:
            bpy.ops.transform.translate(value=(obloc-curloc), proportional_size=1)
            bpy.ops.transform.rotate(axis=obrot, proportional_size=1)

            bpy.ops.mesh.hide(unselected=False)
            bpy.ops.object.mode_set(mode="OBJECT")


        if not ob:
            bpy.ops.mesh.primitive_xyz_function_surface(x_eq=x_eq, y_eq=y_eq, z_eq=z_eq, range_u_min=u_min, range_u_max=u_max, range_v_min=v_min, range_v_max=v_max)
            ob = context.object
            ob.name =  ob.data.name = "PovParametric"
            ob.pov.object_as = "PARAMETRIC"

            ob.pov.u_min = u_min
            ob.pov.u_max = u_max
            ob.pov.v_min = v_min
            ob.pov.v_max = v_max
            ob.pov.x_eq = x_eq
            ob.pov.y_eq = y_eq
            ob.pov.z_eq = z_eq

            bpy.ops.object.mode_set(mode="EDIT")
            bpy.ops.mesh.hide(unselected=False)
            bpy.ops.object.mode_set(mode="OBJECT")
class POVRAY_OT_parametric_add(bpy.types.Operator):
    bl_idname = "pov.addparametric"
    bl_label = "Parametric"
    bl_description = "Add Paramertic"
    bl_options = {'REGISTER', 'UNDO'}

    # XXX Keep it in sync with __init__'s Parametric primitive
    u_min = FloatProperty(name = "U Min",
                    description = "",
                    default = 0.0)
    v_min = FloatProperty(name = "V Min",
                    description = "",
                    default = 0.0)
    u_max = FloatProperty(name = "U Max",
                    description = "",
                    default = 6.28)
    v_max = FloatProperty(name = "V Max",
                    description = "",
                    default = 12.57)
    x_eq = StringProperty(
                    maxlen=1024, default = "cos(v)*(1+cos(u))*sin(v/8)")
    y_eq = StringProperty(
                    maxlen=1024, default = "sin(u)*sin(v/8)+cos(v/8)*1.5")
    z_eq = StringProperty(
                    maxlen=1024, default = "sin(v)*(1+cos(u))*sin(v/8)")

    def execute(self,context):
        props = self.properties
        u_min = props.u_min
        v_min = props.v_min
        u_max = props.u_max
        v_max = props.v_max
        x_eq = props.x_eq
        y_eq = props.y_eq
        z_eq = props.z_eq

        pov_parametric_define(context, self, None)
        self.report({'INFO'}, "This native POV-Ray primitive "
                                 "won't have any vertex to show in edit mode")
        return {'FINISHED'}

class POVRAY_OT_parametric_update(bpy.types.Operator):
    bl_idname = "pov.parametric_update"
    bl_label = "Update"
    bl_description = "Update parametric object"
    bl_options = {'REGISTER', 'UNDO'}
    COMPAT_ENGINES = {'POVRAY_RENDER'}

    @classmethod
    def poll(cls, context):
        engine = context.scene.render.engine
        ob = context.object
        return (ob and ob.data and ob.type == 'MESH' and engine in cls.COMPAT_ENGINES)

    def execute(self, context):

        pov_parametric_define(context, None, context.object)

        return {'FINISHED'}
#######################################################################
class POVRAY_OT_shape_polygon_to_circle_add(bpy.types.Operator):
    bl_idname = "pov.addpolygontocircle"
    bl_label = "Polygon To Circle Blending"
    bl_description = "Add Polygon To Circle Blending Surface"
    bl_options = {'REGISTER', 'UNDO'}
    COMPAT_ENGINES = {'POVRAY_RENDER'}

    # XXX Keep it in sync with __init__'s polytocircle properties
    polytocircle_resolution = IntProperty(name = "Resolution",
                    description = "",
                    default = 3, min = 0, max = 256)
    polytocircle_ngon = IntProperty(name = "NGon",
                    description = "",
                    min = 3, max = 64,default = 5)
    polytocircle_ngonR = FloatProperty(name = "NGon Radius",
                    description = "",
                    default = 0.3)
    polytocircle_circleR = FloatProperty(name = "Circle Radius",
                    description = "",
                    default = 1.0)
    def execute(self,context):
        props = self.properties
        ngon = props.polytocircle_ngon
        ngonR = props.polytocircle_ngonR
        circleR = props.polytocircle_circleR
        resolution = props.polytocircle_resolution
        layers = 20*[False]
        layers[0] = True
        bpy.ops.mesh.primitive_circle_add(vertices=ngon, radius=ngonR, fill_type='NGON',enter_editmode=True, layers=layers)
        bpy.ops.transform.translate(value=(0, 0, 1))
        bpy.ops.mesh.subdivide(number_cuts=resolution)
        numCircleVerts = ngon + (ngon*resolution)
        bpy.ops.mesh.select_all(action='DESELECT')
        bpy.ops.mesh.primitive_circle_add(vertices=numCircleVerts, radius=circleR, fill_type='NGON',enter_editmode=True, layers=layers)
        bpy.ops.transform.translate(value=(0, 0, -1))
        bpy.ops.mesh.select_all(action='SELECT')
        bpy.ops.mesh.bridge_edge_loops()
        if ngon < 5:
            bpy.ops.mesh.select_all(action='DESELECT')
            bpy.ops.mesh.primitive_circle_add(vertices=ngon, radius=ngonR, fill_type='TRIFAN',enter_editmode=True, layers=layers)
            bpy.ops.transform.translate(value=(0, 0, 1))
            bpy.ops.mesh.select_all(action='SELECT')
            bpy.ops.mesh.remove_doubles()
        bpy.ops.object.mode_set(mode='OBJECT')
        ob = context.object
        ob.name = "Polygon_To_Circle"
        ob.pov.object_as = 'POLYCIRCLE'
        ob.pov.ngon = ngon
        ob.pov.ngonR = ngonR
        ob.pov.circleR = circleR
        bpy.ops.object.mode_set(mode="EDIT")
        bpy.ops.mesh.hide(unselected=False)
        bpy.ops.object.mode_set(mode="OBJECT")
        return {'FINISHED'}

#############################IMPORT

class ImportPOV(bpy.types.Operator, ImportHelper):
    """Load Povray files"""
    bl_idname = "import_scene.pov"
    bl_label = "POV-Ray files (.pov/.inc)"
    bl_options = {'PRESET', 'UNDO'}
    COMPAT_ENGINES = {'POVRAY_RENDER'}

    # -----------
    # File props.
    files = CollectionProperty(type=bpy.types.OperatorFileListElement, options={'HIDDEN', 'SKIP_SAVE'})
    directory = StringProperty(maxlen=1024, subtype='FILE_PATH', options={'HIDDEN', 'SKIP_SAVE'})

    filename_ext = {".pov",".inc"}
    filter_glob = StringProperty(
            default="*.pov;*.inc",
            options={'HIDDEN'},
            )

    import_at_cur = BoolProperty(name="Import at Cursor Location",
                                    description = "Ignore Object Matrix",
                                    default=False)

    def execute(self, context):
        from mathutils import Matrix
        verts = []
        faces = []
        materials = []
        blendMats = [] ##############
        povMats = [] ##############
        colors = []
        matNames = []
        lenverts = None
        lenfaces = None
        suffix = -1
        name = 'Mesh2_%s'%suffix
        name_search = False
        verts_search = False
        faces_search = False
        plane_search = False
        box_search = False
        cylinder_search = False
        sphere_search = False
        cone_search = False
        tex_search = False ##################
        cache = []
        matrixes = {}
        writematrix = False
        index = None
        value = None
        #filepov = bpy.path.abspath(self.filepath) #was used for single files

        def mat_search(cache):
            r = g = b = 0.5
            f = t = 0
            color = None

            for item, value in enumerate(cache):

                if value == 'texture':
                    pass

                if value == 'pigment':

                    if cache[item+2] in {'rgb','srgb'}:
                        pass

                    elif cache[item+2] in {'rgbf','srgbf'}:
                        pass

                    elif cache[item+2] in {'rgbt','srgbt'}:
                        try:
                            r,g,b,t = float(cache[item+3]),float(cache[item+4]),float(cache[item+5]),float(cache[item+6])
                        except:
                            r = g = b = t = float(cache[item+2])
                        color = (r,g,b,t)

                    elif cache[item+2] in {'rgbft','srgbft'}:
                        pass

                    else:
                        pass

            if colors == [] or (colors != [] and color not in colors):
                colors.append(color)
                name = ob.name+"_mat"
                matNames.append(name)
                mat = bpy.data.materials.new(name)
                mat.diffuse_color = (r,g,b)
                mat.alpha = 1-t
                if mat.alpha != 1:
                    mat.use_transparency=True
                ob.data.materials.append(mat)

            else:
                for i, value in enumerate(colors):
                    if color == value:
                        ob.data.materials.append(bpy.data.materials[matNames[i]])
        for file in self.files:
            print ("Importing file: "+ file.name)
            filepov = self.directory + file.name
            for line in open(filepov):
                string = line.replace("{"," ")
                string = string.replace("}"," ")
                string = string.replace("<"," ")
                string = string.replace(">"," ")
                string = string.replace(","," ")
                lw = string.split()
                lenwords = len(lw)
                if lw:
                    if lw[0] == "object":
                        writematrix = True
                    if writematrix:
                        if lw[0] not in {"object","matrix"}:
                            index = lw[0]
                        if lw[0] in {"matrix"}:
                            value = [float(lw[1]),float(lw[2]),float(lw[3]),\
                                        float(lw[4]),float(lw[5]),float(lw[6]),\
                                        float(lw[7]),float(lw[8]),float(lw[9]),\
                                        float(lw[10]),float(lw[11]),float(lw[12])]
                            matrixes[index]=value
                            writematrix = False
            for line in open(filepov):
                S = line.replace("{"," { ")
                S = S.replace("}"," } ")
                S = S.replace(","," ")
                S = S.replace("<","")
                S = S.replace(">"," ")
                S = S.replace("="," = ")
                S = S.replace(";"," ; ")
                S = S.split()
                lenS= len(S)
                for i,word in enumerate(S):
    ##################Primitives Import##################
                    if word == 'cone':
                        cone_search = True
                        name_search = False
                    if cone_search:
                        cache.append(word)
                        if cache[-1] == '}':
                            try:
                                x0 = float(cache[2])
                                y0 = float(cache[3])
                                z0 = float(cache[4])
                                r0 = float(cache[5])
                                x1 = float(cache[6])
                                y1 = float(cache[7])
                                z1 = float(cache[8])
                                r1 = float(cache[9])
                                # Y is height in most pov files, not z
                                bpy.ops.pov.cone_add(base=r0, cap=r1, height=(y1-y0))
                                ob = context.object
                                ob.location = (x0,y0,z0)
                                #ob.scale = (r,r,r)
                                mat_search(cache)
                            except (ValueError):
                                pass
                            cache = []
                            cone_search = False
                    if word == 'plane':
                        plane_search = True
                        name_search = False
                    if plane_search:
                        cache.append(word)
                        if cache[-1] == '}':
                            try:
                                bpy.ops.pov.addplane()
                                ob = context.object
                                mat_search(cache)
                            except (ValueError):
                                pass
                            cache = []
                            plane_search = False
                    if word == 'box':
                        box_search = True
                        name_search = False
                    if box_search:
                        cache.append(word)
                        if cache[-1] == '}':
                            try:
                                x0 = float(cache[2])
                                y0 = float(cache[3])
                                z0 = float(cache[4])
                                x1 = float(cache[5])
                                y1 = float(cache[6])
                                z1 = float(cache[7])
                                #imported_corner_1=(x0, y0, z0)
                                #imported_corner_2 =(x1, y1, z1)
                                center = ((x0 + x1)/2,(y0 + y1)/2,(z0 + z1)/2)
                                bpy.ops.pov.addbox()
                                ob = context.object
                                ob.location = center
                                mat_search(cache)

                            except (ValueError):
                                pass
                            cache = []
                            box_search = False
                    if word == 'cylinder':
                        cylinder_search = True
                        name_search = False
                    if cylinder_search:
                        cache.append(word)
                        if cache[-1] == '}':
                            try:
                                x0 = float(cache[2])
                                y0 = float(cache[3])
                                z0 = float(cache[4])
                                x1 = float(cache[5])
                                y1 = float(cache[6])
                                z1 = float(cache[7])
                                imported_cyl_loc=(x0, y0, z0)
                                imported_cyl_loc_cap =(x1, y1, z1)

                                r = float(cache[8])


                                vec = Vector(imported_cyl_loc_cap ) - Vector(imported_cyl_loc)
                                depth = vec.length
                                rot = Vector((0, 0, 1)).rotation_difference(vec)  # Rotation from Z axis.
                                trans = rot * Vector((0, 0, depth / 2)) # Such that origin is at center of the base of the cylinder.
                                #center = ((x0 + x1)/2,(y0 + y1)/2,(z0 + z1)/2)
                                scaleZ = sqrt((x1-x0)**2+(y1-y0)**2+(z1-z0)**2)/2
                                bpy.ops.pov.addcylinder(R=r, imported_cyl_loc=imported_cyl_loc, imported_cyl_loc_cap=imported_cyl_loc_cap)
                                ob = context.object
                                ob.location = (x0, y0, z0)
                                ob.rotation_euler = rot.to_euler()
                                ob.scale = (1,1,scaleZ)

                                #scale data rather than obj?
                                # bpy.ops.object.mode_set(mode='EDIT')
                                # bpy.ops.mesh.reveal()
                                # bpy.ops.mesh.select_all(action='SELECT')
                                # bpy.ops.transform.resize(value=(1,1,scaleZ), constraint_orientation='LOCAL')
                                # bpy.ops.mesh.hide(unselected=False)
                                # bpy.ops.object.mode_set(mode='OBJECT')

                                mat_search(cache)

                            except (ValueError):
                                pass
                            cache = []
                            cylinder_search = False
                    if word == 'sphere':
                        sphere_search = True
                        name_search = False
                    if sphere_search:
                        cache.append(word)
                        if cache[-1] == '}':
                            x = y = z = r = 0
                            try:
                                x = float(cache[2])
                                y = float(cache[3])
                                z = float(cache[4])
                                r = float(cache[5])

                            except (ValueError):
                                pass
                            except:
                                x = y = z = float(cache[2])
                                r = float(cache[3])
                            bpy.ops.pov.addsphere(R=r, imported_loc=(x, y, z))
                            ob = context.object
                            ob.location = (x,y,z)
                            ob.scale = (r,r,r)
                            mat_search(cache)
                            cache = []
                            sphere_search = False
##################End Primitives Import##################
                    if word == '#declare':
                        name_search = True
                    if name_search:
                        cache.append(word)
                        if word == 'mesh2':
                            name_search = False
                            if cache[-2] == '=':
                                name = cache[-3]
                            else:
                                suffix+=1
                            cache = []
                        if word in {'texture',';'}:
                            name_search = False
                            cache = []
                    if word == 'vertex_vectors':
                         verts_search = True
                    if verts_search:
                        cache.append(word)
                        if word == '}':
                            verts_search = False
                            lenverts=cache[2]
                            cache.pop()
                            cache.pop(0)
                            cache.pop(0)
                            cache.pop(0)
                            for i in range(int(lenverts)):
                                x=i*3
                                y=(i*3)+1
                                z=(i*3)+2
                                verts.append((float(cache[x]),float(cache[y]),float(cache[z])))
                            cache = []
                    #if word == 'face_indices':
                         #faces_search = True
                    if word == 'texture_list': ########
                        tex_search = True #######
                    if tex_search: #########
                        if word not in {'texture_list','texture','{','}','face_indices'} and word.isdigit() == False: ##############
                            povMats.append(word) #################
                    if word == 'face_indices':
                        tex_search = False ################
                        faces_search = True
                    if faces_search:
                        cache.append(word)
                        if word == '}':
                            faces_search = False
                            lenfaces = cache[2]
                            cache.pop()
                            cache.pop(0)
                            cache.pop(0)
                            cache.pop(0)
                            lf = int(lenfaces)
                            var=int(len(cache)/lf)
                            for i in range(lf):
                                if var == 3:
                                    v0=i*3
                                    v1=i*3+1
                                    v2=i*3+2
                                    faces.append((int(cache[v0]),int(cache[v1]),int(cache[v2])))
                                if var == 4:
                                    v0=i*4
                                    v1=i*4+1
                                    v2=i*4+2
                                    m=i*4+3
                                    materials.append((int(cache[m])))
                                    faces.append((int(cache[v0]),int(cache[v1]),int(cache[v2])))
                                if var == 6:
                                    v0=i*6
                                    v1=i*6+1
                                    v2=i*6+2
                                    m0=i*6+3
                                    m1=i*6+4
                                    m2=i*6+5
                                    materials.append((int(cache[m0]),int(cache[m1]),int(cache[m2])))
                                    faces.append((int(cache[v0]),int(cache[v1]),int(cache[v2])))
                            #mesh = pov_define_mesh(None, verts, [], faces, name, hide_geometry=False)
                            #ob_base = object_utils.object_data_add(context, mesh, operator=None)
                            #ob = ob_base.object

                            me = bpy.data.meshes.new(name) ########
                            ob = bpy.data.objects.new(name, me) ##########
                            bpy.context.scene.objects.link(ob) #########
                            me.from_pydata(verts, [], faces) ############

                            for mat in bpy.data.materials: ##############
                                blendMats.append(mat.name) #############
                            for mName in povMats: #####################
                                if mName not in blendMats: ###########
                                    povMat = bpy.data.materials.new(mName) #################
                                    mat_search(cache)
                                ob.data.materials.append(bpy.data.materials[mName]) ###################
                            if materials: ##################
                                for i,val in enumerate(materials): ####################
                                    try: ###################
                                        ob.data.polygons[i].material_index = val ####################
                                    except TypeError: ###################
                                        ob.data.polygons[i].material_index = int(val[0]) ##################

                            blendMats = [] #########################
                            povMats = [] #########################
                            materials = [] #########################
                            cache = []
                            name_search = True
                            if name in matrixes and self.import_at_cur==False:
                                global_matrix = Matrix.Rotation(pi / 2.0, 4, 'X')
                                ob = bpy.context.object
                                matrix=ob.matrix_world
                                v=matrixes[name]
                                matrix[0][0] = v[0]
                                matrix[1][0] = v[1]
                                matrix[2][0] = v[2]
                                matrix[0][1] = v[3]
                                matrix[1][1] = v[4]
                                matrix[2][1] = v[5]
                                matrix[0][2] = v[6]
                                matrix[1][2] = v[7]
                                matrix[2][2] = v[8]
                                matrix[0][3] = v[9]
                                matrix[1][3] = v[10]
                                matrix[2][3] = v[11]
                                matrix = global_matrix*ob.matrix_world
                                ob.matrix_world = matrix
                            verts = []
                            faces = []


                    # if word == 'pigment':
                        # try:
                            # #all indices have been incremented once to fit a bad test file
                            # r,g,b,t = float(S[2]),float(S[3]),float(S[4]),float(S[5])
                            # color = (r,g,b,t)

                        # except (IndexError):
                            # #all indices have been incremented once to fit alternate test file
                            # r,g,b,t = float(S[3]),float(S[4]),float(S[5]),float(S[6])
                            # color = (r,g,b,t)
                        # except UnboundLocalError:
                            # # In case no transmit is specified ? put it to 0
                            # r,g,b,t = float(S[2]),float(S[3]),float(S[4],0)
                            # color = (r,g,b,t)

                        # except (ValueError):
                            # color = (0.8,0.8,0.8,0)
                            # pass

                        # if colors == [] or (colors != [] and color not in colors):
                            # colors.append(color)
                            # name = ob.name+"_mat"
                            # matNames.append(name)
                            # mat = bpy.data.materials.new(name)
                            # mat.diffuse_color = (r,g,b)
                            # mat.alpha = 1-t
                            # if mat.alpha != 1:
                                # mat.use_transparency=True
                            # ob.data.materials.append(mat)
                            # print (colors)
                        # else:
                            # for i in range(len(colors)):
                                # if color == colors[i]:
                                    # ob.data.materials.append(bpy.data.materials[matNames[i]])

        ##To keep Avogadro Camera angle:
        # for obj in bpy.context.scene.objects:
            # if obj.type == "CAMERA":
                # track = obj.constraints.new(type = "TRACK_TO")
                # track.target = ob
                # track.track_axis ="TRACK_NEGATIVE_Z"
                # track.up_axis = "UP_Y"
                # obj.location = (0,0,0)
        return {'FINISHED'}

