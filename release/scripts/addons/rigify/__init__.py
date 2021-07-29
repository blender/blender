#====================== BEGIN GPL LICENSE BLOCK ======================
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
#======================= END GPL LICENSE BLOCK ========================

# <pep8 compliant>

bl_info = {
    "name": "Rigify",
    "version": (0, 5),
    "author": "Nathan Vegdahl, Lucio Rossi, Ivan Cappiello",
    "blender": (2, 78, 0),
    "description": "Automatic rigging from building-block components",
    "location": "Armature properties, Bone properties, View3d tools panel, Armature Add menu",
    "wiki_url": "http://wiki.blender.org/index.php/Extensions:2.5/Py/"
                "Scripts/Rigging/Rigify",
    "category": "Rigging"}


if "bpy" in locals():
    import importlib
    importlib.reload(generate)
    importlib.reload(ui)
    importlib.reload(utils)
    importlib.reload(metarig_menu)
    importlib.reload(rig_lists)
else:
    from . import utils, rig_lists, generate, ui, metarig_menu

import bpy
import sys
import os
from bpy.types import AddonPreferences
from bpy.props import BoolProperty


class RigifyPreferences(AddonPreferences):
    # this must match the addon name, use '__package__'
    # when defining this in a submodule of a python package.
    bl_idname = __name__

    def update_legacy(self, context):
        if self.legacy_mode:

            if 'ui' in globals() and 'legacy' in str(globals()['ui']):    # already in legacy mode. needed when rigify is reloaded
                return
            else:
                rigify_dir = os.path.dirname(os.path.realpath(__file__))
                if rigify_dir not in sys.path:
                    sys.path.append(rigify_dir)

                unregister()

                globals().pop('utils')
                globals().pop('rig_lists')
                globals().pop('generate')
                globals().pop('ui')
                globals().pop('metarig_menu')

                import legacy.utils
                import legacy.rig_lists
                import legacy.generate
                import legacy.ui
                import legacy.metarig_menu

                print("ENTERING RIGIFY LEGACY\r\n")

                globals()['utils'] = legacy.utils
                globals()['rig_lists'] = legacy.rig_lists
                globals()['generate'] = legacy.generate
                globals()['ui'] = legacy.ui
                globals()['metarig_menu'] = legacy.metarig_menu

                register()

        else:

            rigify_dir = os.path.dirname(os.path.realpath(__file__))

            if rigify_dir in sys.path:
                id = sys.path.index(rigify_dir)
                sys.path.pop(id)

            unregister()

            globals().pop('utils')
            globals().pop('rig_lists')
            globals().pop('generate')
            globals().pop('ui')
            globals().pop('metarig_menu')

            from . import utils
            from . import rig_lists
            from . import generate
            from . import ui
            from . import metarig_menu

            print("EXIT RIGIFY LEGACY\r\n")

            globals()['utils'] = utils
            globals()['rig_lists'] = rig_lists
            globals()['generate'] = generate
            globals()['ui'] = ui
            globals()['metarig_menu'] = metarig_menu

            register()

    legacy_mode = BoolProperty(
        name='Rigify Legacy Mode',
        description='Select if you want to use Rigify in legacy mode',
        default=False,
        update=update_legacy
    )

    show_expanded = BoolProperty()

    def draw(self, context):
        layout = self.layout
        column = layout.column()
        box = column.box()

        # first stage
        expand = getattr(self, 'show_expanded')
        icon = 'TRIA_DOWN' if expand else 'TRIA_RIGHT'
        col = box.column()
        row = col.row()
        sub = row.row()
        sub.context_pointer_set('addon_prefs', self)
        sub.alignment = 'LEFT'
        op = sub.operator('wm.context_toggle', text='', icon=icon,
                          emboss=False)
        op.data_path = 'addon_prefs.show_expanded'
        sub.label('{}: {}'.format('Rigify', 'Enable Legacy Mode'))
        sub = row.row()
        sub.alignment = 'RIGHT'
        sub.prop(self, 'legacy_mode')

        if expand:
            split = col.row().split(percentage=0.15)
            split.label('Description:')
            split.label(text='When enabled the add-on will run in legacy mode using the old 2.76b feature set.')

        row = layout.row()
        row.label("End of Rigify Preferences")


class RigifyName(bpy.types.PropertyGroup):
    name = bpy.props.StringProperty()


class RigifyColorSet(bpy.types.PropertyGroup):
    name = bpy.props.StringProperty(name="Color Set", default=" ")
    active = bpy.props.FloatVectorProperty(
                                   name="object_color",
                                   subtype='COLOR',
                                   default=(1.0, 1.0, 1.0),
                                   min=0.0, max=1.0,
                                   description="color picker"
                                   )
    normal = bpy.props.FloatVectorProperty(
                                   name="object_color",
                                   subtype='COLOR',
                                   default=(1.0, 1.0, 1.0),
                                   min=0.0, max=1.0,
                                   description="color picker"
                                   )
    select = bpy.props.FloatVectorProperty(
                                   name="object_color",
                                   subtype='COLOR',
                                   default=(1.0, 1.0, 1.0),
                                   min=0.0, max=1.0,
                                   description="color picker"
                                   )
    standard_colors_lock = bpy.props.BoolProperty(default=True)


class RigifySelectionColors(bpy.types.PropertyGroup):

    select = bpy.props.FloatVectorProperty(
                                           name="object_color",
                                           subtype='COLOR',
                                           default=(0.314, 0.784, 1.0),
                                           min=0.0, max=1.0,
                                           description="color picker"
                                           )

    active = bpy.props.FloatVectorProperty(
                                           name="object_color",
                                           subtype='COLOR',
                                           default=(0.549, 1.0, 1.0),
                                           min=0.0, max=1.0,
                                           description="color picker"
                                           )


class RigifyParameters(bpy.types.PropertyGroup):
    name = bpy.props.StringProperty()


class RigifyArmatureLayer(bpy.types.PropertyGroup):

    def get_group(self):
        if 'group_prop' in self.keys():
            return self['group_prop']
        else:
            return 0

    def set_group(self, value):
        arm = bpy.context.object.data
        if value > len(arm.rigify_colors):
            self['group_prop'] = len(arm.rigify_colors)
        else:
            self['group_prop'] = value

    name = bpy.props.StringProperty(name="Layer Name", default=" ")
    row = bpy.props.IntProperty(name="Layer Row", default=1, min=1, max=32, description='UI row for this layer')
    set = bpy.props.BoolProperty(name="Selection Set", default=False, description='Add Selection Set for this layer')
    group = bpy.props.IntProperty(name="Bone Group", default=0, min=0, max=32,
                                  get=get_group, set=set_group, description='Assign Bone Group to this layer')

##### REGISTER #####

def register():
    ui.register()
    metarig_menu.register()

    bpy.utils.register_class(RigifyName)
    bpy.utils.register_class(RigifyParameters)

    bpy.utils.register_class(RigifyColorSet)
    bpy.utils.register_class(RigifySelectionColors)
    bpy.utils.register_class(RigifyArmatureLayer)
    bpy.utils.register_class(RigifyPreferences)
    bpy.types.Armature.rigify_layers = bpy.props.CollectionProperty(type=RigifyArmatureLayer)

    bpy.types.PoseBone.rigify_type = bpy.props.StringProperty(name="Rigify Type", description="Rig type for this bone")
    bpy.types.PoseBone.rigify_parameters = bpy.props.PointerProperty(type=RigifyParameters)

    bpy.types.Armature.rigify_colors = bpy.props.CollectionProperty(type=RigifyColorSet)

    bpy.types.Armature.rigify_selection_colors = bpy.props.PointerProperty(type=RigifySelectionColors)

    bpy.types.Armature.rigify_colors_index = bpy.props.IntProperty(default=-1)
    bpy.types.Armature.rigify_colors_lock = bpy.props.BoolProperty(default=True)
    bpy.types.Armature.rigify_theme_to_add = bpy.props.EnumProperty(items=(('THEME01', 'THEME01', ''),
                                                                          ('THEME02', 'THEME02', ''),
                                                                          ('THEME03', 'THEME03', ''),
                                                                          ('THEME04', 'THEME04', ''),
                                                                          ('THEME05', 'THEME05', ''),
                                                                          ('THEME06', 'THEME06', ''),
                                                                          ('THEME07', 'THEME07', ''),
                                                                          ('THEME08', 'THEME08', ''),
                                                                          ('THEME09', 'THEME09', ''),
                                                                          ('THEME10', 'THEME10', ''),
                                                                          ('THEME11', 'THEME11', ''),
                                                                          ('THEME12', 'THEME12', ''),
                                                                          ('THEME13', 'THEME13', ''),
                                                                          ('THEME14', 'THEME14', ''),
                                                                          ('THEME15', 'THEME15', ''),
                                                                          ('THEME16', 'THEME16', ''),
                                                                          ('THEME17', 'THEME17', ''),
                                                                          ('THEME18', 'THEME18', ''),
                                                                          ('THEME19', 'THEME19', ''),
                                                                          ('THEME20', 'THEME20', '')
                                                                           ), name='Theme')

    IDStore = bpy.types.WindowManager
    IDStore.rigify_collection = bpy.props.EnumProperty(items=rig_lists.col_enum_list, default="All",
                                                       name="Rigify Active Collection",
                                                       description="The selected rig collection")

    IDStore.rigify_types = bpy.props.CollectionProperty(type=RigifyName)
    IDStore.rigify_active_type = bpy.props.IntProperty(name="Rigify Active Type", description="The selected rig type")

    IDStore.rigify_advanced_generation = bpy.props.BoolProperty(name="Advanced Options",
                                                                description="Enables/disables advanced options for Rigify rig generation",
                                                                default=False)

    def update_mode(self, context):
        if self.rigify_generate_mode == 'new':
            self.rigify_force_widget_update = False

    IDStore.rigify_generate_mode = bpy.props.EnumProperty(name="Rigify Generate Rig Mode",
                                                          description="'Generate Rig' mode. In 'overwrite' mode the features of the target rig will be updated as defined by the metarig. In 'new' mode a new rig will be created as defined by the metarig. Current mode",
                                                          update=update_mode,
                                                          items=(('overwrite', 'overwrite', ''),
                                                                 ('new', 'new', '')))

    IDStore.rigify_force_widget_update = bpy.props.BoolProperty(name="Force Widget Update",
                                                                description="Forces Rigify to delete and rebuild all the rig widgets. if unset, only missing widgets will be created",
                                                                default=False)

    IDStore.rigify_target_rigs = bpy.props.CollectionProperty(type=RigifyName)
    IDStore.rigify_target_rig = bpy.props.StringProperty(name="Rigify Target Rig",
                                                         description="Defines which rig to overwrite. If unset, a new one called 'rig' will be created",
                                                         default="")

    IDStore.rigify_rig_uis = bpy.props.CollectionProperty(type=RigifyName)
    IDStore.rigify_rig_ui = bpy.props.StringProperty(name="Rigify Target Rig UI",
                                                         description="Defines the UI to overwrite. It should always be the same as the target rig. If unset, 'rig_ui.py' will be used",
                                                         default="")

    IDStore.rigify_rig_basename = bpy.props.StringProperty(name="Rigify Rig Name",
                                                     description="Defines the name of the Rig. If unset, in 'new' mode 'rig' will be used, in 'overwrite' mode the target rig name will be used",
                                                     default="")

    IDStore.rigify_transfer_only_selected = bpy.props.BoolProperty(name="Transfer Only Selected", description="Transfer selected bones only", default=True)
    IDStore.rigify_transfer_start_frame = bpy.props.IntProperty(name="Start Frame", description="First Frame to Transfer", default=0, min= 0)
    IDStore.rigify_transfer_end_frame = bpy.props.IntProperty(name="End Frame", description="Last Frame to Transfer", default=0, min= 0)

    if (ui and 'legacy' in str(ui)) or bpy.context.user_preferences.addons['rigify'].preferences.legacy_mode:
        # update legacy on restart or reload
        bpy.context.user_preferences.addons['rigify'].preferences.legacy_mode = True

    # Add rig parameters
    for rig in rig_lists.rig_list:
        r = utils.get_rig_type(rig)
        try:
            r.add_parameters(RigifyParameters)
        except AttributeError:
            pass


def unregister():
    del bpy.types.PoseBone.rigify_type
    del bpy.types.PoseBone.rigify_parameters

    IDStore = bpy.types.WindowManager
    del IDStore.rigify_collection
    del IDStore.rigify_types
    del IDStore.rigify_active_type
    del IDStore.rigify_advanced_generation
    del IDStore.rigify_generate_mode
    del IDStore.rigify_force_widget_update
    del IDStore.rigify_target_rig
    del IDStore.rigify_target_rigs
    del IDStore.rigify_rig_uis
    del IDStore.rigify_rig_ui
    del IDStore.rigify_rig_basename
    del IDStore.rigify_transfer_only_selected
    del IDStore.rigify_transfer_start_frame
    del IDStore.rigify_transfer_end_frame

    bpy.utils.unregister_class(RigifyName)
    bpy.utils.unregister_class(RigifyParameters)

    bpy.utils.unregister_class(RigifyColorSet)
    bpy.utils.unregister_class(RigifySelectionColors)

    bpy.utils.unregister_class(RigifyArmatureLayer)
    bpy.utils.unregister_class(RigifyPreferences)

    metarig_menu.unregister()
    ui.unregister()
