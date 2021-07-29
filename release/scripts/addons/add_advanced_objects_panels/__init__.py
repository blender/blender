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

# Contributed to by:
# meta-androcto, Bill Currie, Jorge Hernandez - Melenedez  Jacob Morris, Oscurart  #
# Rebellion, Antonis Karvelas, Eleanor Howick, lijenstina, Daniel Schalla, Domlysz #
# Unnikrishnan(kodemax), Florian Meyer, Omar ahmed, Brian Hinton (Nichod), liero   #
# Atom, Dannyboy, Mano-Wii, Kursad Karatas, teldredge, Phil Cote #

bl_info = {
    "name": "Add Advanced Object Panels",
    "author": "meta-androcto",
    "version": (1, 1, 5),
    "blender": (2, 7, 7),
    "description": "Individual Create Panel Activation List",
    "location": "Addons Preferences",
    "warning": "",
    "wiki_url": "https://wiki.blender.org/index.php/Extensions:2.6"
                "/Py/Scripts/Object/Add_Advanced",
    "category": "Object"
    }

import bpy
from bpy.types import (
        AddonPreferences,
        PropertyGroup,
        )
from bpy.props import (
        BoolProperty,
        BoolVectorProperty,
        EnumProperty,
        FloatProperty,
        FloatVectorProperty,
        IntProperty,
        StringProperty,
        PointerProperty,
        )

sub_modules_names = (
    "drop_to_ground",
    "object_laplace_lightning",
    "object_mangle_tools",
    "unfold_transition",
    "delaunay_voronoi",
    "oscurart_constellation",
    )


sub_modules = [__import__(__package__ + "." + submod, {}, {}, submod) for submod in sub_modules_names]
sub_modules.sort(key=lambda mod: (mod.bl_info['category'], mod.bl_info['name']))


# Add-ons Preferences
def _get_pref_class(mod):
    import inspect

    for obj in vars(mod).values():
        if inspect.isclass(obj) and issubclass(obj, PropertyGroup):
            if hasattr(obj, 'bl_idname') and obj.bl_idname == mod.__name__:
                return obj


def get_addon_preferences(name=''):
    """Acquisition and registration"""
    addons = bpy.context.user_preferences.addons
    if __name__ not in addons:  # wm.read_factory_settings()
        return None
    addon_prefs = addons[__name__].preferences
    if name:
        if not hasattr(addon_prefs, name):
            for mod in sub_modules:
                if mod.__name__.split('.')[-1] == name:
                    cls = _get_pref_class(mod)
                    if cls:
                        prop = PointerProperty(type=cls)
                        setattr(AdvancedObjPreferences1, name, prop)
                        bpy.utils.unregister_class(AdvancedObjPreferences1)
                        bpy.utils.register_class(AdvancedObjPreferences1)
        return getattr(addon_prefs, name, None)
    else:
        return addon_prefs


def register_submodule(mod):
    if not hasattr(mod, '__addon_enabled__'):
        mod.__addon_enabled__ = False
    if not mod.__addon_enabled__:
        mod.register()
        mod.__addon_enabled__ = True


def unregister_submodule(mod):
    if mod.__addon_enabled__:
        mod.unregister()
        mod.__addon_enabled__ = False

        prefs = get_addon_preferences()
        name = mod.__name__.split('.')[-1]
        if hasattr(AdvancedObjPreferences1, name):
            delattr(AdvancedObjPreferences1, name)
            if prefs:
                bpy.utils.unregister_class(AdvancedObjPreferences1)
                bpy.utils.register_class(AdvancedObjPreferences1)
                if name in prefs:
                    del prefs[name]


def enable_all_modules(self, context):
    for mod in sub_modules:
        mod_name = mod.__name__.split('.')[-1]
        setattr(self, 'use_' + mod_name, False)
        if not mod.__addon_enabled__:
            setattr(self, 'use_' + mod_name, True)
            mod.__addon_enabled__ = True

    return None


def disable_all_modules(self, context):
    for mod in sub_modules:
        mod_name = mod.__name__.split('.')[-1]

        if mod.__addon_enabled__:
            setattr(self, 'use_' + mod_name, False)
            mod.__addon_enabled__ = False

    return None


class AdvancedObjPreferences1(AddonPreferences):
    bl_idname = __name__

    enable_all = BoolProperty(
            name="Enable all",
            description="Enable all Advanced Objects' Panels",
            default=False,
            update=enable_all_modules
            )
    disable_all = BoolProperty(
            name="Disable all",
            description="Disable all Advanced Objects' Panels",
            default=False,
            update=disable_all_modules
            )

    def draw(self, context):
        layout = self.layout
        split = layout.split(percentage=0.5, align=True)
        row = split.row()
        row.alignment = "LEFT"
        sub_box = row.box()
        sub_box.prop(self, "enable_all", emboss=False,
                    icon="VISIBLE_IPO_ON", icon_only=True)
        row.label("Enable All")

        row = split.row()
        row.alignment = "RIGHT"
        row.label("Disable All")
        sub_box = row.box()
        sub_box.prop(self, "disable_all", emboss=False,
                    icon="VISIBLE_IPO_OFF", icon_only=True)

        for mod in sub_modules:
            mod_name = mod.__name__.split('.')[-1]
            info = mod.bl_info
            column = layout.column()
            box = column.box()

            # first stage
            expand = getattr(self, 'show_expanded_' + mod_name)
            icon = 'TRIA_DOWN' if expand else 'TRIA_RIGHT'
            col = box.column()
            row = col.row()
            sub = row.row()
            sub.context_pointer_set('addon_prefs', self)
            op = sub.operator('wm.context_toggle', text='', icon=icon,
                              emboss=False)
            op.data_path = 'addon_prefs.show_expanded_' + mod_name
            sub.label('{}: {}'.format(info['category'], info['name']))
            sub = row.row()
            sub.alignment = 'RIGHT'
            if info.get('warning'):
                sub.label('', icon='ERROR')
            sub.prop(self, 'use_' + mod_name, text='')

            # The second stage
            if expand:
                if info.get('description'):
                    split = col.row().split(percentage=0.15)
                    split.label('Description:')
                    split.label(info['description'])
                if info.get('location'):
                    split = col.row().split(percentage=0.15)
                    split.label('Location:')
                    split.label(info['location'])
                if info.get('author'):
                    split = col.row().split(percentage=0.15)
                    split.label('Author:')
                    split.label(info['author'])
                if info.get('version'):
                    split = col.row().split(percentage=0.15)
                    split.label('Version:')
                    split.label('.'.join(str(x) for x in info['version']),
                                translate=False)
                if info.get('warning'):
                    split = col.row().split(percentage=0.15)
                    split.label('Warning:')
                    split.label('  ' + info['warning'], icon='ERROR')

                tot_row = int(bool(info.get('wiki_url')))
                if tot_row:
                    split = col.row().split(percentage=0.15)
                    split.label(text='Internet:')
                    if info.get('wiki_url'):
                        op = split.operator('wm.url_open',
                                            text='Documentation', icon='HELP')
                        op.url = info.get('wiki_url')
                    for i in range(4 - tot_row):
                        split.separator()

                # Details and settings
                if getattr(self, 'use_' + mod_name):
                    prefs = get_addon_preferences(mod_name)

                    if prefs and hasattr(prefs, 'draw'):
                        box = box.column()
                        prefs.layout = box
                        try:
                            prefs.draw(context)
                        except:
                            import traceback
                            traceback.print_exc()
                            box.label(text="Error (see console)", icon="ERROR")
                        del prefs.layout

        row = layout.row()
        row.label(text="End of Advanced Object Panels Activations",
                  icon="FILE_PARENT")


for mod in sub_modules:
    info = mod.bl_info
    mod_name = mod.__name__.split('.')[-1]

    def gen_update(mod):
        def update(self, context):
            if getattr(self, 'use_' + mod.__name__.split('.')[-1]):
                if not mod.__addon_enabled__:
                    register_submodule(mod)
            else:
                if mod.__addon_enabled__:
                    unregister_submodule(mod)
        return update

    prop = BoolProperty(
            name=info['name'],
            description=info.get('description', ''),
            update=gen_update(mod),
            )
    setattr(AdvancedObjPreferences1, 'use_' + mod_name, prop)
    prop = BoolProperty()
    setattr(AdvancedObjPreferences1, 'show_expanded_' + mod_name, prop)


class AdvancedObjProperties1(PropertyGroup):

    # object_laplace_lighting props
    ORIGIN = FloatVectorProperty(
            name="Origin charge"
            )
    GROUNDZ = IntProperty(
            name="Ground Z coordinate"
            )
    HORDER = IntProperty(
            name="Secondary paths orders",
            default=1
            )
    # object_laplace_lighting UI props
    TSTEPS = IntProperty(
            name="Iterations",
            default=350,
            description="Number of cells to create\n"
                        "Will end early if hits ground plane or cloud"
            )
    GSCALE = FloatProperty(
            name="Grid unit size",
            default=0.12,
            description="scale of cells, .25 = 4 cells per blenderUnit"
            )
    BIGVAR = FloatProperty(
            name="Straightness",
            default=6.3,
            description="Straightness/branchiness of bolt, \n"
                        "<2 is mush, >12 is staight line, 6.3 is good"
            )
    GROUNDBOOL = BoolProperty(
            name="Use Ground object",
            description="Use ground plane or not",
            default=True
            )
    GROUNDC = IntProperty(
            name="Ground charge",
            default=-250,
            description="Charge of the ground plane"
            )
    CLOUDBOOL = BoolProperty(
            name="Use Cloud object",
            default=False,
            description="Use cloud object - attracts and terminates like ground but\n"
                        "any obj instead of z plane\n"
                        "Can slow down loop if obj is large, overrides ground"
            )
    CLOUDC = IntProperty(
            name="Cloud charge",
            default=-1,
            description="Charge of a cell in cloud object\n"
                        "(so total charge also depends on obj size)"
            )
    VMMESH = BoolProperty(
            name="Multi mesh",
            default=True,
            description="Output to multi-meshes for different materials on main/sec/side branches"
            )
    VSMESH = BoolProperty(
            name="Single mesh",
            default=False,
            description="Output to single mesh for using build modifier and particles for effects"
            )
    VCUBE = BoolProperty(
            name="Cubes",
            default=False,
            description="CTRL-J after run to JOIN\n"
                        "Outputs a bunch of cube objects, mostly for testing"
            )
    VVOX = BoolProperty(
            name="Voxel (experimental)",
            default=False,
            description="Output to a voxel file to bpy.data.filepath\FSLGvoxels.raw\n"
                        "(doesn't work well right now)"
            )
    IBOOL = BoolProperty(
            name="Use Insulator object",
            default=False,
            description="Use insulator mesh object to prevent growth of bolt in areas"
            )
    OOB = StringProperty(
            name="Select",
            default="",
            description="Origin of bolt, can be an Empty\n"
                        "if object is a mesh will use all verts as charges")
    GOB = StringProperty(
            name="Select",
            default="",
            description="Object to use as ground plane, uses z coord only"
            )
    COB = StringProperty(
            name="Select",
            default="",
            description="Object to use as cloud, best to use a cube"
            )
    IOB = StringProperty(
            name="Select",
            default="",
            description="Object to use as insulator, 'voxelized'\n"
                        "before generating bolt (can be slow)"
            )
    # object_mangle_tools properties
    mangle_constraint_vector = BoolVectorProperty(
            name="Mangle Constraint",
            default=(True, True, True),
            subtype='XYZ',
            description="Constrains Mangle Direction"
            )
    mangle_random_magnitude = IntProperty(
            name="Mangle Severity",
            default=5,
            min=1, max=30,
            description="Severity of mangling"
            )
    mangle_name = StringProperty(
            name="Shape Key Name",
            default="mangle",
            description="Name given for mangled shape keys"
            )
    # unfold_transition properties
    unfold_arm_name = StringProperty(
            default=""
            )
    unfold_modo = EnumProperty(
            name="",
            items=[("cursor", "3D Cursor", "Use the Distance to 3D Cursor"),
                   ("weight", "Weight Map", "Use a Painted Weight map"),
                   ("index", "Mesh Indices", "Use Faces and Vertices index")],
            description="How to Sort Bones for animation", default="cursor"
            )
    unfold_flip = BoolProperty(
            name="Flipping Faces",
            default=False,
            description="Rotate faces around the Center and skip Scaling - "
                        "keep checked for both operators"
            )
    unfold_fold_duration = IntProperty(
            name="Total Time",
            min=5, soft_min=25,
            max=10000, soft_max=2500,
            default=200,
            description="Total animation length"
            )
    unfold_sca_time = IntProperty(
            name="Scale Time",
            min=1,
            max=5000, soft_max=500,
            default=10,
            description="Faces scaling time"
            )
    unfold_rot_time = IntProperty(
            name="Rotation Time",
            min=1, soft_min=5,
            max=5000, soft_max=500,
            default=15,
            description="Faces rotation time"
            )
    unfold_rot_max = IntProperty(
            name="Angle",
            min=-180,
            max=180,
            default=135,
            description="Faces rotation angle"
            )
    unfold_fold_noise = IntProperty(
            name="Noise",
            min=0,
            max=500, soft_max=50,
            default=0,
            description="Offset some faces animation"
            )
    unfold_bounce = FloatProperty(
            name="Bounce",
            min=0,
            max=10, soft_max=2.5,
            default=0,
            description="Add some bounce to rotation"
            )
    unfold_from_point = BoolProperty(
            name="Point",
            default=False,
            description="Scale faces from a Point instead of from an Edge"
            )
    unfold_wiggle_rot = BoolProperty(
            name="Wiggle",
            default=False,
            description="Use all Axis + Random Rotation instead of X Aligned"
            )
    # oscurart_constellation
    constellation_limit = FloatProperty(
            name="Inital Threshold",
            description="Edges will be created only if the distance\n"
                        "between vertices is smaller than this value\n"
                        "This is a starting value on Operator Invoke",
            default=2,
            min=0
            )


# Class list
classes = (
    AdvancedObjPreferences1,
    AdvancedObjProperties1,
    )


def register():
    for cls in classes:
        bpy.utils.register_class(cls)

    bpy.types.Scene.advanced_objects1 = PointerProperty(
                                            type=AdvancedObjProperties1
                                            )

    prefs = get_addon_preferences()
    for mod in sub_modules:
        if not hasattr(mod, '__addon_enabled__'):
            mod.__addon_enabled__ = False
        name = mod.__name__.split('.')[-1]
        if getattr(prefs, 'use_' + name):
            register_submodule(mod)


def unregister():
    for mod in sub_modules:
        if mod.__addon_enabled__:
            unregister_submodule(mod)
    del bpy.types.Scene.advanced_objects1

    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)


if __name__ == "__main__":
    register()
