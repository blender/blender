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
# Main panel for windows
# Author: Antonio Vazquez (antonioya)
#
# This code is base on the windows generator add-on created by SayProduction
# and has been adapted to continuous editing and cycles materials
#
# ----------------------------------------------------------
# noinspection PyUnresolvedReferences
import bpy
from math import cos, sin, radians, sqrt, pi
from mathutils import Vector
from bpy.types import Operator, PropertyGroup, Object, Panel
from bpy.props import StringProperty, FloatProperty, BoolProperty, IntProperty, FloatVectorProperty, \
    CollectionProperty, EnumProperty
from .achm_tools import *


def fitil(vr, fc, px, pz, x, y, z, zz, xx):
    k3 = z * 2
    vr.extend([[px[x] + xx, -z + zz, pz[y] + xx], [px[x] + xx + k3, -z + zz, pz[y] + xx + k3],
               [px[x] + xx + k3, z + zz, pz[y] + xx + k3], [px[x] + xx, z + zz, pz[y] + xx]])
    vr.extend([[px[x] + xx, -z + zz, pz[y + 1] - xx], [px[x] + xx + k3, -z + zz, pz[y + 1] - xx - k3],
               [px[x] + xx + k3, z + zz, pz[y + 1] - xx - k3], [px[x] + xx, z + zz, pz[y + 1] - xx]])
    vr.extend([[px[x + 1] - xx, -z + zz, pz[y + 1] - xx], [px[x + 1] - xx - k3, -z + zz, pz[y + 1] - xx - k3],
               [px[x + 1] - xx - k3, z + zz, pz[y + 1] - xx - k3], [px[x + 1] - xx, z + zz, pz[y + 1] - xx]])
    vr.extend([[px[x + 1] - xx, -z + zz, pz[y] + xx], [px[x + 1] - xx - k3, -z + zz, pz[y] + xx + k3],
               [px[x + 1] - xx - k3, z + zz, pz[y] + xx + k3], [px[x + 1] - xx, z + zz, pz[y] + xx]])
    n = len(vr)
    fc.extend([[n - 16, n - 15, n - 11, n - 12], [n - 15, n - 14, n - 10, n - 11], [n - 14, n - 13, n - 9, n - 10]])
    fc.extend([[n - 12, n - 11, n - 7, n - 8], [n - 11, n - 10, n - 6, n - 7], [n - 10, n - 9, n - 5, n - 6]])
    fc.extend([[n - 8, n - 7, n - 3, n - 4], [n - 7, n - 6, n - 2, n - 3], [n - 6, n - 5, n - 1, n - 2]])
    fc.extend([[n - 4, n - 3, n - 15, n - 16], [n - 3, n - 2, n - 14, n - 15], [n - 2, n - 1, n - 13, n - 14]])
    z = 0.005
    vr.extend([[px[x] + xx + k3, -z + zz, pz[y] + xx + k3], [px[x] + xx + k3, -z + zz, pz[y + 1] - xx - k3],
               [px[x + 1] - xx - k3, -z + zz, pz[y + 1] - xx - k3], [px[x + 1] - xx - k3, -z + zz, pz[y] + xx + k3]])
    vr.extend([[px[x] + xx + k3, z + zz, pz[y] + xx + k3], [px[x] + xx + k3, z + zz, pz[y + 1] - xx - k3],
               [px[x + 1] - xx - k3, z + zz, pz[y + 1] - xx - k3], [px[x + 1] - xx - k3, z + zz, pz[y] + xx + k3]])
    fc.extend([[n + 1, n + 0, n + 3, n + 2], [n + 4, n + 5, n + 6, n + 7]])


def kapak(vr, fc, px, pz, x, y, z, zz):
    k2 = z * 2
    vr.extend(
        [[px[x], -z + zz, pz[y]], [px[x] + k2, -z + zz, pz[y] + k2], [px[x] + k2, z + zz, pz[y] + k2],
         [px[x], z + zz, pz[y]]])
    vr.extend([[px[x], -z + zz, pz[y + 1]], [px[x] + k2, -z + zz, pz[y + 1] - k2], [px[x] + k2, z + zz, pz[y + 1] - k2],
               [px[x], z + zz, pz[y + 1]]])
    vr.extend(
        [[px[x + 1], -z + zz, pz[y + 1]], [px[x + 1] - k2, -z + zz, pz[y + 1] - k2],
         [px[x + 1] - k2, z + zz, pz[y + 1] - k2],
         [px[x + 1], z + zz, pz[y + 1]]])
    vr.extend([[px[x + 1], -z + zz, pz[y]], [px[x + 1] - k2, -z + zz, pz[y] + k2], [px[x + 1] - k2, z + zz, pz[y] + k2],
               [px[x + 1], z + zz, pz[y]]])
    n = len(vr)
    fc.extend([[n - 16, n - 15, n - 11, n - 12], [n - 15, n - 14, n - 10, n - 11], [n - 14, n - 13, n - 9, n - 10],
               [n - 13, n - 16, n - 12, n - 9]])
    fc.extend([[n - 12, n - 11, n - 7, n - 8], [n - 11, n - 10, n - 6, n - 7], [n - 10, n - 9, n - 5, n - 6],
               [n - 9, n - 12, n - 8, n - 5]])
    fc.extend([[n - 8, n - 7, n - 3, n - 4], [n - 7, n - 6, n - 2, n - 3], [n - 6, n - 5, n - 1, n - 2],
               [n - 5, n - 8, n - 4, n - 1]])
    fc.extend([[n - 4, n - 3, n - 15, n - 16], [n - 3, n - 2, n - 14, n - 15], [n - 2, n - 1, n - 13, n - 14],
               [n - 1, n - 4, n - 16, n - 13]])


# -----------------------------------------
# Set default values for each window type
# -----------------------------------------
def set_defaults(s):
    if s.prs == '1':
        s.gen = 3
        s.yuk = 1
        s.kl1 = 5
        s.kl2 = 5
        s.fk = 2
        s.gny0 = 190
        s.mr = True
        s.gnx0 = 60
        s.gnx1 = 110
        s.gnx2 = 60
        s.k00 = True
        s.k01 = False
        s.k02 = True
    if s.prs == '2':
        s.gen = 3
        s.yuk = 1
        s.kl1 = 5
        s.kl2 = 5
        s.fk = 2
        s.gny0 = 190
        s.mr = True
        s.gnx0 = 60
        s.gnx1 = 60
        s.gnx2 = 60
        s.k00 = True
        s.k01 = False
        s.k02 = True
    if s.prs == '3':
        s.gen = 3
        s.yuk = 1
        s.kl1 = 5
        s.kl2 = 5
        s.fk = 2
        s.gny0 = 190
        s.mr = True
        s.gnx0 = 55
        s.gnx1 = 50
        s.gnx2 = 55
        s.k00 = True
        s.k01 = False
        s.k02 = True
    if s.prs == '4':
        s.gen = 3
        s.yuk = 1
        s.kl1 = 5
        s.kl2 = 5
        s.fk = 2
        s.gny0 = 150
        s.mr = True
        s.gnx0 = 55
        s.gnx1 = 50
        s.gnx2 = 55
        s.k00 = True
        s.k01 = False
        s.k02 = True
    if s.prs == '5':
        s.gen = 3
        s.yuk = 1
        s.kl1 = 5
        s.kl2 = 5
        s.fk = 2
        s.gny0 = 150
        s.mr = True
        s.gnx0 = 50
        s.gnx1 = 40
        s.gnx2 = 50
        s.k00 = True
        s.k01 = False
        s.k02 = True
    if s.prs == '6':
        s.gen = 1
        s.yuk = 1
        s.kl1 = 5
        s.kl2 = 5
        s.fk = 2
        s.gny0 = 40
        s.mr = True
        s.gnx0 = 40
        s.k00 = False
    if s.prs == '7':
        s.gen = 1
        s.yuk = 2
        s.kl1 = 5
        s.kl2 = 5
        s.fk = 2
        s.gny0 = 195
        s.gny1 = 40
        s.gnx0 = 70
        s.k00 = True
        s.k10 = False
        s.mr = False
    if s.prs == '8':
        s.gen = 1
        s.yuk = 2
        s.kl1 = 5
        s.kl2 = 5
        s.fk = 2
        s.gny0 = 180
        s.gny1 = 35
        s.gnx0 = 70
        s.k00 = True
        s.k10 = False
        s.mr = False


# ------------------------------------------------------------------
# Define operator class to create window panels
# ------------------------------------------------------------------
class AchmWinPanel(Operator):
    bl_idname = "mesh.archimesh_winpanel"
    bl_label = "Panel Window"
    bl_description = "Generate editable flat windows"
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
    # noinspection PyUnusedLocal
    def execute(self, context):
        if bpy.context.mode == "OBJECT":
            create_window()
            return {'FINISHED'}
        else:
            self.report({'WARNING'}, "Archimesh: Option only valid in Object mode")
            return {'CANCELLED'}


# ------------------------------------------------------------------------------
# Create the window
# ------------------------------------------------------------------------------
def create_window():
    # deselect all objects
    for o in bpy.data.objects:
        o.select = False
    # Create main object
    window_mesh = bpy.data.meshes.new("Window")
    window_object = bpy.data.objects.new("Window", window_mesh)

    # Link object to scene
    bpy.context.scene.objects.link(window_object)
    window_object.WindowPanelGenerator.add()
    window_object.location = bpy.context.scene.cursor_location

    # Shape the mesh.
    do_mesh(window_object, window_mesh)

    # deactivate others
    for o in bpy.data.objects:
        if o.select is True and o.name != window_object.name:
            o.select = False

    # Select, and activate object
    window_object.select = True
    bpy.context.scene.objects.active = window_object

    do_ctrl_box(window_object)
    # Reselect
    window_object.select = True
    bpy.context.scene.objects.active = window_object


# ------------------------------------------------------------------------------
# Update mesh of the window
# ------------------------------------------------------------------------------
# noinspection PyUnusedLocal
def update_window(self, context):
    # When update, the active object is the main object.
    o = bpy.context.active_object
    oldmesh = o.data
    oldname = o.data.name
    # Now deselect that object to not delete it.
    o.select = False
    # # and create a new mesh for the object:
    # tmp_mesh = bpy.data.meshes.new("temp")
    # deselect all objects
    for obj in bpy.data.objects:
        obj.select = False
    # ---------------------------------
    #  Clear Parent objects (autohole)
    # ---------------------------------
    myparent = o.parent
    if myparent is not None:
        ploc = myparent.location
    else:
        ploc = o.location
    if myparent is not None:
        o.parent = None
        o.location = ploc
        # remove_children(parent)
        for child in myparent.children:
            # noinspection PyBroadException
            try:
                # clear child data
                child.hide = False  # must be visible to avoid bug
                child.hide_render = False  # must be visible to avoid bug
                old = child.data
                child.select = True
                bpy.ops.object.delete()
                bpy.data.meshes.remove(old)
            except:
                pass

        myparent.select = True
        bpy.ops.object.delete()

    # Finally create all that again
    tmp_mesh = bpy.data.meshes.new("temp")
    do_mesh(o, tmp_mesh, True)
    o.data = tmp_mesh
    # Remove data (mesh of active object),
    if oldmesh.users == 0:
        bpy.data.meshes.remove(oldmesh)
    else:
        oldmesh.name = "invalid"

    tmp_mesh.name = oldname
    # deactivate others
    for ob in bpy.data.objects:
        if ob.select is True and ob.name != o.name:
            ob.select = False
    # and select, and activate, the object.
    o.select = True
    bpy.context.scene.objects.active = o

    do_ctrl_box(o)

    # Reselect
    o.select = True
    bpy.context.scene.objects.active = o


# ------------------------------------------------------------------------------
# Generate object
# For object, it only shapes mesh
# ------------------------------------------------------------------------------
# noinspection PyUnusedLocal
def do_mesh(myobject, tmp_mesh, update=False):
    # noinspection PyBroadException
    try:
        op = myobject.WindowPanelGenerator[0]
        # Create only mesh, because the object was created before.
        r = generate_window_object(op, tmp_mesh)
        if r is False:
            return False

        # refine unit
        remove_doubles(myobject)
        set_normals(myobject)

        # saves OpenGL data
        # sum width
        totx = myobject.dimensions.x
        op.glpoint_a = (-totx / 2, 0, 0)
        top_a, top_b, top_c = get_high_points(myobject, totx, op.UST)
        op.glpoint_b = (-totx / 2, 0, top_a)
        op.glpoint_c = (totx / 2, 0, top_b)
        op.glpoint_d = (0, 0, top_c)

        # Lock
        myobject.lock_location = (True, True, True)
        myobject.lock_rotation = (True, True, True)
        return True
    except:
        return False


# ------------------------------------------------------------------------------
# Generate ctrl box
#
# ------------------------------------------------------------------------------
# noinspection PyUnusedLocal
def do_ctrl_box(myobject):
    op = myobject.WindowPanelGenerator[0]
    # -------------------------
    # Create empty and parent
    # -------------------------
    bpy.ops.object.empty_add(type='PLAIN_AXES')
    myempty = bpy.data.objects[bpy.context.active_object.name]
    myempty.location = myobject.location

    myempty.name = "Window_Group"
    parentobject(myempty, myobject)
    myobject["archimesh.hole_enable"] = True
    # Rotate Empty
    myempty.rotation_euler.z = radians(op.r)
    # Create control box to open wall holes
    myctrl = create_ctrl_box(myobject, "CTRL_Hole")

    # Add custom property to detect Controller
    myctrl["archimesh.ctrl_hole"] = True

    set_normals(myctrl)
    myctrl.parent = myempty
    myctrl.location.x = 0
    myctrl.location.y = 0
    myctrl.location.z = 0
    myctrl.draw_type = 'WIRE'
    myctrl.hide = False
    myctrl.hide_render = True
    if bpy.context.scene.render.engine == 'CYCLES':
        myctrl.cycles_visibility.camera = False
        myctrl.cycles_visibility.diffuse = False
        myctrl.cycles_visibility.glossy = False
        myctrl.cycles_visibility.transmission = False
        myctrl.cycles_visibility.scatter = False
        myctrl.cycles_visibility.shadow = False

        mat = create_transparent_material("hidden_material", False)
        set_material(myctrl, mat)


# ------------------------------------------------------------------------------
# Update the parameters using a default value
# ------------------------------------------------------------------------------
# noinspection PyUnusedLocal
def update_using_default(self, context):
    o = context.object
    myobject = o.WindowPanelGenerator[0]
    if myobject.son != myobject.prs:
        set_defaults(myobject)
        myobject.son = myobject.prs


# ------------------------------------------------------------------------------
# Generate window object
# ------------------------------------------------------------------------------
def generate_window_object(op, mymesh):
    myvertex = []
    mfaces = []
    # noinspection PyBroadException
    try:
        rst, ft1, cam, mer, sm = generate_vertex_data(op, myvertex, mfaces)
        if rst is not True:
            return False

        mymesh.from_pydata(myvertex, [], mfaces)
        # Uncomment for debug
        # mymesh.validate(verbose=True)
        # Assign materials
        if op.mt1 == '1':
            mymesh.materials.append(create_diffuse_material("PVC", False, 1, 1, 1, 1, 1, 1))
        elif op.mt1 == '2':
            mymesh.materials.append(create_diffuse_material("Wood", False, 0.3, 0.2, 0.1, 0.3, 0.2, 0.1))
        elif op.mt1 == '3':
            mymesh.materials.append(create_diffuse_material("Plastic", False, 0, 0, 0, 0, 0, 0))
        if op.mt2 == '1':
            mymesh.materials.append(create_diffuse_material("PVC", False, 1, 1, 1, 1, 1, 1))
        elif op.mt2 == '2':
            mymesh.materials.append(create_diffuse_material("Wood", False, 0.3, 0.2, 0.1, 0.3, 0.2, 0.1))
        elif op.mt2 == '3':
            mymesh.materials.append(create_diffuse_material("Plastic", False, 0, 0, 0, 0, 0, 0))

        mymesh.materials.append(create_glass_material("Glass", False))
        if op.mr is True:
            mymesh.materials.append(create_diffuse_material("Marble", False, 0.9, 0.8, 0.7, 0.9, 0.8, 0.7))

        p = len(mymesh.polygons)
        for i in ft1:
            if -1 < i < p:
                mymesh.polygons[i].material_index = 1
        for i in cam:
            if -1 < i < p:
                mymesh.polygons[i].material_index = 2
        for i in mer:
            if -1 < i < p:
                mymesh.polygons[i].material_index = 3
        for i in sm:
            if -1 < i < p:
                mymesh.polygons[i].use_smooth = 1

        mymesh.update(calc_edges=True)

        return True
    except:
        return False


# -----------------------------------------
# Generate vertex and faces data
# -----------------------------------------
def generate_vertex_data(op, myvertex, myfaces):
    # noinspection PyBroadException
    try:
        h1 = 0
        c = 0
        t1 = 0

        mx = op.gen
        my = op.yuk
        k1 = op.kl1 / 100
        k2 = op.kl2 / 100
        k3 = op.fk / 200
        res = op.res

        u = op.kl1 / 100
        xlist = [0, round(u, 2)]
        if mx > 0:
            u += op.gnx0 / 100
            xlist.append(round(u, 2))
            u += k2
            xlist.append(round(u, 2))
        if mx > 1:
            u += op.gnx1 / 100
            xlist.append(round(u, 2))
            u += k2
            xlist.append(round(u, 2))
        if mx > 2:
            u += op.gnx2 / 100
            xlist.append(round(u, 2))
            u += k2
            xlist.append(round(u, 2))
        if mx > 3:
            u += op.gnx3 / 100
            xlist.append(round(u, 2))
            u += k2
            xlist.append(round(u, 2))
        if mx > 4:
            u += op.gnx4 / 100
            xlist.append(round(u, 2))
            u += k2
            xlist.append(round(u, 2))
        if mx > 5:
            u += op.gnx5 / 100
            xlist.append(round(u, 2))
            u += k2
            xlist.append(round(u, 2))
        if mx > 6:
            u += op.gnx6 / 100
            xlist.append(round(u, 2))
            u += k2
            xlist.append(round(u, 2))
        if mx > 7:
            u += op.gnx7 / 100
            xlist.append(round(u, 2))
            u += k2
            xlist.append(round(u, 2))

        xlist[-1] = xlist[-2] + k1

        u = op.kl1 / 100
        zlist = [0, round(u, 2)]
        if my > 0:
            u += op.gny0 / 100
            zlist.append(round(u, 2))
            u += k2
            zlist.append(round(u, 2))
        if my > 1:
            u += op.gny1 / 100
            zlist.append(round(u, 2))
            u += k2
            zlist.append(round(u, 2))
        if my > 2:
            u += op.gny2 / 100
            zlist.append(round(u, 2))
            u += k2
            zlist.append(round(u, 2))
        if my > 3:
            u += op.gny3 / 100
            zlist.append(round(u, 2))
            u += k2
            zlist.append(round(u, 2))
        if my > 4:
            u += op.gny4 / 100
            zlist.append(round(u, 2))
            u += k2
            zlist.append(round(u, 2))
        zlist[-1] = zlist[-2] + k1

        u = xlist[-1] / 2
        for i in range(0, len(xlist)):
            xlist[i] -= u
        kx = [[op.k00, op.k10, op.k20, op.k30, op.k40],
              [op.k01, op.k11, op.k21, op.k31, op.k41],
              [op.k02, op.k12, op.k22, op.k32, op.k42],
              [op.k03, op.k13, op.k23, op.k33, op.k43],
              [op.k04, op.k14, op.k24, op.k34, op.k44],
              [op.k05, op.k15, op.k25, op.k35, op.k45],
              [op.k06, op.k16, op.k26, op.k36, op.k46],
              [op.k07, op.k17, op.k27, op.k37, op.k47]]
        cam = []
        mer = []
        ftl = []
        sm = []
        # -------------------------
        # VERTICES
        # -------------------------
        myvertex.extend([[xlist[0], -k1 / 2, zlist[0]], [xlist[0], k1 / 2, zlist[0]]])
        for x in range(1, len(xlist) - 1):
            myvertex.extend([[xlist[x], -k1 / 2, zlist[1]], [xlist[x], k1 / 2, zlist[1]]])
        myvertex.extend([[xlist[-1], -k1 / 2, zlist[0]], [xlist[-1], k1 / 2, zlist[0]]])
        for z in range(2, len(zlist) - 2, 2):
            for x in range(0, len(xlist)):
                myvertex.extend([[xlist[x], -k1 / 2, zlist[z]], [xlist[x], k1 / 2, zlist[z]]])
            for x in range(0, len(xlist)):
                myvertex.extend([[xlist[x], -k1 / 2, zlist[z + 1]], [xlist[x], k1 / 2, zlist[z + 1]]])
        z = len(zlist) - 2
        myvertex.extend([[xlist[0], -k1 / 2, zlist[z + 1]], [xlist[0], k1 / 2, zlist[z + 1]]])
        alt = []
        ust = [len(myvertex) - 2, len(myvertex) - 1]
        for x in range(1, len(xlist) - 1):
            myvertex.extend([[xlist[x], -k1 / 2, zlist[z]], [xlist[x], k1 / 2, zlist[z]]])
            alt.extend([len(myvertex) - 2, len(myvertex) - 1])
        myvertex.extend([[xlist[-1], -k1 / 2, zlist[z + 1]], [xlist[-1], k1 / 2, zlist[z + 1]]])
        son = [len(myvertex) - 2, len(myvertex) - 1]
        # -------------------------
        # FACES
        # -------------------------
        myfaces.append([0, 1, 3 + mx * 4, 2 + mx * 4])
        fb = [0]
        fr = [1]
        for i in range(0, mx * 4, 4):
            myfaces.append([i + 3, i + 2, i + 4, i + 5])
            fb.extend([i + 2, i + 4])
            fr.extend([i + 3, i + 5])
        fr.append(3 + mx * 4)
        fb.append(2 + mx * 4)
        fb.reverse()
        myfaces.extend([fb, fr])
        # Yatay
        y = (mx * 4 + 4)
        v = mx * 4 + 2
        for z in range(0, (my - 1) * y * 2, y * 2):
            myfaces.extend([[z + y + 1, z + y, z + y + 4 + mx * 4, z + y + 5 + mx * 4],
                            [z + y + v, z + y + v + 1, z + y + v + 5 + mx * 4, z + y + v + 4 + mx * 4]])
            for i in range(0, mx * 4 + 2, 2):
                myfaces.extend([[z + i + y + 0, z + i + y + 2, z + i + y + v + 4, z + i + y + v + 2],
                                [z + i + y + 3, z + i + y + 1, z + i + y + v + 3, z + i + y + v + 5]])
            for i in range(0, mx * 4 - 3, 4):
                myfaces.extend([[z + i + y + 2, z + i + y + 3, z + i + y + 5, z + i + y + 4],
                                [z + i + y + v + 5, z + i + y + v + 4, z + i + y + v + 6,
                                 z + i + y + v + 7]])
        # Dikey
        for y in range(0, my):
            z = y * (mx * 4 + 4) * 2
            for i in range(0, mx * 4 + 2, 4):
                myfaces.extend([[z + i + 1, z + i + 0, z + i + v + 2, z + i + v + 3],
                                [z + i + 3, z + i + 1, z + i + v + 3, z + i + v + 5],
                                [z + i + 2, z + i + 3, z + i + v + 5, z + i + v + 4],
                                [z + i + 0, z + i + 2, z + i + v + 4, z + i + v + 2]])
        # Fitil
        if op.UST == '1':
            y1 = my
        else:
            y1 = my - 1
        for y in range(0, y1):
            for x in range(0, mx):
                if kx[x][y] is True:
                    kapak(myvertex, myfaces, xlist, zlist, x * 2 + 1, y * 2 + 1, k2 / 2, (k1 + k2) * 0.5 - 0.01)
                    fitil(myvertex, myfaces, xlist, zlist, x * 2 + 1, y * 2 + 1, k3, (k1 + k2) * 0.5 - 0.01, k2)
                else:
                    fitil(myvertex, myfaces, xlist, zlist, x * 2 + 1, y * 2 + 1, k3, 0, 0)
                m = len(myfaces)
                cam.extend([m - 1, m - 2])
                ftl.extend([m - 3, m - 4, m - 5, m - 6, m - 7, m - 8, m - 9, m - 10, m - 11, m - 12, m - 13, m - 14])
        # -----------------------------------------------------
        if op.UST == '1':  # Duz
            myfaces.append([ust[1], ust[0], son[0], son[1]])
            for i in range(0, mx * 4, 4):
                myfaces.append([alt[i], alt[i + 1], alt[i + 3], alt[i + 2]])
            on = [ust[0]]
            ar = [ust[1]]
            for i in range(0, len(alt) - 1, 2):
                on.append(alt[i])
                ar.append(alt[i + 1])
            on.append(son[0])
            myfaces.append(on)
            ar.append(son[1])
            ar.reverse()
            myfaces.append(ar)
        elif op.UST == '2':  # Arch
            if op.DT2 == '1':
                h1 = op.VL1 / 100
                if op.VL1 < 6:
                    h1 = 6 / 100

                if h1 < 0.01:
                    h1 = 0.01
                    # op.VL1 = 1
                elif h1 >= u:
                    h1 = u - 0.01
                    # op.VL1 = h1 * 100
                if h1 < 0.07:
                    h1 = 0.07

                h = sqrt(u ** 2 + h1 ** 2) / 2
                e = h * (u / h1)
                c = sqrt(h ** 2 + e ** 2)
                t1 = zlist[-1] - h1
            elif op.DT2 == '2':
                c = op.VL2 / 100
                if c < u + 0.01:
                    c = u + 0.01
                    # op.VL2 = c * 100
                t1 = sqrt(c ** 2 - u ** 2) + zlist[-1] - c
            r = c - k1
            z = zlist[-1] - c

            myvertex[ust[0]][2] = t1
            myvertex[ust[1]][2] = t1
            myvertex[son[0]][2] = t1
            myvertex[son[1]][2] = t1
            for i in alt:
                myvertex[i][2] = sqrt(r ** 2 - myvertex[i][0] ** 2) + z

            on = [son[0]]
            u1 = []
            for i in range(0, res):
                a = i * pi / res
                x = cos(a) * c
                if -u < x < u:
                    myvertex.append([x, -k1 / 2, sin(a) * c + z])
                    on.append(len(myvertex) - 1)
            u1.extend(on)
            u1.append(ust[0])
            on.extend([ust[0], alt[0]])
            ar = []
            d1 = []
            d2 = []
            for i in range(0, len(alt) - 2, 4):
                x1 = myvertex[alt[i + 0]][0]
                x2 = myvertex[alt[i + 2]][0]
                on.append(alt[i + 0])
                ar.append(alt[i + 1])
                t1 = [alt[i + 0]]
                t2 = [alt[i + 1]]
                for j in range(0, res):
                    a = j * pi / res
                    x = -cos(a) * r
                    if x1 < x < x2:
                        myvertex.extend([[x, -k1 / 2, sin(a) * r + z], [x, k1 / 2, sin(a) * r + z]])
                        on.append(len(myvertex) - 2)
                        ar.append(len(myvertex) - 1)
                        t1.append(len(myvertex) - 2)
                        t2.append(len(myvertex) - 1)
                on.append(alt[i + 2])
                ar.append(alt[i + 3])
                t1.append(alt[i + 2])
                t2.append(alt[i + 3])
                d1.append(t1)
                d2.append(t2)
            ar.append(son[1])
            u2 = [son[1]]
            for i in range(0, res):
                a = i * pi / res
                x = cos(a) * c
                if -u < x < u:
                    myvertex.append([x, k1 / 2, sin(a) * c + z])
                    ar.append(len(myvertex) - 1)
                    u2.append(len(myvertex) - 1)
            ar.append(ust[1])
            u2.append(ust[1])
            ar.reverse()
            myfaces.extend([on, ar])
            for i in range(0, len(u1) - 1):
                myfaces.append([u1[i + 1], u1[i], u2[i], u2[i + 1]])
                sm.append(len(myfaces) - 1)
            for a in range(0, mx):
                for i in range(0, len(d1[a]) - 1):
                    myfaces.append([d1[a][i + 1], d1[a][i], d2[a][i], d2[a][i + 1]])
                    sm.append(len(myfaces) - 1)
            y = my - 1
            for x in range(0, mx):
                if kx[x][y] is True:
                    fr = (k1 + k2) * 0.5 - 0.01
                    ek = k2
                    r = c - k1
                    k = r - k2

                    x1 = xlist[x * 2 + 1]
                    x2 = xlist[x * 2 + 2]
                    myvertex.extend([[x2, fr - k2 / 2, z + 1], [x2 - k2, fr - k2 / 2, z + 1],
                                     [x2 - k2, fr + k2 / 2, z + 1],
                                     [x2, fr + k2 / 2, z + 1]])
                    myvertex.extend([[x2, fr - k2 / 2, zlist[-3]], [x2 - k2, fr - k2 / 2, zlist[-3] + k2],
                                     [x2 - k2, fr + k2 / 2,
                                      zlist[-3] + k2],
                                     [x2, fr + k2 / 2, zlist[-3]]])
                    myvertex.extend([[x1, fr - k2 / 2, zlist[-3]], [x1 + k2, fr - k2 / 2, zlist[-3] + k2],
                                     [x1 + k2, fr + k2 / 2,
                                      zlist[-3] + k2],
                                     [x1, fr + k2 / 2, zlist[-3]]])
                    myvertex.extend([[x1, fr - k2 / 2, z + 1], [x1 + k2, fr - k2 / 2, z + 1],
                                     [x1 + k2, fr + k2 / 2, z + 1],
                                     [x1, fr + k2 / 2, z + 1]])

                    n = len(myvertex)
                    myfaces.extend([[n - 16, n - 15, n - 11, n - 12], [n - 15, n - 14, n - 10, n - 11],
                                    [n - 14, n - 13, n - 9, n - 10], [n - 13, n - 16, n - 12, n - 9]])
                    myfaces.extend(
                        [[n - 12, n - 11, n - 7, n - 8], [n - 11, n - 10, n - 6, n - 7], [n - 10, n - 9, n - 5, n - 6],
                         [n - 9, n - 12, n - 8, n - 5]])
                    myfaces.extend(
                        [[n - 8, n - 7, n - 3, n - 4], [n - 7, n - 6, n - 2, n - 3], [n - 6, n - 5, n - 1, n - 2],
                         [n - 5, n - 8, n - 4, n - 1]])
                    alt = [n - 16, n - 15, n - 14, n - 13, n - 4, n - 3, n - 2, n - 1]
                    myvertex[alt[0]][2] = sqrt(r ** 2 - myvertex[alt[0]][0] ** 2) + z
                    myvertex[alt[1]][2] = sqrt(k ** 2 - myvertex[alt[1]][0] ** 2) + z
                    myvertex[alt[2]][2] = sqrt(k ** 2 - myvertex[alt[2]][0] ** 2) + z
                    myvertex[alt[3]][2] = sqrt(r ** 2 - myvertex[alt[3]][0] ** 2) + z
                    myvertex[alt[4]][2] = sqrt(r ** 2 - myvertex[alt[4]][0] ** 2) + z
                    myvertex[alt[5]][2] = sqrt(k ** 2 - myvertex[alt[5]][0] ** 2) + z
                    myvertex[alt[6]][2] = sqrt(k ** 2 - myvertex[alt[6]][0] ** 2) + z
                    myvertex[alt[7]][2] = sqrt(r ** 2 - myvertex[alt[7]][0] ** 2) + z

                    d1 = []
                    d2 = []
                    t1 = []
                    t2 = []
                    for i in range(0, res):
                        a = i * pi / res
                        y1 = cos(a) * r
                        y2 = -cos(a) * k
                        if x1 < y1 < x2:
                            myvertex.extend([[y1, fr - k2 / 2, sin(a) * r + z], [y1, fr + k2 / 2,
                                                                                      sin(a) * r + z]])
                            t1.append(len(myvertex) - 2)
                            t2.append(len(myvertex) - 1)
                        if x1 + k2 < y2 < x2 - k2:
                            myvertex.extend([[y2, fr - k2 / 2, sin(a) * k + z], [y2, fr + k2 / 2,
                                                                                      sin(a) * k + z]])
                            d1.append(len(myvertex) - 2)
                            d2.append(len(myvertex) - 1)
                    on = [alt[1], alt[0]]
                    on.extend(t1)
                    on.extend([alt[4], alt[5]])
                    on.extend(d1)
                    ar = [alt[2], alt[3]]
                    ar.extend(t2)
                    ar.extend([alt[7], alt[6]])
                    ar.extend(d2)
                    ar.reverse()

                    if d1 == [] and t1 == []:
                        myfaces.extend([on, ar, [alt[5], alt[6], alt[2], alt[1]], [alt[7], alt[4], alt[0], alt[
                            3]]])
                        m = len(myfaces)
                        sm.extend(
                            [m - 1, m - 2])
                    elif d1 == [] and t1 != []:
                        myfaces.extend([on, ar, [alt[5], alt[6], alt[2], alt[1]], [alt[7], alt[4], t1[-1], t2[-1]],
                                        [alt[0], alt[3], t2[0], t1[0]]])
                        m = len(myfaces)
                        sm.extend(
                            [m - 1, m - 2, m - 3])
                    elif d1 != [] and t1 == []:
                        myfaces.extend([on, ar, [alt[5], alt[6], d2[0], d1[0]], [alt[2], alt[1], d1[-1], d2[-1]],
                                        [alt[7], alt[4], alt[0], alt[3]]])
                        m = len(myfaces)
                        sm.extend(
                            [m - 1, m - 2, m - 3])
                    else:
                        myfaces.extend([on, ar, [alt[5], alt[6], d2[0], d1[0]], [alt[2], alt[1], d1[-1], d2[-1]],
                                        [alt[7], alt[4], t1[-1], t2[-1]], [alt[0], alt[3], t2[0], t1[0]]])
                        m = len(myfaces)
                        sm.extend(
                            [m - 1, m - 2, m - 3, m - 4])

                    for i in range(0, len(d1) - 1):
                        myfaces.append([d1[i + 1], d1[i], d2[i], d2[i + 1]])
                        sm.append(len(myfaces) - 1)
                    for i in range(0, len(t1) - 1):
                        myfaces.append([t1[i + 1], t1[i], t2[i], t2[i + 1]])
                        sm.append(len(myfaces) - 1)
                    r = c - k1 - k2
                    k = r - k3 * 2
                else:
                    fr = 0
                    ek = 0
                    r = c - k1
                    k = r - k3 * 2
                # Fitil
                x1 = xlist[x * 2 + 1] + ek
                x2 = xlist[x * 2 + 2] - ek
                myvertex.extend([[x2, fr - k3, z + 1], [x2 - k3 * 2, fr - k3, z + 1], [x2 - k3 * 2, fr + k3, z + 1],
                                 [x2, fr + k3, z + 1]])
                myvertex.extend([[x2, fr - k3, zlist[-3] + ek], [x2 - k3 * 2, fr - k3, zlist[-3] + ek + k3 * 2],
                                 [x2 - k3 * 2, fr + k3, zlist[-3] + ek + k3 * 2], [x2, fr + k3, zlist[-3] + ek]])
                myvertex.extend([[x1, fr - k3, zlist[-3] + ek], [x1 + k3 * 2, fr - k3, zlist[-3] + ek + k3 * 2],
                                 [x1 + k3 * 2, fr + k3, zlist[-3] + ek + k3 * 2], [x1, fr + k3, zlist[-3] + ek]])
                myvertex.extend([[x1, fr - k3, z + 1], [x1 + k3 * 2, fr - k3, z + 1], [x1 + k3 * 2, fr + k3, z + 1],
                                 [x1, fr + k3, z + 1]])
                n = len(myvertex)
                myfaces.extend(
                    [[n - 16, n - 15, n - 11, n - 12], [n - 15, n - 14, n - 10, n - 11], [n - 14, n - 13, n - 9,
                                                                                          n - 10]])
                myfaces.extend(
                    [[n - 12, n - 11, n - 7, n - 8], [n - 11, n - 10, n - 6, n - 7], [n - 10, n - 9, n - 5, n - 6]])
                myfaces.extend([[n - 8, n - 7, n - 3, n - 4], [n - 7, n - 6, n - 2, n - 3], [n - 6, n - 5, n - 1,
                                                                                             n - 2]])
                m = len(myfaces)
                ftl.extend([m - 1, m - 2, m - 3, m - 4, m - 5, m - 6, m - 7, m - 8, m - 9])
                alt = [n - 16, n - 15, n - 14, n - 13, n - 4, n - 3, n - 2, n - 1]
                myvertex[alt[0]][2] = sqrt(r ** 2 - myvertex[alt[0]][0] ** 2) + z
                myvertex[alt[1]][2] = sqrt(k ** 2 - myvertex[alt[1]][0] ** 2) + z
                myvertex[alt[2]][2] = sqrt(k ** 2 - myvertex[alt[2]][0] ** 2) + z
                myvertex[alt[3]][2] = sqrt(r ** 2 - myvertex[alt[3]][0] ** 2) + z
                myvertex[alt[4]][2] = sqrt(r ** 2 - myvertex[alt[4]][0] ** 2) + z
                myvertex[alt[5]][2] = sqrt(k ** 2 - myvertex[alt[5]][0] ** 2) + z
                myvertex[alt[6]][2] = sqrt(k ** 2 - myvertex[alt[6]][0] ** 2) + z
                myvertex[alt[7]][2] = sqrt(r ** 2 - myvertex[alt[7]][0] ** 2) + z
                d1 = []
                d2 = []
                t1 = []
                t2 = []
                for i in range(0, res):
                    a = i * pi / res
                    y1 = cos(a) * r
                    y2 = -cos(a) * k
                    if x1 < y1 < x2:
                        myvertex.extend([[y1, fr - k3, sin(a) * r + z], [y1, fr + k3, sin(a) * r + z]])
                        t1.append(len(myvertex) - 2)
                        t2.append(len(myvertex) - 1)
                        ftl.extend([len(myfaces) - 1, len(myfaces) - 2])
                    if x1 + k3 * 2 < y2 < x2 - k3 * 2:
                        myvertex.extend([[y2, fr - k3, sin(a) * k + z], [y2, fr + k3, sin(a) * k + z]])
                        d1.append(len(myvertex) - 2)
                        d2.append(len(myvertex) - 1)
                        ftl.extend([len(myfaces) - 1, len(myfaces) - 2])
                on = [alt[1], alt[0]]
                on.extend(t1)
                on.extend([alt[4], alt[5]])
                on.extend(d1)
                ar = [alt[2], alt[3]]
                ar.extend(t2)
                ar.extend([alt[7], alt[6]])
                ar.extend(d2)
                ar.reverse()

                if not d1:
                    myfaces.extend([on, ar, [alt[5], alt[6], alt[2], alt[1]]])
                    m = len(myfaces)
                    ftl.extend([m - 1, m - 2, m - 3])
                    sm.extend([m - 1])
                else:
                    myfaces.extend([on, ar, [alt[5], alt[6], d2[0], d1[0]], [alt[2], alt[1], d1[-1], d2[-1]]])
                    m = len(myfaces)
                    ftl.extend([m - 1, m - 2, m - 3, m - 4])
                    sm.extend([m - 1, m - 2])

                for i in range(0, len(d1) - 1):
                    myfaces.append([d1[i + 1], d1[i], d2[i], d2[i + 1]])
                    ftl.append(len(myfaces) - 1)
                    sm.append(len(myfaces) - 1)
                # Cam
                x1 = xlist[x * 2 + 1] + ek + k3 * 2
                x2 = xlist[x * 2 + 2] - ek - k3 * 2
                on = []
                ar = []
                for i in range(0, res):
                    a = i * pi / res
                    y1 = -cos(a) * k
                    if x1 < y1 < x2:
                        myvertex.extend([[y1, fr - 0.005, sin(a) * k + z], [y1, fr + 0.005, sin(a) * k + z]])
                        n = len(myvertex)
                        on.append(n - 1)
                        ar.append(n - 2)
                myvertex.extend(
                    [[x1, fr - 0.005, sqrt(k ** 2 - x1 ** 2) + z], [x1, fr + 0.005,
                                                                         sqrt(k ** 2 - x1 ** 2) + z]])
                myvertex.extend([[x1, fr - 0.005, zlist[-3] + ek + k3 * 2], [x1, fr + 0.005, zlist[-3] + ek + k3 * 2]])
                myvertex.extend([[x2, fr - 0.005, zlist[-3] + ek + k3 * 2], [x2, fr + 0.005, zlist[-3] + ek + k3 * 2]])
                myvertex.extend(
                    [[x2, fr - 0.005, sqrt(k ** 2 - x2 ** 2) + z], [x2, fr + 0.005,
                                                                         sqrt(k ** 2 - x2 ** 2) + z]])
                n = len(myvertex)
                on.extend([n - 1, n - 3, n - 5, n - 7])
                ar.extend([n - 2, n - 4, n - 6, n - 8])
                myfaces.append(on)
                ar.reverse()
                myfaces.append(ar)
                m = len(myfaces)
                cam.extend([m - 1, m - 2])

        elif op.UST == '3':  # Egri
            if op.DT3 == '1':
                h1 = (op.VL1 / 200) / u
            elif op.DT3 == '2':
                h1 = op.VL3 / 100
            elif op.DT3 == '3':
                h1 = sin(op.VL4 * pi / 180) / cos(op.VL4 * pi / 180)
            z = sqrt(k1 ** 2 + (k1 * h1) ** 2)
            k = sqrt(k2 ** 2 + (k2 * h1) ** 2)
            f = sqrt(k3 ** 2 + (k3 * h1) ** 2) * 2
            myvertex[ust[0]][2] = zlist[-1] + myvertex[ust[0]][0] * h1
            myvertex[ust[1]][2] = zlist[-1] + myvertex[ust[1]][0] * h1
            for i in alt:
                myvertex[i][2] = zlist[-1] + myvertex[i][0] * h1 - z
            myvertex[son[0]][2] = zlist[-1] + myvertex[son[0]][0] * h1
            myvertex[son[1]][2] = zlist[-1] + myvertex[son[1]][0] * h1
            myfaces.append([ust[1], ust[0], son[0], son[1]])
            for i in range(0, mx * 4, 4):
                myfaces.append([alt[i], alt[i + 1], alt[i + 3], alt[i + 2]])
            on = [ust[0]]
            ar = [ust[1]]
            for i in range(0, len(alt) - 1, 2):
                on.append(alt[i])
                ar.append(alt[i + 1])
            on.append(son[0])
            myfaces.append(on)
            ar.append(son[1])
            ar.reverse()
            myfaces.append(ar)
            y = my - 1
            for x in range(0, mx):
                if kx[x][y] is True:
                    kapak(myvertex, myfaces, xlist, zlist, x * 2 + 1, y * 2 + 1, k2 / 2, (k1 + k2) * 0.5 - 0.01)
                    n = len(myvertex)
                    myvertex[n - 5][2] = zlist[-1] + myvertex[n - 5][0] * h1 - z
                    myvertex[n - 6][2] = zlist[-1] + myvertex[n - 6][0] * h1 - z - k
                    myvertex[n - 7][2] = zlist[-1] + myvertex[n - 7][0] * h1 - z - k
                    myvertex[n - 8][2] = zlist[-1] + myvertex[n - 8][0] * h1 - z
                    myvertex[n - 9][2] = zlist[-1] + myvertex[n - 9][0] * h1 - z
                    myvertex[n - 10][2] = zlist[-1] + myvertex[n - 10][0] * h1 - z - k
                    myvertex[n - 11][2] = zlist[-1] + myvertex[n - 11][0] * h1 - z - k
                    myvertex[n - 12][2] = zlist[-1] + myvertex[n - 12][0] * h1 - z
                    fitil(myvertex, myfaces, xlist, zlist, x * 2 + 1, y * 2 + 1, k3, (k1 + k2) * 0.5 - 0.01, k2)
                    n = len(myvertex)
                    myvertex[n - 2][2] = zlist[-1] + myvertex[n - 2][0] * h1 - z - k - f
                    myvertex[n - 3][2] = zlist[-1] + myvertex[n - 3][0] * h1 - z - k - f
                    myvertex[n - 6][2] = zlist[-1] + myvertex[n - 6][0] * h1 - z - k - f
                    myvertex[n - 7][2] = zlist[-1] + myvertex[n - 7][0] * h1 - z - k - f
                    myvertex[n - 13][2] = zlist[-1] + myvertex[n - 13][0] * h1 - z - k
                    myvertex[n - 14][2] = zlist[-1] + myvertex[n - 14][0] * h1 - z - k - f
                    myvertex[n - 15][2] = zlist[-1] + myvertex[n - 15][0] * h1 - z - k - f
                    myvertex[n - 16][2] = zlist[-1] + myvertex[n - 16][0] * h1 - z - k
                    myvertex[n - 17][2] = zlist[-1] + myvertex[n - 17][0] * h1 - z - k
                    myvertex[n - 18][2] = zlist[-1] + myvertex[n - 18][0] * h1 - z - k - f
                    myvertex[n - 19][2] = zlist[-1] + myvertex[n - 19][0] * h1 - z - k - f
                    myvertex[n - 20][2] = zlist[-1] + myvertex[n - 20][0] * h1 - z - k
                else:
                    fitil(myvertex, myfaces, xlist, zlist, x * 2 + 1, y * 2 + 1, k3, 0, 0)
                    n = len(myvertex)
                    myvertex[n - 2][2] = zlist[-1] + myvertex[n - 2][0] * h1 - z - f
                    myvertex[n - 3][2] = zlist[-1] + myvertex[n - 3][0] * h1 - z - f
                    myvertex[n - 6][2] = zlist[-1] + myvertex[n - 6][0] * h1 - z - f
                    myvertex[n - 7][2] = zlist[-1] + myvertex[n - 7][0] * h1 - z - f
                    myvertex[n - 13][2] = zlist[-1] + myvertex[n - 13][0] * h1 - z
                    myvertex[n - 14][2] = zlist[-1] + myvertex[n - 14][0] * h1 - z - f
                    myvertex[n - 15][2] = zlist[-1] + myvertex[n - 15][0] * h1 - z - f
                    myvertex[n - 16][2] = zlist[-1] + myvertex[n - 16][0] * h1 - z
                    myvertex[n - 17][2] = zlist[-1] + myvertex[n - 17][0] * h1 - z
                    myvertex[n - 18][2] = zlist[-1] + myvertex[n - 18][0] * h1 - z - f
                    myvertex[n - 19][2] = zlist[-1] + myvertex[n - 19][0] * h1 - z - f
                    myvertex[n - 20][2] = zlist[-1] + myvertex[n - 20][0] * h1 - z
                m = len(myfaces)
                cam.extend([m - 1, m - 2])
                ftl.extend([m - 3, m - 4, m - 5, m - 6, m - 7, m - 8, m - 9, m - 10, m - 11, m - 12, m - 13, m - 14])
        elif op.UST == '4':  # Ucgen
            if op.DT3 == '1':
                h1 = (op.VL1 / 100) / u
            elif op.DT3 == '2':
                h1 = op.VL3 / 100
            elif op.DT3 == '3':
                h1 = sin(op.VL4 * pi / 180) / cos(op.VL4 * pi / 180)
            z = sqrt(k1 ** 2 + (k1 * h1) ** 2)
            k = sqrt(k2 ** 2 + (k2 * h1) ** 2)
            f = sqrt(k3 ** 2 + (k3 * h1) ** 2) * 2
            myvertex[ust[0]][2] = zlist[-1] + myvertex[ust[0]][0] * h1
            myvertex[ust[1]][2] = zlist[-1] + myvertex[ust[1]][0] * h1
            for i in alt:
                myvertex[i][2] = zlist[-1] - abs(myvertex[i][0]) * h1 - z
            myvertex[son[0]][2] = zlist[-1] - myvertex[son[0]][0] * h1
            myvertex[son[1]][2] = zlist[-1] - myvertex[son[1]][0] * h1
            myvertex.extend([[0, -k1 / 2, zlist[-1]], [0, k1 / 2, zlist[-1]]])

            x = 0
            for j in range(2, len(alt) - 2, 4):
                if myvertex[alt[j]][0] < 0 < myvertex[alt[j + 2]][0]:
                    x = 1

            n = len(myvertex)
            myfaces.extend([[ust[1], ust[0], n - 2, n - 1], [n - 1, n - 2, son[0], son[1]]])
            on = [son[0], n - 2, ust[0]]
            ar = [son[1], n - 1, ust[1]]

            if x == 0:
                myvertex.extend([[0, -k1 / 2, zlist[-1] - z], [0, k1 / 2, zlist[-1] - z]])
            for j in range(0, len(alt) - 2, 4):
                if myvertex[alt[j]][0] < 0 and myvertex[alt[j + 2]][0] < 0:
                    myfaces.append([alt[j], alt[j + 1], alt[j + 3], alt[j + 2]])
                    on.extend([alt[j], alt[j + 2]])
                    ar.extend([alt[j + 1], alt[j + 3]])
                elif myvertex[alt[j]][0] > 0 and myvertex[alt[j + 2]][0] > 0:
                    myfaces.append([alt[j], alt[j + 1], alt[j + 3], alt[j + 2]])
                    on.extend([alt[j], alt[j + 2]])
                    ar.extend([alt[j + 1], alt[j + 3]])
                else:
                    n = len(myvertex)
                    myfaces.extend([[alt[j], alt[j + 1], n - 1, n - 2], [n - 2, n - 1, alt[j + 3], alt[j + 2]]])
                    on.extend([alt[j + 0], n - 2, alt[j + 2]])
                    ar.extend([alt[j + 1], n - 1, alt[j + 3]])
            myfaces.append(on)
            ar.reverse()
            myfaces.append(ar)
            y = my - 1
            for x in range(0, mx):
                if myvertex[alt[x * 4]][0] < 0 and myvertex[alt[x * 4 + 2]][0] < 0:
                    if kx[x][y] is True:
                        kapak(myvertex, myfaces, xlist, zlist, x * 2 + 1, y * 2 + 1, k2 / 2, (k1 + k2) * 0.5 - 0.01)
                        n = len(myvertex)
                        myvertex[n - 5][2] = zlist[-1] + myvertex[n - 5][0] * h1 - z
                        myvertex[n - 6][2] = zlist[-1] + myvertex[n - 6][0] * h1 - z - k
                        myvertex[n - 7][2] = zlist[-1] + myvertex[n - 7][0] * h1 - z - k
                        myvertex[n - 8][2] = zlist[-1] + myvertex[n - 8][0] * h1 - z
                        myvertex[n - 9][2] = zlist[-1] + myvertex[n - 9][0] * h1 - z
                        myvertex[n - 10][2] = zlist[-1] + myvertex[n - 10][0] * h1 - z - k
                        myvertex[n - 11][2] = zlist[-1] + myvertex[n - 11][0] * h1 - z - k
                        myvertex[n - 12][2] = zlist[-1] + myvertex[n - 12][0] * h1 - z
                        fitil(myvertex, myfaces, xlist, zlist, x * 2 + 1, y * 2 + 1, k3, (k1 + k2) * 0.5 - 0.01, k2)
                        n = len(myvertex)
                        myvertex[n - 2][2] = zlist[-1] + myvertex[n - 2][0] * h1 - z - k - f
                        myvertex[n - 3][2] = zlist[-1] + myvertex[n - 3][0] * h1 - z - k - f
                        myvertex[n - 6][2] = zlist[-1] + myvertex[n - 6][0] * h1 - z - k - f
                        myvertex[n - 7][2] = zlist[-1] + myvertex[n - 7][0] * h1 - z - k - f
                        myvertex[n - 13][2] = zlist[-1] + myvertex[n - 13][0] * h1 - z - k
                        myvertex[n - 14][2] = zlist[-1] + myvertex[n - 14][0] * h1 - z - k - f
                        myvertex[n - 15][2] = zlist[-1] + myvertex[n - 15][0] * h1 - z - k - f
                        myvertex[n - 16][2] = zlist[-1] + myvertex[n - 16][0] * h1 - z - k
                        myvertex[n - 17][2] = zlist[-1] + myvertex[n - 17][0] * h1 - z - k
                        myvertex[n - 18][2] = zlist[-1] + myvertex[n - 18][0] * h1 - z - k - f
                        myvertex[n - 19][2] = zlist[-1] + myvertex[n - 19][0] * h1 - z - k - f
                        myvertex[n - 20][2] = zlist[-1] + myvertex[n - 20][0] * h1 - z - k
                    else:
                        fitil(myvertex, myfaces, xlist, zlist, x * 2 + 1, y * 2 + 1, k3, 0, 0)
                        n = len(myvertex)
                        myvertex[n - 2][2] = zlist[-1] + myvertex[n - 2][0] * h1 - z - f
                        myvertex[n - 3][2] = zlist[-1] + myvertex[n - 3][0] * h1 - z - f
                        myvertex[n - 6][2] = zlist[-1] + myvertex[n - 6][0] * h1 - z - f
                        myvertex[n - 7][2] = zlist[-1] + myvertex[n - 7][0] * h1 - z - f
                        myvertex[n - 13][2] = zlist[-1] + myvertex[n - 13][0] * h1 - z
                        myvertex[n - 14][2] = zlist[-1] + myvertex[n - 14][0] * h1 - z - f
                        myvertex[n - 15][2] = zlist[-1] + myvertex[n - 15][0] * h1 - z - f
                        myvertex[n - 16][2] = zlist[-1] + myvertex[n - 16][0] * h1 - z
                        myvertex[n - 17][2] = zlist[-1] + myvertex[n - 17][0] * h1 - z
                        myvertex[n - 18][2] = zlist[-1] + myvertex[n - 18][0] * h1 - z - f
                        myvertex[n - 19][2] = zlist[-1] + myvertex[n - 19][0] * h1 - z - f
                        myvertex[n - 20][2] = zlist[-1] + myvertex[n - 20][0] * h1 - z
                    m = len(myfaces)
                    cam.extend([m - 1, m - 2])
                    ftl.extend([m - 3, m - 4, m - 5, m - 6, m - 7, m - 8, m - 9, m - 10, m - 11,
                                m - 12, m - 13, m - 14])
                elif myvertex[alt[x * 4]][0] > 0 and myvertex[alt[x * 4 + 2]][0] > 0:
                    if kx[x][y] is True:
                        kapak(myvertex, myfaces, xlist, zlist, x * 2 + 1, y * 2 + 1, k2 / 2, (k1 + k2) * 0.5 - 0.01)
                        n = len(myvertex)
                        myvertex[n - 5][2] = zlist[-1] - myvertex[n - 5][0] * h1 - z
                        myvertex[n - 6][2] = zlist[-1] - myvertex[n - 6][0] * h1 - z - k
                        myvertex[n - 7][2] = zlist[-1] - myvertex[n - 7][0] * h1 - z - k
                        myvertex[n - 8][2] = zlist[-1] - myvertex[n - 8][0] * h1 - z
                        myvertex[n - 9][2] = zlist[-1] - myvertex[n - 9][0] * h1 - z
                        myvertex[n - 10][2] = zlist[-1] - myvertex[n - 10][0] * h1 - z - k
                        myvertex[n - 11][2] = zlist[-1] - myvertex[n - 11][0] * h1 - z - k
                        myvertex[n - 12][2] = zlist[-1] - myvertex[n - 12][0] * h1 - z
                        fitil(myvertex, myfaces, xlist, zlist, x * 2 + 1, y * 2 + 1, k3, (k1 + k2) * 0.5 - 0.01, k2)
                        n = len(myvertex)
                        myvertex[n - 2][2] = zlist[-1] - myvertex[n - 2][0] * h1 - z - k - f
                        myvertex[n - 3][2] = zlist[-1] - myvertex[n - 3][0] * h1 - z - k - f
                        myvertex[n - 6][2] = zlist[-1] - myvertex[n - 6][0] * h1 - z - k - f
                        myvertex[n - 7][2] = zlist[-1] - myvertex[n - 7][0] * h1 - z - k - f
                        myvertex[n - 13][2] = zlist[-1] - myvertex[n - 13][0] * h1 - z - k
                        myvertex[n - 14][2] = zlist[-1] - myvertex[n - 14][0] * h1 - z - k - f
                        myvertex[n - 15][2] = zlist[-1] - myvertex[n - 15][0] * h1 - z - k - f
                        myvertex[n - 16][2] = zlist[-1] - myvertex[n - 16][0] * h1 - z - k
                        myvertex[n - 17][2] = zlist[-1] - myvertex[n - 17][0] * h1 - z - k
                        myvertex[n - 18][2] = zlist[-1] - myvertex[n - 18][0] * h1 - z - k - f
                        myvertex[n - 19][2] = zlist[-1] - myvertex[n - 19][0] * h1 - z - k - f
                        myvertex[n - 20][2] = zlist[-1] - myvertex[n - 20][0] * h1 - z - k
                    else:
                        fitil(myvertex, myfaces, xlist, zlist, x * 2 + 1, y * 2 + 1, k3, 0, 0)
                        n = len(myvertex)
                        myvertex[n - 2][2] = zlist[-1] - myvertex[n - 2][0] * h1 - z - f
                        myvertex[n - 3][2] = zlist[-1] - myvertex[n - 3][0] * h1 - z - f
                        myvertex[n - 6][2] = zlist[-1] - myvertex[n - 6][0] * h1 - z - f
                        myvertex[n - 7][2] = zlist[-1] - myvertex[n - 7][0] * h1 - z - f
                        myvertex[n - 13][2] = zlist[-1] - myvertex[n - 13][0] * h1 - z
                        myvertex[n - 14][2] = zlist[-1] - myvertex[n - 14][0] * h1 - z - f
                        myvertex[n - 15][2] = zlist[-1] - myvertex[n - 15][0] * h1 - z - f
                        myvertex[n - 16][2] = zlist[-1] - myvertex[n - 16][0] * h1 - z
                        myvertex[n - 17][2] = zlist[-1] - myvertex[n - 17][0] * h1 - z
                        myvertex[n - 18][2] = zlist[-1] - myvertex[n - 18][0] * h1 - z - f
                        myvertex[n - 19][2] = zlist[-1] - myvertex[n - 19][0] * h1 - z - f
                        myvertex[n - 20][2] = zlist[-1] - myvertex[n - 20][0] * h1 - z
                    m = len(myfaces)
                    cam.extend([m - 1, m - 2])
                    ftl.extend([m - 3, m - 4, m - 5, m - 6, m - 7, m - 8, m - 9, m - 10,
                                m - 11, m - 12, m - 13, m - 14])
                else:
                    k4 = k3 * 2
                    if kx[x][y] is True:
                        zz = (k1 + k2) * 0.5 - 0.01
                        xx = xlist[x * 2 + 1]
                        myvertex.extend([[xx, -k2 / 2 + zz, zlist[-3]], [xx + k2, -k2 / 2 + zz, zlist[-3] + k2],
                                         [xx + k2, k2 / 2 + zz, zlist[-3] + k2], [xx, k2 / 2 + zz, zlist[-3]]])
                        myvertex.extend(
                            [[xx, -k2 / 2 + zz, zlist[-1] + xx * h1 - z], [xx + k2, -k2 / 2 + zz,
                                                                           zlist[-1] + (xx + k2) * h1 - z - k],
                             [xx + k2, k2 / 2 + zz, zlist[-1] + (xx + k2) * h1 - z - k],
                             [xx, k2 / 2 + zz, zlist[-1] + xx * h1 - z]])
                        myvertex.extend([[0, -k2 / 2 + zz, zlist[-1] - z], [0, -k2 / 2 + zz, zlist[-1] - z - k],
                                         [0, k2 / 2 + zz, zlist[-1] - z - k], [0, k2 / 2 + zz, zlist[-1] - z]])
                        xx = xlist[x * 2 + 2]
                        myvertex.extend(
                            [[xx, -k2 / 2 + zz, zlist[-1] - xx * h1 - z], [xx - k2, -k2 / 2 + zz,
                                                                           zlist[-1] - (xx - k2) * h1 - z - k],
                             [xx - k2, k2 / 2 + zz, zlist[-1] - (xx - k2) * h1 - z - k],
                             [xx, k2 / 2 + zz, zlist[-1] - xx * h1 - z]])
                        myvertex.extend([[xx, -k2 / 2 + zz, zlist[-3]], [xx - k2, -k2 / 2 + zz, zlist[-3] + k2],
                                         [xx - k2, k2 / 2 + zz, zlist[-3] + k2], [xx, k2 / 2 + zz, zlist[-3]]])
                        n = len(myvertex)
                        myfaces.extend([[n - 20, n - 19, n - 15, n - 16], [n - 19, n - 18, n - 14, n - 15],
                                        [n - 18, n - 17, n - 13, n - 14], [n - 17, n - 20, n - 16, n - 13]])
                        myfaces.extend([[n - 16, n - 15, n - 11, n - 12], [n - 15, n - 14, n - 10, n - 11],
                                        [n - 14, n - 13, n - 9, n - 10], [n - 13, n - 16, n - 12, n - 9]])
                        myfaces.extend(
                            [[n - 12, n - 11, n - 7, n - 8], [n - 11, n - 10, n - 6, n - 7], [n - 10, n - 9,
                                                                                              n - 5, n - 6],
                             [n - 9, n - 12, n - 8, n - 5]])
                        myfaces.extend(
                            [[n - 8, n - 7, n - 3, n - 4], [n - 7, n - 6, n - 2, n - 3], [n - 6, n - 5, n - 1, n - 2],
                             [n - 5, n - 8, n - 4, n - 1]])
                        myfaces.extend(
                            [[n - 4, n - 3, n - 19, n - 20], [n - 3, n - 2, n - 18, n - 19],
                             [n - 2, n - 1, n - 17, n - 18],
                             [n - 1, n - 4, n - 20, n - 17]])
                        xx = xlist[x * 2 + 1]
                        myvertex.extend([[xx + k2, -k3 + zz, zlist[-3] + k2], [xx + k4 + k2, -k3 + zz,
                                                                               zlist[-3] + k2 + k4],
                                         [xx + k4 + k2, k3 + zz, zlist[-3] + k2 + k4], [xx + k2, k3 + zz,
                                                                                        zlist[-3] + k2]])
                        myvertex.extend([[xx + k2, -k3 + zz, zlist[-1] + (xx + k2) * h1 - z - k],
                                         [xx + k4 + k2, -k3 + zz, zlist[-1] + (xx + k2 + k4) * h1 - z - k - f],
                                         [xx + k4 + k2, k3 + zz, zlist[-1] + (xx + k2 + k4) * h1 - z - k - f],
                                         [xx + k2, k3 + zz, zlist[-1] + (xx + k2) * h1 - z - k]])
                        myvertex.extend([[0, -k3 + zz, zlist[-1] - k - z], [0, -k3 + zz, zlist[-1] - k - z - f],
                                         [0, k3 + zz, zlist[-1] - k - z - f], [0, k3 + zz, zlist[-1] - k - z]])
                        xx = xlist[x * 2 + 2]
                        myvertex.extend([[xx - k2, -k3 + zz, zlist[-1] - (xx - k2) * h1 - z - k],
                                         [xx - k4 - k2, -k3 + zz, zlist[-1] - (xx - k2 - k4) * h1 - z - k - f],
                                         [xx - k4 - k2, k3 + zz, zlist[-1] - (xx - k2 - k4) * h1 - z - k - f],
                                         [xx - k2, k3 + zz, zlist[-1] - (xx - k2) * h1 - z - k]])
                        myvertex.extend([[xx - k2, -k3 + zz, zlist[-3] + k2],
                                         [xx - k4 - k2, -k3 + zz, zlist[-3] + k2 + k4],
                                         [xx - k4 - k2, k3 + zz, zlist[-3] + k2 + k4],
                                         [xx - k2, k3 + zz, zlist[-3] + k2]])
                        n = len(myvertex)
                        myfaces.extend([[n - 20, n - 19, n - 15, n - 16], [n - 19, n - 18, n - 14, n - 15],
                                        [n - 18, n - 17, n - 13, n - 14]])
                        myfaces.extend([[n - 16, n - 15, n - 11, n - 12], [n - 15, n - 14, n - 10, n - 11],
                                        [n - 14, n - 13, n - 9, n - 10]])
                        myfaces.extend(
                            [[n - 12, n - 11, n - 7, n - 8], [n - 11, n - 10, n - 6, n - 7], [n - 10,
                                                                                              n - 9, n - 5, n - 6]])
                        myfaces.extend(
                            [[n - 8, n - 7, n - 3, n - 4], [n - 7, n - 6, n - 2, n - 3], [n - 6, n - 5, n - 1, n - 2]])
                        myfaces.extend([[n - 4, n - 3, n - 19, n - 20], [n - 3, n - 2, n - 18, n - 19],
                                        [n - 2, n - 1, n - 17, n - 18]])
                        xx = xlist[x * 2 + 1]
                        myvertex.extend(
                            [[xx + k4 + k2, -k3 + zz, zlist[-3] + k2 + k4], [xx + k4 + k2, k3 + zz, zlist[-3] +
                                                                             k2 + k4]])
                        myvertex.extend([[xx + k4 + k2, -k3 + zz, zlist[-1] + (xx + k2 + k4) * h1 - z - k - f],
                                         [xx + k4 + k2, k3 + zz, zlist[-1] + (xx + k2 + k4) * h1 - z - k - f]])
                        myvertex.extend([[0, -k3 + zz, zlist[-1] - k - z - f], [0, k3 + zz, zlist[-1] - k - z - f]])
                        xx = xlist[x * 2 + 2]
                        myvertex.extend([[xx - k4 - k2, -k3 + zz, zlist[-1] - (xx - k2 - k4) * h1 - z - k - f],
                                         [xx - k4 - k2, k3 + zz, zlist[-1] - (xx - k2 - k4) * h1 - z - k - f]])
                        myvertex.extend(
                            [[xx - k4 - k2, -k3 + zz, zlist[-3] + k2 + k4], [xx - k4 - k2, k3 + zz, zlist[-3] +
                                                                             k2 + k4]])
                        myfaces.extend([[n + 8, n + 6, n + 4, n + 2, n + 0], [n + 1, n + 3, n + 5, n + 7, n + 9]])
                    else:
                        xx = xlist[x * 2 + 1]
                        myvertex.extend(
                            [[xx, -k3, zlist[-3]], [xx + k4, -k3, zlist[-3] + k4], [xx + k4, k3, zlist[-3] + k4],
                             [xx, k3, zlist[-3]]])
                        myvertex.extend(
                            [[xx, -k3, zlist[-1] + xx * h1 - z], [xx + k4, -k3, zlist[-1] + (xx + k4) * h1 - z - f],
                             [xx + k4, k3, zlist[-1] + (xx + k4) * h1 - z - f], [xx, k3, zlist[-1] + xx * h1 - z]])
                        myvertex.extend(
                            [[0, -k3, zlist[-1] - z], [0, -k3, zlist[-1] - z - f], [0, k3, zlist[-1] - z - f],
                             [0, k3, zlist[-1] - z]])
                        xx = xlist[x * 2 + 2]
                        myvertex.extend(
                            [[xx, -k3, zlist[-1] - xx * h1 - z], [xx - k4, -k3, zlist[-1] - (xx - k4) * h1 - z - f],
                             [xx - k4, k3, zlist[-1] - (xx - k4) * h1 - z - f], [xx, k3, zlist[-1] - xx * h1 - z]])
                        myvertex.extend(
                            [[xx, -k3, zlist[-3]], [xx - k4, -k3, zlist[-3] + k4], [xx - k4, k3, zlist[-3] + k4],
                             [xx, k3, zlist[-3]]])
                        n = len(myvertex)
                        myfaces.extend([[n - 20, n - 19, n - 15, n - 16], [n - 19, n - 18, n - 14, n - 15],
                                        [n - 18, n - 17, n - 13, n - 14]])
                        myfaces.extend([[n - 16, n - 15, n - 11, n - 12], [n - 15, n - 14, n - 10, n - 11],
                                        [n - 14, n - 13, n - 9, n - 10]])
                        myfaces.extend(
                            [[n - 12, n - 11, n - 7, n - 8], [n - 11, n - 10, n - 6, n - 7], [n - 10, n - 9, n - 5,
                                                                                              n - 6]])
                        myfaces.extend(
                            [[n - 8, n - 7, n - 3, n - 4], [n - 7, n - 6, n - 2, n - 3], [n - 6, n - 5, n - 1, n - 2]])
                        myfaces.extend([[n - 4, n - 3, n - 19, n - 20], [n - 3, n - 2, n - 18, n - 19],
                                        [n - 2, n - 1, n - 17, n - 18]])
                        xx = xlist[x * 2 + 1]
                        myvertex.extend([[xx + k4, -0.005, zlist[-3] + k4], [xx + k4, 0.005, zlist[-3] + k4]])
                        myvertex.extend([[xx + k4, -0.005, zlist[-1] + (xx + k4) * h1 - z - f],
                                         [xx + k4, 0.005, zlist[-1] + (xx + k4) * h1 - z - f]])
                        myvertex.extend([[0, -0.005, zlist[-1] - z - f], [0, 0.005, zlist[-1] - z - f]])
                        xx = xlist[x * 2 + 2]
                        myvertex.extend([[xx - k4, -0.005, zlist[-1] - (xx - k4) * h1 - z - f],
                                         [xx - k4, 0.005, zlist[-1] - (xx - k4) * h1 - z - f]])
                        myvertex.extend([[xx - k4, -0.005, zlist[-3] + k4], [xx - k4, 0.005, zlist[-3] + k4]])
                        myfaces.extend([[n + 8, n + 6, n + 4, n + 2, n + 0], [n + 1, n + 3, n + 5, n + 7, n + 9]])
                    m = len(myfaces)
                    cam.extend([m - 1, m - 2])
                    ftl.extend(
                        [m - 3, m - 4, m - 5, m - 6, m - 7, m - 8, m - 9, m - 10, m - 11, m - 12, m - 13,
                         m - 14, m - 15,
                         m - 16, m - 17])
        # Mermer
        if op.mr is True:
            mrh = -op.mr1 / 100
            mrg = op.mr2 / 100
            mdv = (op.mr3 / 200) + mrg
            msv = -(mdv + (op.mr4 / 100))
            myvertex.extend([[-u, mdv, 0], [u, mdv, 0], [-u, msv, 0], [u, msv, 0], [-u, mdv, mrh], [u, mdv, mrh],
                             [-u, msv, mrh],
                             [u, msv, mrh]])
            n = len(myvertex)
            myfaces.extend([[n - 1, n - 2, n - 4, n - 3], [n - 3, n - 4, n - 8, n - 7], [n - 6, n - 5, n - 7, n - 8],
                            [n - 2, n - 1, n - 5, n - 6], [n - 4, n - 2, n - 6, n - 8], [n - 5, n - 1, n - 3, n - 7]])
            n = len(myfaces)
            mer.extend([n - 1, n - 2, n - 3, n - 4, n - 5, n - 6])

        return True, ftl, cam, mer, sm
    except:
        return False, None, None, None, None


# ------------------------------------
# Get highest points of the panel
# ------------------------------------
def get_high_points(selobject, width, tip):
    obverts = selobject.data.vertices

    top_a = 0
    top_b = 0
    top_c = 0
    # --------------------------
    # Recover all vertex
    # --------------------------
    for vertex in obverts:
        if vertex.co[0] == -width / 2:
            if vertex.co[2] >= top_a:
                top_a = vertex.co[2]
        if vertex.co[0] == width / 2:
            if vertex.co[2] >= top_b:
                top_b = vertex.co[2]
        # top center
        if tip == "2":
            if vertex.co[2] >= top_c:
                top_c = vertex.co[2]
        else:
            if vertex.co[0] == 0 and vertex.co[2] >= top_c:
                top_c = vertex.co[2]

    return top_a, top_b, top_c


# ---------------------------------------------------------
# Defines a point
# ---------------------------------------------------------
class Cpoint:

    def __init__(self, x, y):
        self.x = float(x)
        self.y = float(y)


# ---------------------------------------------------------
# Get angle between two vectors
# ---------------------------------------------------------
def get_angle(p1, p2):
    v1 = Vector((p1[0], 0.0, p1[1]))
    v2 = Vector((p2[0], 0.0, p2[1]))

    a = v1.angle(v2)
    return a


# ---------------------------------------------------------
# Get center of circle base on 3 points
#
# Point a: (x,z)
# Point b: (x,z)
# Point c: (x,z)
# Return:
# x, y: center poistion
#  r: radio
# ang: angle
# ---------------------------------------------------------
def get_circle_center(a, b, c):
    try:
        # line between a and b: s1 + k * d1
        s1 = Cpoint((a.x + b.x) / 2.0, (a.y + b.y) / 2.0)
        d1 = Cpoint(b.y - a.y, a.x - b.x)
        # line between a and c: s2 + k * d2
        s2 = Cpoint((a.x + c.x) / 2.0, (a.y + c.y) / 2.0)
        d2 = Cpoint(c.y - a.y, a.x - c.x)
        # intersection of both lines:
        l = d1.x * (s2.y - s1.y) - d1.y * (s2.x - s1.x)
        l /= d2.x * d1.y - d2.y * d1.x
        center = Cpoint(s2.x + l * d2.x, s2.y + l * d2.y)
        dx = center.x - a.x
        dy = center.y - a.y
        radio = sqrt(dx * dx + dy * dy)

        # angle
        v1 = (a.x - center.x, a.y - center.y)
        v2 = (b.x - center.x, b.y - center.y)
        ang = get_angle(v1, v2)
        return center, radio, ang
    except ZeroDivisionError:
        return Cpoint(0, 0), 1, 1


# -----------------------------------------
# Get limits
# lb, rb: limits x
# lt, rt: limits z
# mp: limit z in med point
# lo: Z low limit
# my: max y
# top: top vertex
# -----------------------------------------
def get_limits(myobject):
    verts = myobject.data.vertices

    lb = 0
    lt = 0
    rb = 0
    rt = 0
    mp = 0
    lo = 0
    my = 0
    top = 0
    for v in verts:
        if v.co[2] > top:
            top = v.co[2]

        if v.co[2] < lo:
            lo = v.co[2]

        if v.co[1] > my:
            my = v.co[1]
        if v.co[0] > rb:
            rb = v.co[0]
        if v.co[0] < lb:
            lb = v.co[0]
        if v.co[0] == 0:
            if v.co[2] > mp:
                mp = v.co[2]
    # max sides
    for v in verts:
        if v.co[2] > lt and v.co[0] == lb:
            lt = v.co[2]
        if v.co[2] > rt and v.co[0] == rb:
            rt = v.co[2]

    return lb, lt, rb, rt, mp, lo, my, top


# ------------------------------------------
# Create control box for panels
#
# ------------------------------------------
def create_ctrl_box(parentobj, objname):
    myvertex = []
    myfaces = []

    o = parentobj
    op = o.WindowPanelGenerator[0]

    lb, lt, rb, rt, mp, lo, my, top = get_limits(o)
    ypos = my * 1.8
    # -----------------------------
    # Flat, Triangle and inclined
    # -----------------------------
    if op.UST == "1" or op.UST == "3" or op.UST == "4":
        if mp == 0:
            myvertex.extend([(lb, ypos, lo), (lb, ypos, lt), (rb, ypos, rt), (rb, ypos, lo)])
            myvertex.extend([(lb, -ypos, lo), (lb, -ypos, lt), (rb, -ypos, rt), (rb, -ypos, lo)])
            myfaces.extend([(0, 4, 5, 1), (3, 2, 6, 7), (0, 1, 2, 3), (4, 7, 6, 5), (1, 5, 6, 2), (0, 3, 7, 4)])
        else:
            myvertex.extend([(lb, ypos, lo), (lb, ypos, lt), (0, ypos, mp), (rb, ypos, rt), (rb, ypos, lo)])
            myvertex.extend([(lb, -ypos, lo), (lb, -ypos, lt), (0, -ypos, mp), (rb, -ypos, rt), (rb, -ypos, lo)])
            myfaces.extend([(0, 5, 6, 1), (4, 3, 8, 9), (0, 1, 2, 3, 4), (9, 8, 7, 6, 5), (1, 6, 7, 2), (2, 7, 8, 3),
                            (0, 4, 9, 5)])
    # -----------------------------
    # Arch
    # -----------------------------
    if op.UST == "2":
        center, r, ang = get_circle_center(Cpoint(lb, lt), Cpoint(0, top), Cpoint(rb, rt))

        # cx = center.x
        cz = center.y

        sg = op.res
        arc = ((pi / 2) + ang) - ((pi / 2) - ang)
        step = arc / sg
        a = (pi / 2) + ang

        myvertex.extend([(lb, ypos, lt), (lb, -ypos, lt)])
        for x in range(0, sg):
            myvertex.extend([(r * cos(a), ypos, r * sin(a) + cz),
                             (r * cos(a), -ypos, r * sin(a) + cz)])
            a -= step

        # close sides
        myvertex.extend([(rb, ypos, rt), (rb, -ypos, rt)])

        v = 0
        nf = len(myvertex)
        for x in range(0, nf - 2, 2):
            myfaces.extend([(v, v + 1, v + 3, v + 2)])
            v += 2

        nf = len(myvertex)
        myvertex.extend([(lb, ypos, lo), (lb, -ypos, lo)])
        myvertex.extend([(rb, ypos, lo), (rb, -ypos, lo)])

        nf2 = len(myvertex)
        myfaces.extend([(0, nf2 - 4, nf2 - 3, 1)])
        myfaces.extend([(nf - 2, nf - 1, nf2 - 1, nf2 - 2)])

        nf2 = len(myvertex)
        myfaces.extend([(nf2 - 4, nf2 - 2, nf2 - 1, nf2 - 3)])

    # --------------------------
    # Create mesh
    # --------------------------
    myfaces = check_mesh_errors(myvertex, myfaces)
    mymesh = bpy.data.meshes.new(objname)
    myobj = bpy.data.objects.new(objname, mymesh)

    myobj.location = bpy.context.scene.cursor_location
    bpy.context.scene.objects.link(myobj)

    mymesh.from_pydata(myvertex, [], myfaces)
    mymesh.update(calc_edges=True)

    return myobj


# ------------------------------------------------------------------
# Define property group class to create or modify
# ------------------------------------------------------------------
class GeneralPanelProperties(PropertyGroup):
    prs = EnumProperty(
            items=(
                ('1', "WINDOW 250X200", ""),
                ('2', "WINDOW 200X200", ""),
                ('3', "WINDOW 180X200", ""),
                ('4', "WINDOW 180X160", ""),
                ('5', "WINDOW 160X160", ""),
                ('6', "WINDOW 50X50", ""),
                ('7', "DOOR 80X250", ""),
                ('8', "DOOR 80X230", ""),
                ),
            name="",
            description='Predefined types',
            update=update_using_default,
            )
    son = prs
    gen = IntProperty(
            name='H Count', min=1, max=8, default=3,
            description='Horizontal Panes',
            update=update_window,
            )
    yuk = IntProperty(
            name='V Count', min=1, max=5, default=1,
            description='Vertical Panes',
            update=update_window,
            )
    kl1 = IntProperty(
            name='Outer Frame', min=2, max=50, default=5,
            description='Outside Frame Thickness',
            update=update_window,
            )
    kl2 = IntProperty(
            name='Risers', min=2, max=50, default=5,
            description='Risers Width',
            update=update_window,
            )
    fk = IntProperty(
            name='Inner Frame', min=1, max=20, default=2,
            description='Inside Frame Thickness',
            update=update_window,
            )

    mr = BoolProperty(name='Sill', default=True, description='Window Sill', update=update_window)
    mr1 = IntProperty(name='', min=1, max=20, default=4, description='Height', update=update_window)
    mr2 = IntProperty(name='', min=0, max=20, default=4, description='First Depth', update=update_window)
    mr3 = IntProperty(name='', min=1, max=50, default=20, description='Second Depth', update=update_window)
    mr4 = IntProperty(name='', min=0, max=50, default=0, description='Extrusion for Jamb',
                                update=update_window)

    mt1 = EnumProperty(
            items=(
                ('1', "PVC", ""),
                ('2', "WOOD", ""),
                ('3', "Plastic", ""),
                ),
            name="",
            default='1',
            description='Material to use',
            update=update_window,
            )
    mt2 = EnumProperty(
            items=(
                ('1', "PVC", ""),
                ('2', "WOOD", ""),
                ('3', "Plastic", ""),
                ),
            name="",
            default='3',
            description='Material to use',
            update=update_window,
            )

    r = FloatProperty(
            name='Rotation',
            min=0, max=360, default=0, precision=1,
            description='Panel rotation',
            update=update_window,
            )

    UST = EnumProperty(
            items=(
                ('1', "Flat", ""),
                ('2', "Arch", ""),
                ('3', "Inclined", ""),
                ('4', "Triangle", ""),
                ),
            name="Top", default='1',
            description='Type of window upper section',
            update=update_window,
            )
    DT2 = EnumProperty(
            items=(
                ('1', "Difference", ""),
                ('2', "Radius", ""),
                ),
            name="",
            default='1',
            update=update_window,
            )
    DT3 = EnumProperty(
            items=(
                ('1', "Difference", ""),
                ('2', "Incline %", ""),
                ('3', "Incline Angle", ""),
                ),
            name="",
            default='1', update=update_window,
            )

    VL1 = IntProperty(name='', min=-10000, max=10000, default=30, update=update_window)  # Fark
    VL2 = IntProperty(name='', min=1, max=10000, default=30, update=update_window)  # Cap
    VL3 = IntProperty(name='', min=-100, max=100, default=30, update=update_window)  # Egim %
    VL4 = IntProperty(name='', min=-45, max=45, default=30, update=update_window)  # Egim Aci

    res = IntProperty(name='Resolution', min=2, max=360, default=36, update=update_window)  # Res

    gnx0 = IntProperty(name='', min=1, max=300, default=60, description='1st Window Width',
                                 update=update_window)
    gnx1 = IntProperty(name='', min=1, max=300, default=110, description='2nd Window Width',
                                 update=update_window)
    gnx2 = IntProperty(name='', min=1, max=300, default=60, description='3rd Window Width',
                                 update=update_window)
    gnx3 = IntProperty(name='', min=1, max=300, default=60, description='4th Window Width',
                                 update=update_window)
    gnx4 = IntProperty(name='', min=1, max=300, default=60, description='5th Window Width',
                                 update=update_window)
    gnx5 = IntProperty(name='', min=1, max=300, default=60, description='6th Window Width',
                                 update=update_window)
    gnx6 = IntProperty(name='', min=1, max=300, default=60, description='7th Window Width',
                                 update=update_window)
    gnx7 = IntProperty(name='', min=1, max=300, default=60, description='8th Window Width',
                                 update=update_window)

    gny0 = IntProperty(name='', min=1, max=300, default=190, description='1st Row Height',
                                 update=update_window)
    gny1 = IntProperty(name='', min=1, max=300, default=45, description='2nd Row Height',
                                 update=update_window)
    gny2 = IntProperty(name='', min=1, max=300, default=45, description='3rd Row Height',
                                 update=update_window)
    gny3 = IntProperty(name='', min=1, max=300, default=45, description='4th Row Height',
                                 update=update_window)
    gny4 = IntProperty(name='', min=1, max=300, default=45, description='5th Row Height',
                                 update=update_window)

    k00 = BoolProperty(name='', default=True, update=update_window)
    k01 = BoolProperty(name='', default=False, update=update_window)
    k02 = BoolProperty(name='', default=True, update=update_window)
    k03 = BoolProperty(name='', default=False, update=update_window)
    k04 = BoolProperty(name='', default=False, update=update_window)
    k05 = BoolProperty(name='', default=False, update=update_window)
    k06 = BoolProperty(name='', default=False, update=update_window)
    k07 = BoolProperty(name='', default=False, update=update_window)

    k10 = BoolProperty(name='', default=False, update=update_window)
    k11 = BoolProperty(name='', default=False, update=update_window)
    k12 = BoolProperty(name='', default=False, update=update_window)
    k13 = BoolProperty(name='', default=False, update=update_window)
    k14 = BoolProperty(name='', default=False, update=update_window)
    k15 = BoolProperty(name='', default=False, update=update_window)
    k16 = BoolProperty(name='', default=False, update=update_window)
    k17 = BoolProperty(name='', default=False, update=update_window)

    k20 = BoolProperty(name='', default=False, update=update_window)
    k21 = BoolProperty(name='', default=False, update=update_window)
    k22 = BoolProperty(name='', default=False, update=update_window)
    k23 = BoolProperty(name='', default=False, update=update_window)
    k24 = BoolProperty(name='', default=False, update=update_window)
    k25 = BoolProperty(name='', default=False, update=update_window)
    k26 = BoolProperty(name='', default=False, update=update_window)
    k27 = BoolProperty(name='', default=False, update=update_window)

    k30 = BoolProperty(name='', default=False, update=update_window)
    k31 = BoolProperty(name='', default=False, update=update_window)
    k32 = BoolProperty(name='', default=False, update=update_window)
    k33 = BoolProperty(name='', default=False, update=update_window)
    k34 = BoolProperty(name='', default=False, update=update_window)
    k35 = BoolProperty(name='', default=False, update=update_window)
    k36 = BoolProperty(name='', default=False, update=update_window)
    k37 = BoolProperty(name='', default=False, update=update_window)

    k40 = BoolProperty(name='', default=False, update=update_window)
    k41 = BoolProperty(name='', default=False, update=update_window)
    k42 = BoolProperty(name='', default=False, update=update_window)
    k43 = BoolProperty(name='', default=False, update=update_window)
    k44 = BoolProperty(name='', default=False, update=update_window)
    k45 = BoolProperty(name='', default=False, update=update_window)
    k46 = BoolProperty(name='', default=False, update=update_window)
    k47 = BoolProperty(name='', default=False, update=update_window)
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
    glpoint_c = FloatVectorProperty(
            name="glpointc",
            description="Hidden property for opengl",
            default=(0, 0, 0),
            )
    glpoint_d = FloatVectorProperty(
            name="glpointc",
            description="Hidden property for opengl",
            default=(0, 0, 0),
            )


bpy.utils.register_class(GeneralPanelProperties)
Object.WindowPanelGenerator = CollectionProperty(type=GeneralPanelProperties)


# ------------------------------------------------------------------
# Define panel class to modify myobjects.
# ------------------------------------------------------------------
class AchmWindowEditPanel(Panel):
    bl_idname = "window.edit_panel2"
    bl_label = "Window Panel"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = 'Window'

    # -----------------------------------------------------
    # Verify if visible
    # -----------------------------------------------------
    @classmethod
    def poll(cls, context):
        o = context.object
        if o is None:
            return False
        if 'WindowPanelGenerator' not in o:
            return False
        else:
            return True

    # -----------------------------------------------------
    # Draw (create UI interface)
    # -----------------------------------------------------
    def draw(self, context):
        o = context.object
        # If the selected object didn't be created with the group 'WindowPanelGenerator', this panel is not created.
        # noinspection PyBroadException
        try:
            if 'WindowPanelGenerator' not in o:
                return
        except:
            return

        layout = self.layout
        if bpy.context.mode == 'EDIT_MESH':
            layout.label('Warning: Operator does not work in edit mode.', icon='ERROR')
        else:
            myobject = o.WindowPanelGenerator[0]
            layout.prop(myobject, 'prs')
            box = layout.box()
            box.prop(myobject, 'gen')
            box.prop(myobject, 'yuk')
            box.prop(myobject, 'kl1')
            box.prop(myobject, 'kl2')
            box.prop(myobject, 'fk')
            box.prop(myobject, 'r')  # rotation

            box.prop(myobject, 'mr')
            if myobject.mr is True:
                row = box.row()
                row.prop(myobject, 'mr1')
                row.prop(myobject, 'mr2')
                row = box.row()
                row.prop(myobject, 'mr3')
                row.prop(myobject, 'mr4')
            row = layout.row()
            row.label('Frame')
            row.label('Inner Frame')
            row = layout.row()
            row.prop(myobject, 'mt1')
            row.prop(myobject, 'mt2')

            box.prop(myobject, 'UST')
            if myobject.UST == '2':
                row = box.row()
                row.prop(myobject, 'DT2')
                if myobject.DT2 == '1':
                    row.prop(myobject, 'VL1')
                elif myobject.DT2 == '2':
                    row.prop(myobject, 'VL2')
                box.prop(myobject, 'res')
            elif myobject.UST == '3':
                row = box.row()
                row.prop(myobject, 'DT3')
                if myobject.DT3 == '1':
                    row.prop(myobject, 'VL1')
                elif myobject.DT3 == '2':
                    row.prop(myobject, 'VL3')
                elif myobject.DT3 == '3':
                    row.prop(myobject, 'VL4')
            elif myobject.UST == '4':
                row = box.row()
                row.prop(myobject, 'DT3')
                if myobject.DT3 == '1':
                    row.prop(myobject, 'VL1')
                elif myobject.DT3 == '2':
                    row.prop(myobject, 'VL3')
                elif myobject.DT3 == '3':
                    row.prop(myobject, 'VL4')
            row = layout.row()
            for i in range(0, myobject.gen):
                row.prop(myobject, 'gnx' + str(i))
            for j in range(0, myobject.yuk):
                row = layout.row()
                row.prop(myobject, 'gny' + str(myobject.yuk - j - 1))
                for i in range(0, myobject.gen):
                    row.prop(myobject, 'k' + str(myobject.yuk - j - 1) + str(i))
