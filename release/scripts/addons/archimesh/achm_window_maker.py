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
from math import pi, radians
from bpy.types import Operator, PropertyGroup, Object, Panel
from bpy.props import StringProperty, FloatProperty, BoolProperty, IntProperty, FloatVectorProperty, \
    CollectionProperty, EnumProperty
from .achm_tools import *


# ------------------------------------------------------------------
# Define operator class to create object
# ------------------------------------------------------------------
class AchmWindows(Operator):
    bl_idname = "mesh.archimesh_window"
    bl_label = "Rail Windows"
    bl_description = "Rail Windows Generator"
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
    mainmesh = bpy.data.meshes.new("WindowFrane")
    mainobject = bpy.data.objects.new("WindowFrame", mainmesh)
    mainobject.location = bpy.context.scene.cursor_location
    bpy.context.scene.objects.link(mainobject)
    mainobject.WindowObjectGenerator.add()

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

    # ---------------------------------
    #  Clear Parent objects (autohole)
    # ---------------------------------
    myparent = o.parent
    ploc = myparent.location
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
    mp = mainobject.WindowObjectGenerator[0]
    # Create only mesh, because the object is created before
    if mp.opentype == "1":
        generate_rail_window(mainobject, mp, tmp_mesh)
    else:
        generate_leaf_window(mainobject, mp, tmp_mesh)

    remove_doubles(mainobject)
    set_normals(mainobject)

    # saves OpenGL data
    if mp.blind is True:
        plus = mp.blind_height
    else:
        plus = 0

    mp.glpoint_a = (-mp.width / 2, 0, 0)
    mp.glpoint_b = (-mp.width / 2, 0, mp.height + plus)
    mp.glpoint_c = (mp.width / 2, 0, mp.height + plus)

    # Lock
    mainobject.lock_location = (True, True, True)
    mainobject.lock_rotation = (True, True, True)

    # -------------------------
    # Create empty and parent
    # -------------------------
    bpy.ops.object.empty_add(type='PLAIN_AXES')
    myempty = bpy.data.objects[bpy.context.active_object.name]
    myempty.location = mainobject.location

    myempty.name = "Window_Group"
    parentobject(myempty, mainobject)
    mainobject["archimesh.hole_enable"] = True
    # Rotate Empty
    myempty.rotation_euler.z = radians(mp.r)
    # Create control box to open wall holes
    gap = 0.002
    y = 0
    z = 0
    if mp.blind is True:
        y = mp.blind_rail
    if mp.blind is True and mp.blind_box is True:
        z = mp.blind_height

    myctrl = create_control_box("CTRL_Hole",
                                mp.width - gap,
                                mp.depth * 3,  # + y,
                                mp.height + z - gap)
    # Add custom property to detect Controller
    myctrl["archimesh.ctrl_hole"] = True

    set_normals(myctrl)
    myctrl.parent = myempty
    myctrl.location.x = 0
    myctrl.location.y = -mp.depth * 3 / 2
    myctrl.location.z = 0
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

        mat = create_transparent_material("hidden_material", False)
        set_material(myctrl, mat)

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
            min=0.20, max=50,
            default=1.20, precision=3,
            description='window width',
            update=update_object,
            )
    depth = FloatProperty(
            name='Depth',
            min=0.07, max=1,
            default=0.10, precision=3,
            description='window depth',
            update=update_object,
            )
    height = FloatProperty(
            name='Height',
            min=0.20, max=50,
            default=1, precision=3,
            description='window height',
            update=update_object,
            )
    r = FloatProperty(
            name='Rotation', min=0, max=360, default=0, precision=1,
            description='Window rotation',
            update=update_object,
            )

    external = BoolProperty(
            name="External frame",
            description="Create an external front frame",
            default=True,
            update=update_object,
            )
    frame = FloatProperty(
            name='External Frame',
            min=0.001, max=1,
            default=0.01, precision=3,
            description='External Frame size',
            update=update_object,
            )

    frame_L = FloatProperty(
            name='Frame',
            min=0.02, max=1,
            default=0.06, precision=3,
            description='Frame size',
            update=update_object,
            )
    wf = FloatProperty(
            name='WinFrame',
            min=0.001, max=1,
            default=0.05, precision=3,
            description='Window Frame size',
            update=update_object,
            )
    leafratio = FloatProperty(
            name='Leaf ratio',
            min=0.001, max=0.999,
            default=0.50,
            precision=3,
            description='Leaf thickness ratio',
            update=update_object,
            )
    opentype = EnumProperty(
            items=(
                ('1', "Rail window", ""),
                ('2', "Two leaf", ""),
                ('3', "Right leaf", ""),
                ('4', "Left leaf", "")),
            name="Type",
            description="Defines type of window",
            update=update_object,
            )
    handle = BoolProperty(
            name="Create handles",
            description="Create default handle to the leaf",
            default=True,
            update=update_object,
            )

    sill = BoolProperty(
            name="Sill",
            description="Add sill to window",
            default=True,
            update=update_object,
            )
    sill_thickness = FloatProperty(
            name='Thickness',
            min=0, max=50,
            default=0.01, precision=3,
            description='Sill thickness',
            update=update_object,
            )
    sill_back = FloatProperty(
            name='Back',
            min=0, max=10,
            default=0.0, precision=3,
            description='Extrusion in back side',
            update=update_object,
            )
    sill_front = FloatProperty(
            name='Front',
            min=0, max=10,
            default=0.12, precision=3,
            description='Extrusion in front side',
            update=update_object,
            )

    blind = BoolProperty(
            name="Blind",
            description="Create an external blind",
            default=False,
            update=update_object,
            )
    blind_box = BoolProperty(
            name="Blind box", description="Create a box over frame for blind",
            default=True,
            update=update_object,
            )
    blind_height = FloatProperty(
            name='Height',
            min=0.001, max=10,
            default=0.12, precision=3,
            description='Blind box height',
            update=update_object,
            )
    blind_back = FloatProperty(
            name='Back',
            min=0.001, max=10,
            default=0.002, precision=3,
            description='Extrusion in back side',
            update=update_object,
            )
    blind_rail = FloatProperty(
            name='Separation',
            min=0.001, max=10,
            default=0.10, precision=3,
            description='Separation from frame',
            update=update_object,
            )
    blind_ratio = IntProperty(
            name='Extend',
            min=0, max=100,
            default=20,
            description='% of extension (100 full extend)',
            update=update_object,
            )

    # Materials
    crt_mat = BoolProperty(
            name="Create default Cycles materials",
            description="Create default materials for Cycles render",
            default=True,
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

# Register
bpy.utils.register_class(ObjectProperties)
Object.WindowObjectGenerator = CollectionProperty(type=ObjectProperties)


# ------------------------------------------------------------------
# Define panel class to modify object
# ------------------------------------------------------------------
class AchmWindowObjectgeneratorpanel(Panel):
    bl_idname = "OBJECT_PT_window_generator"
    bl_label = "Window Rail"
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
        if 'WindowObjectGenerator' not in o:
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
            if 'WindowObjectGenerator' not in o:
                return
        except:
            return

        layout = self.layout
        if bpy.context.mode == 'EDIT_MESH':
            layout.label('Warning: Operator does not work in edit mode.', icon='ERROR')
        else:
            myobjdat = o.WindowObjectGenerator[0]
            space = bpy.context.space_data
            if not space.local_view:
                # Imperial units warning
                if bpy.context.scene.unit_settings.system == "IMPERIAL":
                    row = layout.row()
                    row.label("Warning: Imperial units not supported", icon='COLOR_RED')

                box = layout.box()
                row = box.row()
                row.prop(myobjdat, 'opentype')
                row = box.row()
                row.label("Window size")
                row = box.row()
                row.prop(myobjdat, 'width')
                row.prop(myobjdat, 'depth')
                row.prop(myobjdat, 'height')
                row = box.row()
                row.prop(myobjdat, 'wf')
                row = box.row()
                row.prop(myobjdat, 'r')

                row = box.row()
                row.prop(myobjdat, 'external')
                row.prop(myobjdat, 'handle')
                if myobjdat.external:
                    row.prop(myobjdat, 'frame')

                if myobjdat.opentype != "1":
                    row = box.row()
                    row.prop(myobjdat, 'frame_L')
                    row.prop(myobjdat, 'leafratio', slider=True)

                box = layout.box()
                row = box.row()
                row.prop(myobjdat, 'sill')
                if myobjdat.sill:
                    row = box.row()
                    row.prop(myobjdat, 'sill_thickness')
                    row.prop(myobjdat, 'sill_back')
                    row.prop(myobjdat, 'sill_front')

                box = layout.box()
                row = box.row()
                row.prop(myobjdat, 'blind')
                if myobjdat.blind:
                    row = box.row()
                    row.prop(myobjdat, 'blind_rail')
                    row.prop(myobjdat, 'blind_ratio', slider=True)
                    row = box.row()
                    row.prop(myobjdat, 'blind_box')
                    if myobjdat.blind_box:
                        row.prop(myobjdat, 'blind_height')
                        row.prop(myobjdat, 'blind_back')

                box = layout.box()
                if not context.scene.render.engine == 'CYCLES':
                    box.enabled = False
                box.prop(myobjdat, 'crt_mat')
            else:
                row = layout.row()
                row.label("Warning: Operator does not work in local view mode", icon='ERROR')


# ------------------------------------------------------------------------------
# Generate rail windows
# ------------------------------------------------------------------------------
def generate_rail_window(myframe, mp, mymesh):
    myloc = bpy.context.scene.cursor_location

    alummat = None
    if mp.crt_mat and bpy.context.scene.render.engine == 'CYCLES':
        alummat = create_diffuse_material("Window_material", False, 0.8, 0.8, 0.8, 0.6, 0.6, 0.6, 0.15)

    # Frame
    win_size, p1, p2 = create_rail_window_frame(myframe, mymesh,
                                                mp.width, mp.depth, mp.height,
                                                mp.frame,
                                                mp.crt_mat, alummat, mp.external,
                                                mp.blind and mp.blind_box,
                                                mp.blind_height,
                                                mp.blind_back,
                                                mp.blind_rail)

    remove_doubles(myframe)
    set_normals(myframe)

    # Window L
    width = (mp.width / 2) + 0.01
    mywin_l = create_rail_window_leaf("Window.L", "L",
                                      width, win_size, mp.height - 0.05,
                                      mp.wf,
                                      myloc.x, myloc.y, myloc.z,
                                      mp.crt_mat, alummat, mp.handle)

    remove_doubles(mywin_l)
    set_normals(mywin_l)

    mywin_l.parent = myframe
    mywin_l.location.x = (-mp.width / 2) + 0.01
    mywin_l.location.y = p1 - 0.001
    mywin_l.location.z = 0.025
    # Window R
    mywin_r = create_rail_window_leaf("Window.R", "R",
                                      width, win_size, mp.height - 0.05,
                                      mp.wf,
                                      myloc.x, myloc.y, myloc.z,
                                      mp.crt_mat, alummat, mp.handle)

    remove_doubles(mywin_r)
    set_normals(mywin_r)

    mywin_r.parent = myframe
    mywin_r.location.x = (mp.width / 2) - 0.01
    mywin_r.location.y = p2 - 0.001
    mywin_r.location.z = 0.025
    # Sill
    if mp.sill:
        mysill = create_sill("Windows_Sill", mp.width,
                             mp.depth + mp.sill_back + mp.sill_front,
                             mp.sill_thickness, mp.crt_mat)
        mysill.parent = myframe
        mysill.location.x = 0
        mysill.location.y = -mp.depth - mp.sill_back
        mysill.location.z = 0

    # Blind
    if mp.blind:
        myblindrail = create_blind_rail("Blind_rails", mp.width, mp.height,
                                        myloc.x, myloc.y, myloc.z,
                                        mp.crt_mat, alummat, mp.blind_rail)
        set_normals(myblindrail)

        myblindrail.parent = myframe
        myblindrail.location.x = 0
        myblindrail.location.y = 0
        myblindrail.location.z = 0
        # Lock
        myblindrail.lock_location = (True, True, True)
        myblindrail.lock_rotation = (True, True, True)

        myblind = create_blind("Blind", mp.width - 0.006, mp.height,
                               myloc.x, myloc.y, myloc.z,
                               mp.crt_mat, mp.blind_ratio)
        set_normals(myblind)

        myblind.parent = myframe
        myblind.location.x = 0
        myblind.location.y = mp.blind_rail - 0.014
        myblind.location.z = mp.height - 0.098
        # Lock
        myblind.lock_location = (True, True, True)
        myblind.lock_rotation = (True, True, True)


# ------------------------------------------------------------------------------
# Generate leaf windows
# ------------------------------------------------------------------------------
def generate_leaf_window(myframe, mp, mymesh):
    myloc = bpy.context.scene.cursor_location

    alummat = None
    if mp.crt_mat and bpy.context.scene.render.engine == 'CYCLES':
        alummat = create_diffuse_material("Window_material", False, 0.8, 0.8, 0.8, 0.6, 0.6, 0.6, 0.15)

    # Frame
    win_size = create_leaf_window_frame(myframe, mymesh,
                                        mp.width, mp.depth, mp.height,
                                        mp.frame, mp.frame_L, mp.leafratio,
                                        mp.crt_mat, alummat, mp.external,
                                        mp.blind and mp.blind_box, mp.blind_height, mp.blind_back,
                                        mp.blind_rail)

    remove_doubles(myframe)
    set_normals(myframe)

    stepsize = 0.01
    # -----------------------------
    # Window L
    # -----------------------------
    if mp.opentype == "2" or mp.opentype == "4":
        handle = mp.handle
        if mp.opentype == "2":
            width = ((mp.width - (mp.frame_L * 2) + stepsize) / 2) + 0.004
            handle = False  # two windows only one handle
        else:
            width = mp.width - (mp.frame_L * 2) + stepsize + 0.008

        mywin_l = create_leaf_window_leaf("Window.L", "L",
                                          width, win_size, mp.height - (mp.frame_L * 2) + (stepsize * 2) - 0.004,
                                          mp.wf,
                                          myloc.x, myloc.y, myloc.z,
                                          mp.crt_mat, alummat, handle)

        remove_doubles(mywin_l)
        set_normals(mywin_l)

        mywin_l.parent = myframe
        mywin_l.location.x = -mp.width / 2 + mp.frame_L - stepsize + 0.001
        mywin_l.location.y = -mp.depth
        mywin_l.location.z = mp.frame_L - (stepsize / 2) - 0.003
    # -----------------------------
    # Window R
    # -----------------------------
    if mp.opentype == "2" or mp.opentype == "3":
        if mp.opentype == "2":
            width = ((mp.width - (mp.frame_L * 2) + stepsize) / 2) + 0.003
        else:
            width = mp.width - (mp.frame_L * 2) + stepsize + 0.008

        mywin_r = create_leaf_window_leaf("Window.R", "R",
                                          width, win_size, mp.height - (mp.frame_L * 2) + (stepsize * 2) - 0.004,
                                          mp.wf,
                                          myloc.x, myloc.y, myloc.z,
                                          mp.crt_mat, alummat, mp.handle)

        remove_doubles(mywin_r)
        set_normals(mywin_r)

        mywin_r.parent = myframe
        mywin_r.location.x = mp.width / 2 - mp.frame_L + stepsize - 0.001
        mywin_r.location.y = -mp.depth
        mywin_r.location.z = mp.frame_L - (stepsize / 2) - 0.003

    # Sill
    if mp.sill:
        mysill = create_sill("Windows_Sill", mp.width,
                             mp.depth + mp.sill_back + mp.sill_front,
                             mp.sill_thickness, mp.crt_mat)
        mysill.parent = myframe
        mysill.location.x = 0
        mysill.location.y = -mp.depth - mp.sill_back
        mysill.location.z = 0

    # Blind
    if mp.blind:
        myblindrail = create_blind_rail("Blind_rails", mp.width, mp.height,
                                        myloc.x, myloc.y, myloc.z,
                                        mp.crt_mat, alummat, mp.blind_rail)
        myblindrail.parent = myframe
        myblindrail.location.x = 0
        myblindrail.location.y = 0
        myblindrail.location.z = 0
        # Lock
        myblindrail.lock_location = (True, True, True)
        myblindrail.lock_rotation = (True, True, True)

        myblind = create_blind("Blind", mp.width - 0.006, mp.height,
                               myloc.x, myloc.y, myloc.z,
                               mp.crt_mat, mp.blind_ratio)
        myblind.parent = myframe
        myblind.location.x = 0
        myblind.location.y = mp.blind_rail - 0.014
        myblind.location.z = mp.height - 0.098

        # Lock
        myblind.lock_location = (True, True, True)
        myblind.lock_rotation = (True, True, True)

    # deactivate others
    for o in bpy.data.objects:
        if o.select is True:
            o.select = False

    myframe.select = True
    bpy.context.scene.objects.active = myframe

    return myframe


# ------------------------------------------------------------------------------
# Create windows frame
#
# sX: Size in X axis
# sY: Size in Y axis
# sZ: Size in Z axis
# frame: size of external frame
# mat: Flag for creating materials
# matdata: Aluminum material
# external: create external frame flag
# blind: blind flag
# blind_height: height of blind box
# blind_back: front extension
# blind_rail: distance of the rail
# ------------------------------------------------------------------------------
def create_rail_window_frame(mywindow, mymesh, sx, sy, sz, frame, mat, matdata, external,
                             blind, blind_height, blind_back, blind_rail):
    myvertex = []
    myfaces = []
    v = 0
    # ===========================================================================
    # Horizontal pieces
    # ===========================================================================
    m = 0.02  # gap in front
    gap = 0.001  # gap between leafs
    rail = 0.007  # rail width
    thick = 0.002  # aluminum thickness
    w = (sy - m - gap - thick - thick - thick) / 2  # width of each leaf
    p = (w - rail) / 2  # space of each side
    side = 0.02  # vertical
    for z in (0, sz):
        for x in (-sx / 2, sx / 2):
            myvertex.extend([(x, 0, z), (x, 0, z + side),
                             (x, -m - p, z + side),
                             (x, -m - p, z + (side * 2)),  # rail 1
                             (x, -m - p - rail, z + (side * 2)),
                             (x, -m - p - rail, z + side),
                             (x, -m - p - rail - thick - p - gap - p, z + side),
                             (x, -m - p - rail - thick - p - gap - p, z + (side * 2)),  # rail 2
                             (x, -m - p - rail - thick - p - gap - p - rail, z + (side * 2)),
                             (x, -m - p - rail - thick - p - gap - p - rail, z + side),
                             (x, -m - p - rail - thick - p - gap - p - rail - p - thick, z + side)])
        # Faces
        myfaces.extend([(v + 12, v + 1, v + 0, v + 11), (v + 13, v + 2, v + 1, v + 12), (v + 14, v + 3, v + 2, v + 13),
                        (v + 15, v + 4, v + 3, v + 14), (v + 16, v + 5, v + 4, v + 15), (v + 17, v + 6, v + 5, v + 16),
                        (v + 18, v + 7, v + 6, v + 17), (v + 19, v + 8, v + 7, v + 18), (v + 20, v + 9, v + 8, v + 19),
                        (v + 20, v + 21, v + 10, v + 9)])

        side *= -1  # reveser direction
        v = len(myvertex)
    # ===========================================================================
    # Vertical pieces
    # ===========================================================================
    y = 0
    sideb = 0.03
    sides = 0.02
    thickb = 0.002  # aluminum thickness
    win_size = p + rail + p
    p1 = y - m - thick
    p2 = y - m - thick - gap - p - rail - p - thick

    # Left
    for x in (-sx / 2, sx / 2):
        for z in (0, sz):
            myvertex.extend([(x, y, z),
                             (x + thickb, y, z),
                             (x + thickb, y - m, z),
                             (x + thickb + sides, y - m, z),
                             (x + thickb + sides, y - m - thick, z),
                             (x + thickb, y - m - thick, z),
                             (x + thickb, y - m - thick - gap - p - rail - p, z),
                             (x + thickb + sides, y - m - thick - gap - p - rail - p, z),
                             (x + thickb + sides, y - m - thick - gap - p - rail - p - thick, z),
                             (x + thickb, y - m - thick - gap - p - rail - p - thick, z),
                             (x + thickb, y - m - thick - gap - p - rail - p - thick - p - rail - p, z),
                             (x + thickb + sideb, y - m - thick - gap - p - rail - p - thick - p - rail - p, z),
                             (x + thickb + sideb, y - m - thick - gap - p - rail - p - thick - p - rail - p - thick, z),
                             (x, y - m - thick - gap - p - rail - p - thick - p - rail - p - thick, z)])
        # Faces
        myfaces.extend([(v + 13, v + 27, v + 14, v + 0), (v + 13, v + 12, v + 26, v + 27),
                        (v + 11, v + 10, v + 24, v + 25),
                        (v + 6, v + 5, v + 19, v + 20), (v + 10, v + 9, v + 23, v + 24),
                        (v + 25, v + 24, v + 27, v + 26), (v + 24, v + 23, v + 15, v + 16),
                        (v + 22, v + 21, v + 20, v + 23),
                        (v + 17, v + 16, v + 19, v + 18), (v + 9, v + 8, v + 22, v + 23),
                        (v + 7, v + 6, v + 20, v + 21), (v + 3, v + 2, v + 16, v + 17),
                        (v + 5, v + 4, v + 18, v + 19),
                        (v + 4, v + 3, v + 17, v + 18), (v + 7, v + 8, v + 9, v + 6),
                        (v + 3, v + 4, v + 5, v + 2), (v + 11, v + 12, v + 13, v + 10),
                        (v + 6, v + 5, v + 9, v + 10),
                        (v + 1, v + 0, v + 14, v + 15),
                        (v + 19, v + 16, v + 15, v + 14, v + 27, v + 24, v + 23, v + 20),
                        (v + 8, v + 7, v + 21, v + 22), (v + 12, v + 11, v + 25, v + 26),
                        (v + 2, v + 1, v + 15, v + 16),
                        (v + 5, v + 6, v + 9, v + 10, v + 13, v + 0, v + 1, v + 2)])

        v = len(myvertex)
        # reverse
        thickb *= -1
        sideb *= -1
        sides *= -1
    # ===========================================================================
    # Front covers
    # ===========================================================================
    x = sx - 0.005 - (sideb * 2)  # sideB + small gap
    y = y - m - thick - gap - p - rail - p - thick - p - rail - p
    z = sideb
    # Bottom
    myvertex.extend([(-x / 2, y - thick, 0.0),
                     (-x / 2, y, 0.0),
                     (x / 2, y, 0.0),
                     (x / 2, y - thick, 0.0),
                     (-x / 2, y - thick, z),
                     (-x / 2, y, z),
                     (x / 2, y, z),
                     (x / 2, y - thick, z)])

    myfaces.extend([(v + 0, v + 1, v + 2, v + 3), (v + 0, v + 1, v + 5, v + 4), (v + 1, v + 2, v + 6, v + 5),
                    (v + 2, v + 6, v + 7, v + 3), (v + 5, v + 6, v + 7, v + 4), (v + 0, v + 4, v + 7, v + 3)])
    v = len(myvertex)
    # Top
    myvertex.extend([(-x / 2, y - thick, sz - sideb),
                     (-x / 2, y, sz - sideb),
                     (x / 2, y, sz - sideb),
                     (x / 2, y - thick, sz - sideb),
                     (-x / 2, y - thick, sz - sideb + z),
                     (-x / 2, y, sz - sideb + z),
                     (x / 2, y, sz - sideb + z),
                     (x / 2, y - thick, sz - sideb + z)])

    myfaces.extend([(v + 0, v + 1, v + 2, v + 3), (v + 0, v + 1, v + 5, v + 4), (v + 1, v + 2, v + 6, v + 5),
                    (v + 2, v + 6, v + 7, v + 3), (v + 5, v + 6, v + 7, v + 4), (v + 0, v + 4, v + 7, v + 3)])
    v = len(myvertex)
    # ===========================================================================
    # External front covers
    # ===========================================================================
    if external:
        x = sx
        gap = -0.001
        sidem = frame
        box = 0
        if blind:
            box = blind_height

        myvertex.extend([((-x / 2) - sidem, y - thick, sz + sidem + box),
                         ((x / 2) + sidem, y - thick, sz + sidem + box),
                         ((-x / 2) - sidem, y - thick, -sidem),
                         ((x / 2) + sidem, y - thick, -sidem),
                         ((-x / 2) - gap, y - thick, sz + gap + box),
                         ((x / 2) + gap, y - thick, sz + gap + box),
                         ((-x / 2) - gap, y - thick, -gap),
                         ((x / 2) + gap, y - thick, -gap)])
        myvertex.extend([((-x / 2) - sidem, y - thick * 2, sz + sidem + box),
                         ((x / 2) + sidem, y - thick * 2, sz + sidem + box),
                         ((-x / 2) - sidem, y - thick * 2, -sidem),
                         ((x / 2) + sidem, y - thick * 2, -sidem),
                         ((-x / 2) - gap, y - thick * 2, sz + gap + box),
                         ((x / 2) + gap, y - thick * 2, sz + gap + box),
                         ((-x / 2) - gap, y - thick * 2, -gap),
                         ((x / 2) + gap, y - thick * 2, -gap)])

        myfaces.extend([(v + 3, v + 1, v + 9, v + 11), (v + 9, v + 8, v + 0, v + 1),
                        (v + 1, v + 5, v + 4, v + 0),
                        (v + 3, v + 7, v + 5, v + 1),
                        (v + 7, v + 3, v + 2, v + 6),
                        (v + 0, v + 4, v + 6, v + 2),
                        (v + 9, v + 13, v + 12, v + 8), (v + 11, v + 15, v + 13, v + 9),
                        (v + 15, v + 11, v + 10, v + 14),
                        (v + 8, v + 12, v + 14, v + 10),
                        (v + 11, v + 3, v + 2, v + 10),
                        (v + 2, v + 10, v + 8, v + 0), (v + 14, v + 12, v + 4, v + 6),
                        (v + 7, v + 6, v + 14, v + 15),
                        (v + 5, v + 13, v + 12, v + 4),
                        (v + 15, v + 7, v + 5, v + 13)])

    mymesh.from_pydata(myvertex, [], myfaces)
    mymesh.update(calc_edges=True)

    if mat and bpy.context.scene.render.engine == 'CYCLES':
        set_material(mywindow, matdata)
    # --------------
    # Blind Box
    # --------------
    if blind:
        mybox = create_blind_box("Blind_box", sx, sy + blind_back + blind_rail, blind_height)
        set_normals(mybox)

        mybox.parent = mywindow
        mybox.location.x = 0
        mybox.location.y = -blind_back - sy
        mybox.location.z = sz
        if mat and bpy.context.scene.render.engine == 'CYCLES':
            set_material(mybox, matdata)
        # Lock
        mybox.lock_location = (True, True, True)
        mybox.lock_rotation = (True, True, True)

    return win_size, p1, p2


# ------------------------------------------------------------------------------
# Create leafs windows frame
#
# sX: Size in X axis
# sY: Size in Y axis
# sZ: Size in Z axis
# frame: size of external frame
# frame_L: size of main frame
# leafratio: ratio of leaf depth
# mat: Flag for creating materials
# matdata: Aluminum material
# external: create external frame flag
# blind: blind flag
# blind_height: height of blind box
# blind_back: front extension
# blind_rail: distance of the rail
# ------------------------------------------------------------------------------
def create_leaf_window_frame(mywindow, mymesh, sx, sy, sz, frame, frame_l, leafratio, mat, matdata, external,
                             blind, blind_height, blind_back, blind_rail):
    myvertex = []
    myfaces = []
    # ===========================================================================
    # Main frame_L
    # ===========================================================================
    x = sx / 2
    z = sz
    y = sy * leafratio
    gap = 0.01
    size = sy - y - 0.001  # thickness of the leaf

    myvertex.extend([(-x, 0, 0),
                     (-x, 0, z),
                     (x, 0, z),
                     (x, 0, 0),
                     (-x + frame_l, 0, frame_l),
                     (-x + frame_l, 0, z - frame_l),
                     (x - frame_l, 0, z - frame_l),
                     (x - frame_l, 0, frame_l),
                     (-x + frame_l, -y, frame_l),
                     (-x + frame_l, -y, z - frame_l),
                     (x - frame_l, -y, z - frame_l),
                     (x - frame_l, -y, frame_l),
                     (-x + frame_l - gap, -y, frame_l - gap),
                     (-x + frame_l - gap, -y, z - frame_l + gap),
                     (x - frame_l + gap, -y, z - frame_l + gap),
                     (x - frame_l + gap, -y, frame_l - gap),
                     (-x + frame_l - gap, -sy, frame_l - gap),
                     (-x + frame_l - gap, -sy, z - frame_l + gap),
                     (x - frame_l + gap, -sy, z - frame_l + gap),
                     (x - frame_l + gap, -sy, frame_l - gap),
                     (-x, -sy, 0),
                     (-x, -sy, z),
                     (x, -sy, z),
                     (x, -sy, 0)])
    # Faces
    myfaces.extend([(1, 5, 4, 0), (21, 1, 0, 20), (17, 21, 20, 16), (16, 12, 13, 17), (12, 8, 9, 13),
                    (5, 9, 8, 4), (3, 7, 6, 2), (23, 3, 2, 22), (19, 23, 22, 18), (15, 19, 18, 14),
                    (11, 15, 14, 10), (6, 7, 11, 10), (0, 3, 23, 20), (21, 22, 2, 1), (17, 13, 14, 18),
                    (21, 17, 18, 22), (13, 9, 10, 14), (8, 11, 7, 4), (8, 12, 15, 11), (4, 7, 3, 0),
                    (12, 16, 19, 15), (16, 20, 23, 19), (9, 5, 6, 10), (1, 2, 6, 5)])

    v = len(myvertex)
    # ===========================================================================
    # External front covers
    # ===========================================================================
    if external:
        thick = 0.002  # aluminum thickness
        x = sx
        gap = -0.001
        sidem = frame
        box = 0
        if blind:
            box = blind_height

        myvertex.extend([((-x / 2) - sidem, -sy, sz + sidem + box),
                         ((x / 2) + sidem, -sy, sz + sidem + box),
                         ((-x / 2) - sidem, -sy, -sidem),
                         ((x / 2) + sidem, -sy, -sidem),
                         ((-x / 2) - gap, -sy, sz + gap + box),
                         ((x / 2) + gap, -sy, sz + gap + box),
                         ((-x / 2) - gap, -sy, -gap),
                         ((x / 2) + gap, -sy, -gap)])
        myvertex.extend([((-x / 2) - sidem, -sy - thick, sz + sidem + box),
                         ((x / 2) + sidem, -sy - thick, sz + sidem + box),
                         ((-x / 2) - sidem, -sy - thick, -sidem),
                         ((x / 2) + sidem, -sy - thick, -sidem),
                         ((-x / 2) - gap, -sy - thick, sz + gap + box),
                         ((x / 2) + gap, -sy - thick, sz + gap + box),
                         ((-x / 2) - gap, -sy - thick, -gap),
                         ((x / 2) + gap, -sy - thick, -gap)])

        myfaces.extend([(v + 3, v + 1, v + 9, v + 11), (v + 9, v + 8, v + 0, v + 1), (v + 1, v + 5, v + 4, v + 0),
                        (v + 3, v + 7, v + 5, v + 1), (v + 7, v + 3, v + 2, v + 6),
                        (v + 0, v + 4, v + 6, v + 2), (v + 9, v + 13, v + 12, v + 8), (v + 11, v + 15, v + 13, v + 9),
                        (v + 15, v + 11, v + 10, v + 14), (v + 8, v + 12, v + 14, v + 10),
                        (v + 11, v + 3, v + 2, v + 10), (v + 2, v + 10, v + 8, v + 0), (v + 14, v + 12, v + 4, v + 6),
                        (v + 7, v + 6, v + 14, v + 15), (v + 5, v + 13, v + 12, v + 4),
                        (v + 15, v + 7, v + 5, v + 13)])

    mymesh.from_pydata(myvertex, [], myfaces)
    mymesh.update(calc_edges=True)

    if mat and bpy.context.scene.render.engine == 'CYCLES':
        set_material(mywindow, matdata)

    # --------------
    # Blind Box
    # --------------
    if blind:
        mybox = create_blind_box("Blind_box", sx, sy + blind_back + blind_rail, blind_height)
        set_normals(mybox)

        mybox.parent = mywindow
        mybox.location.x = 0
        mybox.location.y = -blind_back - sy
        mybox.location.z = sz
        if mat and bpy.context.scene.render.engine == 'CYCLES':
            set_material(mybox, matdata)
        # Lock
        mybox.lock_location = (True, True, True)
        mybox.lock_rotation = (True, True, True)

    return size


# ------------------------------------------------------------------------------
# Create rail windows leaf
#
# objName: Name for the new object
# hand: Left or Right
# sX: Size in X axis
# sY: Size in Y axis
# sZ: Size in Z axis
# f: size of the frame_L
# pX: position X axis
# pY: position Y axis
# pZ: position Z axis
# mat: Flag for creating materials
# matdata: default material
# handle: create handle flag
# ------------------------------------------------------------------------------
def create_rail_window_leaf(objname, hand, sx, sy, sz, f, px, py, pz, mat, matdata, handle):
    myvertex = []
    myfaces = []
    v = 0
    # ===========================================================================
    # Horizontal pieces
    # ===========================================================================
    rail = 0.010  # rail width
    t = sy - 0.002
    p = ((t - rail) / 2) - 0.002
    side = 0.02  # vertical rail

    x = sx
    z = sz
    fz = f
    if hand == "R":
        x *= -1
        f *= -1
    # ------------------------
    # frame
    # ------------------------
    myvertex.extend([(0, 0, 0),
                     (0, 0, z),
                     (x, 0, z),
                     (x, 0, 0),
                     (f, 0, fz),
                     (f, 0, z - fz),
                     (x - f, 0, z - fz),
                     (x - f, 0, fz),
                     (f, -t / 2, fz),
                     (f, -t / 2, z - fz),
                     (x - f, -t / 2, z - fz),
                     (x - f, -t / 2, fz),
                     (f, -t, fz),
                     (f, -t, z - fz),
                     (x - f, -t, z - fz),
                     (x - f, -t, fz),
                     (0, -t, 0),
                     (0, -t, z),
                     (x, -t, z),
                     (x, -t, 0)])
    # ------------------------
    # Side rails
    # ------------------------
    for z in (0, sz):
        myvertex.extend([(0, -p, z),
                         (x, -p, z),
                         (0, -p, z + side),
                         (x, -p, z + side),
                         (0, -p - rail, z + side),
                         (x, -p - rail, z + side),
                         (0, -p - rail, z),
                         (x, -p - rail, z)])
        side *= -1  # reverse
        # Faces
        myfaces.extend([(v + 10, v + 6, v + 7, v + 11), (v + 9, v + 8, v + 4, v + 5),
                        (v + 13, v + 12, v + 8, v + 9), (v + 14, v + 10, v + 11, v + 15),
                        (v + 6, v + 10, v + 9, v + 5),
                        (v + 9, v + 10, v + 14, v + 13), (v + 11, v + 7, v + 4, v + 8), (v + 12, v + 15, v + 11, v + 8),
                        (v + 3, v + 7, v + 6, v + 2),
                        (v + 5, v + 4, v + 0, v + 1),
                        (v + 4, v + 7, v + 3, v + 0), (v + 5, v + 1, v + 2, v + 6), (v + 17, v + 16, v + 12, v + 13),
                        (v + 15, v + 19, v + 18, v + 14),
                        (v + 15, v + 12, v + 16, v + 19),
                        (v + 14, v + 18, v + 17, v + 13), (v + 29, v + 2, v + 1, v + 28),
                        (v + 35, v + 34, v + 17, v + 18), (v + 35, v + 33, v + 32, v + 34),
                        (v + 31, v + 29, v + 28, v + 30),
                        (v + 33, v + 31, v + 30, v + 32), (v + 25, v + 24, v + 22, v + 23),
                        (v + 19, v + 16, v + 26, v + 27),
                        (v + 3, v + 21, v + 20, v + 0), (v + 25, v + 27, v + 26, v + 24),
                        (v + 23, v + 22, v + 20, v + 21), (v + 3, v + 2, v + 29, v + 21),
                        (v + 19, v + 27, v + 35, v + 18),
                        (v + 31, v + 33, v + 25, v + 23), (v + 32, v + 30, v + 22, v + 24),
                        (v + 16, v + 17, v + 34, v + 26), (v + 0, v + 20, v + 28, v + 1)])
        # Glass
        if z == 0:
            myfaces.extend([(v + 10, v + 9, v + 8, v + 11)])

    v = len(myvertex)

    # Faces (last glass)
    # ------------------------
    # Plastic parts
    # ------------------------
    ps = -0.004
    gap = -0.0002
    space = 0.005

    if hand == "R":
        ps *= -1
        gap *= -1
    for z in (0, sz):
        for x in (0, sx):

            if hand == "R":
                x *= -1
            myvertex.extend([(x + gap, -p, z),
                             (x + ps, -p, z),
                             (x + gap, -p, z + side),
                             (x + ps, -p, z + side),
                             (x + gap, -p - rail, z + side),
                             (x + ps, -p - rail, z + side),
                             (x + gap, -p - rail, z),
                             (x + ps, -p - rail, z),
                             (x + gap, -p + rail - space, z),
                             (x + gap, -p - rail - space, z),
                             (x + gap, -p + rail - space, z + (side * 1.5)),
                             (x + gap, -p - rail - space, z + (side * 1.5)),
                             (x + ps, -p + rail - space, z),
                             (x + ps, -p - rail - space, z),
                             (x + ps, -p + rail - space, z + (side * 1.5)),
                             (x + ps, -p - rail - space, z + (side * 1.5))])
            myfaces.extend([(v + 12, v + 8, v + 10, v + 14), (v + 6, v + 7, v + 5, v + 4), (v + 1, v + 0, v + 2, v + 3),
                            (v + 5, v + 3, v + 2, v + 4), (v + 8, v + 12, v + 1, v + 0),
                            (v + 7, v + 6, v + 9, v + 13), (v + 13, v + 9, v + 11, v + 15),
                            (v + 14, v + 10, v + 11, v + 15),
                            (v + 10, v + 8, v + 0, v + 2, v + 4, v + 6, v + 9, v + 11),
                            (v + 12, v + 14, v + 15, v + 13, v + 7, v + 5, v + 3, v + 1)])

            v = len(myvertex)
            ps *= -1
            gap *= -1

        side *= -1  # reverse vertical

    mymesh = bpy.data.meshes.new(objname)
    mywindow = bpy.data.objects.new(objname, mymesh)

    mywindow.location[0] = px
    mywindow.location[1] = py
    mywindow.location[2] = pz
    bpy.context.scene.objects.link(mywindow)

    mymesh.from_pydata(myvertex, [], myfaces)
    mymesh.update(calc_edges=True)

    set_normals(mywindow)

    # Lock transformation
    mywindow.lock_location = (False, True, True)  # only X axis
    mywindow.lock_rotation = (True, True, True)

    # Handle
    if handle:
        myhandle = create_rail_handle("Handle", mat)
        myhandle.parent = mywindow
        if hand == "R":
            myhandle.location.x = -0.035
        else:
            myhandle.location.x = +0.035

        myhandle.location.y = -sy + 0.001
        if sz / 2 <= 1:
            myhandle.location.z = sz / 2
        else:
            myhandle.location.z = 1

    if mat and bpy.context.scene.render.engine == 'CYCLES':
        set_material(mywindow, matdata)
        # Glass
        glass = create_glass_material("Glass_material", False)
        mywindow.data.materials.append(glass)
        select_faces(mywindow, 32, True)
        set_material_faces(mywindow, 1)
        # Plastic
        plastic = create_diffuse_material("Plastic_material", False, 0.01, 0.01, 0.01, 0.1, 0.1, 0.1, 0.01)
        mywindow.data.materials.append(plastic)
        select_faces(mywindow, 65, True)
        for fa in range(66, 104):
            select_faces(mywindow, fa, False)

        set_material_faces(mywindow, 2)

    return mywindow


# ------------------------------------------------------------------------------
# Create leaf windows leaf
#
# objName: Name for the new object
# hand: Left or Right
# sX: Size in X axis
# sY: Size in Y axis
# sZ: Size in Z axis
# f: size of the frame_L
# pX: position X axis
# pY: position Y axis
# pZ: position Z axis
# mat: Flag for creating materials
# matdata: default material
# handle: include handle
# ------------------------------------------------------------------------------
def create_leaf_window_leaf(objname, hand, sx, sy, sz, f, px, py, pz, mat, matdata, handle):
    myvertex = []
    myfaces = []
    x = sx
    z = sz
    fz = f
    t = sy
    if hand == "R":
        x *= -1
        f *= -1
    # ------------------------
    # frame
    # ------------------------
    myvertex.extend([(0, 0, 0),
                     (0, 0, z),
                     (x, 0, z),
                     (x, 0, 0),
                     (f, 0, fz),
                     (f, 0, z - fz),
                     (x - f, 0, z - fz),
                     (x - f, 0, fz),
                     (f, t / 2, fz),
                     (f, t / 2, z - fz),
                     (x - f, t / 2, z - fz),
                     (x - f, t / 2, fz),
                     (f, t, fz),
                     (f, t, z - fz),
                     (x - f, t, z - fz),
                     (x - f, t, fz),
                     (0, t, 0),
                     (0, t, z),
                     (x, t, z),
                     (x, t, 0)])
    # Faces
    myfaces.extend([(13, 14, 10, 9), (10, 6, 5, 9), (11, 7, 4, 8), (12, 15, 11, 8), (13, 9, 8, 12),
                    (9, 5, 4, 8), (10, 14, 15, 11), (6, 10, 11, 7), (19, 3, 2, 18), (17, 1, 0, 16),
                    (2, 1, 17, 18), (19, 16, 0, 3), (13, 17, 18, 14), (15, 14, 18, 19), (13, 12, 16, 17),
                    (12, 16, 19, 15), (6, 7, 3, 2), (4, 5, 1, 0), (5, 6, 2, 1), (4, 7, 3, 0),
                    (10, 9, 8, 11)])

    mymesh = bpy.data.meshes.new(objname)
    mywindow = bpy.data.objects.new(objname, mymesh)

    mywindow.location[0] = px
    mywindow.location[1] = py
    mywindow.location[2] = pz
    bpy.context.scene.objects.link(mywindow)

    mymesh.from_pydata(myvertex, [], myfaces)
    mymesh.update(calc_edges=True)

    set_normals(mywindow)

    # Lock transformation
    mywindow.lock_location = (True, True, True)
    mywindow.lock_rotation = (True, True, False)

    if handle:
        myhandle = create_leaf_handle("Handle", mat)
        if hand == "L":
            myhandle.rotation_euler = (0, pi, 0)

        myhandle.parent = mywindow
        if hand == "R":
            myhandle.location.x = -sx + 0.025
        else:
            myhandle.location.x = sx - 0.025

        myhandle.location.y = 0
        if sz / 2 <= 1:
            myhandle.location.z = sz / 2
        else:
            myhandle.location.z = 1

        set_smooth(myhandle)
        set_modifier_subsurf(myhandle)

    if mat and bpy.context.scene.render.engine == 'CYCLES':
        set_material(mywindow, matdata)
        # Glass
        glass = create_glass_material("Glass_material", False)
        mywindow.data.materials.append(glass)
        select_faces(mywindow, 20, True)
        set_material_faces(mywindow, 1)
    return mywindow


# ------------------------------------------------------------
# Generate Leaf handle
#
# objName: Object name
# mat: create materials
# ------------------------------------------------------------
def create_leaf_handle(objname, mat):
    # ------------------------------------
    # Mesh data
    # ------------------------------------
    minx = -0.018522918224334717
    maxx = 0.10613098740577698
    miny = -0.04866280406713486
    maxy = 0.0002815350890159607
    minz = -0.06269633769989014
    maxz = 0.06289216876029968

    # Vertex
    myvertex = [(minx + 0.00752672553062439, maxy - 0.0176689475774765, minz + 0.0503292977809906),
                (minx + 0.002989441156387329, maxy - 0.017728276550769806, minz + 0.057490378618240356),
                (minx + 0.002640664577484131, maxy - 0.01777590811252594, maxz - 0.05962774157524109),
                (minx + 0.006573766469955444, maxy - 0.017799079418182373, maxz - 0.05211767554283142),
                (minx + 0.013735026121139526, maxy - 0.01779157668352127, maxz - 0.04758024215698242),
                (minx + 0.0222054123878479, maxy - 0.017755411565303802, maxz - 0.04723122715950012),
                (minx + 0.02971544861793518, maxy - 0.01770026981830597, maxz - 0.0511641800403595),
                (minx + 0.03425273299217224, maxy - 0.017640933394432068, maxz - 0.058325231075286865),
                (minx + 0.03460153937339783, maxy - 0.01759330928325653, minz + 0.05879288911819458),
                (minx + 0.03066837787628174, maxy - 0.017570137977600098, minz + 0.051282793283462524),
                (minx + 0.02350717782974243, maxy - 0.017577648162841797, minz + 0.046745359897613525),
                (minx + 0.01503676176071167, maxy - 0.017613813281059265, minz + 0.046396344900131226),
                (minx + 0.007489442825317383, maxy - 0.009374044835567474, minz + 0.05037441849708557),
                (minx + 0.00295218825340271, maxy - 0.009433373808860779, minz + 0.05753546953201294),
                (minx + 0.0026033520698547363, maxy - 0.009481005370616913, maxz - 0.05958262085914612),
                (minx + 0.006536513566970825, maxy - 0.009206198155879974, maxz - 0.05207255482673645),
                (minx + 0.013697713613510132, maxy - 0.009206198155879974, maxz - 0.04753512144088745),
                (minx + 0.029678165912628174, maxy - 0.009206198155879974, maxz - 0.051119059324264526),
                (minx + 0.034215450286865234, maxy - 0.009206198155879974, maxz - 0.05828014016151428),
                (minx + 0.03456425666809082, maxy - 0.009298399090766907, minz + 0.05883798003196716),
                (minx + 0.03063112497329712, maxy - 0.00927523523569107, minz + 0.051327913999557495),
                (minx + 0.023469924926757812, maxy - 0.009282737970352173, minz + 0.046790480613708496),
                (minx + 0.014999479055404663, maxy - 0.009318903088569641, minz + 0.046441465616226196),
                (minx + 0.009239286184310913, maxy - 0.017671361565589905, minz + 0.052188992500305176),
                (minx + 0.00540238618850708, maxy - 0.017721541225910187, minz + 0.058244675397872925),
                (minx + 0.005107402801513672, maxy - 0.017761819064617157, maxz - 0.06018096208572388),
                (minx + 0.00843346118927002, maxy - 0.01778140664100647, maxz - 0.05383017659187317),
                (minx + 0.014489203691482544, maxy - 0.017775066196918488, maxz - 0.049993157386779785),
                (minx + 0.021652132272720337, maxy - 0.017744481563568115, maxz - 0.04969802498817444),
                (minx + 0.028002887964248657, maxy - 0.01769784837961197, maxz - 0.053023844957351685),
                (minx + 0.03183978796005249, maxy - 0.01764768362045288, maxz - 0.059079527854919434),
                (minx + 0.03213474154472351, maxy - 0.01760740578174591, minz + 0.05934610962867737),
                (minx + 0.02880874276161194, maxy - 0.017587818205356598, minz + 0.05299532413482666),
                (minx + 0.02275294065475464, maxy - 0.01759415864944458, minz + 0.04915827512741089),
                (minx + 0.015590071678161621, maxy - 0.017624743282794952, minz + 0.04886317253112793),
                (minx + 0.004389584064483643, maxy - 0.0009383484721183777, minz + 0.05804264545440674),
                (minx + 0.00849863886833191, maxy - 0.0009383484721183777, minz + 0.0515575110912323),
                (minx + 0.00407370924949646, maxy - 0.0009383484721183777, maxz - 0.05987495183944702),
                (minx + 0.007635623216629028, maxy - 0.0009383484721183777, maxz - 0.053073734045028687),
                (minx + 0.014120936393737793, maxy - 0.0009383484721183777, maxz - 0.04896456003189087),
                (minx + 0.021791845560073853, maxy - 0.0009383484721183777, maxz - 0.04864847660064697),
                (minx + 0.0285930335521698, maxy - 0.0009383484721183777, maxz - 0.052210211753845215),
                (minx + 0.03270205855369568, maxy - 0.0009383484721183777, maxz - 0.05869537591934204),
                (minx + 0.03301793336868286, maxy - 0.0009383484721183777, minz + 0.05922222137451172),
                (minx + 0.02945604920387268, maxy - 0.0009383484721183777, minz + 0.052421003580093384),
                (minx + 0.022970736026763916, maxy - 0.0009383484721183777, minz + 0.048311829566955566),
                (minx + 0.015299826860427856, maxy - 0.0009383484721183777, minz + 0.04799577593803406),
                (minx + 0.009323716163635254, maxy - 0.012187294661998749, minz + 0.05233737826347351),
                (minx + 0.0055314600467681885, maxy - 0.01223689317703247, minz + 0.05832257866859436),
                (minx + 0.005239963531494141, maxy - 0.012276701629161835, maxz - 0.06018644571304321),
                (minx + 0.008527249097824097, maxy - 0.012296058237552643, maxz - 0.05390956997871399),
                (minx + 0.01451253890991211, maxy - 0.012289784848690033, maxz - 0.05011719465255737),
                (minx + 0.02159211039543152, maxy - 0.012259557843208313, maxz - 0.04982548952102661),
                (minx + 0.027868926525115967, maxy - 0.012213476002216339, maxz - 0.05311262607574463),
                (minx + 0.03166118264198303, maxy - 0.012163884937763214, maxz - 0.05909779667854309),
                (minx + 0.03195270895957947, maxy - 0.01212407648563385, minz + 0.059411197900772095),
                (minx + 0.028665393590927124, maxy - 0.012104712426662445, minz + 0.05313432216644287),
                (minx + 0.02268010377883911, maxy - 0.012110985815525055, minz + 0.049341946840286255),
                (minx + 0.01560056209564209, maxy - 0.012141212821006775, minz + 0.04905024170875549),
                (minx + 0.009444117546081543, miny + 0.009956002235412598, minz + 0.05219161510467529),
                (minx + 0.005651921033859253, miny + 0.00990641862154007, minz + 0.05817681550979614),
                (minx + 0.005360394716262817, miny + 0.009866602718830109, maxz - 0.06033217906951904),
                (minx + 0.008647710084915161, miny + 0.009847238659858704, maxz - 0.05405530333518982),
                (minx + 0.014632970094680786, miny + 0.009853512048721313, maxz - 0.0502629280090332),
                (minx + 0.021712541580200195, miny + 0.00988374650478363, maxz - 0.04997122287750244),
                (minx + 0.02798938751220703, miny + 0.009929820895195007, maxz - 0.05325835943222046),
                (minx + 0.03178161382675171, miny + 0.00997941941022873, maxz - 0.05924355983734131),
                (minx + 0.032073140144348145, miny + 0.010019220411777496, minz + 0.05926543474197388),
                (minx + 0.02878585457801819, miny + 0.010038584470748901, minz + 0.05298855900764465),
                (minx + 0.022800534963607788, miny + 0.010032311081886292, minz + 0.04919618368148804),
                (minx + 0.015720993280410767, miny + 0.010002091526985168, minz + 0.04890450835227966),
                (minx + 0.009488403797149658, miny + 0.0001087486743927002, minz + 0.05213809013366699),
                (minx + 0.0056961774826049805, miny + 5.917251110076904e-05, minz + 0.058123260736465454),
                (minx + 0.005404621362686157, miny + 1.9356608390808105e-05, maxz - 0.06038573384284973),
                (minx + 0.008691936731338501, miny, maxz - 0.05410885810852051),
                (minx + 0.014677256345748901, miny + 6.258487701416016e-06, maxz - 0.05031648278236389),
                (minx + 0.021756768226623535, miny + 3.650784492492676e-05, maxz - 0.05002477765083313),
                (minx + 0.02803361415863037, miny + 8.258223533630371e-05, maxz - 0.05331191420555115),
                (minx + 0.031825870275497437, miny + 0.00013218075037002563, maxz - 0.05929708480834961),
                (minx + 0.03211739659309387, miny + 0.00017196685075759888, minz + 0.059211909770965576),
                (minx + 0.028830111026763916, miny + 0.00019133836030960083, minz + 0.052935004234313965),
                (minx + 0.022844791412353516, miny + 0.0001850724220275879, minz + 0.04914262890815735),
                (minx + 0.015765219926834106, miny + 0.00015483051538467407, minz + 0.04885092377662659),
                (maxx - 0.010264694690704346, miny + 0.0024030879139900208, minz + 0.0574510395526886),
                (maxx - 0.009389877319335938, miny + 0.0028769299387931824, minz + 0.05982285737991333),
                (maxx - 0.00899556279182434, miny + 0.003135383129119873, maxz - 0.06170690059661865),
                (maxx - 0.00918734073638916, miny + 0.003109179437160492, maxz - 0.0570487380027771),
                (maxx - 0.009913921356201172, miny + 0.002805367112159729, maxz - 0.0530393123626709),
                (maxx - 0.010980546474456787, miny + 0.002305328845977783, maxz - 0.0507529079914093),
                (maxx - 0.011445850133895874, miny + 0.008283689618110657, minz + 0.05754268169403076),
                (maxx - 0.010571062564849854, miny + 0.008757516741752625, minz + 0.059914469718933105),
                (maxx - 0.01017671823501587, miny + 0.009015955030918121, maxz - 0.06161528825759888),
                (maxx - 0.010368555784225464, miny + 0.008989766240119934, maxz - 0.056957095861434937),
                (maxx - 0.011095106601715088, miny + 0.008685953915119171, maxz - 0.05294764041900635),
                (maxx - 0.012161701917648315, miny + 0.008185915648937225, maxz - 0.050661295652389526),
                (maxx - 0.0007557570934295654, miny + 0.019280850887298584, minz + 0.05762714147567749),
                (maxx - 0.0026130378246307373, miny + 0.019916504621505737, minz + 0.05755424499511719),
                (maxx - 0.00020641088485717773, miny + 0.020433299243450165, minz + 0.059989362955093384),
                (maxx, miny + 0.021083541214466095, maxz - 0.06154590845108032),
                (maxx - 0.00019183754920959473, miny + 0.021057337522506714, maxz - 0.05688774585723877),
                (maxx - 0.0007305145263671875, miny + 0.020361721515655518, maxz - 0.05287277698516846),
                (maxx - 0.0014716684818267822, miny + 0.019183076918125153, maxz - 0.05057680606842041),
                (maxx - 0.0033288896083831787, miny + 0.0198187455534935, maxz - 0.0506497323513031),
                (maxx - 0.0020636916160583496, miny + 0.021068952977657318, minz + 0.05991646647453308),
                (maxx - 0.0018572509288787842, miny + 0.021719202399253845, maxz - 0.061618804931640625),
                (maxx - 0.002049088478088379, miny + 0.021692998707294464, maxz - 0.05696064233779907),
                (maxx - 0.002587735652923584, miny + 0.020997390151023865, maxz - 0.05294567346572876),
                (minx + 0.018761008977890015, miny + 9.564310312271118e-05, minz + 0.062207311391830444),
                (minx + 0.0222054123878479, maxy - 0.009206198155879974, maxz - 0.04723122715950012),
                (minx, maxy - 0.009349517524242401, minz),
                (minx, maxy, minz),
                (minx + 0.03702586889266968, maxy, minz),
                (minx + 0.03702586889266968, maxy - 0.009349517524242401, minz),
                (minx, maxy - 0.009349517524242401, maxz),
                (minx, maxy, maxz),
                (minx + 0.03702586889266968, maxy, maxz),
                (minx + 0.03702586889266968, maxy - 0.009349517524242401, maxz),
                (minx, maxy - 0.009349517524242401, minz + 0.0038556158542633057),
                (minx, maxy - 0.009349517524242401, maxz - 0.0038556158542633057),
                (minx, maxy, maxz - 0.0038556158542633057),
                (minx, maxy, minz + 0.0038556158542633057),
                (minx + 0.03702586889266968, maxy, maxz - 0.0038556158542633057),
                (minx + 0.03702586889266968, maxy, minz + 0.0038556158542633057),
                (minx + 0.03702586889266968, maxy - 0.009349517524242401, maxz - 0.0038556158542633057),
                (minx + 0.03702586889266968, maxy - 0.009349517524242401, minz + 0.0038556158542633057),
                (minx, maxy, maxz),
                (minx, maxy, minz),
                (minx + 0.03702586889266968, maxy, maxz),
                (minx + 0.03702586889266968, maxy, minz),
                (minx, maxy, maxz - 0.0038556158542633057),
                (minx, maxy, minz + 0.0038556158542633057),
                (minx + 0.03702586889266968, maxy, maxz - 0.0038556158542633057),
                (minx + 0.03702586889266968, maxy, minz + 0.0038556158542633057),
                (minx + 0.00467991828918457, maxy, maxz),
                (minx + 0.03234601020812988, maxy, maxz),
                (minx + 0.03234601020812988, maxy, minz),
                (minx + 0.00467991828918457, maxy, minz),
                (minx + 0.03234601020812988, maxy - 0.009349517524242401, maxz),
                (minx + 0.00467991828918457, maxy - 0.009349517524242401, maxz),
                (minx + 0.00467991828918457, maxy - 0.009349517524242401, minz),
                (minx + 0.03234601020812988, maxy - 0.009349517524242401, minz),
                (minx + 0.03234601020812988, maxy, maxz - 0.0038556158542633057),
                (minx + 0.00467991828918457, maxy, maxz - 0.0038556158542633057),
                (minx + 0.03234601020812988, maxy, minz + 0.0038556158542633057),
                (minx + 0.00467991828918457, maxy, minz + 0.0038556158542633057),
                (minx + 0.00467991828918457, maxy - 0.009349517524242401, maxz - 0.0038556158542633057),
                (minx + 0.03234601020812988, maxy - 0.009349517524242401, maxz - 0.0038556158542633057),
                (minx + 0.00467991828918457, maxy - 0.009349517524242401, minz + 0.0038556158542633057),
                (minx + 0.03234601020812988, maxy - 0.009349517524242401, minz + 0.0038556158542633057),
                (minx + 0.00467991828918457, maxy, minz),
                (minx + 0.03234601020812988, maxy, minz),
                (minx + 0.03234601020812988, maxy, maxz),
                (minx + 0.00467991828918457, maxy, maxz),
                (minx + 0.01765689253807068, maxy - 0.008991599082946777, maxz - 0.00847548246383667),
                (minx + 0.014916181564331055, maxy - 0.008991599082946777, maxz - 0.00961071252822876),
                (minx + 0.013780921697616577, maxy - 0.008991606533527374, maxz - 0.012351423501968384),
                (minx + 0.014916181564331055, maxy - 0.008991606533527374, maxz - 0.015092134475708008),
                (minx + 0.01765689253807068, maxy - 0.008991606533527374, maxz - 0.016227364540100098),
                (minx + 0.02039763331413269, maxy - 0.008991606533527374, maxz - 0.015092134475708008),
                (minx + 0.021532833576202393, maxy - 0.008991606533527374, maxz - 0.012351423501968384),
                (minx + 0.02039763331413269, maxy - 0.008991599082946777, maxz - 0.00961071252822876),
                (minx + 0.01765689253807068, maxy - 0.0095299631357193, maxz - 0.00847548246383667),
                (minx + 0.014916181564331055, maxy - 0.0095299631357193, maxz - 0.00961071252822876),
                (minx + 0.013780921697616577, maxy - 0.0095299631357193, maxz - 0.012351423501968384),
                (minx + 0.014916181564331055, maxy - 0.0095299631357193, maxz - 0.015092134475708008),
                (minx + 0.01765689253807068, maxy - 0.0095299631357193, maxz - 0.016227364540100098),
                (minx + 0.02039763331413269, maxy - 0.0095299631357193, maxz - 0.015092134475708008),
                (minx + 0.021532833576202393, maxy - 0.0095299631357193, maxz - 0.012351423501968384),
                (minx + 0.02039763331413269, maxy - 0.0095299631357193, maxz - 0.00961071252822876),
                (minx + 0.01765689253807068, maxy - 0.0095299631357193, maxz - 0.009734481573104858),
                (minx + 0.0158064067363739, maxy - 0.0095299631357193, maxz - 0.010500967502593994),
                (minx + 0.015039980411529541, maxy - 0.0095299631357193, maxz - 0.012351423501968384),
                (minx + 0.0158064067363739, maxy - 0.0095299631357193, maxz - 0.014201879501342773),
                (minx + 0.01765689253807068, maxy - 0.0095299631357193, maxz - 0.014968395233154297),
                (minx + 0.01950731873512268, maxy - 0.0095299631357193, maxz - 0.014201879501342773),
                (minx + 0.020273834466934204, maxy - 0.0095299631357193, maxz - 0.012351423501968384),
                (minx + 0.01950731873512268, maxy - 0.0095299631357193, maxz - 0.010500967502593994),
                (minx + 0.01765689253807068, maxy - 0.009312078356742859, maxz - 0.009734481573104858),
                (minx + 0.0158064067363739, maxy - 0.009312078356742859, maxz - 0.010500967502593994),
                (minx + 0.015039980411529541, maxy - 0.009312078356742859, maxz - 0.012351423501968384),
                (minx + 0.0158064067363739, maxy - 0.009312078356742859, maxz - 0.014201879501342773),
                (minx + 0.01765689253807068, maxy - 0.009312078356742859, maxz - 0.014968395233154297),
                (minx + 0.01950731873512268, maxy - 0.009312078356742859, maxz - 0.014201879501342773),
                (minx + 0.020273834466934204, maxy - 0.009312078356742859, maxz - 0.012351423501968384),
                (minx + 0.01950731873512268, maxy - 0.009312078356742859, maxz - 0.010500967502593994),
                (minx + 0.01765689253807068, maxy - 0.009312078356742859, maxz - 0.01099047064781189),
                (minx + 0.01669454574584961, maxy - 0.009312078356742859, maxz - 0.011389046907424927),
                (minx + 0.016295909881591797, maxy - 0.009312078356742859, maxz - 0.012351423501968384),
                (minx + 0.01669454574584961, maxy - 0.009312078356742859, maxz - 0.013313770294189453),
                (minx + 0.01765689253807068, maxy - 0.009312078356742859, maxz - 0.013712406158447266),
                (minx + 0.01861920952796936, maxy - 0.009312078356742859, maxz - 0.013313770294189453),
                (minx + 0.019017815589904785, maxy - 0.009312078356742859, maxz - 0.012351423501968384),
                (minx + 0.01861920952796936, maxy - 0.009312078356742859, maxz - 0.011389046907424927),
                (minx + 0.01765689253807068, maxy - 0.009312078356742859, maxz - 0.011496275663375854),
                (minx + 0.01705223321914673, maxy - 0.009312078356742859, maxz - 0.011746734380722046),
                (minx + 0.01680171489715576, maxy - 0.009312078356742859, maxz - 0.012351423501968384),
                (minx + 0.01705223321914673, maxy - 0.009312078356742859, maxz - 0.012956112623214722),
                (minx + 0.01765689253807068, maxy - 0.009312078356742859, maxz - 0.013206571340560913),
                (minx + 0.018261581659317017, maxy - 0.009312078356742859, maxz - 0.012956112623214722),
                (minx + 0.018512040376663208, maxy - 0.009312078356742859, maxz - 0.012351423501968384),
                (minx + 0.018261581659317017, maxy - 0.009312078356742859, maxz - 0.011746734380722046),
                (minx + 0.01765689253807068, maxy - 0.009564712643623352, maxz - 0.009734481573104858),
                (minx + 0.0158064067363739, maxy - 0.009564712643623352, maxz - 0.010500967502593994),
                (minx + 0.015039980411529541, maxy - 0.009564712643623352, maxz - 0.012351423501968384),
                (minx + 0.0158064067363739, maxy - 0.009564712643623352, maxz - 0.014201879501342773),
                (minx + 0.01765689253807068, maxy - 0.009564712643623352, maxz - 0.014968395233154297),
                (minx + 0.01950731873512268, maxy - 0.009564712643623352, maxz - 0.014201879501342773),
                (minx + 0.020273834466934204, maxy - 0.009564712643623352, maxz - 0.012351423501968384),
                (minx + 0.01950731873512268, maxy - 0.009564712643623352, maxz - 0.010500967502593994),
                (minx + 0.01765689253807068, maxy - 0.009564712643623352, maxz - 0.01099047064781189),
                (minx + 0.01669454574584961, maxy - 0.009564712643623352, maxz - 0.011389046907424927),
                (minx + 0.016295909881591797, maxy - 0.009564712643623352, maxz - 0.012351423501968384),
                (minx + 0.01669454574584961, maxy - 0.009564712643623352, maxz - 0.013313770294189453),
                (minx + 0.01765689253807068, maxy - 0.009564712643623352, maxz - 0.013712406158447266),
                (minx + 0.01861920952796936, maxy - 0.009564712643623352, maxz - 0.013313770294189453),
                (minx + 0.019017815589904785, maxy - 0.009564712643623352, maxz - 0.012351423501968384),
                (minx + 0.01861920952796936, maxy - 0.009564712643623352, maxz - 0.011389046907424927),
                (minx + 0.01765689253807068, maxy - 0.009564712643623352, maxz - 0.011496275663375854),
                (minx + 0.01705223321914673, maxy - 0.009564712643623352, maxz - 0.011746734380722046),
                (minx + 0.01680171489715576, maxy - 0.009564712643623352, maxz - 0.012351423501968384),
                (minx + 0.01705223321914673, maxy - 0.009564712643623352, maxz - 0.012956112623214722),
                (minx + 0.01765689253807068, maxy - 0.009564712643623352, maxz - 0.013206571340560913),
                (minx + 0.018261581659317017, maxy - 0.009564712643623352, maxz - 0.012956112623214722),
                (minx + 0.018512040376663208, maxy - 0.009564712643623352, maxz - 0.012351423501968384),
                (minx + 0.018261581659317017, maxy - 0.009564712643623352, maxz - 0.011746734380722046),
                (minx + 0.01765689253807068, maxy - 0.008991599082946777, minz + 0.017180711030960083),
                (minx + 0.014916181564331055, maxy - 0.008991599082946777, minz + 0.016045480966567993),
                (minx + 0.013780921697616577, maxy - 0.008991606533527374, minz + 0.01330476999282837),
                (minx + 0.014916181564331055, maxy - 0.008991606533527374, minz + 0.010564059019088745),
                (minx + 0.01765689253807068, maxy - 0.008991606533527374, minz + 0.009428799152374268),
                (minx + 0.02039763331413269, maxy - 0.008991606533527374, minz + 0.010564059019088745),
                (minx + 0.021532833576202393, maxy - 0.008991606533527374, minz + 0.01330476999282837),
                (minx + 0.02039763331413269, maxy - 0.008991599082946777, minz + 0.016045480966567993),
                (minx + 0.01765689253807068, maxy - 0.0095299631357193, minz + 0.017180711030960083),
                (minx + 0.014916181564331055, maxy - 0.0095299631357193, minz + 0.016045480966567993),
                (minx + 0.013780921697616577, maxy - 0.0095299631357193, minz + 0.01330476999282837),
                (minx + 0.014916181564331055, maxy - 0.0095299631357193, minz + 0.010564059019088745),
                (minx + 0.01765689253807068, maxy - 0.0095299631357193, minz + 0.009428799152374268),
                (minx + 0.02039763331413269, maxy - 0.0095299631357193, minz + 0.010564059019088745),
                (minx + 0.021532833576202393, maxy - 0.0095299631357193, minz + 0.01330476999282837),
                (minx + 0.02039763331413269, maxy - 0.0095299631357193, minz + 0.016045480966567993),
                (minx + 0.01765689253807068, maxy - 0.0095299631357193, minz + 0.015921711921691895),
                (minx + 0.0158064067363739, maxy - 0.0095299631357193, minz + 0.015155225992202759),
                (minx + 0.015039980411529541, maxy - 0.0095299631357193, minz + 0.01330476999282837),
                (minx + 0.0158064067363739, maxy - 0.0095299631357193, minz + 0.01145431399345398),
                (minx + 0.01765689253807068, maxy - 0.0095299631357193, minz + 0.010687828063964844),
                (minx + 0.01950731873512268, maxy - 0.0095299631357193, minz + 0.01145431399345398),
                (minx + 0.020273834466934204, maxy - 0.0095299631357193, minz + 0.01330476999282837),
                (minx + 0.01950731873512268, maxy - 0.0095299631357193, minz + 0.015155225992202759),
                (minx + 0.01765689253807068, maxy - 0.009312078356742859, minz + 0.015921711921691895),
                (minx + 0.0158064067363739, maxy - 0.009312078356742859, minz + 0.015155225992202759),
                (minx + 0.015039980411529541, maxy - 0.009312078356742859, minz + 0.01330476999282837),
                (minx + 0.0158064067363739, maxy - 0.009312078356742859, minz + 0.01145431399345398),
                (minx + 0.01765689253807068, maxy - 0.009312078356742859, minz + 0.010687828063964844),
                (minx + 0.01950731873512268, maxy - 0.009312078356742859, minz + 0.01145431399345398),
                (minx + 0.020273834466934204, maxy - 0.009312078356742859, minz + 0.01330476999282837),
                (minx + 0.01950731873512268, maxy - 0.009312078356742859, minz + 0.015155225992202759),
                (minx + 0.01765689253807068, maxy - 0.009312078356742859, minz + 0.014665752649307251),
                (minx + 0.01669454574584961, maxy - 0.009312078356742859, minz + 0.014267116785049438),
                (minx + 0.016295909881591797, maxy - 0.009312078356742859, minz + 0.01330476999282837),
                (minx + 0.01669454574584961, maxy - 0.009312078356742859, minz + 0.012342393398284912),
                (minx + 0.01765689253807068, maxy - 0.009312078356742859, minz + 0.011943817138671875),
                (minx + 0.01861920952796936, maxy - 0.009312078356742859, minz + 0.012342393398284912),
                (minx + 0.019017815589904785, maxy - 0.009312078356742859, minz + 0.01330476999282837),
                (minx + 0.01861920952796936, maxy - 0.009312078356742859, minz + 0.014267116785049438),
                (minx + 0.01765689253807068, maxy - 0.009312078356742859, minz + 0.014159917831420898),
                (minx + 0.01705223321914673, maxy - 0.009312078356742859, minz + 0.01390942931175232),
                (minx + 0.01680171489715576, maxy - 0.009312078356742859, minz + 0.01330476999282837),
                (minx + 0.01705223321914673, maxy - 0.009312078356742859, minz + 0.012700080871582031),
                (minx + 0.01765689253807068, maxy - 0.009312078356742859, minz + 0.012449592351913452),
                (minx + 0.018261581659317017, maxy - 0.009312078356742859, minz + 0.012700080871582031),
                (minx + 0.018512040376663208, maxy - 0.009312078356742859, minz + 0.01330476999282837),
                (minx + 0.018261581659317017, maxy - 0.009312078356742859, minz + 0.01390942931175232),
                (minx + 0.01765689253807068, maxy - 0.009564712643623352, minz + 0.015921711921691895),
                (minx + 0.0158064067363739, maxy - 0.009564712643623352, minz + 0.015155225992202759),
                (minx + 0.015039980411529541, maxy - 0.009564712643623352, minz + 0.01330476999282837),
                (minx + 0.0158064067363739, maxy - 0.009564712643623352, minz + 0.01145431399345398),
                (minx + 0.01765689253807068, maxy - 0.009564712643623352, minz + 0.010687828063964844),
                (minx + 0.01950731873512268, maxy - 0.009564712643623352, minz + 0.01145431399345398),
                (minx + 0.020273834466934204, maxy - 0.009564712643623352, minz + 0.01330476999282837),
                (minx + 0.01950731873512268, maxy - 0.009564712643623352, minz + 0.015155225992202759),
                (minx + 0.01765689253807068, maxy - 0.009564712643623352, minz + 0.014665752649307251),
                (minx + 0.01669454574584961, maxy - 0.009564712643623352, minz + 0.014267116785049438),
                (minx + 0.016295909881591797, maxy - 0.009564712643623352, minz + 0.01330476999282837),
                (minx + 0.01669454574584961, maxy - 0.009564712643623352, minz + 0.012342393398284912),
                (minx + 0.01765689253807068, maxy - 0.009564712643623352, minz + 0.011943817138671875),
                (minx + 0.01861920952796936, maxy - 0.009564712643623352, minz + 0.012342393398284912),
                (minx + 0.019017815589904785, maxy - 0.009564712643623352, minz + 0.01330476999282837),
                (minx + 0.01861920952796936, maxy - 0.009564712643623352, minz + 0.014267116785049438),
                (minx + 0.01765689253807068, maxy - 0.009564712643623352, minz + 0.014159917831420898),
                (minx + 0.01705223321914673, maxy - 0.009564712643623352, minz + 0.01390942931175232),
                (minx + 0.01680171489715576, maxy - 0.009564712643623352, minz + 0.01330476999282837),
                (minx + 0.01705223321914673, maxy - 0.009564712643623352, minz + 0.012700080871582031),
                (minx + 0.01765689253807068, maxy - 0.009564712643623352, minz + 0.012449592351913452),
                (minx + 0.018261581659317017, maxy - 0.009564712643623352, minz + 0.012700080871582031),
                (minx + 0.018512040376663208, maxy - 0.009564712643623352, minz + 0.01330476999282837),
                (minx + 0.018261581659317017, maxy - 0.009564712643623352, minz + 0.01390942931175232)]

    # Faces
    myfaces = [(12, 0, 1, 13), (13, 1, 2, 14), (14, 2, 3, 15), (15, 3, 4, 16), (17, 6, 7, 18),
               (18, 7, 8, 19), (19, 8, 9, 20), (20, 9, 10, 21), (21, 10, 11, 22), (22, 11, 0, 12),
               (1, 0, 23, 24), (2, 1, 24, 25), (3, 2, 25, 26), (4, 3, 26, 27), (5, 4, 27, 28),
               (6, 5, 28, 29), (7, 6, 29, 30), (8, 7, 30, 31), (9, 8, 31, 32), (10, 9, 32, 33),
               (11, 10, 33, 34), (0, 11, 34, 23), (24, 23, 36, 35), (25, 24, 35, 37), (26, 25, 37, 38),
               (27, 26, 38, 39), (28, 27, 39, 40), (29, 28, 40, 41), (30, 29, 41, 42), (31, 30, 42, 43),
               (32, 31, 43, 44), (33, 32, 44, 45), (34, 33, 45, 46), (23, 34, 46, 36), (48, 47, 59, 60),
               (49, 48, 60, 61), (50, 49, 61, 62), (51, 50, 62, 63), (52, 51, 63, 64), (53, 52, 64, 65),
               (54, 53, 65, 66), (55, 54, 66, 67), (56, 55, 67, 68), (57, 56, 68, 69), (58, 57, 69, 70),
               (59, 47, 58, 70), (60, 59, 71, 72), (61, 60, 72, 73), (62, 61, 73, 74), (63, 62, 74, 75),
               (64, 63, 75, 76), (70, 69, 81, 82), (70, 82, 71, 59), (81, 69, 89, 83), (80, 81, 83, 84),
               (79, 80, 84, 85), (78, 79, 85, 86), (77, 78, 86, 87), (76, 77, 87, 88), (64, 76, 88, 94),
               (69, 68, 90, 89), (68, 67, 91, 90), (67, 66, 92, 91), (66, 65, 93, 92), (65, 64, 94, 93),
               (83, 89, 96, 95), (84, 83, 95, 97), (85, 84, 97, 98), (86, 85, 98, 99), (87, 86, 99, 100),
               (88, 87, 100, 101), (94, 88, 101, 102), (89, 90, 103, 96), (90, 91, 104, 103), (91, 92, 105, 104),
               (92, 93, 106, 105), (93, 94, 102, 106), (100, 106, 102, 101), (99, 105, 106, 100), (98, 104, 105, 99),
               (97, 103, 104, 98), (95, 96, 103, 97), (72, 71, 107), (73, 72, 107), (74, 73, 107),
               (75, 74, 107), (76, 75, 107), (77, 76, 107), (78, 77, 107), (79, 78, 107),
               (80, 79, 107), (81, 80, 107), (82, 81, 107), (71, 82, 107), (17, 108, 5, 6),
               (5, 108, 16, 4), (130, 120, 110, 126), (143, 122, 111, 135), (132, 124, 112, 128), (147, 117, 109, 139),
               (150, 135, 111, 128), (152, 133, 114, 125), (125, 114, 119, 129),
               (129, 119, 120, 130), (134, 115, 121, 141),
               (141, 121, 122, 143), (127, 116, 123, 131), (131, 123, 124, 132),
               (138, 113, 118, 145), (145, 118, 117, 147),
               (117, 130, 126, 109), (122, 132, 128, 111), (140, 150, 128, 112),
               (138, 152, 125, 113), (113, 125, 129, 118),
               (118, 129, 130, 117), (115, 127, 131, 121), (121, 131, 132, 122),
               (120, 144, 136, 110), (144, 143, 135, 136),
               (124, 148, 140, 112), (148, 147, 139, 140), (126, 110, 136, 149),
               (149, 136, 135, 150), (127, 115, 134, 151),
               (151, 134, 133, 152), (114, 133, 142, 119), (133, 134, 141, 142),
               (119, 142, 144, 120), (142, 141, 143, 144),
               (116, 137, 146, 123), (137, 138, 145, 146), (123, 146, 148, 124),
               (146, 145, 147, 148), (109, 126, 149, 139),
               (139, 149, 150, 140), (116, 127, 151, 137), (137, 151, 152, 138),
               (153, 160, 168, 161), (160, 159, 167, 168),
               (159, 158, 166, 167), (158, 157, 165, 166), (157, 156, 164, 165),
               (156, 155, 163, 164), (155, 154, 162, 163),
               (154, 153, 161, 162), (161, 168, 176, 169), (168, 167, 175, 176),
               (167, 166, 174, 175), (166, 165, 173, 174),
               (165, 164, 172, 173), (164, 163, 171, 172), (163, 162, 170, 171),
               (162, 161, 169, 170), (169, 176, 184, 177),
               (176, 175, 183, 184), (175, 174, 182, 183), (174, 173, 181, 182),
               (173, 172, 180, 181), (172, 171, 179, 180),
               (171, 170, 178, 179), (170, 169, 177, 178), (197, 189, 213, 221),
               (184, 183, 191, 192), (196, 197, 221, 220),
               (182, 181, 189, 190), (185, 177, 201, 209), (180, 179, 187, 188),
               (195, 187, 211, 219), (178, 177, 185, 186),
               (198, 199, 223, 222), (192, 191, 199, 200), (191, 183, 207, 215),
               (190, 189, 197, 198), (200, 193, 217, 224),
               (188, 187, 195, 196), (189, 181, 205, 213), (186, 185, 193, 194),
               (194, 193, 200, 199, 198, 197, 196, 195),
               (201, 208, 216, 209),
               (207, 206, 214, 215), (205, 204, 212, 213), (203, 202, 210, 211),
               (209, 216, 224, 217), (215, 214, 222, 223),
               (213, 212, 220, 221), (211, 210, 218, 219), (194, 195, 219, 218),
               (199, 191, 215, 223), (178, 186, 210, 202),
               (193, 185, 209, 217), (177, 184, 208, 201), (180, 188, 212, 204),
               (183, 182, 206, 207), (182, 190, 214, 206),
               (186, 194, 218, 210), (181, 180, 204, 205), (184, 192, 216, 208),
               (188, 196, 220, 212), (179, 178, 202, 203),
               (190, 198, 222, 214), (192, 200, 224, 216), (187, 179, 203, 211),
               (225, 232, 240, 233), (232, 231, 239, 240),
               (231, 230, 238, 239), (230, 229, 237, 238), (229, 228, 236, 237),
               (228, 227, 235, 236), (227, 226, 234, 235),
               (226, 225, 233, 234), (233, 240, 248, 241), (240, 239, 247, 248),
               (239, 238, 246, 247), (238, 237, 245, 246),
               (237, 236, 244, 245), (236, 235, 243, 244), (235, 234, 242, 243),
               (234, 233, 241, 242), (241, 248, 256, 249),
               (248, 247, 255, 256), (247, 246, 254, 255), (246, 245, 253, 254),
               (245, 244, 252, 253), (244, 243, 251, 252),
               (243, 242, 250, 251), (242, 241, 249, 250), (269, 261, 285, 293),
               (256, 255, 263, 264), (268, 269, 293, 292),
               (254, 253, 261, 262), (257, 249, 273, 281), (252, 251, 259, 260),
               (267, 259, 283, 291), (250, 249, 257, 258),
               (270, 271, 295, 294), (264, 263, 271, 272), (263, 255, 279, 287),
               (262, 261, 269, 270), (272, 265, 289, 296),
               (260, 259, 267, 268), (261, 253, 277, 285), (258, 257, 265, 266),
               (266, 265, 272, 271, 270, 269, 268, 267),
               (273, 280, 288, 281),
               (279, 278, 286, 287), (277, 276, 284, 285), (275, 274, 282, 283),
               (281, 288, 296, 289), (287, 286, 294, 295),
               (285, 284, 292, 293), (283, 282, 290, 291), (266, 267, 291, 290),
               (271, 263, 287, 295), (250, 258, 282, 274),
               (265, 257, 281, 289), (249, 256, 280, 273), (252, 260, 284, 276),
               (255, 254, 278, 279), (254, 262, 286, 278),
               (258, 266, 290, 282), (253, 252, 276, 277), (256, 264, 288, 280),
               (260, 268, 292, 284), (251, 250, 274, 275),
               (262, 270, 294, 286), (264, 272, 296, 288), (259, 251, 275, 283)]

    mesh = bpy.data.meshes.new(objname)
    myobject = bpy.data.objects.new(objname, mesh)

    myobject.location = bpy.context.scene.cursor_location
    bpy.context.scene.objects.link(myobject)

    mesh.from_pydata(myvertex, [], myfaces)
    mesh.update(calc_edges=True)

    # Create materials
    if mat and bpy.context.scene.render.engine == 'CYCLES':
        alumat = create_glossy_material("Handle_material", False, 0.733, 0.779, 0.8)
        set_material(myobject, alumat)

    return myobject


# ------------------------------------------------------------
# Generate rail handle
#
# objName: Object name
# mat: create materials
# ------------------------------------------------------------
def create_rail_handle(objname, mat):
    # ------------------------------------
    # Mesh data
    # ------------------------------------
    minx = -0.007970962673425674
    maxx = 0.007971039041876793
    miny = -0.0038057267665863037
    maxy = 6.780028343200684e-07
    minz = -0.07533407211303711
    maxz = 0.05025443434715271

    # Vertex
    myvertex = [(minx, miny + 0.0009473562240600586, minz),
                (minx, maxy, minz),
                (maxx, maxy, minz),
                (maxx, miny + 0.0009473562240600586, minz),
                (minx, miny + 0.0009473562240600586, maxz),
                (minx, maxy, maxz),
                (maxx, maxy, maxz),
                (maxx, miny + 0.0009473562240600586, maxz),
                (minx, miny + 0.0009473562240600586, minz + 0.0038556158542633057),
                (minx, miny + 0.0009473562240600586, maxz - 0.0038556158542633057),
                (minx, maxy, minz + 0.0038556158542633057),
                (maxx, miny + 0.0009473562240600586, maxz - 0.0038556158542633057),
                (maxx, miny + 0.0009473562240600586, minz + 0.0038556158542633057),
                (minx, maxy, maxz - 0.0038556158542633057),
                (maxx, maxy, maxz - 0.0038556158542633057),
                (maxx, maxy, minz + 0.0038556158542633057),
                (maxx - 0.002014978788793087, maxy, maxz),
                (minx + 0.0020150020718574524, maxy, minz),
                (maxx - 0.002014978788793087, miny + 0.0009473562240600586, maxz),
                (minx + 0.0020150020718574524, miny + 0.0009473562240600586, maxz),
                (minx + 0.0020150020718574524, miny + 0.0009473562240600586, minz),
                (maxx - 0.002014978788793087, miny + 0.0009473562240600586, minz),
                (maxx - 0.002014978788793087, maxy, maxz - 0.0038556158542633057),
                (minx + 0.0020150020718574524, maxy, maxz - 0.0038556158542633057),
                (maxx - 0.002014978788793087, maxy, minz + 0.0038556158542633057),
                (minx + 0.0020150020718574524, maxy, minz + 0.0038556158542633057),
                (minx + 0.0020150020718574524, miny + 0.0009473562240600586, maxz - 0.0038556158542633057),
                (maxx - 0.002014978788793087, miny + 0.0009473562240600586, maxz - 0.0038556158542633057),
                (minx + 0.0020150020718574524, miny + 0.0009473562240600586, minz + 0.0038556158542633057),
                (maxx - 0.002014978788793087, miny + 0.0009473562240600586, minz + 0.0038556158542633057),
                (maxx - 0.002014978788793087, maxy, minz),
                (minx + 0.0020150020718574524, maxy, maxz),
                (minx + 0.007114947948139161, miny + 0.001102180453017354, maxz - 0.004768103361129761),
                (minx + 0.0057074506767094135, miny + 0.001102180453017354, maxz - 0.005351103842258453),
                (minx + 0.005124435992911458, miny + 0.001102176494896412, maxz - 0.006758600473403931),
                (minx + 0.0057074506767094135, miny + 0.001102176494896412, maxz - 0.008166097104549408),
                (minx + 0.007114947948139161, miny + 0.001102176494896412, maxz - 0.0087490975856781),
                (maxx - 0.0074195414781570435, miny + 0.001102176494896412, maxz - 0.008166097104549408),
                (maxx - 0.006836557062342763, miny + 0.001102176494896412, maxz - 0.006758600473403931),
                (maxx - 0.0074195414781570435, miny + 0.001102180453017354, maxz - 0.005351103842258453),
                (minx + 0.007114947948139161, miny + 0.0008257024455815554, maxz - 0.004768103361129761),
                (minx + 0.0057074506767094135, miny + 0.0008257024455815554, maxz - 0.005351103842258453),
                (minx + 0.005124435992911458, miny + 0.0008257024455815554, maxz - 0.006758600473403931),
                (minx + 0.0057074506767094135, miny + 0.0008257024455815554, maxz - 0.008166097104549408),
                (minx + 0.007114947948139161, miny + 0.0008257024455815554, maxz - 0.0087490975856781),
                (maxx - 0.0074195414781570435, miny + 0.0008257024455815554, maxz - 0.008166097104549408),
                (maxx - 0.006836557062342763, miny + 0.0008257024455815554, maxz - 0.006758600473403931),
                (maxx - 0.0074195414781570435, miny + 0.0008257024455815554, maxz - 0.005351103842258453),
                (minx + 0.007114947948139161, miny + 0.000937597593292594, maxz - 0.0054146647453308105),
                (minx + 0.006164627615362406, miny + 0.000937597593292594, maxz - 0.00580829381942749),
                (minx + 0.0057710278779268265, miny + 0.000937597593292594, maxz - 0.006758600473403931),
                (minx + 0.006164627615362406, miny + 0.000937597593292594, maxz - 0.007708907127380371),
                (minx + 0.007114947948139161, miny + 0.000937597593292594, maxz - 0.008102551102638245),
                (maxx - 0.007876764051616192, miny + 0.000937597593292594, maxz - 0.007708907127380371),
                (maxx - 0.007483118446543813, miny + 0.000937597593292594, maxz - 0.006758600473403931),
                (maxx - 0.007876764051616192, miny + 0.000937597593292594, maxz - 0.00580829381942749),
                (minx + 0.007114947948139161, miny + 0.000937597593292594, maxz - 0.006059680134057999),
                (minx + 0.006620732950977981, miny + 0.000937597593292594, maxz - 0.006264369934797287),
                (minx + 0.006416012765839696, miny + 0.000937597593292594, maxz - 0.006758600473403931),
                (minx + 0.006620732950977981, miny + 0.000937597593292594, maxz - 0.0072528161108493805),
                (minx + 0.007114947948139161, miny + 0.000937597593292594, maxz - 0.0074575357139110565),
                (minx + 0.0076091475784778595, miny + 0.000937597593292594, maxz - 0.0072528161108493805),
                (minx + 0.007813852455001324, miny + 0.000937597593292594, maxz - 0.006758600473403931),
                (minx + 0.0076091475784778595, miny + 0.000937597593292594, maxz - 0.006264369934797287),
                (minx + 0.007114947948139161, miny + 0.000937597593292594, maxz - 0.006319437175989151),
                (minx + 0.006804424105212092, miny + 0.000937597593292594, maxz - 0.0064480602741241455),
                (minx + 0.00667576992418617, miny + 0.000937597593292594, maxz - 0.006758600473403931),
                (minx + 0.006804424105212092, miny + 0.000937597593292594, maxz - 0.007069140672683716),
                (minx + 0.007114947948139161, miny + 0.000937597593292594, maxz - 0.00719776377081871),
                (minx + 0.0074254871578887105, miny + 0.000937597593292594, maxz - 0.007069140672683716),
                (minx + 0.007554110663477331, miny + 0.000937597593292594, maxz - 0.006758600473403931),
                (minx + 0.0074254871578887105, miny + 0.000937597593292594, maxz - 0.0064480602741241455),
                (minx + 0.007114947948139161, miny + 0.0008078569080680609, maxz - 0.0054146647453308105),
                (minx + 0.006164627615362406, miny + 0.0008078569080680609, maxz - 0.00580829381942749),
                (minx + 0.0057710278779268265, miny + 0.0008078569080680609, maxz - 0.006758600473403931),
                (minx + 0.006164627615362406, miny + 0.0008078569080680609, maxz - 0.007708907127380371),
                (minx + 0.007114947948139161, miny + 0.0008078569080680609, maxz - 0.008102551102638245),
                (maxx - 0.007876764051616192, miny + 0.0008078569080680609, maxz - 0.007708907127380371),
                (maxx - 0.007483118446543813, miny + 0.0008078569080680609, maxz - 0.006758600473403931),
                (maxx - 0.007876764051616192, miny + 0.0008078569080680609, maxz - 0.00580829381942749),
                (minx + 0.007114947948139161, miny + 0.0008078569080680609, maxz - 0.006059680134057999),
                (minx + 0.006620732950977981, miny + 0.0008078569080680609, maxz - 0.006264369934797287),
                (minx + 0.006416012765839696, miny + 0.0008078569080680609, maxz - 0.006758600473403931),
                (minx + 0.006620732950977981, miny + 0.0008078569080680609, maxz - 0.0072528161108493805),
                (minx + 0.007114947948139161, miny + 0.0008078569080680609, maxz - 0.0074575357139110565),
                (minx + 0.0076091475784778595, miny + 0.0008078569080680609, maxz - 0.0072528161108493805),
                (minx + 0.007813852455001324, miny + 0.0008078569080680609, maxz - 0.006758600473403931),
                (minx + 0.0076091475784778595, miny + 0.0008078569080680609, maxz - 0.006264369934797287),
                (minx + 0.007114947948139161, miny + 0.0008078569080680609, maxz - 0.006319437175989151),
                (minx + 0.006804424105212092, miny + 0.0008078569080680609, maxz - 0.0064480602741241455),
                (minx + 0.00667576992418617, miny + 0.0008078569080680609, maxz - 0.006758600473403931),
                (minx + 0.006804424105212092, miny + 0.0008078569080680609, maxz - 0.007069140672683716),
                (minx + 0.007114947948139161, miny + 0.0008078569080680609, maxz - 0.00719776377081871),
                (minx + 0.0074254871578887105, miny + 0.0008078569080680609, maxz - 0.007069140672683716),
                (minx + 0.007554110663477331, miny + 0.0008078569080680609, maxz - 0.006758600473403931),
                (minx + 0.0074254871578887105, miny + 0.0008078569080680609, maxz - 0.0064480602741241455),
                (minx + 0.0074254871578887105, miny + 0.0008078569080680609, minz + 0.007765233516693115),
                (minx + 0.007554110663477331, miny + 0.0008078569080680609, minz + 0.00745469331741333),
                (minx + 0.0074254871578887105, miny + 0.0008078569080680609, minz + 0.007144153118133545),
                (minx + 0.007114947948139161, miny + 0.0008078569080680609, minz + 0.007015526294708252),
                (minx + 0.006804424105212092, miny + 0.0008078569080680609, minz + 0.007144153118133545),
                (minx + 0.00667576992418617, miny + 0.0008078569080680609, minz + 0.00745469331741333),
                (minx + 0.006804424105212092, miny + 0.0008078569080680609, minz + 0.007765233516693115),
                (minx + 0.007114947948139161, miny + 0.0008078569080680609, minz + 0.007893860340118408),
                (minx + 0.0076091475784778595, miny + 0.0008078569080680609, minz + 0.007948920130729675),
                (minx + 0.007813852455001324, miny + 0.0008078569080680609, minz + 0.00745469331741333),
                (minx + 0.0076091475784778595, miny + 0.0008078569080680609, minz + 0.006960481405258179),
                (minx + 0.007114947948139161, miny + 0.0008078569080680609, minz + 0.006755754351615906),
                (minx + 0.006620732950977981, miny + 0.0008078569080680609, minz + 0.006960481405258179),
                (minx + 0.006416012765839696, miny + 0.0008078569080680609, minz + 0.00745469331741333),
                (minx + 0.006620732950977981, miny + 0.0008078569080680609, minz + 0.007948920130729675),
                (minx + 0.007114947948139161, miny + 0.0008078569080680609, minz + 0.00815361738204956),
                (maxx - 0.007876764051616192, miny + 0.0008078569080680609, minz + 0.00840499997138977),
                (maxx - 0.007483118446543813, miny + 0.0008078569080680609, minz + 0.00745469331741333),
                (maxx - 0.007876764051616192, miny + 0.0008078569080680609, minz + 0.00650438666343689),
                (minx + 0.007114947948139161, miny + 0.0008078569080680609, minz + 0.006110742688179016),
                (minx + 0.006164627615362406, miny + 0.0008078569080680609, minz + 0.00650438666343689),
                (minx + 0.0057710278779268265, miny + 0.0008078569080680609, minz + 0.00745469331741333),
                (minx + 0.006164627615362406, miny + 0.0008078569080680609, minz + 0.00840499997138977),
                (minx + 0.007114947948139161, miny + 0.0008078569080680609, minz + 0.00879862904548645),
                (minx + 0.0074254871578887105, miny + 0.000937597593292594, minz + 0.007765233516693115),
                (minx + 0.007554110663477331, miny + 0.000937597593292594, minz + 0.00745469331741333),
                (minx + 0.0074254871578887105, miny + 0.000937597593292594, minz + 0.007144153118133545),
                (minx + 0.007114947948139161, miny + 0.000937597593292594, minz + 0.007015526294708252),
                (minx + 0.006804424105212092, miny + 0.000937597593292594, minz + 0.007144153118133545),
                (minx + 0.00667576992418617, miny + 0.000937597593292594, minz + 0.00745469331741333),
                (minx + 0.006804424105212092, miny + 0.000937597593292594, minz + 0.007765233516693115),
                (minx + 0.007114947948139161, miny + 0.000937597593292594, minz + 0.007893860340118408),
                (minx + 0.0076091475784778595, miny + 0.000937597593292594, minz + 0.007948920130729675),
                (minx + 0.007813852455001324, miny + 0.000937597593292594, minz + 0.00745469331741333),
                (minx + 0.0076091475784778595, miny + 0.000937597593292594, minz + 0.006960481405258179),
                (minx + 0.007114947948139161, miny + 0.000937597593292594, minz + 0.006755754351615906),
                (minx + 0.006620732950977981, miny + 0.000937597593292594, minz + 0.006960481405258179),
                (minx + 0.006416012765839696, miny + 0.000937597593292594, minz + 0.00745469331741333),
                (minx + 0.006620732950977981, miny + 0.000937597593292594, minz + 0.007948920130729675),
                (minx + 0.007114947948139161, miny + 0.000937597593292594, minz + 0.00815361738204956),
                (maxx - 0.007876764051616192, miny + 0.000937597593292594, minz + 0.00840499997138977),
                (maxx - 0.007483118446543813, miny + 0.000937597593292594, minz + 0.00745469331741333),
                (maxx - 0.007876764051616192, miny + 0.000937597593292594, minz + 0.00650438666343689),
                (minx + 0.007114947948139161, miny + 0.000937597593292594, minz + 0.006110742688179016),
                (minx + 0.006164627615362406, miny + 0.000937597593292594, minz + 0.00650438666343689),
                (minx + 0.0057710278779268265, miny + 0.000937597593292594, minz + 0.00745469331741333),
                (minx + 0.006164627615362406, miny + 0.000937597593292594, minz + 0.00840499997138977),
                (minx + 0.007114947948139161, miny + 0.000937597593292594, minz + 0.00879862904548645),
                (maxx - 0.0074195414781570435, miny + 0.0008257024455815554, minz + 0.008862189948558807),
                (maxx - 0.006836557062342763, miny + 0.0008257024455815554, minz + 0.00745469331741333),
                (maxx - 0.0074195414781570435, miny + 0.0008257024455815554, minz + 0.006047196686267853),
                (minx + 0.007114947948139161, miny + 0.0008257024455815554, minz + 0.00546419620513916),
                (minx + 0.0057074506767094135, miny + 0.0008257024455815554, minz + 0.006047196686267853),
                (minx + 0.005124435992911458, miny + 0.0008257024455815554, minz + 0.00745469331741333),
                (minx + 0.0057074506767094135, miny + 0.0008257024455815554, minz + 0.008862189948558807),
                (minx + 0.007114947948139161, miny + 0.0008257024455815554, minz + 0.0094451904296875),
                (maxx - 0.0074195414781570435, miny + 0.001102180453017354, minz + 0.008862189948558807),
                (maxx - 0.006836557062342763, miny + 0.001102176494896412, minz + 0.00745469331741333),
                (maxx - 0.0074195414781570435, miny + 0.001102176494896412, minz + 0.006047196686267853),
                (minx + 0.007114947948139161, miny + 0.001102176494896412, minz + 0.00546419620513916),
                (minx + 0.0057074506767094135, miny + 0.001102176494896412, minz + 0.006047196686267853),
                (minx + 0.005124435992911458, miny + 0.001102176494896412, minz + 0.00745469331741333),
                (minx + 0.0057074506767094135, miny + 0.001102180453017354, minz + 0.008862189948558807),
                (minx + 0.007114947948139161, miny + 0.001102180453017354, minz + 0.0094451904296875),
                (maxx - 0.002014978788793087, maxy, maxz - 0.015039850026369095),
                (maxx - 0.002014978788793087, maxy, minz + 0.015039850026369095),
                (minx, miny + 0.0009473562240600586, minz + 0.015039850026369095),
                (minx, miny + 0.0009473562240600586, maxz - 0.015039850026369095),
                (minx, maxy, minz + 0.015039850026369095),
                (maxx, maxy, maxz - 0.015039850026369095),
                (maxx, miny + 0.0009473562240600586, maxz - 0.015039850026369095),
                (maxx, miny + 0.0009473562240600586, minz + 0.015039850026369095),
                (maxx - 0.002014978788793087, miny + 0.0009473562240600586, maxz - 0.015039850026369095),
                (maxx - 0.002014978788793087, miny + 0.0009473562240600586, minz + 0.015039850026369095),
                (minx + 0.0020150020718574524, maxy, maxz - 0.015039850026369095),
                (minx + 0.0020150020718574524, maxy, minz + 0.015039850026369095),
                (minx + 0.0020150020718574524, miny + 0.0009473562240600586, maxz - 0.015039850026369095),
                (minx + 0.0020150020718574524, miny + 0.0009473562240600586, minz + 0.015039850026369095),
                (maxx, maxy, minz + 0.015039850026369095),
                (minx, maxy, maxz - 0.015039850026369095),
                (maxx - 0.002014978788793087, maxy - 0.0017695352435112, maxz - 0.015039850026369095),
                (maxx - 0.002014978788793087, maxy - 0.0017695352435112, minz + 0.015039850026369095),
                (minx + 0.0020150020718574524, maxy - 0.0017695352435112, maxz - 0.015039850026369095),
                (minx + 0.0020150020718574524, maxy - 0.0017695352435112, minz + 0.015039850026369095),
                (maxx - 0.002014978788793087, maxy, maxz - 0.020450454205274582),
                (minx, miny + 0.0009473562240600586, maxz - 0.020450454205274582),
                (minx, maxy, maxz - 0.020450454205274582),
                (maxx, maxy, maxz - 0.020450454205274582),
                (maxx, miny + 0.0009473562240600586, maxz - 0.020450454205274582),
                (maxx - 0.002014978788793087, miny + 0.0009473562240600586, maxz - 0.020450454205274582),
                (minx + 0.0020150020718574524, maxy, maxz - 0.020450454205274582),
                (minx + 0.0020150020718574524, miny + 0.0009473562240600586, maxz - 0.020450454205274582),
                (maxx - 0.002014978788793087, maxy - 0.0017695352435112, maxz - 0.020450454205274582),
                (minx + 0.0020150020718574524, maxy - 0.0017695352435112, maxz - 0.020450454205274582),
                (minx, miny + 0.0009473562240600586, maxz - 0.04870907962322235),
                (maxx - 0.002014978788793087, maxy, maxz - 0.04870907962322235),
                (minx, maxy, maxz - 0.04870907962322235),
                (maxx, miny + 0.0009473562240600586, maxz - 0.04870907962322235),
                (maxx - 0.002014978788793087, miny + 0.0009473562240600586, maxz - 0.04870907962322235),
                (minx + 0.0020150020718574524, maxy, maxz - 0.04870907962322235),
                (minx + 0.0020150020718574524, miny + 0.0009473562240600586, maxz - 0.04870907962322235),
                (maxx, maxy, maxz - 0.04870907962322235),
                (maxx - 0.002014978788793087, maxy - 0.0017695352435112, maxz - 0.04870907962322235),
                (minx + 0.0020150020718574524, maxy - 0.0017695352435112, maxz - 0.04870907962322235),
                (maxx - 0.0027115284465253353, miny + 0.0009342432022094727, maxz - 0.020450454205274582),
                (minx + 0.0027115517295897007, miny + 0.0009342432022094727, maxz - 0.020450454205274582),
                (maxx - 0.0027115284465253353, miny + 0.0009342432022094727, maxz - 0.04870907962322235),
                (minx + 0.0027115517295897007, miny + 0.0009342432022094727, maxz - 0.04870907962322235),
                (minx, miny + 0.0009473562240600586, maxz - 0.026037774980068207),
                (maxx - 0.002014978788793087, maxy, maxz - 0.026037774980068207),
                (minx, maxy, maxz - 0.026037774980068207),
                (maxx, maxy, maxz - 0.026037774980068207),
                (maxx, miny + 0.0009473562240600586, maxz - 0.026037774980068207),
                (maxx - 0.002014978788793087, miny + 0.0009473562240600586, maxz - 0.026037774980068207),
                (minx + 0.0020150020718574524, maxy, maxz - 0.026037774980068207),
                (minx + 0.0020150020718574524, miny + 0.0009473562240600586, maxz - 0.026037774980068207),
                (maxx - 0.002014978788793087, maxy - 0.0017695352435112, maxz - 0.026037774980068207),
                (minx + 0.0020150020718574524, maxy - 0.0017695352435112, maxz - 0.026037774980068207),
                (maxx - 0.0027115284465253353, miny + 0.0009342432022094727, maxz - 0.026037774980068207),
                (minx + 0.0027115517295897007, miny + 0.0009342432022094727, maxz - 0.026037774980068207),
                (minx, miny + 0.0009473562240600586, maxz - 0.03058292716741562),
                (maxx - 0.002014978788793087, maxy, maxz - 0.03058292716741562),
                (maxx, miny + 0.0009473562240600586, maxz - 0.03058292716741562),
                (maxx - 0.002014978788793087, miny + 0.0009473562240600586, maxz - 0.03058292716741562),
                (minx + 0.0020150020718574524, maxy, maxz - 0.03058292716741562),
                (minx + 0.0020150020718574524, miny + 0.0009473562240600586, maxz - 0.03058292716741562),
                (maxx, maxy, maxz - 0.03058292716741562),
                (minx, maxy, maxz - 0.03058292716741562),
                (maxx - 0.002014978788793087, maxy - 0.0017695352435112, maxz - 0.03058292716741562),
                (minx + 0.0020150020718574524, maxy - 0.0017695352435112, maxz - 0.03058292716741562),
                (maxx - 0.0027115284465253353, miny + 0.0009342432022094727, maxz - 0.03058292716741562),
                (minx + 0.0027115517295897007, miny + 0.0009342432022094727, maxz - 0.03058292716741562),
                (maxx - 0.004523299168795347, miny, maxz - 0.026820629835128784),
                (minx + 0.004523322451859713, miny, maxz - 0.026820629835128784),
                (maxx - 0.004523299168795347, miny, maxz - 0.02980007231235504),
                (minx + 0.004523322451859713, miny, maxz - 0.02980007231235504)]

    # Faces
    myfaces = [(28, 8, 0, 20), (174, 167, 12, 15), (19, 4, 9, 26), (173, 162, 8, 28), (10, 25, 17, 1),
               (12, 29, 21, 3), (29, 28, 20, 21), (164, 171, 25, 10), (171, 161, 24, 25), (7, 18, 27, 11),
               (18, 19, 26, 27), (167, 169, 29, 12), (169, 173, 28, 29), (32, 40, 47, 39), (39, 47, 46, 38),
               (38, 46, 45, 37), (37, 45, 44, 36), (36, 44, 43, 35), (35, 43, 42, 34), (34, 42, 41, 33),
               (33, 41, 40, 32), (68, 92, 84, 60), (55, 63, 62, 54), (67, 91, 92, 68), (53, 61, 60, 52),
               (56, 80, 72, 48), (51, 59, 58, 50), (66, 90, 82, 58), (49, 57, 56, 48), (69, 93, 94, 70),
               (63, 71, 70, 62), (62, 86, 78, 54), (61, 69, 68, 60), (71, 95, 88, 64), (59, 67, 66, 58),
               (60, 84, 76, 52), (57, 65, 64, 56), (65, 66, 67, 68, 69, 70, 71, 64), (72, 80, 87, 79), (78, 86, 85, 77),
               (76, 84, 83, 75), (74, 82, 81, 73), (80, 88, 95, 87), (86, 94, 93, 85), (84, 92, 91, 83),
               (82, 90, 89, 81), (65, 89, 90, 66), (70, 94, 86, 62), (49, 73, 81, 57), (64, 88, 80, 56),
               (48, 55, 79, 72), (51, 75, 83, 59), (54, 53, 77, 78), (53, 77, 85, 61), (57, 81, 89, 65),
               (52, 51, 75, 76), (55, 79, 87, 63), (59, 83, 91, 67), (50, 49, 73, 74), (61, 85, 93, 69),
               (63, 87, 95, 71), (58, 82, 74, 50), (133, 109, 117, 141), (128, 104, 96, 120), (130, 106, 98, 122),
               (141, 142, 118, 117), (132, 108, 100, 124), (136, 112, 104, 128), (139, 140, 116, 115),
               (134, 110, 102, 126),
               (138, 114, 106, 130), (137, 138, 114, 113), (140, 116, 108, 132), (143, 136, 112, 119),
               (127, 103, 111, 135),
               (142, 118, 110, 134), (121, 97, 105, 129), (126, 102, 101, 125), (109, 101, 102, 110),
               (107, 99, 100, 108),
               (105, 97, 98, 106), (111, 103, 96, 104), (117, 109, 110, 118), (115, 107, 108, 116),
               (113, 105, 106, 114),
               (119, 111, 104, 112), (126, 125, 124, 123, 122, 121, 120, 127), (134, 126, 127, 135),
               (131, 107, 115, 139),
               (132, 124, 125, 133),
               (120, 96, 103, 127), (130, 122, 123, 131), (129, 105, 113, 137),
               (128, 120, 121, 129), (122, 98, 97, 121),
               (142, 134, 135, 143), (125, 101, 109, 133), (140, 132, 133, 141),
               (135, 111, 119, 143), (138, 130, 131, 139),
               (124, 100, 99, 123), (136, 128, 129, 137), (123, 99, 107, 131),
               (158, 150, 151, 159), (157, 149, 150, 158),
               (156, 148, 149, 157), (155, 147, 148, 156), (154, 146, 147, 155),
               (153, 145, 146, 154), (152, 144, 145, 153),
               (159, 151, 144, 152), (197, 193, 167, 174), (26, 9, 163, 172),
               (196, 190, 162, 173), (9, 13, 175, 163),
               (192, 195, 171, 164), (23, 22, 160, 170), (195, 191, 161, 171),
               (11, 27, 168, 166), (193, 194, 169, 167),
               (27, 26, 172, 168), (173, 169, 177, 179), (198, 199, 179, 177),
               (168, 172, 178, 176), (196, 173, 179, 199),
               (185, 168, 176, 188), (160, 165, 183, 180), (172, 163, 181, 187),
               (170, 160, 180, 186), (166, 168, 185, 184),
               (176, 178, 189, 188), (172, 187, 189, 178), (209, 185, 188, 212),
               (222, 218, 193, 197), (221, 216, 190, 196),
               (220, 217, 191, 195), (218, 219, 194, 193), (199, 198, 202, 203),
               (221, 196, 199, 225), (169, 194, 198, 177),
               (226, 227, 203, 202), (225, 199, 203, 227), (212, 188, 200, 214),
               (188, 189, 201, 200), (219, 209, 212, 224),
               (180, 183, 207, 205), (187, 181, 204, 211), (182, 186, 210, 206),
               (186, 180, 205, 210), (184, 185, 209, 208),
               (187, 211, 213, 189), (200, 201, 215, 214), (189, 213, 215, 201),
               (224, 212, 214, 226), (211, 204, 216, 221),
               (210, 205, 217, 220), (208, 209, 219, 218), (211, 221, 225, 213),
               (227, 226, 230, 231), (213, 225, 227, 215),
               (194, 219, 224, 198), (198, 224, 226, 202), (228, 229, 231, 230),
               (215, 227, 231, 229), (226, 214, 228, 230),
               (214, 215, 229, 228), (24, 15, 2, 30), (15, 12, 3, 2), (16, 6, 14, 22), (161, 174, 15, 24),
               (6, 7, 11, 14), (8, 10, 1, 0), (21, 30, 2, 3), (19, 31, 5, 4), (4, 5, 13, 9),
               (162, 164, 10, 8), (25, 24, 30, 17), (5, 31, 23, 13), (31, 16, 22, 23), (0, 1, 17, 20),
               (20, 17, 30, 21), (7, 6, 16, 18), (18, 16, 31, 19), (40, 72, 79, 47), (47, 79, 78, 46),
               (46, 78, 77, 45), (45, 77, 76, 44), (44, 76, 75, 43), (43, 75, 74, 42), (42, 74, 73, 41),
               (41, 73, 72, 40), (79, 55, 54, 78), (77, 53, 52, 76), (75, 51, 50, 74), (73, 49, 48, 72),
               (118, 142, 143, 119), (116, 140, 141, 117), (114, 138, 139, 115),
               (112, 136, 137, 113), (150, 118, 119, 151),
               (149, 117, 118, 150), (148, 116, 117, 149), (147, 115, 116, 148),
               (146, 114, 115, 147), (145, 113, 114, 146),
               (144, 112, 113, 145), (151, 119, 112, 144), (22, 14, 165, 160),
               (191, 197, 174, 161), (14, 11, 166, 165),
               (190, 192, 164, 162), (13, 23, 170, 175), (165, 166, 184, 183),
               (163, 175, 182, 181), (175, 170, 186, 182),
               (217, 222, 197, 191), (216, 223, 192, 190), (223, 220, 195, 192),
               (183, 184, 208, 207), (181, 182, 206, 204),
               (205, 207, 222, 217), (207, 208, 218, 222), (204, 206, 223, 216), (206, 210, 220, 223)]

    mesh = bpy.data.meshes.new(objname)
    myobject = bpy.data.objects.new(objname, mesh)

    myobject.location = bpy.context.scene.cursor_location
    bpy.context.scene.objects.link(myobject)

    mesh.from_pydata(myvertex, [], myfaces)
    mesh.update(calc_edges=True)

    # Create materials
    if mat and bpy.context.scene.render.engine == 'CYCLES':
        plastic = create_diffuse_material("Plastic_Handle_material", False, 0.01, 0.01, 0.01, 0.082, 0.079, 0.02, 0.01)
        set_material(myobject, plastic)

    return myobject


# ------------------------------------------------------------------------------
# Create rectangular sill
#
# objName: Object name
# x: size x axis
# y: size y axis
# z: size z axis
# mat: material flag
# ------------------------------------------------------------------------------
def create_sill(objname, x, y, z, mat):
    myvertex = [(-x / 2, 0, 0.0),
                (-x / 2, y, 0.0),
                (x / 2, y, 0.0),
                (x / 2, 0, 0.0),
                (-x / 2, 0, -z),
                (-x / 2, y, -z),
                (x / 2, y, -z),
                (x / 2, 0, -z)]

    myfaces = [(0, 1, 2, 3), (0, 1, 5, 4), (1, 2, 6, 5), (2, 6, 7, 3), (5, 6, 7, 4), (0, 4, 7, 3)]

    mesh = bpy.data.meshes.new(objname)
    myobject = bpy.data.objects.new(objname, mesh)

    myobject.location = bpy.context.scene.cursor_location
    bpy.context.scene.objects.link(myobject)

    mesh.from_pydata(myvertex, [], myfaces)
    mesh.update(calc_edges=True)

    if mat and bpy.context.scene.render.engine == 'CYCLES':
        mymat = create_diffuse_material("Sill_material", False, 0.8, 0.8, 0.8)
        set_material(myobject, mymat)

    return myobject


# ------------------------------------------------------------------------------
# Create blind box
#
# objName: Object name
# x: size x axis
# y: size y axis
# z: size z axis
# mat: material flag
# ------------------------------------------------------------------------------
def create_blind_box(objname, x, y, z):
    myvertex = [(-x / 2, 0, 0.0),
                (-x / 2, y, 0.0),
                (x / 2, y, 0.0),
                (x / 2, 0, 0.0),
                (-x / 2, 0, z),
                (-x / 2, y, z),
                (x / 2, y, z),
                (x / 2, 0, z)]

    myfaces\
        = [(0, 1, 2, 3), (0, 1, 5, 4), (1, 2, 6, 5), (2, 6, 7, 3), (5, 6, 7, 4), (0, 4, 7, 3)]

    mesh = bpy.data.meshes.new(objname)
    myobject = bpy.data.objects.new(objname, mesh)

    myobject.location = bpy.context.scene.cursor_location
    bpy.context.scene.objects.link(myobject)

    mesh.from_pydata(myvertex, [], myfaces)
    mesh.update(calc_edges=True)

    return myobject


# ------------------------------------------------------------------------------
# Create blind rails
#
# objName: Name for the new object
# sX: Size in X axis
# sZ: Size in Z axis
# pX: position X axis
# pY: position Y axis
# pZ: position Z axis
# mat: Flag for creating materials
# matdata: Aluminum material
# blind_rail: distance of the rail
# ------------------------------------------------------------------------------
def create_blind_rail(objname, sx, sz, px, py, pz, mat, matdata, blind_rail):
    myvertex = []
    myfaces = []
    sideb = 0.04
    space = 0.012  # blind is 10 mm thick
    thicka = 0.002  # aluminum thickness
    thickb = 0.002  # aluminum thickness

    for x in (-sx / 2, sx / 2):
        for z in (0, sz):
            myvertex.extend([(x, 0, z),
                             (x, blind_rail, z),
                             (x + sideb, blind_rail, z),
                             (x + sideb, blind_rail - thicka, z),
                             (x + thickb, blind_rail - thicka, z),
                             (x + thickb, blind_rail - thicka - space, z),
                             (x + sideb, blind_rail - thicka - space, z),
                             (x + sideb, blind_rail - thicka - space - thicka, z),
                             (x + thickb, blind_rail - thicka - space - thicka, z),
                             (x + thickb, 0, z)])

        # reverse
        thickb *= -1
        sideb *= -1

    # Faces
    myfaces.extend([(31, 30, 20, 21), (32, 31, 21, 22), (33, 32, 22, 23), (37, 36, 26, 27), (35, 34, 24, 25),
                    (26, 36, 35, 25), (37, 27, 28, 38), (33, 23, 24, 34), (39, 38, 28, 29), (37, 38, 35, 36),
                    (31, 32, 33, 34), (31, 34, 39, 30), (21, 24, 23, 22), (27, 26, 25, 28), (21, 20, 29, 24),
                    (11, 1, 0, 10), (12, 2, 1, 11), (13, 14, 4, 3), (12, 13, 3, 2), (17, 7, 6, 16),
                    (16, 6, 5, 15), (14, 15, 5, 4), (17, 18, 8, 7), (19, 9, 8, 18), (17, 16, 15, 18),
                    (11, 14, 13, 12), (11, 10, 19, 14), (7, 8, 5, 6), (2, 3, 4, 1), (1, 4, 9, 0)])

    mymesh = bpy.data.meshes.new(objname)
    myblind = bpy.data.objects.new(objname, mymesh)

    myblind.location[0] = px
    myblind.location[1] = py
    myblind.location[2] = pz
    bpy.context.scene.objects.link(myblind)

    mymesh.from_pydata(myvertex, [], myfaces)
    mymesh.update(calc_edges=True)

    if mat and bpy.context.scene.render.engine == 'CYCLES':
        set_material(myblind, matdata)

    return myblind


# ------------------------------------------------------------------------------
# Create blind estructure
#
# objName: Name for the new object
# sX: Size in X axis
# sY: Size in Y axis
# sZ: Size in Z axis
# pX: position X axis
# pY: position Y axis
# pZ: position Z axis
# mat: Flag for creating materials
# blind_ratio: extension factor
# ------------------------------------------------------------------------------
def create_blind(objname, sx, sz, px, py, pz, mat, blind_ratio):
    myvertex = []
    myfaces = []
    v = 0
    h = 0.05
    railgap = 0.005
    # calculate total pieces
    pieces = int((sz * (blind_ratio / 100)) / h)
    if pieces * h < sz:
        pieces += 1

    z = h
    for p in range(pieces):
        for x in (-sx / 2, sx / 2):
            myvertex.extend([(x, 0, z),
                             (x, 0, z + h - railgap),
                             (x, 0.002, z + h - railgap),
                             (x, 0.002, z + h),
                             (x, 0.008, z + h),
                             (x, 0.008, z + h - railgap),
                             (x, 0.01, z + h - railgap),
                             (x, 0.01, z)])

        z -= h
        # Faces
        myfaces.extend([(v + 15, v + 7, v + 6, v + 14), (v + 7, v + 15, v + 8, v + 0),
                        (v + 9, v + 1, v + 0, v + 8),
                        (v + 10, v + 2, v + 1, v + 9), (v + 13, v + 14, v + 6, v + 5),
                        (v + 13, v + 5, v + 4, v + 12), (v + 10, v + 11, v + 3, v + 2),
                        (v + 4, v + 3, v + 11, v + 12)])
        v = len(myvertex)

    mymesh = bpy.data.meshes.new(objname)
    myblind = bpy.data.objects.new(objname, mymesh)

    myblind.location[0] = px
    myblind.location[1] = py
    myblind.location[2] = pz
    bpy.context.scene.objects.link(myblind)

    mymesh.from_pydata(myvertex, [], myfaces)
    mymesh.update(calc_edges=True)

    myblind.lock_location = (True, True, False)  # only Z axis

    if mat and bpy.context.scene.render.engine == 'CYCLES':
        mat = create_diffuse_material("Blind_plastic_material", False, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.15)
        set_material(myblind, mat)

    return myblind
