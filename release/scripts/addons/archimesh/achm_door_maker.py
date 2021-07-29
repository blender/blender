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
import math
# noinspection PyUnresolvedReferences
from bpy.types import Operator, PropertyGroup, Object, Panel
from bpy.props import FloatProperty, BoolProperty, EnumProperty, FloatVectorProperty, CollectionProperty
from .achm_tools import *


# ------------------------------------------------------------------
# Define operator class to create object
# ------------------------------------------------------------------
class AchmDoor(Operator):
    bl_idname = "mesh.archimesh_door"
    bl_label = "Door"
    bl_description = "Door"
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
    mainmesh = bpy.data.meshes.new("DoorFrane")
    mainobject = bpy.data.objects.new("DoorFrame", mainmesh)
    mainobject.location = bpy.context.scene.cursor_location
    bpy.context.scene.objects.link(mainobject)
    mainobject.DoorObjectGenerator.add()

    # we shape the main object and create other objects as children
    shape_mesh(mainobject, mainmesh)
    shape_children(mainobject)

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

    # ---------------------------------
    #  Clear Parent objects (autohole)
    # ---------------------------------
    myparent = o.parent
    if myparent is not None:
        ploc = myparent.location
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
                dummy = -1

        myparent.select = True
        bpy.ops.object.delete()

    # -----------------------
    # remove all children
    # -----------------------
    # first granchild
    for child in o.children:
        remove_children(child)
    # now children of main object
    remove_children(o)

    # Finally we create all that again (except main object),
    shape_mesh(o, tmp_mesh, True)
    o.data = tmp_mesh
    shape_children(o, True)
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
def shape_mesh(mainobject, tmp_mesh, update=False):
    mp = mainobject.DoorObjectGenerator[0]
    # Create only mesh, because the object is created before
    create_doorframe(mp, tmp_mesh)

    remove_doubles(mainobject)
    set_normals(mainobject)

    # saves OpenGL data
    mp.glpoint_a = (-mp.frame_width / 2, 0, 0)
    mp.glpoint_b = (-mp.frame_width / 2, 0, mp.frame_height)
    mp.glpoint_c = (mp.frame_width / 2, 0, mp.frame_height)
    mp.glpoint_d = (-mp.frame_width / 2 + mp.frame_size, 0, mp.frame_height - mp.frame_size - 0.01)
    mp.glpoint_e = (mp.frame_width / 2 - mp.frame_size, 0, mp.frame_height - mp.frame_size - 0.01)

    # Lock
    mainobject.lock_location = (True, True, True)
    mainobject.lock_rotation = (True, True, True)

    return

# ------------------------------------------------------------------------------
# Generate all Children
#
# ------------------------------------------------------------------------------
# noinspection PyUnusedLocal


def shape_children(mainobject, update=False):
    mp = mainobject.DoorObjectGenerator[0]

    if mp.openside != "3":
        make_one_door(mp, mainobject, mp.frame_width, mp.openside)
    else:
        w = mp.frame_width
        widthl = (w * mp.factor)
        widthr = w - widthl

        # left door
        mydoor = make_one_door(mp, mainobject, widthl + mp.frame_size, "2")
        mydoor.location.x = -mp.frame_width / 2 + mp.frame_size
        # right door (pending width)
        mydoor = make_one_door(mp, mainobject, widthr + mp.frame_size, "1")
        mydoor.location.x = mp.frame_width / 2 - mp.frame_size

    if mp.crt_mat and bpy.context.scene.render.engine == 'CYCLES':
        mat = create_diffuse_material("Door_material", False, 0.8, 0.8, 0.8)
        set_material(mainobject, mat)

    # -------------------------
    # Create empty and parent
    # -------------------------
    bpy.ops.object.empty_add(type='PLAIN_AXES')
    myempty = bpy.data.objects[bpy.context.active_object.name]
    myempty.location = mainobject.location

    myempty.name = "Door_Group"
    parentobject(myempty, mainobject)
    mainobject["archimesh.hole_enable"] = True
    # Rotate Empty
    myempty.rotation_euler.z = math.radians(mp.r)
    # Create control box to open wall holes
    gap = 0.002
    myctrl = create_control_box("CTRL_Hole",
                                mp.frame_width, mp.frame_thick * 3, mp.frame_height)
    # Add custom property to detect Controller
    myctrl["archimesh.ctrl_hole"] = True

    set_normals(myctrl)
    myctrl.parent = myempty
    myctrl.location.x = 0
    myctrl.location.y = -((mp.frame_thick * 3) / 2)
    myctrl.location.z = -gap
    myctrl.draw_type = 'BOUNDS'
    myctrl.hide = False
    myctrl.hide_render = True
    if bpy.context.scene.render.engine == 'CYCLES':
        myctrl.cycles_visibility.camera = False
        myctrl.cycles_visibility.diffuse = False
        myctrl.cycles_visibility.glossy = False
        myctrl.cycles_visibility.transmission = False
        myctrl.cycles_visibility.scatter = False
        myctrl.cycles_visibility.shadow = False

    # Create control box for baseboard
    myctrlbase = create_control_box("CTRL_Baseboard",
                                    mp.frame_width, 0.40, 0.40,
                                    False)
    # Add custom property to detect Controller
    myctrlbase["archimesh.ctrl_base"] = True

    set_normals(myctrlbase)
    myctrlbase.parent = myempty
    myctrlbase.location.x = 0
    myctrlbase.location.y = -0.15 - (mp.frame_thick / 3)
    myctrlbase.location.z = -0.10
    myctrlbase.draw_type = 'BOUNDS'
    myctrlbase.hide = False
    myctrlbase.hide_render = True
    if bpy.context.scene.render.engine == 'CYCLES':
        myctrlbase.cycles_visibility.camera = False
        myctrlbase.cycles_visibility.diffuse = False
        myctrlbase.cycles_visibility.glossy = False
        myctrlbase.cycles_visibility.transmission = False
        myctrlbase.cycles_visibility.scatter = False
        myctrlbase.cycles_visibility.shadow = False

        mat = create_transparent_material("hidden_material", False)
        set_material(myctrl, mat)
        set_material(myctrlbase, mat)

    # deactivate others
    for o in bpy.data.objects:
        if o.select is True and o.name != mainobject.name:
            o.select = False


# ------------------------------------------------------------------
# Define property group class to create or modify
# ------------------------------------------------------------------
class ObjectProperties(PropertyGroup):
    frame_width = FloatProperty(
            name='Frame width',
            min=0.25, max=10,
            default=1, precision=2,
            description='Doorframe width', update=update_object,
            )
    frame_height = FloatProperty(
            name='Frame height',
            min=0.25, max=10,
            default=2.1, precision=2,
            description='Doorframe height', update=update_object,
            )
    frame_thick = FloatProperty(
            name='Frame thickness',
            min=0.05, max=0.50,
            default=0.08, precision=2,
            description='Doorframe thickness', update=update_object,
            )
    frame_size = FloatProperty(
            name='Frame size',
            min=0.05, max=0.25,
            default=0.08, precision=2,
            description='Doorframe size', update=update_object,
            )
    crt_mat = BoolProperty(
            name="Create default Cycles materials",
            description="Create default materials for Cycles render",
            default=True,
            update=update_object,
            )
    factor = FloatProperty(
            name='',
            min=0.2, max=1,
            default=0.5, precision=3, description='Door ratio',
            update=update_object,
            )
    r = FloatProperty(
            name='Rotation', min=0, max=360,
            default=0, precision=1,
            description='Door rotation', update=update_object,
            )

    openside = EnumProperty(
            name="Open side",
            items=(
                ('1', "Right open", ""),
                ('2', "Left open", ""),
                ('3', "Both sides", ""),
                ),
            description="Defines the direction for opening the door",
            update=update_object,
            )

    model = EnumProperty(
            name="Model",
            items=(
                ('1', "Model 01", ""),
                ('2', "Model 02", ""),
                ('3', "Model 03", ""),
                ('4', "Model 04", ""),
                ('5', "Model 05", "Glass"),
                ('6', "Model 06", "Glass"),
                ),
            description="Door model",
            update=update_object,
            )

    handle = EnumProperty(
            name="Handle",
            items=(
                ('1', "Handle 01", ""),
                ('2', "Handle 02", ""),
                ('3', "Handle 03", ""),
                ('4', "Handle 04", ""),
                ('0', "None", ""),
                ),
            description="Handle model",
            update=update_object,
            )

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
    glpoint_e = FloatVectorProperty(
            name="glpointc",
            description="Hidden property for opengl",
            default=(0, 0, 0),
            )

# Register
bpy.utils.register_class(ObjectProperties)
Object.DoorObjectGenerator = CollectionProperty(type=ObjectProperties)


# ------------------------------------------------------------------
# Define panel class to modify object
# ------------------------------------------------------------------
class AchmDoorObjectgeneratorpanel(Panel):
    bl_idname = "OBJECT_PT_door_generator"
    bl_label = "Door"
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
        if 'DoorObjectGenerator' not in o:
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
            if 'DoorObjectGenerator' not in o:
                return
        except:
            return

        layout = self.layout
        if bpy.context.mode == 'EDIT_MESH':
            layout.label('Warning: Operator does not work in edit mode.', icon='ERROR')
        else:
            myobjdat = o.DoorObjectGenerator[0]
            space = bpy.context.space_data
            if not space.local_view:
                # Imperial units warning
                if bpy.context.scene.unit_settings.system == "IMPERIAL":
                    row = layout.row()
                    row.label("Warning: Imperial units not supported", icon='COLOR_RED')
                box = layout.box()
                row = box.row()
                row.prop(myobjdat, 'frame_width')
                row.prop(myobjdat, 'frame_height')
                row = box.row()
                row.prop(myobjdat, 'frame_thick')
                row.prop(myobjdat, 'frame_size')
                row = box.row()
                row.prop(myobjdat, 'r')

                box = layout.box()
                row = box.row()
                row.prop(myobjdat, 'openside')
                if myobjdat.openside == "3":
                    row.prop(myobjdat, "factor")

                layout.prop(myobjdat, 'model')
                layout.prop(myobjdat, 'handle')

                box = layout.box()
                if not context.scene.render.engine == 'CYCLES':
                    box.enabled = False
                box.prop(myobjdat, 'crt_mat')
            else:
                row = layout.row()
                row.label("Warning: Operator does not work in local view mode", icon='ERROR')


# ------------------------------------------------------------------------------
# Create Doorframe
# ------------------------------------------------------------------------------
def create_doorframe(mp, mymesh):
    tf = mp.frame_thick / 3
    sf = mp.frame_size
    wf = (mp.frame_width / 2) - sf
    hf = mp.frame_height - sf
    gap = 0.02
    deep = mp.frame_thick * 0.50

    verts = [(-wf - sf, -tf, 0),
             (-wf - sf, tf * 2, 0),
             (-wf, tf * 2, 0),
             (-wf - sf, -tf, hf + sf),
             (-wf - sf, tf * 2, hf + sf),
             (wf + sf, tf * 2, hf + sf),
             (wf + sf, -tf, hf + sf),
             (wf, -tf, hf),
             (-wf, tf * 2, hf),
             (wf, -tf, 0),
             (wf + sf, -tf, 0),
             (wf + sf, tf * 2, 0),
             (wf, -tf + deep, hf),
             (-wf, -tf + deep, hf),
             (-wf, -tf + deep, 0),
             (-wf + gap, -tf + deep, hf),
             (-wf + gap, -tf + deep, 0),
             (-wf + gap, tf * 2, hf),
             (-wf + gap, tf * 2, 0),
             (wf, -tf + deep, 0),
             (-wf, -tf, hf),
             (-wf, -tf, 0),
             (wf, tf * 2, hf),
             (wf, tf * 2, 0),
             (wf - gap, tf * 2, 0),
             (wf - gap, -tf + deep, 0),
             (wf - gap, tf * 2, hf),
             (wf - gap, -tf + deep, hf - gap),
             (wf - gap, -tf + deep, hf),
             (-wf + gap, tf * 2, hf - gap),
             (-wf + gap, -tf + deep, hf - gap),
             (wf - gap, tf * 2, hf - gap)]

    faces = [(3, 4, 1, 0), (7, 12, 19, 9), (4, 3, 6, 5), (10, 11, 5, 6), (13, 20, 21, 14), (17, 15, 16, 18),
             (11, 23, 22, 5),
             (20, 13, 12, 7), (20, 3, 0, 21), (9, 10, 6, 7), (13, 14, 16, 15), (4, 8, 2, 1), (29, 30, 27, 31),
             (7, 6, 3, 20),
             (8, 4, 5, 22), (14, 2, 18, 16), (17, 18, 2, 8), (28, 25, 19, 12), (28, 26, 24, 25), (25, 24, 23, 19),
             (22, 23, 24, 26),
             (29, 31, 26, 17), (15, 28, 27, 30), (8, 22, 26)]

    mymesh.from_pydata(verts, [], faces)
    mymesh.update(calc_edges=True)

    return


# ------------------------------------------------------------------------------
# Make one door
#
# ------------------------------------------------------------------------------
def make_one_door(self, myframe, width, openside):
    mydoor = create_door_data(self, myframe, width, openside)
    handle1 = None
    handle2 = None
    if self.handle != "0":
        handle1 = create_handle(self, mydoor, "Front", width, openside)
        handle1.select = True
        bpy.context.scene.objects.active = handle1
        set_smooth(handle1)
        set_modifier_subsurf(handle1)
        handle2 = create_handle(self, mydoor, "Back", width, openside)
        set_smooth(handle2)
        set_modifier_subsurf(handle2)
    # Create materials
    if self.crt_mat and bpy.context.scene.render.engine == 'CYCLES':
        # Door material
        mat = create_diffuse_material("Door_material", False, 0.8, 0.8, 0.8)
        set_material(mydoor, mat)
        # Handle material
        if self.handle != "0":
            mat = create_glossy_material("Handle_material", False, 0.733, 0.779, 0.8)
            set_material(handle1, mat)
            set_material(handle2, mat)
        if self.model == "5" or self.model == "6":
            mat = create_glass_material("DoorGlass_material", False)
            mydoor.data.materials.append(mat)
            if self.model == "5":
                select_faces(mydoor, 20, True)
                select_faces(mydoor, 41, False)
            if self.model == "6":
                select_faces(mydoor, 37, True)
                select_faces(mydoor, 76, False)
            set_material_faces(mydoor, 1)

    set_normals(mydoor)

    return mydoor


# ------------------------------------------------------------------------------
# Create Door
# All custom values are passed using self container (self.myvariable)
# ------------------------------------------------------------------------------
def create_door_data(self, myframe, width, openside):
    # Retry mesh data
    if self.model == "1":
        mydata = door_model_01(self.frame_size, width, self.frame_height, self.frame_thick, openside)
    elif self.model == "2":
        mydata = door_model_02(self.frame_size, width, self.frame_height, self.frame_thick, openside)
    elif self.model == "3":
        mydata = door_model_03(self.frame_size, width, self.frame_height, self.frame_thick, openside)
    elif self.model == "4":
        mydata = door_model_04(self.frame_size, width, self.frame_height, self.frame_thick, openside)
    elif self.model == "5":
        mydata = door_model_04(self.frame_size, width, self.frame_height, self.frame_thick,
                               openside)  # uses the same mesh
    elif self.model == "6":
        mydata = door_model_02(self.frame_size, width, self.frame_height, self.frame_thick,
                               openside)  # uses the same mesh
    else:
        mydata = door_model_01(self.frame_size, width, self.frame_height, self.frame_thick, openside)  # default model

    # move data
    verts = mydata[0]
    faces = mydata[1]
    wf = mydata[2]
    deep = mydata[3]
    side = mydata[4]

    mymesh = bpy.data.meshes.new("Door")
    myobject = bpy.data.objects.new("Door", mymesh)

    myobject.location = bpy.context.scene.cursor_location
    bpy.context.scene.objects.link(myobject)

    mymesh.from_pydata(verts, [], faces)
    mymesh.update(calc_edges=True)

    # Translate to doorframe and parent
    myobject.parent = myframe
    myobject.lock_rotation = (True, True, False)
    myobject.lock_location = (True, True, True)

    myobject.location.x = ((wf / 2) * side)
    myobject.location.y = -(deep * 0.65)
    myobject.location.z = self.frame_height / 2

    return myobject


# ------------------------------------------------------------------------------
# Create Handles
# All custom values are passed using self container (self.myvariable)
# ------------------------------------------------------------------------------
def create_handle(self, mydoor, pos, frame_width, openside):
    # Retry mesh data
    if self.handle == "1":
        mydata = handle_model_01()
    elif self.handle == "2":
        mydata = handle_model_02()
    elif self.handle == "3":
        mydata = handle_model_03()
    elif self.handle == "4":
        mydata = handle_model_04()
    else:
        mydata = handle_model_01()  # default model

    # move data
    verts = mydata[0]
    faces = mydata[1]

    gap = 0.002
    sf = self.frame_size
    wf = frame_width - (sf * 2) - (gap * 2)
    deep = (self.frame_thick * 0.50) - (gap * 3)
    # Open to right or left
    if openside == "1":
        side = -1
    else:
        side = 1

    mymesh = bpy.data.meshes.new("Handle_" + pos)
    myobject = bpy.data.objects.new("Handle_" + pos, mymesh)

    myobject.location = bpy.context.scene.cursor_location
    bpy.context.scene.objects.link(myobject)

    mymesh.from_pydata(verts, [], faces)
    mymesh.update(calc_edges=True)
    # Rotate if pos is front
    xrot = 0.0
    yrot = 0.0
    if self.handle == "1":
        if openside != "1":
            yrot = math.pi
    else:
        yrot = 0.0

    if pos == "Front":
        xrot = math.pi

    myobject.rotation_euler = (xrot, yrot, 0.0)  # radians PI=180

    # Translate to door and parent (depend of model of door)
    if self.model == "1":
        myobject.location.x = (wf * side) + (0.072 * side * -1)
        if pos == "Front":
            myobject.location.y = deep - 0.005
        else:
            myobject.location.y = 0.005

    if self.model == "2" or self.model == "6":
        myobject.location.x = (wf * side) + (0.060 * side * -1)
        if pos == "Front":
            myobject.location.y = deep - 0.011
        else:
            myobject.location.y = 0.00665

    if self.model == "3":
        myobject.location.x = (wf * side) + (0.060 * side * -1)
        if pos == "Front":
            myobject.location.y = deep - 0.011
        else:
            myobject.location.y = 0.00665

    if self.model == "4" or self.model == "5":
        myobject.location.x = (wf * side) + (0.060 * side * -1)
        if pos == "Front":
            myobject.location.y = deep - 0.011
        else:
            myobject.location.y = 0.00665

    myobject.location.z = 0
    myobject.parent = mydoor
    myobject.lock_rotation = (True, False, True)

    return myobject


# ----------------------------------------------
# Door model 01
# ----------------------------------------------
def door_model_01(frame_size, frame_width, frame_height, frame_thick, openside):
    # ------------------------------------
    # Mesh data
    # ------------------------------------
    gap = 0.002
    sf = frame_size
    wf = frame_width - (sf * 2) - (gap * 2)
    hf = (frame_height / 2) - (gap * 2)
    deep = (frame_thick * 0.50) - (gap * 3)
    # Open to right or left
    if openside == "1":
        side = 1
        minx = wf * -1
        maxx = 0.0
    else:
        side = -1
        minx = 0.0
        maxx = wf

    miny = 0.0  # locked
    maxy = deep
    minz = -hf
    maxz = hf - sf - gap

    # Vertex
    myvertex = [(minx, miny, minz),
                (minx, maxy, minz),
                (maxx, maxy, minz),
                (maxx, miny, minz),
                (minx, miny, maxz),
                (minx, maxy, maxz),
                (maxx, maxy, maxz),
                (maxx, miny, maxz)]

    # Faces
    myfaces = [(4, 5, 1, 0), (5, 6, 2, 1), (6, 7, 3, 2), (7, 4, 0, 3), (0, 1, 2, 3),
               (7, 6, 5, 4)]

    return myvertex, myfaces, wf, deep, side


# ----------------------------------------------
# Door model 02
# ----------------------------------------------
def door_model_02(frame_size, frame_width, frame_height, frame_thick, openside):
    gap = 0.002
    sf = frame_size
    wf = frame_width - (sf * 2) - (gap * 2)
    hf = (frame_height / 2) - (gap * 2)
    deep = (frame_thick * 0.50)

    # ------------------------------------
    # Mesh data
    # ------------------------------------
    # Open to right or left
    if openside == "1":
        side = 1
        minx = wf * -1
        maxx = 0.0
    else:
        side = -1
        minx = 0.0
        maxx = wf

    maxy = deep
    minz = -hf
    maxz = hf - sf - gap

    # Vertex
    myvertex = [(minx, -1.57160684466362e-08, minz + 2.384185791015625e-06),
                (maxx, -1.5599653124809265e-08, minz),
                (minx, -1.5599653124809265e-08, maxz),
                (minx, -1.5599653124809265e-08, maxz - 0.12999999523162842),
                (minx, -1.57160684466362e-08, minz + 0.2500007152557373),
                (maxx, -1.5599653124809265e-08, minz + 0.25000011920928955),
                (maxx, -1.5599653124809265e-08, maxz),
                (maxx, -1.5599653124809265e-08, maxz - 0.12999999523162842),
                (maxx - 0.11609852313995361, -1.5599653124809265e-08, maxz),
                (maxx - 0.12357193231582642, -1.5599653124809265e-08, minz),
                (maxx - 0.11658430099487305, -1.5599653124809265e-08, maxz - 0.12999999523162842),
                (maxx - 0.12263774871826172, -1.5599653124809265e-08, minz + 0.25000011920928955),
                (minx, -1.57160684466362e-08, minz + 0.8700000941753387),
                (maxx, -1.5599653124809265e-08, minz + 0.8700000941753387),
                (maxx - 0.12076938152313232, -1.5599653124809265e-08, minz + 0.7500001192092896),
                (minx + 0.11735659837722778, -1.57160684466362e-08, minz + 0.25000011920928955),
                (minx + 0.12341010570526123, -1.5599653124809265e-08, maxz - 0.12999999523162842),
                (minx + 0.11642247438430786, -1.57160684466362e-08, minz),
                (minx + 0.11967337131500244, -1.57160684466362e-08, minz + 0.8700000941753387),
                (minx, -1.57160684466362e-08, minz + 0.7500001192092896),
                (maxx - 0.12032097578048706, -1.5599653124809265e-08, minz + 0.8700000941753387),
                (minx + 0.12389582395553589, -1.5599653124809265e-08, maxz),
                (maxx, -1.5599653124809265e-08, minz + 0.7500001192092896),
                (minx + 0.11922496557235718, -1.57160684466362e-08, minz + 0.7500001192092896),
                (minx + 0.11922496557235718, -0.010000014677643776, minz + 0.7500001192092896),
                (minx + 0.12341010570526123, -0.010000014677643776, maxz - 0.12999999523162842),
                (maxx - 0.12032097578048706, -0.010000014677643776, minz + 0.8700000941753387),
                (minx + 0.11735659837722778, -0.010000014677643776, minz + 0.25000011920928955),
                (maxx - 0.11658430099487305, -0.010000014677643776, maxz - 0.12999999523162842),
                (maxx - 0.12263774871826172, -0.010000014677643776, minz + 0.25000011920928955),
                (minx + 0.11967337131500244, -0.010000014677643776, minz + 0.8700000941753387),
                (maxx - 0.12076938152313232, -0.010000014677643776, minz + 0.7500001192092896),
                (minx + 0.13388586044311523, -0.010000014677643776, minz + 0.7375001013278961),
                (minx + 0.1321108341217041, -0.010000014677643776, minz + 0.2625001072883606),
                (maxx - 0.1372986137866974, -0.010000014677643776, minz + 0.2625001072883606),
                (maxx - 0.13552364706993103, -0.010000014677643776, minz + 0.7375001013278961),
                (minx + 0.13802427053451538, -0.010000014677643776, maxz - 0.14747536182403564),
                (maxx - 0.13493508100509644, -0.010000014677643776, minz + 0.8866067305207253),
                (maxx - 0.13138526678085327, -0.010000014677643776, maxz - 0.14747536182403564),
                (minx + 0.13447439670562744, -0.010000014677643776, minz + 0.8866067305207253),
                (minx + 0.13388586044311523, -0.008776669390499592, minz + 0.7375001013278961),
                (minx + 0.1321108341217041, -0.008776669390499592, minz + 0.2625001072883606),
                (maxx - 0.1372986137866974, -0.008776669390499592, minz + 0.2625001072883606),
                (maxx - 0.13552364706993103, -0.008776669390499592, minz + 0.7375001013278961),
                (minx + 0.13802427053451538, -0.008776669390499592, maxz - 0.14747536182403564),
                (maxx - 0.13493508100509644, -0.008776669390499592, minz + 0.8866067305207253),
                (maxx - 0.13138526678085327, -0.008776669390499592, maxz - 0.14747536182403564),
                (minx + 0.13447439670562744, -0.008776669390499592, minz + 0.8866067305207253),
                (minx, maxy - 0.009999999776482582, minz + 2.384185791015625e-06),
                (maxx, maxy - 0.009999999776482582, minz),
                (minx, maxy - 0.009999999776482582, maxz),
                (minx, maxy - 0.009999999776482582, maxz - 0.12999999523162842),
                (minx, maxy - 0.009999999776482582, minz + 0.2500007152557373),
                (maxx, maxy - 0.009999999776482582, minz + 0.25000011920928955),
                (maxx, maxy - 0.009999999776482582, maxz),
                (maxx, maxy - 0.009999999776482582, maxz - 0.12999999523162842),
                (maxx - 0.11609852313995361, maxy - 0.009999999776482582, maxz),
                (maxx - 0.12357193231582642, maxy - 0.009999999776482582, minz),
                (maxx - 0.11658430099487305, maxy - 0.009999999776482582, maxz - 0.12999999523162842),
                (maxx - 0.12263774871826172, maxy - 0.009999999776482582, minz + 0.25000011920928955),
                (minx, maxy - 0.009999999776482582, minz + 0.8700000941753387),
                (maxx, maxy - 0.009999999776482582, minz + 0.8700000941753387),
                (maxx - 0.12076938152313232, maxy - 0.009999999776482582, minz + 0.7500001192092896),
                (minx + 0.11735659837722778, maxy - 0.009999999776482582, minz + 0.25000011920928955),
                (minx + 0.12341010570526123, maxy - 0.009999999776482582, maxz - 0.12999999523162842),
                (minx + 0.11642247438430786, maxy - 0.009999999776482582, minz),
                (minx + 0.11967337131500244, maxy - 0.009999999776482582, minz + 0.8700000941753387),
                (minx, maxy - 0.009999999776482582, minz + 0.7500001192092896),
                (maxx - 0.12032097578048706, maxy - 0.009999999776482582, minz + 0.8700000941753387),
                (minx + 0.12389582395553589, maxy - 0.009999999776482582, maxz),
                (maxx, maxy - 0.009999999776482582, minz + 0.7500001192092896),
                (minx + 0.11922496557235718, maxy - 0.009999999776482582, minz + 0.7500001192092896),
                (minx + 0.11922496557235718, maxy, minz + 0.7500001192092896),
                (minx + 0.12341010570526123, maxy, maxz - 0.12999999523162842),
                (maxx - 0.12032097578048706, maxy, minz + 0.8700000941753387),
                (minx + 0.11735659837722778, maxy, minz + 0.25000011920928955),
                (maxx - 0.11658430099487305, maxy, maxz - 0.12999999523162842),
                (maxx - 0.12263774871826172, maxy, minz + 0.25000011920928955),
                (minx + 0.11967337131500244, maxy, minz + 0.8700000941753387),
                (maxx - 0.12076938152313232, maxy, minz + 0.7500001192092896),
                (minx + 0.13388586044311523, maxy, minz + 0.7375001013278961),
                (minx + 0.1321108341217041, maxy, minz + 0.2625001072883606),
                (maxx - 0.1372986137866974, maxy, minz + 0.2625001072883606),
                (maxx - 0.13552364706993103, maxy, minz + 0.7375001013278961),
                (minx + 0.13802427053451538, maxy, maxz - 0.14747536182403564),
                (maxx - 0.13493508100509644, maxy, minz + 0.8866067305207253),
                (maxx - 0.13138526678085327, maxy, maxz - 0.14747536182403564),
                (minx + 0.13447439670562744, maxy, minz + 0.8866067305207253),
                (minx + 0.13388586044311523, maxy - 0.0012233443558216095, minz + 0.7375001013278961),
                (minx + 0.1321108341217041, maxy - 0.0012233443558216095, minz + 0.2625001072883606),
                (maxx - 0.1372986137866974, maxy - 0.0012233443558216095, minz + 0.2625001072883606),
                (maxx - 0.13552364706993103, maxy - 0.0012233443558216095, minz + 0.7375001013278961),
                (minx + 0.13802427053451538, maxy - 0.0012233443558216095, maxz - 0.14747536182403564),
                (maxx - 0.13493508100509644, maxy - 0.0012233443558216095, minz + 0.8866067305207253),
                (maxx - 0.13138526678085327, maxy - 0.0012233443558216095, maxz - 0.14747536182403564),
                (minx + 0.13447439670562744, maxy - 0.0012233443558216095, minz + 0.8866067305207253)]

    # Faces
    myfaces = [(15, 4, 0, 17), (21, 2, 3, 16), (23, 19, 4, 15), (6, 8, 10, 7), (8, 21, 16, 10),
               (16, 3, 12, 18), (11, 15, 17, 9), (20, 18, 23, 14), (18, 12, 19, 23), (5, 11, 9, 1),
               (22, 14, 11, 5), (7, 10, 20, 13), (13, 20, 14, 22), (20, 10, 28, 26), (10, 16, 25, 28),
               (16, 18, 30, 25), (18, 20, 26, 30), (15, 11, 29, 27), (14, 23, 24, 31), (23, 15, 27, 24),
               (11, 14, 31, 29), (31, 24, 32, 35), (24, 27, 33, 32), (27, 29, 34, 33), (29, 31, 35, 34),
               (26, 28, 38, 37), (30, 26, 37, 39), (28, 25, 36, 38), (25, 30, 39, 36), (33, 34, 42, 41),
               (36, 39, 47, 44), (34, 35, 43, 42), (37, 38, 46, 45), (32, 33, 41, 40), (38, 36, 44, 46),
               (35, 32, 40, 43), (39, 37, 45, 47), (18, 20, 10, 16), (14, 23, 15, 11), (63, 52, 48, 65),
               (69, 50, 51, 64), (71, 67, 52, 63), (54, 56, 58, 55), (56, 69, 64, 58), (64, 51, 60, 66),
               (59, 63, 65, 57), (68, 66, 71, 62), (66, 60, 67, 71), (53, 59, 57, 49), (70, 62, 59, 53),
               (55, 58, 68, 61), (61, 68, 62, 70), (68, 58, 76, 74), (58, 64, 73, 76), (64, 66, 78, 73),
               (66, 68, 74, 78), (63, 59, 77, 75), (62, 71, 72, 79), (71, 63, 75, 72), (59, 62, 79, 77),
               (79, 72, 80, 83), (72, 75, 81, 80), (75, 77, 82, 81), (77, 79, 83, 82), (74, 76, 86, 85),
               (78, 74, 85, 87), (76, 73, 84, 86), (73, 78, 87, 84), (81, 82, 90, 89), (84, 87, 95, 92),
               (82, 83, 91, 90), (85, 86, 94, 93), (80, 81, 89, 88), (86, 84, 92, 94), (83, 80, 88, 91),
               (87, 85, 93, 95), (66, 68, 58, 64), (62, 71, 63, 59), (50, 2, 21, 69), (8, 56, 69, 21),
               (6, 54, 56, 8), (54, 6, 7, 55), (55, 7, 13, 61), (61, 13, 22, 70), (5, 53, 70, 22),
               (1, 49, 53, 5), (49, 1, 9, 57), (57, 9, 17, 65), (0, 48, 65, 17), (48, 0, 4, 52),
               (52, 4, 19, 67), (12, 60, 67, 19), (3, 51, 60, 12), (2, 50, 51, 3)]

    return myvertex, myfaces, wf, deep, side


# ----------------------------------------------
# Door model 03
# ----------------------------------------------
def door_model_03(frame_size, frame_width, frame_height, frame_thick, openside):
    gap = 0.002
    sf = frame_size
    wf = frame_width - (sf * 2) - (gap * 2)
    hf = (frame_height / 2) - (gap * 2)
    deep = (frame_thick * 0.50)

    # ------------------------------------
    # Mesh data
    # ------------------------------------
    # Open to right or left
    if openside == "1":
        side = 1
        minx = wf * -1
        maxx = 0.0
    else:
        side = -1
        minx = 0.0
        maxx = wf

    miny = 0.0  # Locked

    maxy = deep
    minz = -hf
    maxz = hf - sf - gap

    # Vertex
    myvertex = [(minx, -1.5599653124809265e-08, maxz),
                (maxx, -1.5599653124809265e-08, maxz),
                (minx, maxy, maxz),
                (maxx, maxy, maxz),
                (maxx - 0.10429960489273071, -1.5832483768463135e-08, maxz),
                (minx + 0.10429966449737549, -1.5832483768463135e-08, maxz),
                (minx + 0.10429966449737549, maxy, maxz),
                (minx, -1.5628756955266e-08, maxz - 0.5012519359588623),
                (maxx, -1.5599653124809265e-08, maxz - 0.5012525320053101),
                (minx, maxy, maxz - 0.5012519359588623),
                (maxx, maxy, maxz - 0.5012525320053101),
                (maxx - 0.10429960489273071, -1.5832483768463135e-08, maxz - 0.501252293586731),
                (minx + 0.10429966449737549, -1.5832483768463135e-08, maxz - 0.5012521147727966),
                (minx + 0.10429966449737549, maxy, maxz - 0.5012521147727966),
                (maxx - 0.10429960489273071, maxy, maxz - 0.501252293586731),
                (minx + 0.11909735202789307, -1.5832483768463135e-08, maxz),
                (maxx - 0.11909729242324829, -1.5832483768463135e-08, maxz),
                (minx + 0.11909735202789307, maxy, maxz),
                (minx + 0.11909735202789307, -1.5832483768463135e-08, maxz - 0.5012521743774414),
                (maxx - 0.11909729242324829, -1.5832483768463135e-08, maxz - 0.5012522339820862),
                (minx, -1.5629622041046787e-08, maxz - 0.516154021024704),
                (maxx, -1.5599653124809265e-08, maxz - 0.5161546468734741),
                (minx, maxy, maxz - 0.516154021024704),
                (maxx, maxy, maxz - 0.5161546468734741),
                (maxx - 0.10429960489273071, -1.5832483768463135e-08, maxz - 0.516154408454895),
                (minx + 0.10429966449737549, -1.5832483768463135e-08, maxz - 0.5161541998386383),
                (maxx - 0.10429960489273071, maxy, maxz - 0.516154408454895),
                (maxx - 0.11909729242324829, -1.5832483768463135e-08, maxz - 0.5161543190479279),
                (minx + 0.11909735202789307, -1.5832483768463135e-08, maxz - 0.5161542594432831),
                (minx + 0.11909735202789307, maxy, maxz - 0.5161542594432831),
                (maxx - 0.10429960489273071, miny + 0.009999999776482582, maxz),
                (minx + 0.10429966449737549, miny + 0.009999999776482582, maxz),
                (maxx - 0.10429960489273071, miny + 0.009999999776482582, maxz - 0.501252293586731),
                (minx + 0.10429966449737549, miny + 0.009999999776482582, maxz - 0.5012521147727966),
                (minx + 0.11909735202789307, miny + 0.009999999776482582, maxz),
                (maxx - 0.11909729242324829, miny + 0.009999999776482582, maxz),
                (minx + 0.11909735202789307, miny + 0.009999999776482582, maxz - 0.5012521743774414),
                (maxx - 0.11909729242324829, miny + 0.009999999776482582, maxz - 0.5012522339820862),
                (maxx - 0.10429960489273071, miny + 0.009999999776482582, maxz - 0.516154408454895),
                (minx + 0.10429966449737549, miny + 0.009999999776482582, maxz - 0.5161541998386383),
                (maxx - 0.11909729242324829, miny + 0.009999999776482582, maxz - 0.5161543190479279),
                (minx + 0.11909735202789307, miny + 0.009999999776482582, maxz - 0.5161542594432831),
                (maxx - 0.11909729242324829, -1.5832483768463135e-08, maxz - 0.992994874715805),
                (minx + 0.11909735202789307, -1.5832483768463135e-08, maxz - 0.9929947257041931),
                (minx + 0.11909735202789307, maxy, maxz - 0.9929947257041931),
                (maxx - 0.11909738183021545, maxy, maxz - 0.992994874715805),
                (minx, -1.565730833874568e-08, maxz - 0.9929942488670349),
                (maxx, -1.5599653124809265e-08, maxz - 0.9929954260587692),
                (minx, maxy, maxz - 0.9929942488670349),
                (maxx, maxy, maxz - 0.9929954260587692),
                (maxx - 0.10429960489273071, -1.5832483768463135e-08, maxz - 0.9929950088262558),
                (minx + 0.10429966449737549, -1.5832483768463135e-08, maxz - 0.9929945915937424),
                (maxx - 0.10429960489273071, maxy, maxz - 0.9929950088262558),
                (maxx - 0.10429960489273071, miny + 0.009999999776482582, maxz - 0.9929950088262558),
                (minx + 0.10429966449737549, miny + 0.009999999776482582, maxz - 0.9929945915937424),
                (maxx - 0.11909729242324829, miny + 0.009999999776482582, maxz - 0.992994874715805),
                (minx + 0.11909735202789307, miny + 0.009999999776482582, maxz - 0.9929947257041931),
                (maxx - 0.11909729242324829, maxy - 0.0004077646881341934, maxz - 0.992994874715805),
                (maxx - 0.10429960489273071, maxy - 0.0004077646881341934, maxz - 0.9929950088262558),
                (maxx - 0.10429960489273071, maxy, maxz),
                (maxx - 0.11909729242324829, maxy, maxz),
                (maxx - 0.11909738183021545, maxy, maxz - 0.5012522339820862),
                (minx + 0.11909735202789307, maxy, maxz - 0.5012521743774414),
                (minx + 0.10429966449737549, maxy, maxz - 0.5161541998386383),
                (maxx - 0.11909738183021545, maxy, maxz - 0.5161543190479279),
                (minx + 0.10429966449737549, maxy, maxz - 0.9929945915937424),
                (maxx - 0.10429960489273071, maxy - 0.008999999612569809, maxz),
                (minx + 0.10429966449737549, maxy - 0.008999999612569809, maxz),
                (minx + 0.10429966449737549, maxy - 0.008999999612569809, maxz - 0.5012521147727966),
                (maxx - 0.10429960489273071, maxy - 0.008999999612569809, maxz - 0.501252293586731),
                (minx + 0.11909735202789307, maxy - 0.008999999612569809, maxz),
                (maxx - 0.11909729242324829, maxy - 0.008999999612569809, maxz),
                (maxx - 0.11909738183021545, maxy - 0.008999999612569809, maxz - 0.5012522339820862),
                (minx + 0.11909735202789307, maxy - 0.008999999612569809, maxz - 0.5012521743774414),
                (minx + 0.10429966449737549, maxy - 0.008999999612569809, maxz - 0.5161541998386383),
                (maxx - 0.10429960489273071, maxy - 0.008999999612569809, maxz - 0.516154408454895),
                (minx + 0.11909735202789307, maxy - 0.008999999612569809, maxz - 0.5161542594432831),
                (maxx - 0.11909738183021545, maxy - 0.008999999612569809, maxz - 0.5161543190479279),
                (minx + 0.11909735202789307, maxy - 0.008999999612569809, maxz - 0.9929947257041931),
                (maxx - 0.11909738183021545, maxy - 0.008999999612569809, maxz - 0.992994874715805),
                (minx + 0.10429966449737549, maxy - 0.008999999612569809, maxz - 0.9929945915937424),
                (maxx - 0.10429960489273071, maxy - 0.008999999612569809, maxz - 0.9929950088262558),
                (minx, -1.5599653124809265e-08, minz),
                (maxx, -1.5599653124809265e-08, minz),
                (minx, maxy, minz),
                (maxx, maxy, minz),
                (maxx - 0.10429960489273071, -1.5832483768463135e-08, minz),
                (minx + 0.10429966449737549, -1.5832483768463135e-08, minz),
                (minx + 0.10429966449737549, maxy, minz),
                (minx, -1.5628756955266e-08, minz + 0.5012519359588623),
                (minx, -1.5657860785722733e-08, minz + 1.0025038719177246),
                (maxx, -1.5599653124809265e-08, minz + 0.5012525320053101),
                (maxx, -1.5599653124809265e-08, minz + 1.0025050640106201),
                (minx, maxy, minz + 0.5012519359588623),
                (minx, maxy, minz + 1.0025038719177246),
                (maxx, maxy, minz + 0.5012525320053101),
                (maxx, maxy, minz + 1.0025050640106201),
                (maxx - 0.10429960489273071, -1.5832483768463135e-08, minz + 0.501252293586731),
                (maxx - 0.10429960489273071, -1.5832483768463135e-08, minz + 1.0025046467781067),
                (minx + 0.10429966449737549, -1.5832483768463135e-08, minz + 0.5012521147727966),
                (minx + 0.10429966449737549, -1.5832483768463135e-08, minz + 1.0025042295455933),
                (minx + 0.10429966449737549, maxy, minz + 0.5012521147727966),
                (minx + 0.10429966449737549, maxy, minz + 1.0025042295455933),
                (maxx - 0.10429960489273071, maxy, minz + 0.501252293586731),
                (maxx - 0.10429960489273071, maxy, minz + 1.0025046467781067),
                (minx + 0.11909735202789307, -1.5832483768463135e-08, minz),
                (maxx - 0.11909729242324829, -1.5832483768463135e-08, minz),
                (minx + 0.11909735202789307, maxy, minz),
                (minx + 0.11909735202789307, -1.5832483768463135e-08, minz + 0.5012521743774414),
                (maxx - 0.11909729242324829, -1.5832483768463135e-08, minz + 0.5012522339820862),
                (minx + 0.11909735202789307, maxy, minz + 1.0025043686230788),
                (minx, -1.5629622041046787e-08, minz + 0.516154021024704),
                (maxx, -1.5599653124809265e-08, minz + 0.5161546468734741),
                (minx, maxy, minz + 0.516154021024704),
                (maxx, maxy, minz + 0.5161546468734741),
                (maxx - 0.10429960489273071, -1.5832483768463135e-08, minz + 0.516154408454895),
                (minx + 0.10429966449737549, -1.5832483768463135e-08, minz + 0.5161541998386383),
                (maxx - 0.10429960489273071, maxy, minz + 0.516154408454895),
                (maxx - 0.11909729242324829, -1.5832483768463135e-08, minz + 0.5161543190479279),
                (minx + 0.11909735202789307, -1.5832483768463135e-08, minz + 0.5161542594432831),
                (minx + 0.11909735202789307, maxy, minz + 0.5161542594432831),
                (maxx - 0.10429960489273071, miny + 0.009999999776482582, minz),
                (minx + 0.10429966449737549, miny + 0.009999999776482582, minz),
                (maxx - 0.10429960489273071, miny + 0.009999999776482582, minz + 0.501252293586731),
                (maxx - 0.10429960489273071, miny + 0.009999999776482582, minz + 1.0025046467781067),
                (minx + 0.10429966449737549, miny + 0.009999999776482582, minz + 0.5012521147727966),
                (minx + 0.10429966449737549, miny + 0.009999999776482582, minz + 1.0025042295455933),
                (minx + 0.11909735202789307, miny + 0.009999999776482582, minz),
                (maxx - 0.11909729242324829, miny + 0.009999999776482582, minz),
                (minx + 0.11909735202789307, miny + 0.009999999776482582, minz + 0.5012521743774414),
                (maxx - 0.11909729242324829, miny + 0.009999999776482582, minz + 0.5012522339820862),
                (maxx - 0.10429960489273071, miny + 0.009999999776482582, minz + 0.516154408454895),
                (minx + 0.10429966449737549, miny + 0.009999999776482582, minz + 0.5161541998386383),
                (maxx - 0.11909729242324829, miny + 0.009999999776482582, minz + 0.5161543190479279),
                (minx + 0.11909735202789307, miny + 0.009999999776482582, minz + 0.5161542594432831),
                (maxx - 0.11909729242324829, -1.5832483768463135e-08, minz + 0.992994874715805),
                (minx + 0.11909735202789307, -1.5832483768463135e-08, minz + 0.9929947257041931),
                (minx + 0.11909735202789307, maxy, minz + 0.9929947257041931),
                (maxx - 0.11909738183021545, maxy, minz + 0.992994874715805),
                (minx, -1.565730833874568e-08, minz + 0.9929942488670349),
                (maxx, -1.5599653124809265e-08, minz + 0.9929954260587692),
                (minx, maxy, minz + 0.9929942488670349),
                (maxx, maxy, minz + 0.9929954260587692),
                (maxx - 0.10429960489273071, -1.5832483768463135e-08, minz + 0.9929950088262558),
                (minx + 0.10429966449737549, -1.5832483768463135e-08, minz + 0.9929945915937424),
                (maxx - 0.10429960489273071, maxy, minz + 0.9929950088262558),
                (maxx - 0.10429960489273071, miny + 0.009999999776482582, minz + 0.9929950088262558),
                (minx + 0.10429966449737549, miny + 0.009999999776482582, minz + 0.9929945915937424),
                (minx + 0.11909735202789307, miny + 0.009999999776482582, minz + 1.0025043686231356),
                (maxx - 0.11909729242324829, miny + 0.009999999776482582, minz + 1.0025045077006212),
                (maxx - 0.10429960489273071, maxy - 0.0004077646881341934, minz + 1.0025046467781067),
                (maxx - 0.11909729242324829, maxy - 0.0004077646881341934, minz + 1.0025045077006212),
                (maxx - 0.11909729242324829, miny + 0.009999999776482582, minz + 0.992994874715805),
                (minx + 0.11909735202789307, miny + 0.009999999776482582, minz + 0.9929947257041931),
                (maxx - 0.11909729242324829, maxy - 0.0004077646881341934, minz + 0.992994874715805),
                (maxx - 0.10429960489273071, maxy - 0.0004077646881341934, minz + 0.9929950088262558),
                (maxx - 0.10429960489273071, maxy, minz),
                (maxx - 0.11909729242324829, maxy, minz),
                (maxx - 0.11909738183021545, maxy, minz + 0.5012522339820862),
                (minx + 0.11909735202789307, maxy, minz + 0.5012521743774414),
                (maxx - 0.11909738183021545, maxy, minz + 1.0025045077005643),
                (minx + 0.10429966449737549, maxy, minz + 0.5161541998386383),
                (maxx - 0.11909738183021545, maxy, minz + 0.5161543190479279),
                (minx + 0.10429966449737549, maxy, minz + 0.9929945915937424),
                (maxx - 0.10429960489273071, maxy - 0.008999999612569809, minz),
                (minx + 0.10429966449737549, maxy - 0.008999999612569809, minz),
                (minx + 0.10429966449737549, maxy - 0.008999999612569809, minz + 0.5012521147727966),
                (minx + 0.10429966449737549, maxy - 0.008999999612569809, minz + 1.0025042295455933),
                (maxx - 0.10429960489273071, maxy - 0.008999999612569809, minz + 0.501252293586731),
                (maxx - 0.10429960489273071, maxy - 0.008999999612569809, minz + 1.0025046467781067),
                (minx + 0.11909735202789307, maxy - 0.008999999612569809, minz),
                (maxx - 0.11909729242324829, maxy - 0.008999999612569809, minz),
                (maxx - 0.11909738183021545, maxy - 0.008999999612569809, minz + 0.5012522339820862),
                (minx + 0.11909735202789307, maxy - 0.008999999612569809, minz + 0.5012521743774414),
                (maxx - 0.11909738183021545, maxy - 0.008999999612569809, minz + 1.0025045077005643),
                (minx + 0.11909735202789307, maxy - 0.008999999612569809, minz + 1.0025043686230788),
                (minx + 0.10429966449737549, maxy - 0.008999999612569809, minz + 0.5161541998386383),
                (maxx - 0.10429960489273071, maxy - 0.008999999612569809, minz + 0.516154408454895),
                (minx + 0.11909735202789307, maxy - 0.008999999612569809, minz + 0.5161542594432831),
                (maxx - 0.11909738183021545, maxy - 0.008999999612569809, minz + 0.5161543190479279),
                (minx + 0.11909735202789307, maxy - 0.008999999612569809, minz + 0.9929947257041931),
                (maxx - 0.11909738183021545, maxy - 0.008999999612569809, minz + 0.992994874715805),
                (minx + 0.10429966449737549, maxy - 0.008999999612569809, minz + 0.9929945915937424),
                (maxx - 0.10429960489273071, maxy - 0.008999999612569809, minz + 0.9929950088262558)]

    # Faces
    myfaces = [(2, 0, 5, 6), (3, 1, 8, 10), (49, 47, 92, 96), (0, 2, 9, 7), (46, 48, 94, 90),
               (5, 0, 7, 12), (51, 46, 90, 100), (52, 49, 96, 104), (1, 4, 11, 8), (47, 50, 98, 92),
               (12, 25, 39, 33), (2, 6, 13, 9), (5, 12, 33, 31), (16, 15, 18, 19), (18, 15, 34, 36),
               (10, 8, 21, 23), (7, 9, 22, 20), (12, 7, 20, 25), (14, 10, 23, 26), (8, 11, 24, 21),
               (51, 100, 126, 54), (24, 11, 32, 38), (16, 19, 37, 35), (34, 31, 33, 36), (30, 35, 37, 32),
               (36, 33, 39, 41), (32, 37, 40, 38), (37, 36, 41, 40), (19, 18, 36, 37), (28, 27, 40, 41),
               (20, 22, 48, 46), (11, 4, 30, 32), (23, 21, 47, 49), (50, 24, 38, 53), (25, 20, 46, 51),
               (26, 23, 49, 52), (21, 24, 50, 47), (27, 28, 43, 42), (25, 51, 54, 39), (98, 50, 53, 124),
               (55, 56, 148, 149), (126, 148, 56, 54), (42, 43, 56, 55), (124, 53, 55, 149), (61, 60, 71, 72),
               (35, 30, 66, 71), (31, 34, 70, 67), (71, 66, 69, 72), (79, 81, 169, 174), (67, 70, 73, 68),
               (80, 78, 175, 167), (78, 79, 174, 175), (72, 69, 75, 77), (68, 73, 76, 74), (73, 72, 77, 76),
               (77, 75, 81, 79), (74, 76, 78, 80), (62, 61, 72, 73), (65, 63, 74, 80), (59, 4, 1, 3),
               (59, 3, 10, 14), (48, 65, 102, 94), (17, 15, 16, 60), (17, 60, 61, 62), (9, 13, 63, 22),
               (43, 28, 41, 56), (27, 42, 55, 40), (22, 63, 65, 48), (29, 64, 45, 44), (41, 39, 54, 56),
               (38, 40, 55, 53), (29, 44, 78, 76), (63, 13, 68, 74), (17, 62, 73, 70), (52, 104, 169, 81),
               (64, 29, 76, 77), (13, 6, 67, 68), (59, 14, 69, 66), (44, 45, 79, 78), (45, 64, 77, 79),
               (14, 26, 75, 69), (26, 52, 81, 75), (102, 65, 80, 167), (84, 88, 87, 82), (85, 95, 91, 83),
               (142, 96, 92, 140), (82, 89, 93, 84), (139, 90, 94, 141), (87, 99, 89, 82), (144, 100, 90, 139),
               (145, 104, 96, 142), (83, 91, 97, 86), (140, 92, 98, 143), (99, 125, 132, 116), (84, 93, 101, 88),
               (87, 122, 125, 99), (106, 109, 108, 105), (108, 129, 127, 105), (95, 114, 112, 91), (89, 111, 113, 93),
               (99, 116, 111, 89), (103, 117, 114, 95), (91, 112, 115, 97), (144, 147, 126, 100), (115, 131, 123, 97),
               (106, 128, 130, 109), (127, 129, 125, 122), (121, 123, 130, 128), (129, 134, 132, 125),
               (123, 131, 133, 130),
               (130, 133, 134, 129), (109, 130, 129, 108), (119, 134, 133, 118), (111, 139, 141, 113),
               (97, 123, 121, 86),
               (114, 142, 140, 112), (143, 146, 131, 115), (116, 144, 139, 111), (117, 145, 142, 114),
               (112, 140, 143, 115),
               (118, 135, 136, 119), (116, 132, 147, 144), (98, 124, 146, 143), (152, 149, 148, 153),
               (126, 147, 153, 148),
               (135, 152, 153, 136), (124, 149, 152, 146), (158, 172, 171, 157), (128, 171, 164, 121),
               (122, 165, 170, 127),
               (171, 172, 168, 164), (181, 174, 169, 183), (165, 166, 173, 170), (182, 167, 175, 180),
               (180, 175, 174, 181),
               (172, 179, 177, 168), (166, 176, 178, 173), (173, 178, 179, 172), (179, 181, 183, 177),
               (176, 182, 180, 178),
               (159, 173, 172, 158), (163, 182, 176, 161), (156, 85, 83, 86), (156, 103, 95, 85), (141, 94, 102, 163),
               (107, 157, 106, 105), (107, 159, 158, 157), (93, 113, 161, 101), (136, 153, 134, 119),
               (118, 133, 152, 135),
               (113, 141, 163, 161), (120, 137, 138, 162), (134, 153, 147, 132), (131, 146, 152, 133),
               (120, 178, 180, 137),
               (161, 176, 166, 101), (107, 170, 173, 159), (145, 183, 169, 104), (162, 179, 178, 120),
               (101, 166, 165, 88),
               (160, 174, 175, 110), (156, 164, 168, 103), (137, 180, 181, 138), (138, 181, 179, 162),
               (103, 168, 177, 117),
               (117, 177, 183, 145), (102, 167, 182, 163)]

    return myvertex, myfaces, wf, deep, side


# ----------------------------------------------
# Door model 04
# ----------------------------------------------
def door_model_04(frame_size, frame_width, frame_height, frame_thick, openside):
    gap = 0.002
    sf = frame_size
    wf = frame_width - (sf * 2) - (gap * 2)
    hf = (frame_height / 2) - (gap * 2)
    deep = (frame_thick * 0.50)

    # ------------------------------------
    # Mesh data
    # ------------------------------------
    # Open to right or left
    if openside == "1":
        side = 1
        minx = wf * -1
        maxx = 0.0
    else:
        side = -1
        minx = 0.0
        maxx = wf

    miny = 0.0  # Locked

    maxy = deep
    minz = -hf
    maxz = hf - sf - gap

    # Vertex
    myvertex = [(minx, miny + 0.009999997913837433, minz + 2.384185791015625e-06),
                (maxx, miny + 0.009999997913837433, minz),
                (minx, miny + 0.009999997913837433, maxz),
                (minx, miny + 0.009999997913837433, maxz - 0.12999999523162842),
                (minx, miny + 0.009999997913837433, minz + 0.25000083446502686),
                (maxx, miny + 0.009999997913837433, minz + 0.2500002384185791),
                (maxx, miny + 0.009999997913837433, maxz),
                (maxx, miny + 0.009999997913837433, maxz - 0.12999999523162842),
                (maxx - 0.11968576908111572, miny + 0.009999997913837433, maxz),
                (maxx - 0.11968576908111572, miny + 0.009999997913837433, minz),
                (maxx - 0.11968576908111572, miny + 0.009999997913837433, maxz - 0.12999999523162842),
                (maxx - 0.11968576908111572, miny + 0.009999997913837433, minz + 0.2500002384185791),
                (minx + 0.12030857801437378, miny + 0.009999997913837433, minz + 0.2500002384185791),
                (minx + 0.12030857801437378, miny + 0.009999997913837433, maxz - 0.12999999523162842),
                (minx + 0.12030857801437378, miny + 0.009999997913837433, minz),
                (minx + 0.12030857801437378, miny + 0.009999997913837433, maxz),
                (minx + 0.12030857801437378, -0.009999997913837433, maxz - 0.12999999523162842),
                (minx + 0.12030857801437378, -0.009999997913837433, minz + 0.2500002384185791),
                (maxx - 0.11968576908111572, -0.009999997913837433, maxz - 0.12999999523162842),
                (maxx - 0.11968576908111572, -0.009999997913837433, minz + 0.2500002384185791),
                (maxx - 0.13532748818397522, -0.008776653558015823, minz + 0.2625002861022949),
                (maxx - 0.13532748818397522, -0.009388323873281479, maxz - 0.14747536182403564),
                (minx + 0.13506758213043213, -0.009388323873281479, minz + 0.2625002861022949),
                (minx + 0.13506758213043213, -0.009388323873281479, maxz - 0.14747536182403564),
                (maxx - 0.13532748818397522, -0.0003883242607116699, minz + 0.2625002861022949),
                (maxx - 0.13532748818397522, -0.0003883242607116699, maxz - 0.14747536182403564),
                (minx + 0.13506758213043213, -0.0003883242607116699, minz + 0.2625002861022949),
                (minx + 0.13506758213043213, -0.0003883242607116699, maxz - 0.14747536182403564),
                (minx, maxy - 0.010000001639127731, minz + 2.384185791015625e-06),
                (maxx, maxy - 0.010000001639127731, minz),
                (minx, maxy - 0.010000001639127731, maxz),
                (minx, maxy - 0.010000001639127731, maxz - 0.12999999523162842),
                (minx, maxy - 0.010000001639127731, minz + 0.25000083446502686),
                (maxx, maxy - 0.010000001639127731, minz + 0.2500002384185791),
                (maxx, maxy - 0.010000001639127731, maxz),
                (maxx, maxy - 0.010000001639127731, maxz - 0.12999999523162842),
                (maxx - 0.11968576908111572, maxy - 0.010000001639127731, maxz),
                (maxx - 0.11968576908111572, maxy - 0.010000001639127731, minz),
                (maxx - 0.11968576908111572, maxy - 0.010000001639127731, maxz - 0.12999999523162842),
                (maxx - 0.11968576908111572, maxy - 0.010000001639127731, minz + 0.2500002384185791),
                (minx + 0.12030857801437378, maxy - 0.010000001639127731, minz + 0.2500002384185791),
                (minx + 0.12030857801437378, maxy - 0.010000001639127731, maxz - 0.12999999523162842),
                (minx + 0.12030857801437378, maxy - 0.010000001639127731, minz),
                (minx + 0.12030857801437378, maxy - 0.010000001639127731, maxz),
                (minx + 0.12030857801437378, maxy, maxz - 0.12999999523162842),
                (minx + 0.12030857801437378, maxy, minz + 0.2500002384185791),
                (maxx - 0.11968576908111572, maxy, maxz - 0.12999999523162842),
                (maxx - 0.11968576908111572, maxy, minz + 0.2500002384185791),
                (maxx - 0.1353275179862976, maxy - 0.001223348081111908, minz + 0.2625002861022949),
                (maxx - 0.1353275179862976, maxy - 0.0006116703152656555, maxz - 0.14747536182403564),
                (minx + 0.13506758213043213, maxy - 0.0006116703152656555, minz + 0.2625002861022949),
                (minx + 0.13506758213043213, maxy - 0.0006116703152656555, maxz - 0.14747536182403564),
                (maxx - 0.1353275179862976, maxy - 0.010223347693681717, minz + 0.2625002861022949),
                (maxx - 0.1353275179862976, maxy - 0.009611673653125763, maxz - 0.14747536182403564),
                (minx + 0.13506758213043213, maxy - 0.009611673653125763, minz + 0.2625002861022949),
                (minx + 0.13506758213043213, maxy - 0.009611673653125763, maxz - 0.14747536182403564)]

    # Faces
    myfaces = [(12, 4, 0, 14), (15, 2, 3, 13), (6, 8, 10, 7), (8, 15, 13, 10), (11, 12, 14, 9),
               (5, 11, 9, 1), (10, 13, 16, 18), (12, 11, 19, 17), (3, 4, 12, 13), (5, 7, 10, 11),
               (20, 22, 17, 19), (18, 21, 20, 19), (17, 22, 23, 16), (17, 16, 13, 12), (11, 10, 18, 19),
               (21, 18, 16, 23), (22, 26, 27, 23), (21, 23, 27, 25), (21, 25, 24, 20), (20, 24, 26, 22),
               (24, 25, 27, 26), (40, 42, 28, 32), (43, 41, 31, 30), (34, 35, 38, 36), (36, 38, 41, 43),
               (39, 37, 42, 40), (33, 29, 37, 39), (38, 46, 44, 41), (40, 45, 47, 39), (31, 41, 40, 32),
               (33, 39, 38, 35), (48, 47, 45, 50), (46, 47, 48, 49), (45, 44, 51, 50), (45, 40, 41, 44),
               (39, 47, 46, 38), (49, 51, 44, 46), (50, 51, 55, 54), (49, 53, 55, 51), (49, 48, 52, 53),
               (48, 50, 54, 52), (52, 54, 55, 53), (34, 36, 8, 6), (36, 43, 15, 8), (2, 15, 43, 30),
               (6, 7, 35, 34), (7, 5, 33, 35), (29, 33, 5, 1), (1, 9, 37, 29), (9, 14, 42, 37),
               (28, 42, 14, 0), (32, 4, 3, 31), (30, 31, 3, 2), (32, 28, 0, 4)]
    return myvertex, myfaces, wf, deep, side


# ----------------------------------------------
# Handle model 01
# ----------------------------------------------
def handle_model_01():
    # ------------------------------------
    # Mesh data
    # ------------------------------------
    minx = -0.04349547624588013
    maxx = 0.13793155550956726
    miny = -0.07251644879579544
    maxy = 0
    minz = -0.04352371022105217
    maxz = 0.04349301755428314

    # Vertex
    myvertex = [(minx + 0.013302795588970184, maxy - 0.002780601382255554, minz + 0.010707870125770569),
                (minx + 0.0009496212005615234, maxy - 0.002942140679806471, minz + 0.030204588547348976),
                (minx, maxy - 0.003071820829063654, maxz - 0.033750676549971104),
                (minx + 0.010708402842283249, maxy - 0.0031348932534456253, maxz - 0.013303784653544426),
                (minx + 0.03020550962537527, maxy - 0.003114458406344056, maxz - 0.0009501762688159943),
                (minx + 0.053267089650034904, maxy - 0.003015991533175111, maxz - 0.0),
                (minx + 0.07371381670236588, maxy - 0.0028658765368163586, maxz - 0.010707847774028778),
                (minx + 0.08606699481606483, maxy - 0.0027043374720960855, maxz - 0.030204561538994312),
                (minx + 0.08701662346720695, maxy - 0.0025746573228389025, minz + 0.03375071194022894),
                (minx + 0.0763082429766655, maxy - 0.002511584199965, minz + 0.013303810730576515),
                (minx + 0.05681113991886377, maxy - 0.0025320190470665693, minz + 0.0009501948952674866),
                (minx + 0.03374955803155899, maxy - 0.0026304861530661583, minz),
                (minx + 0.014472760260105133, maxy - 0.019589224830269814, minz + 0.011804874986410141),
                (minx + 0.002567145973443985, maxy - 0.019744910299777985, minz + 0.030595174990594387),
                (minx + 0.001651916652917862, maxy - 0.019869891926646233, maxz - 0.034195657819509506),
                (minx + 0.011972300708293915, maxy - 0.019930677488446236, maxz - 0.014489583671092987),
                (minx + 0.03076297417283058, maxy - 0.019910985603928566, maxz - 0.0025835558772087097),
                (minx + 0.0529889902099967, maxy - 0.019816085696220398, maxz - 0.0016677752137184143),
                (minx + 0.07269490510225296, maxy - 0.01967141032218933, maxz - 0.011987630277872086),
                (minx + 0.0846005342900753, maxy - 0.01951572299003601, maxz - 0.030777926556766033),
                (minx + 0.08551576733589172, maxy - 0.019390743225812912, minz + 0.03401290811598301),
                (minx + 0.07519540190696716, maxy - 0.01932995393872261, minz + 0.014306826516985893),
                (minx + 0.056404732167720795, maxy - 0.01934964768588543, minz + 0.002400781959295273),
                (minx + 0.03417872078716755, maxy - 0.019444547593593597, minz + 0.001484982669353485),
                (minx + 0.043508310547622386, maxy - 0.0028232389595359564, maxz - 0.043508357635801076),
                (minx + 0.029034355655312538, maxy - 0.019612153992056847, minz + 0.027617475017905235),
                (minx + 0.023084014654159546, maxy - 0.01968996599316597, minz + 0.03700872650370002),
                (minx + 0.022626593708992004, maxy - 0.01975242979824543, maxz - 0.03889966616407037),
                (minx + 0.027784643694758415, maxy - 0.019782811403274536, maxz - 0.029050718992948532),
                (minx + 0.03717608004808426, maxy - 0.019772969186306, maxz - 0.023100173100829124),
                (minx + 0.048284475691616535, maxy - 0.019725536927580833, maxz - 0.022642474621534348),
                (minx + 0.058133346028625965, maxy - 0.019653232768177986, maxz - 0.02780025824904442),
                (minx + 0.06408369168639183, maxy - 0.019575420767068863, maxz - 0.0371915097348392),
                (minx + 0.06454112380743027, maxy - 0.019512956961989403, minz + 0.03871688432991505),
                (minx + 0.059383073821663857, maxy - 0.019482573494315147, minz + 0.02886793203651905),
                (minx + 0.04999163839966059, maxy - 0.019492419436573982, minz + 0.022917380556464195),
                (minx + 0.038883245550096035, maxy - 0.0195398461073637, minz + 0.022459672763943672),
                (minx + 0.029087782837450504, maxy - 0.03150090575218201, minz + 0.027552824467420578),
                (minx + 0.023137442767620087, maxy - 0.03157871589064598, minz + 0.036944076884537935),
                (minx + 0.022680018097162247, maxy - 0.03164118155837059, maxz - 0.03896431624889374),
                (minx + 0.027838071808218956, maxy - 0.031671565026044846, maxz - 0.029115368612110615),
                (minx + 0.0372295081615448, maxy - 0.03166172280907631, maxz - 0.023164819926023483),
                (minx + 0.04833790427073836, maxy - 0.03161429241299629, maxz - 0.022707123309373856),
                (minx + 0.05818677507340908, maxy - 0.03154198080301285, maxz - 0.027864910662174225),
                (minx + 0.06413711979985237, maxy - 0.031464170664548874, maxz - 0.037256159354001284),
                (minx + 0.06459455192089081, maxy - 0.03140170872211456, minz + 0.038652234710752964),
                (minx + 0.059436503797769547, maxy - 0.03137132152915001, minz + 0.028803281486034393),
                (minx + 0.05004506651312113, maxy - 0.031381167471408844, minz + 0.022852730005979538),
                (minx + 0.038936673663556576, maxy - 0.03142859786748886, minz + 0.022395022213459015),
                (minx + 0.029038896784186363, maxy - 0.020622700452804565, minz + 0.027611978352069855),
                (minx + 0.02308855764567852, maxy - 0.02070051059126854, minz + 0.0370032312348485),
                (minx + 0.02263113297522068, maxy - 0.020762978121638298, maxz - 0.038905161898583174),
                (minx + 0.02778918668627739, maxy - 0.020793357864022255, maxz - 0.029056214727461338),
                (minx + 0.037180622573941946, maxy - 0.02078351564705372, maxz - 0.023105667904019356),
                (minx + 0.04828901821747422, maxy - 0.020736083388328552, maxz - 0.02264796942472458),
                (minx + 0.05813788715749979, maxy - 0.020663777366280556, maxz - 0.0278057549148798),
                (minx + 0.0640882346779108, maxy - 0.020585965365171432, maxz - 0.03719700500369072),
                (minx + 0.06454566307365894, maxy - 0.020523501560091972, minz + 0.0387113899923861),
                (minx + 0.05938761495053768, maxy - 0.020493119955062866, minz + 0.028862436302006245),
                (minx + 0.04999618045985699, maxy - 0.020502964034676552, minz + 0.022911883890628815),
                (minx + 0.03888778714463115, maxy - 0.02055039070546627, minz + 0.02245417609810829),
                (minx + 0.03133368864655495, maxy - 0.031504075974226, minz + 0.02999168261885643),
                (minx + 0.02630186453461647, maxy - 0.03156987577676773, minz + 0.03793327230960131),
                (minx + 0.025915050879120827, maxy - 0.03162270039319992, maxz - 0.039689837489277124),
                (minx + 0.0302768861874938, maxy - 0.031648389995098114, maxz - 0.03136120364069939),
                (minx + 0.03821863234043121, maxy - 0.03164006769657135, maxz - 0.026329202577471733),
                (minx + 0.04761230247095227, maxy - 0.03159996122121811, maxz - 0.025942156091332436),
                (minx + 0.05594087019562721, maxy - 0.03153881058096886, maxz - 0.030303767882287502),
                (minx + 0.06097269989550114, maxy - 0.03147301450371742, maxz - 0.038245356641709805),
                (minx + 0.06135952286422253, maxy - 0.03142019361257553, minz + 0.039377753622829914),
                (minx + 0.05699768662452698, maxy - 0.03139450028538704, minz + 0.03104911558330059),
                (minx + 0.049055942334234715, maxy - 0.0314028225839138, minz + 0.02601710893213749),
                (minx + 0.03966227453202009, maxy - 0.031442929059267044, minz + 0.025630054995417595),
                (minx + 0.024973656982183456, maxy - 0.009611732326447964, minz + 0.037668352015316486),
                (minx + 0.030362362042069435, maxy - 0.009541265666484833, minz + 0.029163507744669914),
                (minx + 0.02455940842628479, maxy - 0.009668299928307533, maxz - 0.03928851708769798),
                (minx + 0.029230606742203236, maxy - 0.009695813991129398, maxz - 0.030369175598025322),
                (minx + 0.03773562144488096, maxy - 0.009686900302767754, maxz - 0.02498028054833412),
                (minx + 0.04779553506523371, maxy - 0.009643946774303913, maxz - 0.02456578239798546),
                (minx + 0.056714802980422974, maxy - 0.009578464552760124, maxz - 0.02923674415796995),
                (minx + 0.0621035173535347, maxy - 0.009507997892796993, maxz - 0.037741586565971375),
                (minx + 0.06251777522265911, maxy - 0.009451429359614849, minz + 0.03921528346836567),
                (minx + 0.05784657597541809, maxy - 0.009423915296792984, minz + 0.03029593825340271),
                (minx + 0.0493415636010468, maxy - 0.009432828985154629, minz + 0.02490703947842121),
                (minx + 0.039281651843339205, maxy - 0.009475781582295895, minz + 0.02449253387749195),
                (minx + 0.03144440520554781, maxy - 0.02431209199130535, minz + 0.030186276882886887),
                (minx + 0.02647113800048828, maxy - 0.0243771281093359, minz + 0.038035438396036625),
                (minx + 0.026088828220963478, maxy - 0.024429334327578545, maxz - 0.03969699679873884),
                (minx + 0.030399901792407036, maxy - 0.02445472590625286, maxz - 0.031465294770896435),
                (minx + 0.0382492202334106, maxy - 0.024446498602628708, maxz - 0.026491858065128326),
                (minx + 0.04753356333822012, maxy - 0.024406857788562775, maxz - 0.02610931731760502),
                (minx + 0.05576520040631294, maxy - 0.024346424266695976, maxz - 0.03042016737163067),
                (minx + 0.060738470405340195, maxy - 0.024281391873955727, maxz - 0.03826932841911912),
                (minx + 0.06112079136073589, maxy - 0.024229183793067932, minz + 0.03946310793980956),
                (minx + 0.056809717789292336, maxy - 0.024203790351748466, minz + 0.03123140148818493),
                (minx + 0.04896040167659521, maxy - 0.02421201765537262, minz + 0.026257958263158798),
                (minx + 0.03967605973593891, maxy - 0.024251656606793404, minz + 0.025875410065054893),
                (minx + 0.03160235192626715, miny + 0.013056624680757523, minz + 0.02999513689428568),
                (minx + 0.02662908472120762, miny + 0.012991588562726974, minz + 0.03784429794177413),
                (minx + 0.026246773079037666, miny + 0.012939386069774628, maxz - 0.039888136787340045),
                (minx + 0.030557849444448948, miny + 0.012913990765810013, maxz - 0.03165643382817507),
                (minx + 0.03840716602280736, miny + 0.012922219932079315, maxz - 0.02668299712240696),
                (minx + 0.04769151005893946, miny + 0.012961860746145248, maxz - 0.02630045637488365),
                (minx + 0.05592314712703228, miny + 0.013022292405366898, maxz - 0.030611306428909302),
                (minx + 0.06089641526341438, miny + 0.013087328523397446, maxz - 0.038460468873381615),
                (minx + 0.06127873808145523, miny + 0.013139534741640091, minz + 0.03927196795120835),
                (minx + 0.05696766451001167, miny + 0.013164930045604706, minz + 0.031040262430906296),
                (minx + 0.04911834839731455, miny + 0.013156700879335403, minz + 0.026066819205880165),
                (minx + 0.0398340062238276, miny + 0.013117063790559769, minz + 0.02568427100777626),
                (minx + 0.03166038449853659, miny + 0.00014262646436691284, minz + 0.029924907721579075),
                (minx + 0.026687119156122208, miny + 7.76052474975586e-05, minz + 0.0377740697003901),
                (minx + 0.026304809376597404, miny + 2.5391578674316406e-05, maxz - 0.039958365727216005),
                (minx + 0.030615881085395813, miny, maxz - 0.031726663932204247),
                (minx + 0.0384651985950768, miny + 8.217990398406982e-06, maxz - 0.026753226295113564),
                (minx + 0.0477495426312089, miny + 4.7869980335235596e-05, maxz - 0.026370685547590256),
                (minx + 0.05598117969930172, miny + 0.00010830163955688477, maxz - 0.03068153653293848),
                (minx + 0.06095444969832897, miny + 0.00017333775758743286, maxz - 0.038530697114765644),
                (minx + 0.06133677065372467, miny + 0.0002255365252494812, minz + 0.039201739244163036),
                (minx + 0.05702569708228111, miny + 0.00025093555450439453, minz + 0.030970032326877117),
                (minx + 0.04917638096958399, miny + 0.00024271011352539062, minz + 0.02599659003317356),
                (minx + 0.039892038563266397, miny + 0.00020306557416915894, minz + 0.025614041835069656),
                (maxx - 0.012196376919746399, miny + 0.0031514912843704224, minz + 0.03689247788861394),
                (maxx - 0.011049121618270874, miny + 0.0037728995084762573, minz + 0.04000293998979032),
                (maxx - 0.010531991720199585, miny + 0.004111833870410919, maxz - 0.041690999176353216),
                (maxx - 0.010783538222312927, miny + 0.0040774866938591, maxz - 0.035582118667662144),
                (maxx - 0.011736378073692322, miny + 0.003679051995277405, maxz - 0.030324016697704792),
                (maxx - 0.013135172426700592, miny + 0.003023289144039154, maxz - 0.027325598523020744),
                (maxx - 0.013745412230491638, miny + 0.010863490402698517, minz + 0.03701266320422292),
                (maxx - 0.012598156929016113, miny + 0.011484891176223755, minz + 0.0401231253053993),
                (maxx - 0.012081027030944824, miny + 0.011823825538158417, maxz - 0.041570812463760376),
                (maxx - 0.01233258843421936, miny + 0.011789467185735703, maxz - 0.035461933352053165),
                (maxx - 0.013285413384437561, miny + 0.011391039937734604, maxz - 0.030203829519450665),
                (maxx - 0.014684207737445831, miny + 0.010735277086496353, maxz - 0.027205411344766617),
                (maxx - 0.000991135835647583, maxy - 0.01982143148779869, minz + 0.03712343191727996),
                (maxx - 0.0034268200397491455, maxy - 0.018987802788615227, minz + 0.03702782467007637),
                (maxx - 0.00027070939540863037, maxy - 0.018310068175196648, minz + 0.040221322793513536),
                (maxx, maxy - 0.017457325011491776, maxz - 0.04147987486794591),
                (maxx - 0.00025157630443573, maxy - 0.01749167963862419, maxz - 0.03537099435925484),
                (maxx - 0.000957980751991272, maxy - 0.018403928726911545, maxz - 0.030105633661150932),
                (maxx - 0.001929953694343567, maxy - 0.019949644804000854, maxz - 0.02709464356303215),
                (maxx - 0.0043656229972839355, maxy - 0.01911601796746254, maxz - 0.027190251275897026),
                (maxx - 0.002706393599510193, maxy - 0.01747644878923893, minz + 0.04012571508064866),
                (maxx - 0.0024356693029403687, maxy - 0.01662370003759861, maxz - 0.04157548164948821),
                (maxx - 0.0026872456073760986, maxy - 0.016658056527376175, maxz - 0.03546660114079714),
                (maxx - 0.0033936500549316406, maxy - 0.017570307478308678, maxz - 0.030201241374015808),
                (minx + 0.04382078559137881, miny + 0.00012543797492980957, minz + 0.04313003408606164)]

    # Faces
    myfaces = [(24, 0, 1), (24, 1, 2), (24, 2, 3), (24, 3, 4), (24, 4, 5),
               (24, 5, 6), (24, 6, 7), (24, 7, 8), (24, 8, 9), (24, 9, 10),
               (24, 10, 11), (11, 0, 24), (0, 12, 13, 1), (1, 13, 14, 2), (2, 14, 15, 3),
               (3, 15, 16, 4), (4, 16, 17, 5), (5, 17, 18, 6), (6, 18, 19, 7), (7, 19, 20, 8),
               (8, 20, 21, 9), (9, 21, 22, 10), (10, 22, 23, 11), (12, 0, 11, 23), (13, 12, 25, 26),
               (14, 13, 26, 27), (15, 14, 27, 28), (16, 15, 28, 29), (17, 16, 29, 30), (18, 17, 30, 31),
               (19, 18, 31, 32), (20, 19, 32, 33), (21, 20, 33, 34), (22, 21, 34, 35), (23, 22, 35, 36),
               (12, 23, 36, 25), (25, 49, 50, 26), (49, 37, 38, 50), (26, 50, 51, 27), (50, 38, 39, 51),
               (27, 51, 52, 28), (51, 39, 40, 52), (28, 52, 53, 29), (52, 40, 41, 53), (29, 53, 54, 30),
               (53, 41, 42, 54), (30, 54, 55, 31), (54, 42, 43, 55), (31, 55, 56, 32), (55, 43, 44, 56),
               (32, 56, 57, 33), (56, 44, 45, 57), (33, 57, 58, 34), (57, 45, 46, 58), (34, 58, 59, 35),
               (58, 46, 47, 59), (35, 59, 60, 36), (59, 47, 48, 60), (36, 60, 49, 25), (60, 48, 37, 49),
               (38, 37, 61, 62), (39, 38, 62, 63), (40, 39, 63, 64), (41, 40, 64, 65), (42, 41, 65, 66),
               (43, 42, 66, 67), (44, 43, 67, 68), (45, 44, 68, 69), (46, 45, 69, 70), (47, 46, 70, 71),
               (48, 47, 71, 72), (37, 48, 72, 61), (62, 61, 74, 73), (63, 62, 73, 75), (64, 63, 75, 76),
               (65, 64, 76, 77), (66, 65, 77, 78), (67, 66, 78, 79), (68, 67, 79, 80), (69, 68, 80, 81),
               (70, 69, 81, 82), (71, 70, 82, 83), (72, 71, 83, 84), (61, 72, 84, 74), (86, 85, 97, 98),
               (87, 86, 98, 99), (88, 87, 99, 100), (89, 88, 100, 101), (90, 89, 101, 102), (91, 90, 102, 103),
               (92, 91, 103, 104), (93, 92, 104, 105), (94, 93, 105, 106), (95, 94, 106, 107), (96, 95, 107, 108),
               (97, 85, 96, 108), (98, 97, 109, 110), (99, 98, 110, 111), (100, 99, 111, 112), (101, 100, 112, 113),
               (102, 101, 113, 114), (108, 107, 119, 120), (108, 120, 109, 97), (119, 107, 127, 121),
               (118, 119, 121, 122),
               (117, 118, 122, 123), (116, 117, 123, 124), (115, 116, 124, 125), (114, 115, 125, 126),
               (102, 114, 126, 132),
               (107, 106, 128, 127), (106, 105, 129, 128), (105, 104, 130, 129), (104, 103, 131, 130),
               (103, 102, 132, 131),
               (121, 127, 134, 133), (122, 121, 133, 135), (123, 122, 135, 136), (124, 123, 136, 137),
               (125, 124, 137, 138),
               (126, 125, 138, 139), (132, 126, 139, 140), (127, 128, 141, 134), (128, 129, 142, 141),
               (129, 130, 143, 142),
               (130, 131, 144, 143), (131, 132, 140, 144), (138, 144, 140, 139), (137, 143, 144, 138),
               (136, 142, 143, 137),
               (135, 141, 142, 136), (133, 134, 141, 135), (110, 109, 145), (111, 110, 145), (112, 111, 145),
               (113, 112, 145), (114, 113, 145), (115, 114, 145), (116, 115, 145), (117, 116, 145),
               (118, 117, 145), (119, 118, 145), (120, 119, 145), (109, 120, 145)]

    return myvertex, myfaces


# ----------------------------------------------
# Handle model 02
# ----------------------------------------------
def handle_model_02():
    # ------------------------------------
    # Mesh data
    # ------------------------------------
    minx = -0.04349547624588013
    maxx = 0.04352114722132683
    miny = -0.08959200233221054
    maxy = 0
    minz = -0.04352371022105217
    maxz = 0.04349301755428314

    # Vertex
    myvertex = [(minx + 0.013302795588970184, maxy - 0.002780601382255554, minz + 0.010707870125770569),
                (minx + 0.0009496212005615234, maxy - 0.002942140679806471, minz + 0.030204588547348976),
                (minx, maxy - 0.003071820829063654, maxz - 0.033750676549971104),
                (minx + 0.010708402842283249, maxy - 0.0031348932534456253, maxz - 0.013303784653544426),
                (minx + 0.03020550962537527, maxy - 0.003114458406344056, maxz - 0.0009501762688159943),
                (maxx - 0.03374953381717205, maxy - 0.003015991533175111, maxz),
                (maxx - 0.01330280676484108, maxy - 0.0028658765368163586, maxz - 0.010707847774028778),
                (maxx - 0.0009496286511421204, maxy - 0.0027043374720960855, maxz - 0.030204561538994312),
                (maxx, maxy - 0.0025746573228389025, minz + 0.03375071194022894),
                (maxx - 0.010708380490541458, maxy - 0.002511584199965, minz + 0.013303810730576515),
                (maxx - 0.03020548354834318, maxy - 0.0025320190470665693, minz + 0.0009501948952674866),
                (minx + 0.03374955803155899, maxy - 0.0026304861530661583, minz),
                (minx + 0.014472760260105133, maxy - 0.019589224830269814, minz + 0.011804874986410141),
                (minx + 0.002567145973443985, maxy - 0.019744910299777985, minz + 0.030595174990594387),
                (minx + 0.001651916652917862, maxy - 0.019869891926646233, maxz - 0.034195657819509506),
                (minx + 0.011972300708293915, maxy - 0.019930677488446236, maxz - 0.014489583671092987),
                (minx + 0.03076297417283058, maxy - 0.019910985603928566, maxz - 0.0025835558772087097),
                (maxx - 0.034027633257210255, maxy - 0.019816085696220398, maxz - 0.0016677752137184143),
                (maxx - 0.014321718364953995, maxy - 0.01967141032218933, maxz - 0.011987630277872086),
                (maxx - 0.002416089177131653, maxy - 0.01951572299003601, maxz - 0.030777926556766033),
                (maxx - 0.0015008561313152313, maxy - 0.019390743225812912, minz + 0.03401290811598301),
                (maxx - 0.011821221560239792, maxy - 0.01932995393872261, minz + 0.014306826516985893),
                (maxx - 0.03061189129948616, maxy - 0.01934964768588543, minz + 0.002400781959295273),
                (minx + 0.03417872078716755, maxy - 0.019444547593593597, minz + 0.001484982669353485),
                (minx + 0.043508310547622386, maxy - 0.005668943747878075, maxz - 0.043508357635801076),
                (minx + 0.029034355655312538, maxy - 0.019612153992056847, minz + 0.027617475017905235),
                (minx + 0.023084014654159546, maxy - 0.01968996599316597, minz + 0.03700872650370002),
                (minx + 0.022626593708992004, maxy - 0.01975242979824543, maxz - 0.03889966616407037),
                (minx + 0.027784643694758415, maxy - 0.019782811403274536, maxz - 0.029050718992948532),
                (minx + 0.03717608004808426, maxy - 0.019772969186306, maxz - 0.023100173100829124),
                (maxx - 0.03873214777559042, maxy - 0.019725536927580833, maxz - 0.022642474621534348),
                (maxx - 0.02888327743858099, maxy - 0.019653232768177986, maxz - 0.02780025824904442),
                (maxx - 0.022932931780815125, maxy - 0.019575420767068863, maxz - 0.0371915097348392),
                (maxx - 0.022475499659776688, maxy - 0.019512956961989403, minz + 0.03871688432991505),
                (maxx - 0.0276335496455431, maxy - 0.019482573494315147, minz + 0.02886793203651905),
                (maxx - 0.03702498506754637, maxy - 0.019492419436573982, minz + 0.022917380556464195),
                (minx + 0.038883245550096035, maxy - 0.0195398461073637, minz + 0.022459672763943672),
                (minx + 0.029087782837450504, maxy - 0.03150090575218201, minz + 0.027552824467420578),
                (minx + 0.023137442767620087, maxy - 0.03157871589064598, minz + 0.036944076884537935),
                (minx + 0.022680018097162247, maxy - 0.03164118155837059, maxz - 0.03896431624889374),
                (minx + 0.027838071808218956, maxy - 0.031671565026044846, maxz - 0.029115368612110615),
                (minx + 0.0372295081615448, maxy - 0.03166172280907631, maxz - 0.023164819926023483),
                (maxx - 0.03867871919646859, maxy - 0.03161429241299629, maxz - 0.022707123309373856),
                (maxx - 0.028829848393797874, maxy - 0.03154198080301285, maxz - 0.027864910662174225),
                (maxx - 0.022879503667354584, maxy - 0.031464170664548874, maxz - 0.037256159354001284),
                (maxx - 0.022422071546316147, maxy - 0.03140170872211456, minz + 0.038652234710752964),
                (maxx - 0.02758011966943741, maxy - 0.03137132152915001, minz + 0.028803281486034393),
                (maxx - 0.03697155695408583, maxy - 0.031381167471408844, minz + 0.022852730005979538),
                (minx + 0.038936673663556576, maxy - 0.03142859786748886, minz + 0.022395022213459015),
                (minx + 0.029038896784186363, maxy - 0.020622700452804565, minz + 0.027611978352069855),
                (minx + 0.02308855764567852, maxy - 0.02070051059126854, minz + 0.0370032312348485),
                (minx + 0.02263113297522068, maxy - 0.020762978121638298, maxz - 0.038905161898583174),
                (minx + 0.02778918668627739, maxy - 0.020793357864022255, maxz - 0.029056214727461338),
                (minx + 0.037180622573941946, maxy - 0.02078351564705372, maxz - 0.023105667904019356),
                (maxx - 0.03872760524973273, maxy - 0.020736083388328552, maxz - 0.02264796942472458),
                (maxx - 0.028878736309707165, maxy - 0.020663777366280556, maxz - 0.0278057549148798),
                (maxx - 0.02292838878929615, maxy - 0.020585965365171432, maxz - 0.03719700500369072),
                (maxx - 0.022470960393548012, maxy - 0.020523501560091972, minz + 0.0387113899923861),
                (maxx - 0.027629008516669273, maxy - 0.020493119955062866, minz + 0.028862436302006245),
                (maxx - 0.03702044300734997, maxy - 0.020502964034676552, minz + 0.022911883890628815),
                (minx + 0.03888778714463115, maxy - 0.02055039070546627, minz + 0.02245417609810829),
                (minx + 0.03503026906400919, maxy - 0.0326739065349102, minz + 0.03399384953081608),
                (minx + 0.03150810860097408, maxy - 0.032719966024160385, minz + 0.03955277753993869),
                (minx + 0.03123734798282385, maxy - 0.03275693953037262, maxz - 0.04088863683864474),
                (minx + 0.034290531650185585, maxy - 0.032774921506643295, maxz - 0.035058788023889065),
                (minx + 0.039849569322541356, maxy - 0.0327690951526165, maxz - 0.03153650462627411),
                (maxx - 0.04059170465916395, maxy - 0.03274102136492729, maxz - 0.03126558102667332),
                (maxx - 0.03476190101355314, maxy - 0.032698217779397964, maxz - 0.03431860730051994),
                (maxx - 0.031239738687872887, maxy - 0.03265216201543808, maxz - 0.039877534145489335),
                (maxx - 0.03096897155046463, maxy - 0.032615188509225845, minz + 0.040563880698755383),
                (maxx - 0.03402215428650379, maxy - 0.03259720280766487, minz + 0.03473402839154005),
                (maxx - 0.03958118986338377, maxy - 0.032603029161691666, minz + 0.03121174033731222),
                (minx + 0.04086008481681347, maxy - 0.032631102949380875, minz + 0.030940811149775982),
                (minx + 0.026877090334892273, maxy - 0.04475956782698631, minz + 0.02504805289208889),
                (minx + 0.020004114136099815, miny + 0.044742558151483536, minz + 0.03589546587318182),
                (minx + 0.019475765526294708, miny + 0.044670410454273224, maxz - 0.03829052206128836),
                (minx + 0.025433603674173355, miny + 0.04463531821966171, maxz - 0.0269144456833601),
                (minx + 0.03628123179078102, miny + 0.04464668035507202, maxz - 0.020041238516569138),
                (maxx - 0.0379045819863677, miny + 0.0447014644742012, maxz - 0.01951257325708866),
                (maxx - 0.02652859501540661, miny + 0.044784992933273315, maxz - 0.02547009475529194),
                (maxx - 0.01965562254190445, maxy - 0.04471714794635773, maxz - 0.036317508202046156),
                (maxx - 0.019127257168293, maxy - 0.04464499279856682, minz + 0.03786848206073046),
                (maxx - 0.02508508786559105, maxy - 0.04460989683866501, minz + 0.026492400094866753),
                (maxx - 0.03593271458521485, maxy - 0.044621266424655914, minz + 0.019619181752204895),
                (minx + 0.03825310105457902, maxy - 0.044676050543785095, minz + 0.01909050904214382),
                (minx + 0.01721818558871746, miny + 0.00031135231256484985, minz + 0.01437518559396267),
                (minx + 0.006362196058034897, miny + 0.00016936659812927246, minz + 0.03150887507945299),
                (minx + 0.005527656525373459, miny + 5.542486906051636e-05, maxz - 0.03524145483970642),
                (minx + 0.014938175678253174, miny, maxz - 0.017272725701332092),
                (minx + 0.032072206027805805, miny + 1.7955899238586426e-05, maxz - 0.006416358053684235),
                (maxx - 0.03467791061848402, miny + 0.00010447949171066284, maxz - 0.0055813267827034),
                (maxx - 0.016709323972463608, miny + 0.00023641437292099, maxz - 0.01499134860932827),
                (maxx - 0.005853328853845596, miny + 0.00037835538387298584, maxz - 0.032125042751431465),
                (maxx - 0.0050187669694423676, miny + 0.0004923418164253235, minz + 0.03462529182434082),
                (maxx - 0.014429278671741486, miny + 0.0005477666854858398, minz + 0.016656557098031044),
                (maxx - 0.03156330715864897, miny + 0.0005298107862472534, minz + 0.005800176411867142),
                (minx + 0.03518681041896343, miny + 0.000443287193775177, minz + 0.0049651265144348145),
                (minx + 0.02942624967545271, miny + 0.0012636110186576843, minz + 0.027632080018520355),
                (minx + 0.023563016206026077, miny + 0.0011869296431541443, minz + 0.03688584640622139),
                (minx + 0.023112289607524872, miny + 0.0011253878474235535, maxz - 0.039185164496302605),
                (minx + 0.028194833546876907, miny + 0.0010954588651657104, maxz - 0.029480399563908577),
                (minx + 0.037448784336447716, miny + 0.0011051595211029053, maxz - 0.023616963997483253),
                (maxx - 0.038622063118964434, miny + 0.0011518821120262146, maxz - 0.023165971040725708),
                (maxx - 0.028917375952005386, miny + 0.001223146915435791, maxz - 0.02824824769049883),
                (maxx - 0.02305414155125618, miny + 0.0012998059391975403, maxz - 0.0375020164065063),
                (maxx - 0.02260340191423893, miny + 0.0013613700866699219, minz + 0.03856899822130799),
                (maxx - 0.027685942128300667, miny + 0.001391299068927765, minz + 0.028864230029284954),
                (maxx - 0.0369398919865489, miny + 0.001381605863571167, minz + 0.023000789806246758),
                (minx + 0.03913095686584711, miny + 0.0013348758220672607, minz + 0.022549785673618317),
                (minx + 0.03738117218017578, miny + 0.0037613436579704285, minz + 0.03627043403685093),
                (minx + 0.03477128129452467, miny + 0.0037272050976753235, minz + 0.04038954642601311),
                (minx + 0.034570650197565556, miny + 0.0036998093128204346, maxz - 0.041754934238269925),
                (minx + 0.03683303436264396, miny + 0.0036864876747131348, maxz - 0.03743506921455264),
                (minx + 0.040952228708192706, miny + 0.0036908015608787537, maxz - 0.03482509031891823),
                (maxx - 0.0411921211052686, miny + 0.003711603581905365, maxz - 0.03462434001266956),
                (maxx - 0.03687229100614786, miny + 0.0037433207035064697, maxz - 0.03688660357147455),
                (maxx - 0.034262401051819324, miny + 0.003777444362640381, maxz - 0.04100571759045124),
                (maxx - 0.03406176343560219, miny + 0.0038048475980758667, minz + 0.0411387647036463),
                (maxx - 0.036324144806712866, miny + 0.0038181766867637634, minz + 0.03681889921426773),
                (maxx - 0.04044333938509226, miny + 0.0038138628005981445, minz + 0.03420891519635916),
                (minx + 0.04170101135969162, miny + 0.003793060779571533, minz + 0.034008161164820194),
                (maxx - 0.043253868410829455, miny + 0.00480072945356369, minz + 0.04320027763606049),
                (minx + 0.03971285093575716, maxy - 0.041327137500047684, maxz - 0.031046375632286072),
                (maxx - 0.03359287604689598, maxy - 0.04114784672856331, minz + 0.03433086443692446),
                (minx + 0.03072980046272278, maxy - 0.04131445661187172, maxz - 0.040801193099468946),
                (minx + 0.031012218445539474, maxy - 0.04127589240670204, minz + 0.03935709968209267),
                (minx + 0.04076687735505402, maxy - 0.04118320718407631, minz + 0.030374319292604923),
                (minx + 0.034451283514499664, maxy - 0.03338594362139702, minz + 0.033365121111273766),
                (minx + 0.030692334286868572, maxy - 0.03343509882688522, minz + 0.039297766517847776),
                (minx + 0.03040337096899748, maxy - 0.03347455710172653, maxz - 0.040701600490137935),
                (minx + 0.03366181440651417, maxy - 0.03349374979734421, maxz - 0.03447982110083103),
                (minx + 0.03959457715973258, maxy - 0.033487528562545776, maxz - 0.03072074055671692),
                (maxx - 0.040404647355899215, maxy - 0.033457569777965546, maxz - 0.030431604012846947),
                (maxx - 0.03418291546404362, maxy - 0.03341188654303551, maxz - 0.03368987888097763),
                (maxx - 0.030423964373767376, maxy - 0.0333627350628376, maxz - 0.03962252289056778),
                (maxx - 0.030134993605315685, maxy - 0.03332327678799629, minz + 0.04037684458307922),
                (maxx - 0.033393437042832375, maxy - 0.03330408036708832, minz + 0.03415506146848202),
                (maxx - 0.03932619746774435, maxy - 0.03331030160188675, minz + 0.030395975336432457),
                (minx + 0.040673027746379375, maxy - 0.03334026038646698, minz + 0.030106833204627037),
                (minx + 0.030282274819910526, maxy - 0.005427400581538677, maxz - 0.0011750981211662292),
                (minx + 0.013463903218507767, maxy - 0.005095209460705519, minz + 0.0108589306473732),
                (minx + 0.010882444679737091, maxy - 0.005447734147310257, maxz - 0.013467073440551758),
                (minx + 0.0011723600327968597, maxy - 0.005255943164229393, minz + 0.030258373357355595),
                (minx + 0.0002274736762046814, maxy - 0.005384976044297218, maxz - 0.033811951987445354),
                (maxx - 0.0134431142359972, maxy - 0.005180059932172298, maxz - 0.010884080082178116),
                (maxx - 0.033787828870117664, maxy - 0.005329424981027842, maxz - 0.00022966042160987854),
                (maxx - 0.0302614476531744, maxy - 0.004847868345677853, minz + 0.0011499449610710144),
                (maxx - 0.00020667165517807007, maxy - 0.004890293348580599, minz + 0.03378681745380163),
                (maxx - 0.0011515654623508453, maxy - 0.0050193266943097115, maxz - 0.03028351627290249),
                (minx + 0.033808655105531216, maxy - 0.004945843946188688, minz + 0.0002044886350631714),
                (maxx - 0.010861624032258987, maxy - 0.004827534779906273, minz + 0.013441929593682289),
                (minx + 0.03468604106456041, maxy - 0.04122784733772278, minz + 0.033558815717697144),
                (minx + 0.033914451487362385, maxy - 0.041333213448524475, maxz - 0.03472032118588686),
                (maxx - 0.04044530005194247, maxy - 0.04129785671830177, maxz - 0.03076378908008337),
                (maxx - 0.034364476799964905, maxy - 0.04125320911407471, maxz - 0.03394827153533697),
                (maxx - 0.03069065511226654, maxy - 0.04120517522096634, maxz - 0.03974655526690185),
                (maxx - 0.030408228747546673, maxy - 0.04116660729050636, minz + 0.04041173937730491),
                (maxx - 0.03939127502962947, maxy - 0.0411539226770401, minz + 0.030656912364065647),
                (minx + 0.03147818427532911, maxy - 0.033236272633075714, minz + 0.03954096930101514),
                (minx + 0.031206720508635044, maxy - 0.03327333927154541, maxz - 0.04088335996493697),
                (minx + 0.034267837181687355, maxy - 0.033291369676589966, maxz - 0.03503836318850517),
                (minx + 0.03984131896868348, maxy - 0.03328552842140198, maxz - 0.03150692768394947),
                (maxx - 0.040582869900390506, maxy - 0.0332573801279068, maxz - 0.03123530000448227),
                (maxx - 0.03473791852593422, maxy - 0.033214468508958817, maxz - 0.03429625928401947),
                (maxx - 0.031206604093313217, maxy - 0.03316829353570938, maxz - 0.03986963024362922),
                (maxx - 0.030935133807361126, maxy - 0.03313122317194939, minz + 0.040554699720814824),
                (maxx - 0.03399624954909086, maxy - 0.03311318904161453, minz + 0.03470969945192337),
                (maxx - 0.03956972947344184, maxy - 0.03311903029680252, minz + 0.031178259290754795),
                (minx + 0.04085446032695472, maxy - 0.0331471785902977, minz + 0.030906626023352146),
                (minx + 0.035009496845304966, maxy - 0.03319009393453598, minz + 0.03396759741008282)]

    # Faces
    myfaces = [(24, 0, 1), (24, 1, 2), (24, 2, 3), (24, 3, 4), (24, 4, 5),
               (24, 5, 6), (24, 6, 7), (24, 7, 8), (24, 8, 9), (24, 9, 10),
               (24, 10, 11), (11, 0, 24), (140, 12, 13, 142), (142, 13, 14, 143), (143, 14, 15, 141),
               (141, 15, 16, 139), (139, 16, 17, 145), (145, 17, 18, 144), (144, 18, 19, 148), (148, 19, 20, 147),
               (147, 20, 21, 150), (150, 21, 22, 146), (146, 22, 23, 149), (140, 0, 11, 149), (13, 12, 25, 26),
               (14, 13, 26, 27), (15, 14, 27, 28), (16, 15, 28, 29), (17, 16, 29, 30), (18, 17, 30, 31),
               (19, 18, 31, 32), (20, 19, 32, 33), (21, 20, 33, 34), (22, 21, 34, 35), (23, 22, 35, 36),
               (12, 23, 36, 25), (25, 49, 50, 26), (49, 37, 38, 50), (26, 50, 51, 27), (50, 38, 39, 51),
               (27, 51, 52, 28), (51, 39, 40, 52), (28, 52, 53, 29), (52, 40, 41, 53), (29, 53, 54, 30),
               (53, 41, 42, 54), (30, 54, 55, 31), (54, 42, 43, 55), (31, 55, 56, 32), (55, 43, 44, 56),
               (32, 56, 57, 33), (56, 44, 45, 57), (33, 57, 58, 34), (57, 45, 46, 58), (34, 58, 59, 35),
               (58, 46, 47, 59), (35, 59, 60, 36), (59, 47, 48, 60), (36, 60, 49, 25), (60, 48, 37, 49),
               (38, 37, 61, 62), (39, 38, 62, 63), (40, 39, 63, 64), (41, 40, 64, 65), (42, 41, 65, 66),
               (43, 42, 66, 67), (44, 43, 67, 68), (45, 44, 68, 69), (46, 45, 69, 70), (47, 46, 70, 71),
               (48, 47, 71, 72), (37, 48, 72, 61), (124, 125, 74, 75), (74, 73, 85, 86), (79, 78, 90, 91),
               (80, 79, 91, 92), (77, 76, 88, 89), (82, 81, 93, 94), (76, 75, 87, 88), (81, 80, 92, 93),
               (73, 84, 96, 85), (84, 83, 95, 96), (83, 82, 94, 95), (78, 77, 89, 90), (75, 74, 86, 87),
               (90, 89, 101, 102), (86, 85, 97, 98), (93, 92, 104, 105), (96, 95, 107, 108), (85, 96, 108, 97),
               (89, 88, 100, 101), (91, 90, 102, 103), (88, 87, 99, 100), (92, 91, 103, 104), (95, 94, 106, 107),
               (94, 93, 105, 106), (87, 86, 98, 99), (105, 104, 116, 117), (108, 107, 119, 120), (97, 108, 120, 109),
               (101, 100, 112, 113), (103, 102, 114, 115), (100, 99, 111, 112), (104, 103, 115, 116),
               (107, 106, 118, 119),
               (106, 105, 117, 118), (99, 98, 110, 111), (102, 101, 113, 114), (98, 97, 109, 110), (120, 119, 121),
               (109, 120, 121), (113, 112, 121), (115, 114, 121), (112, 111, 121), (116, 115, 121),
               (119, 118, 121), (118, 117, 121), (111, 110, 121), (114, 113, 121), (110, 109, 121),
               (117, 116, 121), (169, 158, 62, 61), (158, 159, 63, 62), (159, 160, 64, 63), (160, 161, 65, 64),
               (161, 162, 66, 65), (162, 163, 67, 66), (163, 164, 68, 67), (164, 165, 69, 68), (165, 166, 70, 69),
               (166, 167, 71, 70), (167, 168, 72, 71), (168, 169, 61, 72), (72, 138, 127, 61), (63, 129, 130, 64),
               (67, 133, 134, 68), (64, 130, 131, 65), (61, 127, 128, 62), (69, 135, 136, 70), (66, 132, 133, 67),
               (65, 131, 132, 66), (71, 137, 138, 72), (70, 136, 137, 71), (62, 128, 129, 63), (68, 134, 135, 69),
               (0, 140, 142, 1), (1, 142, 143, 2), (2, 143, 141, 3), (3, 141, 139, 4), (4, 139, 145, 5),
               (5, 145, 144, 6), (6, 144, 148, 7), (7, 148, 147, 8), (8, 147, 150, 9), (9, 150, 146, 10),
               (10, 146, 149, 11), (12, 140, 149, 23), (153, 154, 163, 162), (154, 155, 164, 163), (155, 156, 165, 164),
               (125, 151, 73, 74), (152, 124, 75, 76), (122, 152, 76, 77), (153, 122, 77, 78), (154, 153, 78, 79),
               (155, 154, 79, 80), (156, 155, 80, 81), (123, 156, 81, 82), (157, 123, 82, 83), (126, 157, 83, 84),
               (73, 151, 126, 84), (151, 125, 158, 169), (125, 124, 159, 158), (124, 152, 160, 159),
               (152, 122, 161, 160),
               (122, 153, 162, 161), (156, 123, 166, 165), (123, 157, 167, 166), (157, 126, 168, 167),
               (126, 151, 169, 168)]

    return myvertex, myfaces


# ----------------------------------------------
# Handle model 03
# ----------------------------------------------
def handle_model_03():
    # ------------------------------------
    # Mesh data
    # ------------------------------------
    minx = -0.04349547624588013
    maxx = 0.04352114722132683
    miny = -0.09871400892734528
    maxy = 0
    minz = -0.04352371022105217
    maxz = 0.04349301755428314

    # Vertex
    myvertex = [(minx + 0.013302795588970184, maxy - 0.002780601382255554, minz + 0.010707870125770569),
                (minx + 0.0009496212005615234, maxy - 0.002942140679806471, minz + 0.030204588547348976),
                (minx, maxy - 0.003071820829063654, maxz - 0.033750676549971104),
                (minx + 0.010708402842283249, maxy - 0.0031348932534456253, maxz - 0.013303784653544426),
                (minx + 0.03020550962537527, maxy - 0.003114458406344056, maxz - 0.0009501762688159943),
                (maxx - 0.03374953381717205, maxy - 0.003015991533175111, maxz),
                (maxx - 0.01330280676484108, maxy - 0.0028658765368163586, maxz - 0.010707847774028778),
                (maxx - 0.0009496286511421204, maxy - 0.0027043374720960855, maxz - 0.030204561538994312),
                (maxx, maxy - 0.0025746573228389025, minz + 0.03375071194022894),
                (maxx - 0.010708380490541458, maxy - 0.002511584199965, minz + 0.013303810730576515),
                (maxx - 0.03020548354834318, maxy - 0.0025320190470665693, minz + 0.0009501948952674866),
                (minx + 0.03374955803155899, maxy - 0.0026304861530661583, minz),
                (minx + 0.014472760260105133, maxy - 0.019589224830269814, minz + 0.011804874986410141),
                (minx + 0.002567145973443985, maxy - 0.019744910299777985, minz + 0.030595174990594387),
                (minx + 0.001651916652917862, maxy - 0.019869891926646233, maxz - 0.034195657819509506),
                (minx + 0.011972300708293915, maxy - 0.019930677488446236, maxz - 0.014489583671092987),
                (minx + 0.03076297417283058, maxy - 0.019910985603928566, maxz - 0.0025835558772087097),
                (maxx - 0.034027633257210255, maxy - 0.019816085696220398, maxz - 0.0016677752137184143),
                (maxx - 0.014321718364953995, maxy - 0.01967141032218933, maxz - 0.011987630277872086),
                (maxx - 0.002416089177131653, maxy - 0.01951572299003601, maxz - 0.030777926556766033),
                (maxx - 0.0015008561313152313, maxy - 0.019390743225812912, minz + 0.03401290811598301),
                (maxx - 0.011821221560239792, maxy - 0.01932995393872261, minz + 0.014306826516985893),
                (maxx - 0.03061189129948616, maxy - 0.01934964768588543, minz + 0.002400781959295273),
                (minx + 0.03417872078716755, maxy - 0.019444547593593597, minz + 0.001484982669353485),
                (minx + 0.043508310547622386, maxy - 0.005668943747878075, maxz - 0.043508357635801076),
                (minx + 0.029034355655312538, maxy - 0.019612153992056847, minz + 0.027617475017905235),
                (minx + 0.023084014654159546, maxy - 0.01968996599316597, minz + 0.03700872650370002),
                (minx + 0.022626593708992004, maxy - 0.01975242979824543, maxz - 0.03889966616407037),
                (minx + 0.027784643694758415, maxy - 0.019782811403274536, maxz - 0.029050718992948532),
                (minx + 0.03717608004808426, maxy - 0.019772969186306, maxz - 0.023100173100829124),
                (maxx - 0.03873214777559042, maxy - 0.019725536927580833, maxz - 0.022642474621534348),
                (maxx - 0.02888327743858099, maxy - 0.019653232768177986, maxz - 0.02780025824904442),
                (maxx - 0.022932931780815125, maxy - 0.019575420767068863, maxz - 0.0371915097348392),
                (maxx - 0.022475499659776688, maxy - 0.019512956961989403, minz + 0.03871688432991505),
                (maxx - 0.0276335496455431, maxy - 0.019482573494315147, minz + 0.02886793203651905),
                (maxx - 0.03702498506754637, maxy - 0.019492419436573982, minz + 0.022917380556464195),
                (minx + 0.038883245550096035, maxy - 0.0195398461073637, minz + 0.022459672763943672),
                (minx + 0.029087782837450504, maxy - 0.03150090575218201, minz + 0.027552824467420578),
                (minx + 0.023137442767620087, maxy - 0.03157871589064598, minz + 0.036944076884537935),
                (minx + 0.022680018097162247, maxy - 0.03164118155837059, maxz - 0.03896431624889374),
                (minx + 0.027838071808218956, maxy - 0.031671565026044846, maxz - 0.029115368612110615),
                (minx + 0.0372295081615448, maxy - 0.03166172280907631, maxz - 0.023164819926023483),
                (maxx - 0.03867871919646859, maxy - 0.03161429241299629, maxz - 0.022707123309373856),
                (maxx - 0.028829848393797874, maxy - 0.03154198080301285, maxz - 0.027864910662174225),
                (maxx - 0.022879503667354584, maxy - 0.031464170664548874, maxz - 0.037256159354001284),
                (maxx - 0.022422071546316147, maxy - 0.03140170872211456, minz + 0.038652234710752964),
                (maxx - 0.02758011966943741, maxy - 0.03137132152915001, minz + 0.028803281486034393),
                (maxx - 0.03697155695408583, maxy - 0.031381167471408844, minz + 0.022852730005979538),
                (minx + 0.038936673663556576, maxy - 0.03142859786748886, minz + 0.022395022213459015),
                (minx + 0.029038896784186363, maxy - 0.020622700452804565, minz + 0.027611978352069855),
                (minx + 0.02308855764567852, maxy - 0.02070051059126854, minz + 0.0370032312348485),
                (minx + 0.02263113297522068, maxy - 0.020762978121638298, maxz - 0.038905161898583174),
                (minx + 0.02778918668627739, maxy - 0.020793357864022255, maxz - 0.029056214727461338),
                (minx + 0.037180622573941946, maxy - 0.02078351564705372, maxz - 0.023105667904019356),
                (maxx - 0.03872760524973273, maxy - 0.020736083388328552, maxz - 0.02264796942472458),
                (maxx - 0.028878736309707165, maxy - 0.020663777366280556, maxz - 0.0278057549148798),
                (maxx - 0.02292838878929615, maxy - 0.020585965365171432, maxz - 0.03719700500369072),
                (maxx - 0.022470960393548012, maxy - 0.020523501560091972, minz + 0.0387113899923861),
                (maxx - 0.027629008516669273, maxy - 0.020493119955062866, minz + 0.028862436302006245),
                (maxx - 0.03702044300734997, maxy - 0.020502964034676552, minz + 0.022911883890628815),
                (minx + 0.03888778714463115, maxy - 0.02055039070546627, minz + 0.02245417609810829),
                (minx + 0.03503026906400919, maxy - 0.0326739065349102, minz + 0.03399384953081608),
                (minx + 0.03150810860097408, maxy - 0.032719966024160385, minz + 0.03955277753993869),
                (minx + 0.03123734798282385, maxy - 0.03275693953037262, maxz - 0.04088863683864474),
                (minx + 0.034290531650185585, maxy - 0.032774921506643295, maxz - 0.035058788023889065),
                (minx + 0.039849569322541356, maxy - 0.0327690951526165, maxz - 0.03153650462627411),
                (maxx - 0.04059170465916395, maxy - 0.03274102136492729, maxz - 0.03126558102667332),
                (maxx - 0.03476190101355314, maxy - 0.032698217779397964, maxz - 0.03431860730051994),
                (maxx - 0.031239738687872887, maxy - 0.03265216201543808, maxz - 0.039877534145489335),
                (maxx - 0.03096897155046463, maxy - 0.032615188509225845, minz + 0.040563880698755383),
                (maxx - 0.03402215428650379, maxy - 0.03259720280766487, minz + 0.03473402839154005),
                (maxx - 0.03958118986338377, maxy - 0.032603029161691666, minz + 0.03121174033731222),
                (minx + 0.04086008481681347, maxy - 0.032631102949380875, minz + 0.030940811149775982),
                (minx + 0.026877090334892273, maxy - 0.04475956782698631, minz + 0.02504805289208889),
                (minx + 0.020004114136099815, maxy - 0.044849444180727005, minz + 0.03589546587318182),
                (minx + 0.019475765526294708, maxy - 0.04492159187793732, maxz - 0.03829052206128836),
                (minx + 0.025433603674173355, maxy - 0.04495668411254883, maxz - 0.0269144456833601),
                (minx + 0.03628123179078102, maxy - 0.04494532197713852, maxz - 0.020041238516569138),
                (maxx - 0.0379045819863677, maxy - 0.04489053785800934, maxz - 0.01951257325708866),
                (maxx - 0.02652859501540661, maxy - 0.044807009398937225, maxz - 0.02547009475529194),
                (maxx - 0.01965562254190445, maxy - 0.04471714794635773, maxz - 0.036317508202046156),
                (maxx - 0.019127257168293, maxy - 0.04464499279856682, minz + 0.03786848206073046),
                (maxx - 0.02508508786559105, maxy - 0.04460989683866501, minz + 0.026492400094866753),
                (maxx - 0.03593271458521485, maxy - 0.044621266424655914, minz + 0.019619181752204895),
                (minx + 0.03825310105457902, maxy - 0.044676050543785095, minz + 0.01909050904214382),
                (minx + 0.021551070734858513, miny + 0.00942724198102951, minz + 0.01908031851053238),
                (minx + 0.01246710866689682, miny + 0.009308435022830963, minz + 0.03341726865619421),
                (minx + 0.011768791824579239, miny + 0.009213089942932129, maxz - 0.03664115583524108),
                (minx + 0.019643226638436317, miny + 0.009166710078716278, maxz - 0.0216054730117321),
                (minx + 0.033980460837483406, miny + 0.009181737899780273, maxz - 0.012521196156740189),
                (maxx - 0.036077769473195076, miny + 0.009254135191440582, maxz - 0.011822465807199478),
                (maxx - 0.021042203530669212, miny + 0.0093645378947258, maxz - 0.019696485251188278),
                (maxx - 0.011958237737417221, miny + 0.009483307600021362, maxz - 0.03403343725949526),
                (maxx - 0.011259902268648148, miny + 0.009578689932823181, minz + 0.03602499142289162),
                (maxx - 0.01913433149456978, miny + 0.009625062346458435, minz + 0.020989302545785904),
                (maxx - 0.03347156383097172, miny + 0.009610041975975037, minz + 0.011905014514923096),
                (minx + 0.03658666601404548, miny + 0.00953763723373413, minz + 0.011206269264221191),
                (minx + 0.02942624967545271, miny + 0.001430809497833252, minz + 0.027632080018520355),
                (minx + 0.023563016206026077, miny + 0.001354128122329712, minz + 0.03688584640622139),
                (minx + 0.023112289607524872, miny + 0.001292586326599121, maxz - 0.039185164496302605),
                (minx + 0.028194833546876907, miny + 0.001262657344341278, maxz - 0.029480399563908577),
                (minx + 0.037448784336447716, miny + 0.001272358000278473, maxz - 0.023616963997483253),
                (maxx - 0.038622063118964434, miny + 0.0013190805912017822, maxz - 0.023165971040725708),
                (maxx - 0.028917375952005386, miny + 0.0013903453946113586, maxz - 0.02824824769049883),
                (maxx - 0.02305414155125618, miny + 0.001467004418373108, maxz - 0.0375020164065063),
                (maxx - 0.02260340191423893, miny + 0.0015285685658454895, minz + 0.03856899822130799),
                (maxx - 0.027685942128300667, miny + 0.0015584975481033325, minz + 0.028864230029284954),
                (maxx - 0.0369398919865489, miny + 0.0015488043427467346, minz + 0.023000789806246758),
                (minx + 0.03913095686584711, miny + 0.0015020743012428284, minz + 0.022549785673618317),
                (minx + 0.03738117218017578, miny + 0.001003175973892212, minz + 0.03627043403685093),
                (minx + 0.03477128129452467, miny + 0.0009690374135971069, minz + 0.04038954642601311),
                (minx + 0.034570650197565556, miny + 0.000941641628742218, maxz - 0.041754934238269925),
                (minx + 0.03683303436264396, miny + 0.0009283199906349182, maxz - 0.03743506921455264),
                (minx + 0.040952228708192706, miny + 0.0009326338768005371, maxz - 0.03482509031891823),
                (maxx - 0.0411921211052686, miny + 0.0009534358978271484, maxz - 0.03462434001266956),
                (maxx - 0.03687229100614786, miny + 0.0009851530194282532, maxz - 0.03688660357147455),
                (maxx - 0.034262401051819324, miny + 0.0010192766785621643, maxz - 0.04100571759045124),
                (maxx - 0.03406176343560219, miny + 0.0010466799139976501, minz + 0.0411387647036463),
                (maxx - 0.036324144806712866, miny + 0.0010600090026855469, minz + 0.03681889921426773),
                (maxx - 0.04044333938509226, miny + 0.001055695116519928, minz + 0.03420891519635916),
                (minx + 0.04170101135969162, miny + 0.0010348930954933167, minz + 0.034008161164820194),
                (maxx - 0.043253868410829455, miny, minz + 0.04320027763606049),
                (minx + 0.03971285093575716, maxy - 0.041327137500047684, maxz - 0.031046375632286072),
                (maxx - 0.03359287604689598, maxy - 0.04114784672856331, minz + 0.03433086443692446),
                (minx + 0.03072980046272278, maxy - 0.04131445661187172, maxz - 0.040801193099468946),
                (minx + 0.031012218445539474, maxy - 0.04127589240670204, minz + 0.03935709968209267),
                (minx + 0.04076687735505402, maxy - 0.04118320718407631, minz + 0.030374319292604923),
                (minx + 0.034451283514499664, maxy - 0.03338594362139702, minz + 0.033365121111273766),
                (minx + 0.030692334286868572, maxy - 0.03343509882688522, minz + 0.039297766517847776),
                (minx + 0.03040337096899748, maxy - 0.03347455710172653, maxz - 0.040701600490137935),
                (minx + 0.03366181440651417, maxy - 0.03349374979734421, maxz - 0.03447982110083103),
                (minx + 0.03959457715973258, maxy - 0.033487528562545776, maxz - 0.03072074055671692),
                (maxx - 0.040404647355899215, maxy - 0.033457569777965546, maxz - 0.030431604012846947),
                (maxx - 0.03418291546404362, maxy - 0.03341188654303551, maxz - 0.03368987888097763),
                (maxx - 0.030423964373767376, maxy - 0.0333627350628376, maxz - 0.03962252289056778),
                (maxx - 0.030134993605315685, maxy - 0.03332327678799629, minz + 0.04037684458307922),
                (maxx - 0.033393437042832375, maxy - 0.03330408036708832, minz + 0.03415506146848202),
                (maxx - 0.03932619746774435, maxy - 0.03331030160188675, minz + 0.030395975336432457),
                (minx + 0.040673027746379375, maxy - 0.03334026038646698, minz + 0.030106833204627037),
                (minx + 0.030282274819910526, maxy - 0.005427400581538677, maxz - 0.0011750981211662292),
                (minx + 0.013463903218507767, maxy - 0.005095209460705519, minz + 0.0108589306473732),
                (minx + 0.010882444679737091, maxy - 0.005447734147310257, maxz - 0.013467073440551758),
                (minx + 0.0011723600327968597, maxy - 0.005255943164229393, minz + 0.030258373357355595),
                (minx + 0.0002274736762046814, maxy - 0.005384976044297218, maxz - 0.033811951987445354),
                (maxx - 0.0134431142359972, maxy - 0.005180059932172298, maxz - 0.010884080082178116),
                (maxx - 0.033787828870117664, maxy - 0.005329424981027842, maxz - 0.00022966042160987854),
                (maxx - 0.0302614476531744, maxy - 0.004847868345677853, minz + 0.0011499449610710144),
                (maxx - 0.00020667165517807007, maxy - 0.004890293348580599, minz + 0.03378681745380163),
                (maxx - 0.0011515654623508453, maxy - 0.0050193266943097115, maxz - 0.03028351627290249),
                (minx + 0.033808655105531216, maxy - 0.004945843946188688, minz + 0.0002044886350631714),
                (maxx - 0.010861624032258987, maxy - 0.004827534779906273, minz + 0.013441929593682289),
                (minx + 0.03468604106456041, maxy - 0.04122784733772278, minz + 0.033558815717697144),
                (minx + 0.033914451487362385, maxy - 0.041333213448524475, maxz - 0.03472032118588686),
                (maxx - 0.04044530005194247, maxy - 0.04129785671830177, maxz - 0.03076378908008337),
                (maxx - 0.034364476799964905, maxy - 0.04125320911407471, maxz - 0.03394827153533697),
                (maxx - 0.03069065511226654, maxy - 0.04120517522096634, maxz - 0.03974655526690185),
                (maxx - 0.030408228747546673, maxy - 0.04116660729050636, minz + 0.04041173937730491),
                (maxx - 0.03939127502962947, maxy - 0.0411539226770401, minz + 0.030656912364065647),
                (minx + 0.03147818427532911, maxy - 0.033236272633075714, minz + 0.03954096930101514),
                (minx + 0.031206720508635044, maxy - 0.03327333927154541, maxz - 0.04088335996493697),
                (minx + 0.034267837181687355, maxy - 0.033291369676589966, maxz - 0.03503836318850517),
                (minx + 0.03984131896868348, maxy - 0.03328552842140198, maxz - 0.03150692768394947),
                (maxx - 0.040582869900390506, maxy - 0.0332573801279068, maxz - 0.03123530000448227),
                (maxx - 0.03473791852593422, maxy - 0.033214468508958817, maxz - 0.03429625928401947),
                (maxx - 0.031206604093313217, maxy - 0.03316829353570938, maxz - 0.03986963024362922),
                (maxx - 0.030935133807361126, maxy - 0.03313122317194939, minz + 0.040554699720814824),
                (maxx - 0.03399624954909086, maxy - 0.03311318904161453, minz + 0.03470969945192337),
                (maxx - 0.03956972947344184, maxy - 0.03311903029680252, minz + 0.031178259290754795),
                (minx + 0.04085446032695472, maxy - 0.0331471785902977, minz + 0.030906626023352146),
                (minx + 0.035009496845304966, maxy - 0.03319009393453598, minz + 0.03396759741008282),
                (minx + 0.019410474225878716, miny + 0.020503833889961243, minz + 0.016801605001091957),
                (minx + 0.009459223598241806, miny + 0.020373672246932983, minz + 0.032507372088730335),
                (maxx - 0.03541257046163082, miny + 0.02031419426202774, maxz - 0.008743710815906525),
                (maxx - 0.0189414881169796, miny + 0.02043512463569641, maxz - 0.017369499430060387),
                (maxx - 0.008990231901407242, miny + 0.02056524157524109, maxz - 0.03307527117431164),
                (minx + 0.017320478335022926, miny + 0.02021842449903488, maxz - 0.01946074701845646),
                (minx + 0.03302655927836895, miny + 0.02023487538099289, maxz - 0.009509153664112091),
                (maxx - 0.008225221186876297, miny + 0.02066972106695175, minz + 0.0353640653192997),
                (maxx - 0.016851460561156273, miny + 0.020720526576042175, minz + 0.018892847001552582),
                (minx + 0.008694231510162354, miny + 0.020269230008125305, maxz - 0.03593196161091328),
                (minx + 0.035881591495126486, miny + 0.020624756813049316, minz + 0.008175786584615707),
                (maxx - 0.032557537779212, miny + 0.020704075694084167, minz + 0.008941244333982468),
                (minx + 0.008214566856622696, miny + 0.023270338773727417, minz + 0.03213237784802914),
                (maxx - 0.018073920160531998, miny + 0.023333996534347534, maxz - 0.016406163573265076),
                (maxx - 0.007764074951410294, miny + 0.023468807339668274, maxz - 0.03267789073288441),
                (minx + 0.03263115230947733, miny + 0.023126527667045593, maxz - 0.008262567222118378),
                (maxx - 0.015908580273389816, miny + 0.023629695177078247, minz + 0.018027253448963165),
                (minx + 0.01852441392838955, miny + 0.023405179381370544, minz + 0.015860654413700104),
                (maxx - 0.03513853810727596, miny + 0.023208707571029663, maxz - 0.007469546049833298),
                (minx + 0.016359103843569756, miny + 0.02310948073863983, maxz - 0.018572768196463585),
                (maxx - 0.006971497088670731, miny + 0.023577049374580383, minz + 0.0350920120254159),
                (minx + 0.007422015070915222, miny + 0.023162126541137695, maxz - 0.03563752118498087),
                (minx + 0.035589066334068775, miny + 0.023530468344688416, minz + 0.00692400336265564),
                (maxx - 0.032180625945329666, miny + 0.023612648248672485, minz + 0.0077170394361019135),
                (minx + 0.021761823445558548, miny + 0.020728543400764465, minz + 0.019355909898877144),
                (minx + 0.012772375717759132, miny + 0.020610973238945007, minz + 0.03354368917644024),
                (maxx - 0.03617278253659606, miny + 0.020557239651679993, maxz - 0.012130718678236008),
                (maxx - 0.021293656900525093, miny + 0.020666487514972687, maxz - 0.019922811537981033),
                (maxx - 0.012304211035370827, miny + 0.02078402042388916, maxz - 0.03411059454083443),
                (minx + 0.019873831421136856, miny + 0.020470723509788513, maxz - 0.021811936050653458),
                (minx + 0.034061891958117485, miny + 0.020485587418079376, maxz - 0.01282217912375927),
                (maxx - 0.011613138020038605, miny + 0.020878411829471588, minz + 0.0361242787912488),
                (maxx - 0.019405635073781013, miny + 0.02092430740594864, minz + 0.02124503068625927),
                (minx + 0.012081325054168701, miny + 0.020516619086265564, maxz - 0.03669118043035269),
                (minx + 0.03664098121225834, miny + 0.02083779126405716, minz + 0.01156378909945488),
                (maxx - 0.03359369467943907, miny + 0.020909443497657776, minz + 0.012255258858203888),
                (minx + 0.01420576497912407, miny + 0.023059040307998657, minz + 0.03400459885597229),
                (maxx - 0.022325390949845314, miny + 0.023111969232559204, maxz - 0.021023839712142944),
                (maxx - 0.013754449784755707, miny + 0.02322402596473694, maxz - 0.034551107324659824),
                (minx + 0.034504144452512264, miny + 0.022939488291740417, maxz - 0.014253776520490646),
                (maxx - 0.020525267347693443, miny + 0.023357778787612915, minz + 0.02227850630879402),
                (minx + 0.022776709869503975, miny + 0.023171141743659973, minz + 0.020477334037423134),
                (maxx - 0.036511816550046206, miny + 0.023007795214653015, maxz - 0.013594510033726692),
                (minx + 0.020976610481739044, miny + 0.02292531728744507, maxz - 0.02282501384615898),
                (maxx - 0.013095550239086151, miny + 0.02331402897834778, minz + 0.03646504878997803),
                (minx + 0.013546885922551155, miny + 0.0229690819978714, maxz - 0.037011553067713976),
                (minx + 0.03696316387504339, miny + 0.023275285959243774, minz + 0.013047976419329643),
                (maxx - 0.03405279852449894, miny + 0.023343607783317566, minz + 0.013707255944609642)]

    # Faces
    myfaces = [(24, 0, 1), (24, 1, 2), (24, 2, 3), (24, 3, 4), (24, 4, 5),
               (24, 5, 6), (24, 6, 7), (24, 7, 8), (24, 8, 9), (24, 9, 10),
               (24, 10, 11), (11, 0, 24), (140, 12, 13, 142), (142, 13, 14, 143), (143, 14, 15, 141),
               (141, 15, 16, 139), (139, 16, 17, 145), (145, 17, 18, 144), (144, 18, 19, 148), (148, 19, 20, 147),
               (147, 20, 21, 150), (150, 21, 22, 146), (146, 22, 23, 149), (140, 0, 11, 149), (13, 12, 25, 26),
               (14, 13, 26, 27), (15, 14, 27, 28), (16, 15, 28, 29), (17, 16, 29, 30), (18, 17, 30, 31),
               (19, 18, 31, 32), (20, 19, 32, 33), (21, 20, 33, 34), (22, 21, 34, 35), (23, 22, 35, 36),
               (12, 23, 36, 25), (25, 49, 50, 26), (49, 37, 38, 50), (26, 50, 51, 27), (50, 38, 39, 51),
               (27, 51, 52, 28), (51, 39, 40, 52), (28, 52, 53, 29), (52, 40, 41, 53), (29, 53, 54, 30),
               (53, 41, 42, 54), (30, 54, 55, 31), (54, 42, 43, 55), (31, 55, 56, 32), (55, 43, 44, 56),
               (32, 56, 57, 33), (56, 44, 45, 57), (33, 57, 58, 34), (57, 45, 46, 58), (34, 58, 59, 35),
               (58, 46, 47, 59), (35, 59, 60, 36), (59, 47, 48, 60), (36, 60, 49, 25), (60, 48, 37, 49),
               (38, 37, 61, 62), (39, 38, 62, 63), (40, 39, 63, 64), (41, 40, 64, 65), (42, 41, 65, 66),
               (43, 42, 66, 67), (44, 43, 67, 68), (45, 44, 68, 69), (46, 45, 69, 70), (47, 46, 70, 71),
               (48, 47, 71, 72), (37, 48, 72, 61), (124, 125, 74, 75), (171, 170, 85, 86), (173, 172, 90, 91),
               (174, 173, 91, 92), (176, 175, 88, 89), (178, 177, 93, 94), (175, 179, 87, 88), (177, 174, 92, 93),
               (170, 180, 96, 85), (180, 181, 95, 96), (181, 178, 94, 95), (172, 176, 89, 90), (179, 171, 86, 87),
               (90, 89, 101, 102), (86, 85, 97, 98), (93, 92, 104, 105), (96, 95, 107, 108), (85, 96, 108, 97),
               (89, 88, 100, 101), (91, 90, 102, 103), (88, 87, 99, 100), (92, 91, 103, 104), (95, 94, 106, 107),
               (94, 93, 105, 106), (87, 86, 98, 99), (105, 104, 116, 117), (108, 107, 119, 120), (97, 108, 120, 109),
               (101, 100, 112, 113), (103, 102, 114, 115), (100, 99, 111, 112), (104, 103, 115, 116),
               (107, 106, 118, 119),
               (106, 105, 117, 118), (99, 98, 110, 111), (102, 101, 113, 114), (98, 97, 109, 110), (120, 119, 121),
               (109, 120, 121), (113, 112, 121), (115, 114, 121), (112, 111, 121), (116, 115, 121),
               (119, 118, 121), (118, 117, 121), (111, 110, 121), (114, 113, 121), (110, 109, 121),
               (117, 116, 121), (169, 158, 62, 61), (158, 159, 63, 62), (159, 160, 64, 63), (160, 161, 65, 64),
               (161, 162, 66, 65), (162, 163, 67, 66), (163, 164, 68, 67), (164, 165, 69, 68), (165, 166, 70, 69),
               (166, 167, 71, 70), (167, 168, 72, 71), (168, 169, 61, 72), (72, 138, 127, 61), (63, 129, 130, 64),
               (67, 133, 134, 68), (64, 130, 131, 65), (61, 127, 128, 62), (69, 135, 136, 70), (66, 132, 133, 67),
               (65, 131, 132, 66), (71, 137, 138, 72), (70, 136, 137, 71), (62, 128, 129, 63), (68, 134, 135, 69),
               (0, 140, 142, 1), (1, 142, 143, 2), (2, 143, 141, 3), (3, 141, 139, 4), (4, 139, 145, 5),
               (5, 145, 144, 6), (6, 144, 148, 7), (7, 148, 147, 8), (8, 147, 150, 9), (9, 150, 146, 10),
               (10, 146, 149, 11), (12, 140, 149, 23), (153, 154, 163, 162), (154, 155, 164, 163), (155, 156, 165, 164),
               (125, 151, 73, 74), (152, 124, 75, 76), (122, 152, 76, 77), (153, 122, 77, 78), (154, 153, 78, 79),
               (155, 154, 79, 80), (156, 155, 80, 81), (123, 156, 81, 82), (157, 123, 82, 83), (126, 157, 83, 84),
               (73, 151, 126, 84), (151, 125, 158, 169), (125, 124, 159, 158), (124, 152, 160, 159),
               (152, 122, 161, 160),
               (122, 153, 162, 161), (156, 123, 166, 165), (123, 157, 167, 166), (157, 126, 168, 167),
               (126, 151, 169, 168),
               (185, 189, 213, 209), (192, 193, 217, 216), (172, 173, 197, 196), (174, 177, 201, 198),
               (171, 179, 203, 195),
               (184, 183, 207, 208), (187, 192, 216, 211), (170, 171, 195, 194), (179, 175, 199, 203),
               (176, 172, 196, 200),
               (183, 188, 212, 207), (190, 184, 208, 214), (74, 73, 187, 182), (79, 78, 188, 183), (80, 79, 183, 184),
               (77, 76, 189, 185), (82, 81, 190, 186), (76, 75, 191, 189), (81, 80, 184, 190), (73, 84, 192, 187),
               (84, 83, 193, 192), (83, 82, 186, 193), (78, 77, 185, 188), (75, 74, 182, 191), (206, 211, 194, 195),
               (207, 212, 196, 197), (208, 207, 197, 198), (209, 213, 199, 200), (210, 214, 201, 202),
               (213, 215, 203, 199),
               (214, 208, 198, 201), (211, 216, 204, 194), (216, 217, 205, 204), (217, 210, 202, 205),
               (212, 209, 200, 196),
               (215, 206, 195, 203), (180, 170, 194, 204), (173, 174, 198, 197), (193, 186, 210, 217),
               (186, 190, 214, 210),
               (181, 180, 204, 205), (175, 176, 200, 199), (188, 185, 209, 212), (189, 191, 215, 213),
               (182, 187, 211, 206),
               (178, 181, 205, 202), (177, 178, 202, 201), (191, 182, 206, 215)]

    return myvertex, myfaces


# ----------------------------------------------
# Handle model 04
# ----------------------------------------------
def handle_model_04():
    # ------------------------------------
    # Mesh data
    # ------------------------------------
    minx = -0.04349547624588013
    maxx = 0.04352114722132683
    miny = -0.07034946978092194
    maxy = 0
    minz = -0.12514087557792664
    maxz = 0.12514087557792664

    # Vertex
    myvertex = [(minx + 0.013302795588970184, maxy - 0.002780601382255554, minz + 0.010707870125770569),
                (minx + 0.0009496212005615234, maxy - 0.002942140679806471, minz + 0.030204586684703827),
                (minx, maxy - 0.003071820829063654, minz + 0.053266048431396484),
                (minx + 0.010708402842283249, maxy - 0.0031348932534456253, minz + 0.07371294498443604),
                (minx + 0.03020550962537527, maxy - 0.003114458406344056, minz + 0.08606655150651932),
                (maxx - 0.03374953381717205, maxy - 0.003015991533175111, minz + 0.08701672777533531),
                (maxx - 0.01330280676484108, maxy - 0.0028658765368163586, minz + 0.07630888000130653),
                (maxx - 0.0009496286511421204, maxy - 0.0027043374720960855, minz + 0.056812167167663574),
                (maxx, maxy - 0.0025746573228389025, minz + 0.033750712871551514),
                (maxx - 0.010708380490541458, maxy - 0.002511584199965, minz + 0.013303808867931366),
                (maxx - 0.03020548354834318, maxy - 0.0025320190470665693, minz + 0.0009501948952674866),
                (minx + 0.03374955803155899, maxy - 0.0026304861530661583, minz),
                (minx + 0.014472760260105133, maxy - 0.019589224830269814, minz + 0.01180487871170044),
                (minx + 0.002567145973443985, maxy - 0.019744910299777985, minz + 0.03059517592191696),
                (minx + 0.001651916652917862, maxy - 0.019869891926646233, minz + 0.052821069955825806),
                (minx + 0.011972300708293915, maxy - 0.019930677488446236, minz + 0.07252714410424232),
                (minx + 0.03076297417283058, maxy - 0.019910985603928566, minz + 0.0844331718981266),
                (maxx - 0.034027633257210255, maxy - 0.019816085696220398, minz + 0.0853489525616169),
                (maxx - 0.014321718364953995, maxy - 0.01967141032218933, minz + 0.07502909749746323),
                (maxx - 0.002416089177131653, maxy - 0.01951572299003601, minz + 0.056238800287246704),
                (maxx - 0.0015008561313152313, maxy - 0.019390743225812912, minz + 0.03401290625333786),
                (maxx - 0.011821221560239792, maxy - 0.01932995393872261, minz + 0.014306828379631042),
                (maxx - 0.03061189129948616, maxy - 0.01934964768588543, minz + 0.0024007856845855713),
                (minx + 0.03417872078716755, maxy - 0.019444547593593597, minz + 0.001484982669353485),
                (minx + 0.043508310547622386, maxy - 0.005668943747878075, minz + 0.043508365750312805),
                (minx + 0.029034355655312538, maxy - 0.019612153992056847, minz + 0.027617476880550385),
                (minx + 0.023084014654159546, maxy - 0.01968996599316597, minz + 0.037008725106716156),
                (minx + 0.022626593708992004, maxy - 0.01975242979824543, minz + 0.048117056488990784),
                (minx + 0.027784643694758415, maxy - 0.019782811403274536, minz + 0.05796600878238678),
                (minx + 0.03717608004808426, maxy - 0.019772969186306, minz + 0.06391655653715134),
                (maxx - 0.03873214777559042, maxy - 0.019725536927580833, minz + 0.06437425315380096),
                (maxx - 0.02888327743858099, maxy - 0.019653232768177986, minz + 0.059216469526290894),
                (maxx - 0.022932931780815125, maxy - 0.019575420767068863, minz + 0.04982522130012512),
                (maxx - 0.022475499659776688, maxy - 0.019512956961989403, minz + 0.0387168824672699),
                (maxx - 0.0276335496455431, maxy - 0.019482573494315147, minz + 0.0288679301738739),
                (maxx - 0.03702498506754637, maxy - 0.019492419436573982, minz + 0.022917382419109344),
                (minx + 0.038883245550096035, maxy - 0.0195398461073637, minz + 0.022459670901298523),
                (minx + 0.029087782837450504, maxy - 0.03150090575218201, minz + 0.027552828192710876),
                (minx + 0.023137442767620087, maxy - 0.03157871589064598, minz + 0.03694407641887665),
                (minx + 0.022680018097162247, maxy - 0.03164118155837059, minz + 0.04805241525173187),
                (minx + 0.027838071808218956, maxy - 0.031671565026044846, minz + 0.05790136009454727),
                (minx + 0.0372295081615448, maxy - 0.03166172280907631, minz + 0.06385190784931183),
                (maxx - 0.03867871919646859, maxy - 0.03161429241299629, minz + 0.06430960446596146),
                (maxx - 0.028829848393797874, maxy - 0.03154198080301285, minz + 0.05915181338787079),
                (maxx - 0.022879503667354584, maxy - 0.031464170664548874, minz + 0.04976056516170502),
                (maxx - 0.022422071546316147, maxy - 0.03140170872211456, minz + 0.03865223377943039),
                (maxx - 0.02758011966943741, maxy - 0.03137132152915001, minz + 0.028803281486034393),
                (maxx - 0.03697155695408583, maxy - 0.031381167471408844, minz + 0.022852733731269836),
                (minx + 0.038936673663556576, maxy - 0.03142859786748886, minz + 0.022395022213459015),
                (minx + 0.029038896784186363, maxy - 0.020622700452804565, minz + 0.027611978352069855),
                (minx + 0.02308855764567852, maxy - 0.02070051059126854, minz + 0.03700323402881622),
                (minx + 0.02263113297522068, maxy - 0.020762978121638298, minz + 0.04811156541109085),
                (minx + 0.02778918668627739, maxy - 0.020793357864022255, minz + 0.05796051025390625),
                (minx + 0.037180622573941946, maxy - 0.02078351564705372, minz + 0.0639110580086708),
                (maxx - 0.03872760524973273, maxy - 0.020736083388328552, minz + 0.06436875835061073),
                (maxx - 0.028878736309707165, maxy - 0.020663777366280556, minz + 0.059210970997810364),
                (maxx - 0.02292838878929615, maxy - 0.020585965365171432, minz + 0.04981972277164459),
                (maxx - 0.022470960393548012, maxy - 0.020523501560091972, minz + 0.038711391389369965),
                (maxx - 0.027629008516669273, maxy - 0.020493119955062866, minz + 0.02886243909597397),
                (maxx - 0.03702044300734997, maxy - 0.020502964034676552, minz + 0.022911883890628815),
                (minx + 0.03888778714463115, maxy - 0.02055039070546627, minz + 0.022454172372817993),
                (minx + 0.03503026906400919, maxy - 0.0326739065349102, minz + 0.03399384766817093),
                (minx + 0.03150810860097408, maxy - 0.032719966024160385, minz + 0.039552778005599976),
                (minx + 0.03123734798282385, maxy - 0.03275693953037262, minz + 0.04612809419631958),
                (minx + 0.034290531650185585, maxy - 0.032774921506643295, minz + 0.051957935094833374),
                (minx + 0.039849569322541356, maxy - 0.0327690951526165, minz + 0.0554802268743515),
                (maxx - 0.04059170465916395, maxy - 0.03274102136492729, minz + 0.055751144886016846),
                (maxx - 0.03476190101355314, maxy - 0.032698217779397964, minz + 0.05269812047481537),
                (maxx - 0.031239738687872887, maxy - 0.03265216201543808, minz + 0.04713919758796692),
                (maxx - 0.03096897155046463, maxy - 0.032615188509225845, minz + 0.040563881397247314),
                (maxx - 0.03402215428650379, maxy - 0.03259720280766487, minz + 0.03473402559757233),
                (maxx - 0.03958118986338377, maxy - 0.032603029161691666, minz + 0.031211741268634796),
                (minx + 0.04086008481681347, maxy - 0.032631102949380875, minz + 0.030940808355808258),
                (minx + 0.03971285093575716, miny + 0.029022332280874252, minz + 0.05597035586833954),
                (maxx - 0.03359287604689598, miny + 0.029201623052358627, minz + 0.034330859780311584),
                (minx + 0.03072980046272278, miny + 0.029035013169050217, minz + 0.04621553421020508),
                (minx + 0.031012218445539474, miny + 0.029073577374219894, minz + 0.03935709595680237),
                (minx + 0.04076687735505402, miny + 0.029166262596845627, minz + 0.03037431836128235),
                (minx + 0.034451283514499664, maxy - 0.03338594362139702, minz + 0.033365122973918915),
                (minx + 0.030692334286868572, maxy - 0.03343509882688522, minz + 0.039297766983509064),
                (minx + 0.03040337096899748, maxy - 0.03347455710172653, minz + 0.04631512612104416),
                (minx + 0.03366181440651417, maxy - 0.03349374979734421, minz + 0.05253690481185913),
                (minx + 0.03959457715973258, maxy - 0.033487528562545776, minz + 0.05629599094390869),
                (maxx - 0.040404647355899215, maxy - 0.033457569777965546, minz + 0.056585125625133514),
                (maxx - 0.03418291546404362, maxy - 0.03341188654303551, minz + 0.05332684516906738),
                (maxx - 0.030423964373767376, maxy - 0.0333627350628376, minz + 0.047394201159477234),
                (maxx - 0.030134993605315685, maxy - 0.03332327678799629, minz + 0.04037684202194214),
                (maxx - 0.033393437042832375, maxy - 0.03330408036708832, minz + 0.03415506333112717),
                (maxx - 0.03932619746774435, maxy - 0.03331030160188675, minz + 0.030395977199077606),
                (minx + 0.040673027746379375, maxy - 0.03334026038646698, minz + 0.030106835067272186),
                (minx + 0.030282274819910526, maxy - 0.005427400581538677, minz + 0.08584162965416908),
                (minx + 0.013463903218507767, maxy - 0.005095209460705519, minz + 0.0108589306473732),
                (minx + 0.010882444679737091, maxy - 0.005447734147310257, minz + 0.07354965433478355),
                (minx + 0.0011723600327968597, maxy - 0.005255943164229393, minz + 0.03025837242603302),
                (minx + 0.0002274736762046814, maxy - 0.005384976044297218, minz + 0.05320477485656738),
                (maxx - 0.0134431142359972, maxy - 0.005180059932172298, minz + 0.0761326476931572),
                (maxx - 0.033787828870117664, maxy - 0.005329424981027842, minz + 0.08678706735372543),
                (maxx - 0.0302614476531744, maxy - 0.004847868345677853, minz + 0.0011499449610710144),
                (maxx - 0.00020667165517807007, maxy - 0.004890293348580599, minz + 0.03378681838512421),
                (maxx - 0.0011515654623508453, maxy - 0.0050193266943097115, minz + 0.05673321336507797),
                (minx + 0.033808655105531216, maxy - 0.004945843946188688, minz + 0.0002044886350631714),
                (maxx - 0.010861624032258987, maxy - 0.004827534779906273, minz + 0.01344192773103714),
                (minx + 0.03468604106456041, miny + 0.029121622443199158, minz + 0.033558815717697144),
                (minx + 0.033914451487362385, miny + 0.02901625633239746, minz + 0.05229640752077103),
                (maxx - 0.04044530005194247, miny + 0.029051613062620163, minz + 0.056252941489219666),
                (maxx - 0.034364476799964905, miny + 0.02909626066684723, minz + 0.053068459033966064),
                (maxx - 0.03069065511226654, miny + 0.029144294559955597, minz + 0.04727017134428024),
                (maxx - 0.030408228747546673, miny + 0.029182862490415573, minz + 0.04041174054145813),
                (maxx - 0.03939127502962947, miny + 0.029195547103881836, minz + 0.030656911432743073),
                (minx + 0.03147818427532911, maxy - 0.033236272633075714, minz + 0.03954096883535385),
                (minx + 0.031206720508635044, maxy - 0.03327333927154541, minz + 0.0461333692073822),
                (minx + 0.034267837181687355, maxy - 0.033291369676589966, minz + 0.05197836458683014),
                (minx + 0.03984131896868348, maxy - 0.03328552842140198, minz + 0.05550979822874069),
                (maxx - 0.040582869900390506, maxy - 0.0332573801279068, minz + 0.055781424045562744),
                (maxx - 0.03473791852593422, maxy - 0.033214468508958817, minz + 0.05272047221660614),
                (maxx - 0.031206604093313217, maxy - 0.03316829353570938, minz + 0.04714709520339966),
                (maxx - 0.030935133807361126, maxy - 0.03313122317194939, minz + 0.040554702281951904),
                (maxx - 0.03399624954909086, maxy - 0.03311318904161453, minz + 0.03470969945192337),
                (maxx - 0.03956972947344184, maxy - 0.03311903029680252, minz + 0.03117825835943222),
                (minx + 0.04085446032695472, maxy - 0.0331471785902977, minz + 0.03090662509202957),
                (minx + 0.035009496845304966, maxy - 0.03319009393453598, minz + 0.033967599272727966),
                (maxx - 0.03939127502962947, miny + 0.0002205297350883484, minz + 0.0343027338385582),
                (maxx - 0.030408228747546673, miny + 0.007109262049198151, minz + 0.04120940715074539),
                (maxx - 0.03069065511226654, miny + 0.011931635439395905, minz + 0.046086326241493225),
                (maxx - 0.034364476799964905, miny + 0.01599767804145813, minz + 0.050220295786857605),
                (maxx - 0.04044530005194247, miny + 0.01821787655353546, minz + 0.05250363051891327),
                (minx + 0.033914451487362385, miny + 0.015395186841487885, minz + 0.04973094165325165),
                (minx + 0.03468604106456041, miny + 0.0022202134132385254, minz + 0.03640696406364441),
                (minx + 0.04076687735505402, miny, minz + 0.03412361443042755),
                (minx + 0.031012218445539474, miny + 0.006286241114139557, minz + 0.040540941059589386),
                (minx + 0.03072980046272278, miny + 0.011108621954917908, minz + 0.04541786015033722),
                (maxx - 0.03359287604689598, miny + 0.002822697162628174, minz + 0.036896318197250366),
                (minx + 0.03971285093575716, miny + 0.01799735426902771, minz + 0.05232451856136322),
                (minx + 0.0343002462759614, miny + 0.015705399215221405, maxz - 0.10733164101839066),
                (minx + 0.030871009454131126, miny + 0.011495128273963928, maxz - 0.10745517536997795),
                (minx + 0.030871009454131126, miny + 0.006645478308200836, maxz - 0.1074824407696724),
                (minx + 0.0343002462759614, miny + 0.0024559199810028076, maxz - 0.10740615427494049),
                (minx + 0.04023986402899027, miny + 4.902482032775879e-05, maxz - 0.10724674165248871),
                (maxx - 0.03991828765720129, miny + 6.973743438720703e-05, maxz - 0.10704692453145981),
                (maxx - 0.03397867642343044, miny + 0.0025124847888946533, maxz - 0.10686022788286209),
                (maxx - 0.030549442395567894, miny + 0.00672275573015213, maxz - 0.1067366972565651),
                (maxx - 0.030549442395567894, miny + 0.011572405695915222, maxz - 0.10670943185687065),
                (maxx - 0.03397867642343044, miny + 0.015761971473693848, maxz - 0.10678572952747345),
                (maxx - 0.03991828765720129, miny + 0.0181688591837883, maxz - 0.10694513842463493),
                (minx + 0.04023986402899027, miny + 0.018148154020309448, maxz - 0.10714496672153473),
                (minx + 0.013302795588970184, maxy - 0.002780601382255554, maxz - 0.010707870125770569),
                (minx + 0.0009496212005615234, maxy - 0.002942140679806471, maxz - 0.030204586684703827),
                (minx, maxy - 0.003071820829063654, maxz - 0.053266048431396484),
                (minx + 0.010708402842283249, maxy - 0.0031348932534456253, maxz - 0.07371294498443604),
                (minx + 0.03020550962537527, maxy - 0.003114458406344056, maxz - 0.08606655150651932),
                (maxx - 0.03374953381717205, maxy - 0.003015991533175111, maxz - 0.08701672777533531),
                (maxx - 0.01330280676484108, maxy - 0.0028658765368163586, maxz - 0.07630888000130653),
                (maxx - 0.0009496286511421204, maxy - 0.0027043374720960855, maxz - 0.056812167167663574),
                (maxx, maxy - 0.0025746573228389025, maxz - 0.033750712871551514),
                (maxx - 0.010708380490541458, maxy - 0.002511584199965, maxz - 0.013303808867931366),
                (maxx - 0.03020548354834318, maxy - 0.0025320190470665693, maxz - 0.0009501948952674866),
                (minx + 0.03374955803155899, maxy - 0.0026304861530661583, maxz),
                (minx + 0.014472760260105133, maxy - 0.019589224830269814, maxz - 0.01180487871170044),
                (minx + 0.002567145973443985, maxy - 0.019744910299777985, maxz - 0.03059517592191696),
                (minx + 0.001651916652917862, maxy - 0.019869891926646233, maxz - 0.052821069955825806),
                (minx + 0.011972300708293915, maxy - 0.019930677488446236, maxz - 0.07252714410424232),
                (minx + 0.03076297417283058, maxy - 0.019910985603928566, maxz - 0.0844331718981266),
                (maxx - 0.034027633257210255, maxy - 0.019816085696220398, maxz - 0.0853489525616169),
                (maxx - 0.014321718364953995, maxy - 0.01967141032218933, maxz - 0.07502909749746323),
                (maxx - 0.002416089177131653, maxy - 0.01951572299003601, maxz - 0.056238800287246704),
                (maxx - 0.0015008561313152313, maxy - 0.019390743225812912, maxz - 0.03401290625333786),
                (maxx - 0.011821221560239792, maxy - 0.01932995393872261, maxz - 0.014306828379631042),
                (maxx - 0.03061189129948616, maxy - 0.01934964768588543, maxz - 0.0024007856845855713),
                (minx + 0.03417872078716755, maxy - 0.019444547593593597, maxz - 0.001484982669353485),
                (minx + 0.043508310547622386, maxy - 0.005668943747878075, maxz - 0.043508365750312805),
                (minx + 0.029034355655312538, maxy - 0.019612153992056847, maxz - 0.027617476880550385),
                (minx + 0.023084014654159546, maxy - 0.01968996599316597, maxz - 0.037008725106716156),
                (minx + 0.022626593708992004, maxy - 0.01975242979824543, maxz - 0.048117056488990784),
                (minx + 0.027784643694758415, maxy - 0.019782811403274536, maxz - 0.05796600878238678),
                (minx + 0.03717608004808426, maxy - 0.019772969186306, maxz - 0.06391655653715134),
                (maxx - 0.03873214777559042, maxy - 0.019725536927580833, maxz - 0.06437425315380096),
                (maxx - 0.02888327743858099, maxy - 0.019653232768177986, maxz - 0.059216469526290894),
                (maxx - 0.022932931780815125, maxy - 0.019575420767068863, maxz - 0.04982522130012512),
                (maxx - 0.022475499659776688, maxy - 0.019512956961989403, maxz - 0.0387168824672699),
                (maxx - 0.0276335496455431, maxy - 0.019482573494315147, maxz - 0.0288679301738739),
                (maxx - 0.03702498506754637, maxy - 0.019492419436573982, maxz - 0.022917382419109344),
                (minx + 0.038883245550096035, maxy - 0.0195398461073637, maxz - 0.022459670901298523),
                (minx + 0.029087782837450504, maxy - 0.03150090575218201, maxz - 0.027552828192710876),
                (minx + 0.023137442767620087, maxy - 0.03157871589064598, maxz - 0.03694407641887665),
                (minx + 0.022680018097162247, maxy - 0.03164118155837059, maxz - 0.04805241525173187),
                (minx + 0.027838071808218956, maxy - 0.031671565026044846, maxz - 0.05790136009454727),
                (minx + 0.0372295081615448, maxy - 0.03166172280907631, maxz - 0.06385190784931183),
                (maxx - 0.03867871919646859, maxy - 0.03161429241299629, maxz - 0.06430960446596146),
                (maxx - 0.028829848393797874, maxy - 0.03154198080301285, maxz - 0.05915181338787079),
                (maxx - 0.022879503667354584, maxy - 0.031464170664548874, maxz - 0.04976056516170502),
                (maxx - 0.022422071546316147, maxy - 0.03140170872211456, maxz - 0.03865223377943039),
                (maxx - 0.02758011966943741, maxy - 0.03137132152915001, maxz - 0.028803281486034393),
                (maxx - 0.03697155695408583, maxy - 0.031381167471408844, maxz - 0.022852733731269836),
                (minx + 0.038936673663556576, maxy - 0.03142859786748886, maxz - 0.022395022213459015),
                (minx + 0.029038896784186363, maxy - 0.020622700452804565, maxz - 0.027611978352069855),
                (minx + 0.02308855764567852, maxy - 0.02070051059126854, maxz - 0.03700323402881622),
                (minx + 0.02263113297522068, maxy - 0.020762978121638298, maxz - 0.04811156541109085),
                (minx + 0.02778918668627739, maxy - 0.020793357864022255, maxz - 0.05796051025390625),
                (minx + 0.037180622573941946, maxy - 0.02078351564705372, maxz - 0.0639110580086708),
                (maxx - 0.03872760524973273, maxy - 0.020736083388328552, maxz - 0.06436875835061073),
                (maxx - 0.028878736309707165, maxy - 0.020663777366280556, maxz - 0.059210970997810364),
                (maxx - 0.02292838878929615, maxy - 0.020585965365171432, maxz - 0.04981972277164459),
                (maxx - 0.022470960393548012, maxy - 0.020523501560091972, maxz - 0.038711391389369965),
                (maxx - 0.027629008516669273, maxy - 0.020493119955062866, maxz - 0.02886243909597397),
                (maxx - 0.03702044300734997, maxy - 0.020502964034676552, maxz - 0.022911883890628815),
                (minx + 0.03888778714463115, maxy - 0.02055039070546627, maxz - 0.022454172372817993),
                (minx + 0.03503026906400919, maxy - 0.0326739065349102, maxz - 0.03399384766817093),
                (minx + 0.03150810860097408, maxy - 0.032719966024160385, maxz - 0.039552778005599976),
                (minx + 0.03123734798282385, maxy - 0.03275693953037262, maxz - 0.04612809419631958),
                (minx + 0.034290531650185585, maxy - 0.032774921506643295, maxz - 0.051957935094833374),
                (minx + 0.039849569322541356, maxy - 0.0327690951526165, maxz - 0.0554802268743515),
                (maxx - 0.04059170465916395, maxy - 0.03274102136492729, maxz - 0.055751144886016846),
                (maxx - 0.03476190101355314, maxy - 0.032698217779397964, maxz - 0.05269812047481537),
                (maxx - 0.031239738687872887, maxy - 0.03265216201543808, maxz - 0.04713919758796692),
                (maxx - 0.03096897155046463, maxy - 0.032615188509225845, maxz - 0.040563881397247314),
                (maxx - 0.03402215428650379, maxy - 0.03259720280766487, maxz - 0.03473402559757233),
                (maxx - 0.03958118986338377, maxy - 0.032603029161691666, maxz - 0.031211741268634796),
                (minx + 0.04086008481681347, maxy - 0.032631102949380875, maxz - 0.030940808355808258),
                (minx + 0.03971285093575716, miny + 0.029022332280874252, maxz - 0.05597035586833954),
                (maxx - 0.03359287604689598, miny + 0.029201623052358627, maxz - 0.034330859780311584),
                (minx + 0.03072980046272278, miny + 0.029035013169050217, maxz - 0.04621553421020508),
                (minx + 0.031012218445539474, miny + 0.029073577374219894, maxz - 0.03935709595680237),
                (minx + 0.04076687735505402, miny + 0.029166262596845627, maxz - 0.03037431836128235),
                (minx + 0.034451283514499664, maxy - 0.03338594362139702, maxz - 0.033365122973918915),
                (minx + 0.030692334286868572, maxy - 0.03343509882688522, maxz - 0.039297766983509064),
                (minx + 0.03040337096899748, maxy - 0.03347455710172653, maxz - 0.04631512612104416),
                (minx + 0.03366181440651417, maxy - 0.03349374979734421, maxz - 0.05253690481185913),
                (minx + 0.03959457715973258, maxy - 0.033487528562545776, maxz - 0.05629599094390869),
                (maxx - 0.040404647355899215, maxy - 0.033457569777965546, maxz - 0.056585125625133514),
                (maxx - 0.03418291546404362, maxy - 0.03341188654303551, maxz - 0.05332684516906738),
                (maxx - 0.030423964373767376, maxy - 0.0333627350628376, maxz - 0.047394201159477234),
                (maxx - 0.030134993605315685, maxy - 0.03332327678799629, maxz - 0.04037684202194214),
                (maxx - 0.033393437042832375, maxy - 0.03330408036708832, maxz - 0.03415506333112717),
                (maxx - 0.03932619746774435, maxy - 0.03331030160188675, maxz - 0.030395977199077606),
                (minx + 0.040673027746379375, maxy - 0.03334026038646698, maxz - 0.030106835067272186),
                (minx + 0.030282274819910526, maxy - 0.005427400581538677, maxz - 0.08584162965416908),
                (minx + 0.013463903218507767, maxy - 0.005095209460705519, maxz - 0.0108589306473732),
                (minx + 0.010882444679737091, maxy - 0.005447734147310257, maxz - 0.07354965433478355),
                (minx + 0.0011723600327968597, maxy - 0.005255943164229393, maxz - 0.03025837242603302),
                (minx + 0.0002274736762046814, maxy - 0.005384976044297218, maxz - 0.05320477485656738),
                (maxx - 0.0134431142359972, maxy - 0.005180059932172298, maxz - 0.0761326476931572),
                (maxx - 0.033787828870117664, maxy - 0.005329424981027842, maxz - 0.08678706735372543),
                (maxx - 0.0302614476531744, maxy - 0.004847868345677853, maxz - 0.0011499449610710144),
                (maxx - 0.00020667165517807007, maxy - 0.004890293348580599, maxz - 0.03378681838512421),
                (maxx - 0.0011515654623508453, maxy - 0.0050193266943097115, maxz - 0.05673321336507797),
                (minx + 0.033808655105531216, maxy - 0.004945843946188688, maxz - 0.0002044886350631714),
                (maxx - 0.010861624032258987, maxy - 0.004827534779906273, maxz - 0.01344192773103714),
                (minx + 0.03468604106456041, miny + 0.029121622443199158, maxz - 0.033558815717697144),
                (minx + 0.033914451487362385, miny + 0.02901625633239746, maxz - 0.05229640752077103),
                (maxx - 0.04044530005194247, miny + 0.029051613062620163, maxz - 0.056252941489219666),
                (maxx - 0.034364476799964905, miny + 0.02909626066684723, maxz - 0.053068459033966064),
                (maxx - 0.03069065511226654, miny + 0.029144294559955597, maxz - 0.04727017134428024),
                (maxx - 0.030408228747546673, miny + 0.029182862490415573, maxz - 0.04041174054145813),
                (maxx - 0.03939127502962947, miny + 0.029195547103881836, maxz - 0.030656911432743073),
                (minx + 0.03147818427532911, maxy - 0.033236272633075714, maxz - 0.03954096883535385),
                (minx + 0.031206720508635044, maxy - 0.03327333927154541, maxz - 0.0461333692073822),
                (minx + 0.034267837181687355, maxy - 0.033291369676589966, maxz - 0.05197836458683014),
                (minx + 0.03984131896868348, maxy - 0.03328552842140198, maxz - 0.05550979822874069),
                (maxx - 0.040582869900390506, maxy - 0.0332573801279068, maxz - 0.055781424045562744),
                (maxx - 0.03473791852593422, maxy - 0.033214468508958817, maxz - 0.05272047221660614),
                (maxx - 0.031206604093313217, maxy - 0.03316829353570938, maxz - 0.04714709520339966),
                (maxx - 0.030935133807361126, maxy - 0.03313122317194939, maxz - 0.040554702281951904),
                (maxx - 0.03399624954909086, maxy - 0.03311318904161453, maxz - 0.03470969945192337),
                (maxx - 0.03956972947344184, maxy - 0.03311903029680252, maxz - 0.03117825835943222),
                (minx + 0.04085446032695472, maxy - 0.0331471785902977, maxz - 0.03090662509202957),
                (minx + 0.035009496845304966, maxy - 0.03319009393453598, maxz - 0.033967599272727966),
                (maxx - 0.03939127502962947, miny + 0.0002205297350883484, maxz - 0.0343027338385582),
                (maxx - 0.030408228747546673, miny + 0.007109262049198151, maxz - 0.04120940715074539),
                (maxx - 0.03069065511226654, miny + 0.011931635439395905, maxz - 0.046086326241493225),
                (maxx - 0.034364476799964905, miny + 0.01599767804145813, maxz - 0.050220295786857605),
                (maxx - 0.04044530005194247, miny + 0.01821787655353546, maxz - 0.05250363051891327),
                (minx + 0.033914451487362385, miny + 0.015395186841487885, maxz - 0.04973094165325165),
                (minx + 0.03468604106456041, miny + 0.0022202134132385254, maxz - 0.03640696406364441),
                (minx + 0.04076687735505402, miny, maxz - 0.03412361443042755),
                (minx + 0.031012218445539474, miny + 0.006286241114139557, maxz - 0.040540941059589386),
                (minx + 0.03072980046272278, miny + 0.011108621954917908, maxz - 0.04541786015033722),
                (maxx - 0.03359287604689598, miny + 0.002822697162628174, maxz - 0.036896318197250366),
                (minx + 0.03971285093575716, miny + 0.01799735426902771, maxz - 0.05232451856136322),
                (minx + 0.0343002462759614, miny + 0.015705399215221405, minz + 0.10733164101839066),
                (minx + 0.030871009454131126, miny + 0.011495128273963928, minz + 0.10745517536997795),
                (minx + 0.030871009454131126, miny + 0.006645478308200836, minz + 0.1074824407696724),
                (minx + 0.0343002462759614, miny + 0.0024559199810028076, minz + 0.10740615427494049),
                (minx + 0.04023986402899027, miny + 4.902482032775879e-05, minz + 0.10724674165248871),
                (maxx - 0.03991828765720129, miny + 6.973743438720703e-05, minz + 0.10704692453145981),
                (maxx - 0.03397867642343044, miny + 0.0025124847888946533, minz + 0.10686022788286209),
                (maxx - 0.030549442395567894, miny + 0.00672275573015213, minz + 0.1067366972565651),
                (maxx - 0.030549442395567894, miny + 0.011572405695915222, minz + 0.10670943185687065),
                (maxx - 0.03397867642343044, miny + 0.015761971473693848, minz + 0.10678572952747345),
                (maxx - 0.03991828765720129, miny + 0.0181688591837883, minz + 0.10694513842463493),
                (minx + 0.04023986402899027, miny + 0.018148154020309448, minz + 0.10714496672153473)]

    # Faces
    myfaces = [(24, 0, 1), (24, 1, 2), (24, 2, 3), (24, 3, 4), (24, 4, 5),
               (24, 5, 6), (24, 6, 7), (24, 7, 8), (24, 8, 9), (24, 9, 10),
               (24, 10, 11), (11, 0, 24), (91, 12, 13, 93), (93, 13, 14, 94), (94, 14, 15, 92),
               (92, 15, 16, 90), (90, 16, 17, 96), (96, 17, 18, 95), (95, 18, 19, 99), (99, 19, 20, 98),
               (98, 20, 21, 101), (101, 21, 22, 97), (97, 22, 23, 100), (91, 0, 11, 100), (13, 12, 25, 26),
               (14, 13, 26, 27), (15, 14, 27, 28), (16, 15, 28, 29), (17, 16, 29, 30), (18, 17, 30, 31),
               (19, 18, 31, 32), (20, 19, 32, 33), (21, 20, 33, 34), (22, 21, 34, 35), (23, 22, 35, 36),
               (12, 23, 36, 25), (25, 49, 50, 26), (49, 37, 38, 50), (26, 50, 51, 27), (50, 38, 39, 51),
               (27, 51, 52, 28), (51, 39, 40, 52), (28, 52, 53, 29), (52, 40, 41, 53), (29, 53, 54, 30),
               (53, 41, 42, 54), (30, 54, 55, 31), (54, 42, 43, 55), (31, 55, 56, 32), (55, 43, 44, 56),
               (32, 56, 57, 33), (56, 44, 45, 57), (33, 57, 58, 34), (57, 45, 46, 58), (34, 58, 59, 35),
               (58, 46, 47, 59), (35, 59, 60, 36), (59, 47, 48, 60), (36, 60, 49, 25), (60, 48, 37, 49),
               (38, 37, 61, 62), (39, 38, 62, 63), (40, 39, 63, 64), (41, 40, 64, 65), (42, 41, 65, 66),
               (43, 42, 66, 67), (44, 43, 67, 68), (45, 44, 68, 69), (46, 45, 69, 70), (47, 46, 70, 71),
               (48, 47, 71, 72), (37, 48, 72, 61), (120, 109, 62, 61), (109, 110, 63, 62), (110, 111, 64, 63),
               (111, 112, 65, 64), (112, 113, 66, 65), (113, 114, 67, 66), (114, 115, 68, 67), (115, 116, 69, 68),
               (116, 117, 70, 69), (117, 118, 71, 70), (118, 119, 72, 71), (119, 120, 61, 72), (72, 89, 78, 61),
               (63, 80, 81, 64), (67, 84, 85, 68), (64, 81, 82, 65), (61, 78, 79, 62), (69, 86, 87, 70),
               (66, 83, 84, 67), (65, 82, 83, 66), (71, 88, 89, 72), (70, 87, 88, 71), (62, 79, 80, 63),
               (68, 85, 86, 69), (0, 91, 93, 1), (1, 93, 94, 2), (2, 94, 92, 3), (3, 92, 90, 4),
               (4, 90, 96, 5), (5, 96, 95, 6), (6, 95, 99, 7), (7, 99, 98, 8), (8, 98, 101, 9),
               (9, 101, 97, 10), (10, 97, 100, 11), (12, 91, 100, 23), (104, 105, 114, 113), (105, 106, 115, 114),
               (106, 107, 116, 115), (102, 76, 109, 120), (76, 75, 110, 109), (75, 103, 111, 110), (103, 73, 112, 111),
               (73, 104, 113, 112), (107, 74, 117, 116), (74, 108, 118, 117), (108, 77, 119, 118), (77, 102, 120, 119),
               (74, 107, 122, 131), (107, 106, 123, 122), (104, 73, 132, 125), (106, 105, 124, 123), (75, 76, 129, 130),
               (73, 103, 126, 132), (105, 104, 125, 124), (102, 77, 128, 127), (103, 75, 130, 126), (77, 108, 121, 128),
               (76, 102, 127, 129), (108, 74, 131, 121), (126, 130, 134, 133), (130, 129, 135, 134),
               (129, 127, 136, 135),
               (127, 128, 137, 136), (128, 121, 138, 137), (121, 131, 139, 138), (131, 122, 140, 139),
               (122, 123, 141, 140),
               (123, 124, 142, 141), (124, 125, 143, 142), (125, 132, 144, 143), (132, 126, 133, 144),
               (169, 146, 145),
               (169, 147, 146), (169, 148, 147), (169, 149, 148), (169, 150, 149), (169, 151, 150),
               (169, 152, 151), (169, 153, 152), (169, 154, 153), (169, 155, 154), (169, 156, 155),
               (156, 169, 145), (236, 238, 158, 157), (238, 239, 159, 158), (239, 237, 160, 159), (237, 235, 161, 160),
               (235, 241, 162, 161), (241, 240, 163, 162), (240, 244, 164, 163), (244, 243, 165, 164),
               (243, 246, 166, 165),
               (246, 242, 167, 166), (242, 245, 168, 167), (236, 245, 156, 145), (158, 171, 170, 157),
               (159, 172, 171, 158),
               (160, 173, 172, 159), (161, 174, 173, 160), (162, 175, 174, 161), (163, 176, 175, 162),
               (164, 177, 176, 163),
               (165, 178, 177, 164), (166, 179, 178, 165), (167, 180, 179, 166), (168, 181, 180, 167),
               (157, 170, 181, 168),
               (170, 171, 195, 194), (194, 195, 183, 182), (171, 172, 196, 195), (195, 196, 184, 183),
               (172, 173, 197, 196),
               (196, 197, 185, 184), (173, 174, 198, 197), (197, 198, 186, 185), (174, 175, 199, 198),
               (198, 199, 187, 186),
               (175, 176, 200, 199), (199, 200, 188, 187), (176, 177, 201, 200), (200, 201, 189, 188),
               (177, 178, 202, 201),
               (201, 202, 190, 189), (178, 179, 203, 202), (202, 203, 191, 190), (179, 180, 204, 203),
               (203, 204, 192, 191),
               (180, 181, 205, 204), (204, 205, 193, 192), (181, 170, 194, 205), (205, 194, 182, 193),
               (183, 207, 206, 182),
               (184, 208, 207, 183), (185, 209, 208, 184), (186, 210, 209, 185), (187, 211, 210, 186),
               (188, 212, 211, 187),
               (189, 213, 212, 188), (190, 214, 213, 189), (191, 215, 214, 190), (192, 216, 215, 191),
               (193, 217, 216, 192),
               (182, 206, 217, 193), (265, 206, 207, 254), (254, 207, 208, 255), (255, 208, 209, 256),
               (256, 209, 210, 257),
               (257, 210, 211, 258), (258, 211, 212, 259), (259, 212, 213, 260), (260, 213, 214, 261),
               (261, 214, 215, 262),
               (262, 215, 216, 263), (263, 216, 217, 264), (264, 217, 206, 265), (217, 206, 223, 234),
               (208, 209, 226, 225),
               (212, 213, 230, 229), (209, 210, 227, 226), (206, 207, 224, 223), (214, 215, 232, 231),
               (211, 212, 229, 228),
               (210, 211, 228, 227), (216, 217, 234, 233), (215, 216, 233, 232), (207, 208, 225, 224),
               (213, 214, 231, 230),
               (145, 146, 238, 236), (146, 147, 239, 238), (147, 148, 237, 239), (148, 149, 235, 237),
               (149, 150, 241, 235),
               (150, 151, 240, 241), (151, 152, 244, 240), (152, 153, 243, 244), (153, 154, 246, 243),
               (154, 155, 242, 246),
               (155, 156, 245, 242), (157, 168, 245, 236), (249, 258, 259, 250), (250, 259, 260, 251),
               (251, 260, 261, 252),
               (247, 265, 254, 221), (221, 254, 255, 220), (220, 255, 256, 248), (248, 256, 257, 218),
               (218, 257, 258, 249),
               (252, 261, 262, 219), (219, 262, 263, 253), (253, 263, 264, 222), (222, 264, 265, 247),
               (219, 276, 267, 252),
               (252, 267, 268, 251), (249, 270, 277, 218), (251, 268, 269, 250), (220, 275, 274, 221),
               (218, 277, 271, 248),
               (250, 269, 270, 249), (247, 272, 273, 222), (248, 271, 275, 220), (222, 273, 266, 253),
               (221, 274, 272, 247),
               (253, 266, 276, 219), (271, 278, 279, 275), (275, 279, 280, 274), (274, 280, 281, 272),
               (272, 281, 282, 273),
               (273, 282, 283, 266), (266, 283, 284, 276), (276, 284, 285, 267), (267, 285, 286, 268),
               (268, 286, 287, 269),
               (269, 287, 288, 270), (270, 288, 289, 277), (277, 289, 278, 271)]

    return myvertex, myfaces
