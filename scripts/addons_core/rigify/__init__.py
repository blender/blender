# SPDX-FileCopyrightText: 2010-2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

bl_info = {
    "name": "Rigify",
    "version": (0, 6, 10),
    # This is now displayed as the maintainer, so show the foundation.
    # "author": "Nathan Vegdahl, Lucio Rossi, Ivan Cappiello, Alexander Gavrilov", # Original Authors
    "author": "Blender Foundation",
    "blender": (4, 0, 0),
    "description": "Automatic rigging from building-block components",
    "location": "Armature properties, Bone properties, View3d tools panel, Armature Add menu",
    "doc_url": "{BLENDER_MANUAL_URL}/addons/rigging/rigify/index.html",
    "support": "OFFICIAL",
    "category": "Rigging",
}

import importlib
import sys
import bpy
import typing

from bpy.app.translations import (
    pgettext_iface as iface_,
    pgettext_rpt as rpt_,
)

# The order in which core modules of the addon are loaded and reloaded.
# Modules not in this list are removed from memory upon reload.
# With the sole exception of 'utils', modules must be listed in the
# correct dependency order.
initial_load_order = [
    'utils.errors',
    'utils.misc',
    'utils.rig',
    'utils.naming',
    'utils.bones',
    'utils.collections',
    'utils.layers',
    'utils.widgets',
    'utils.widgets_basic',
    'utils.widgets_special',
    'utils',
    'utils.mechanism',
    'utils.animation',
    'utils.metaclass',
    'utils.objects',
    'feature_sets',
    'rigs',
    'rigs.utils',
    'base_rig',
    'base_generate',
    'feature_set_list',
    'rig_lists',
    'metarig_menu',
    'rig_ui_template',
    'utils.action_layers',
    'generate',
    'rot_mode',
    'operators',
    'ui',
]


def get_loaded_modules():
    prefix = __name__ + '.'
    return [name for name in sys.modules if name.startswith(prefix)]


def reload_modules():
    fixed_modules = set(reload_list)

    for name in get_loaded_modules():
        if name not in fixed_modules:
            del sys.modules[name]

    for name in reload_list:
        importlib.reload(sys.modules[name])


def compare_module_list(a: list[str], b: list[str]):
    # HACK: ignore the "utils" module when comparing module load orders,
    # because it is inconsistent for reasons unknown.
    # See rBAa918332cc3f821f5a70b1de53b65dd9ca596b093.
    utils_module_name = __name__ + '.utils'
    a_copy = list(a)
    a_copy.remove(utils_module_name)
    b_copy = list(b)
    b_copy.remove(utils_module_name)
    return a_copy == b_copy


def load_initial_modules() -> list[str]:
    names = [__name__ + '.' + name for name in initial_load_order]

    for i, name in enumerate(names):
        importlib.import_module(name)

        module_list = get_loaded_modules()
        expected_list = names[0: max(11, i + 1)]

        if not compare_module_list(module_list, expected_list):
            print(f'!!! RIGIFY: initial load order mismatch after {name} - expected: \n',
                  expected_list, '\nGot:\n', module_list)

    return names


def load_rigs():
    rig_lists.get_internal_rigs()
    metarig_menu.init_metarig_menu()


if "reload_list" in locals():
    reload_modules()
else:
    load_list = load_initial_modules()

    from . import (utils, base_rig, base_generate, rig_ui_template, feature_set_list, rig_lists,
                   generate, ui, metarig_menu, operators)

    reload_list = reload_list_init = get_loaded_modules()

    if not compare_module_list(reload_list, load_list):
        print('!!! RIGIFY: initial load order mismatch - expected: \n',
              load_list, '\nGot:\n', reload_list)

load_rigs()


from bpy.types import AddonPreferences  # noqa: E402
from bpy.props import (                 # noqa: E402
    BoolProperty,
    IntProperty,
    EnumProperty,
    StringProperty,
    FloatVectorProperty,
    PointerProperty,
    CollectionProperty,
)


def get_generator():
    """Returns the currently active generator instance."""
    return base_generate.BaseGenerator.instance


class RigifyFeatureSets(bpy.types.PropertyGroup):
    name: bpy.props.StringProperty()
    module_name: bpy.props.StringProperty()
    link: bpy.props.StringProperty()
    has_errors: bpy.props.BoolProperty()
    has_exceptions: bpy.props.BoolProperty()

    def toggle_feature_set(self, context):
        feature_set_list.call_register_function(self.module_name, self.enabled)
        RigifyPreferences.get_instance(context).update_external_rigs()

    enabled: bpy.props.BoolProperty(
        name="Enabled",
        description="Whether this feature-set is registered or not",
        update=toggle_feature_set,
        default=True
    )


# noinspection PyPep8Naming
class RIGIFY_UL_FeatureSets(bpy.types.UIList):
    def draw_item(self, context, layout, data, item, icon, active_data, active_propname, _index=0, _flag=0):
        # rigify_prefs: RigifyPreferences = data
        # feature_sets = rigify_prefs.rigify_feature_sets
        # active_set: RigifyFeatureSets = feature_sets[rigify_prefs.active_feature_set_index]
        feature_set_entry: RigifyFeatureSets = item
        row = layout.row()

        name = feature_set_entry.name
        icon = "BLANK1"

        if not feature_set_entry.module_name:
            name += iface_(" (not installed)")
            icon = "URL"
        elif feature_set_entry.has_errors or feature_set_entry.has_exceptions:
            icon = "ERROR"
            row.alert = True

        row.label(text=name, icon=icon, translate=False)

        if feature_set_entry.module_name:
            icon = 'CHECKBOX_HLT' if feature_set_entry.enabled else 'CHECKBOX_DEHLT'
            row.enabled = feature_set_entry.enabled
            layout.prop(feature_set_entry, 'enabled', text="", icon=icon, emboss=False)
        else:
            row.enabled = False


class RigifyPreferences(AddonPreferences):
    # this must match the addon name, use '__package__'
    # when defining this in a submodule of a python package.
    bl_idname = __name__

    @staticmethod
    def get_instance(context: bpy.types.Context = None) -> 'RigifyPreferences':
        prefs = (context or bpy.context).preferences.addons[__package__].preferences
        assert isinstance(prefs, RigifyPreferences)
        return prefs

    def register_feature_sets(self, do_register: bool):
        """Call register or unregister of external feature sets"""
        self.refresh_installed_feature_sets()
        for set_name in feature_set_list.get_enabled_modules_names():
            feature_set_list.call_register_function(set_name, do_register)

    def refresh_installed_feature_sets(self):
        """Synchronize preferences entries with what's actually in the file system."""
        feature_set_prefs = self.rigify_feature_sets

        module_names = feature_set_list.get_installed_modules_names()

        # If there is a feature set preferences entry with no corresponding
        # installed module, user must've manually removed it from the filesystem,
        # so let's remove such entries.
        to_delete = [i for i, fs in enumerate(feature_set_prefs)
                     if fs.module_name not in module_names]
        for i in reversed(to_delete):
            feature_set_prefs.remove(i)

        # If there is an installed feature set in the file system but no corresponding
        # entry, user must've installed it manually. Make sure it has an entry.
        for module_name in module_names:
            for fs in feature_set_prefs:
                if module_name == fs.module_name:
                    break
            else:
                fs = feature_set_prefs.add()
                fs.name = feature_set_list.get_ui_name(module_name)
                fs.module_name = module_name

        # Update the feature set info
        fs_info = [feature_set_list.get_info_dict(fs.module_name) for fs in feature_set_prefs]

        for fs, info in zip(feature_set_prefs, fs_info):
            if "name" in info:
                fs.name = info["name"]
            if "link" in info:
                fs.link = info["link"]

        for fs, info in zip(feature_set_prefs, fs_info):
            fs.has_errors = check_feature_set_error(fs, info, None)
            fs.has_exceptions = False

        # Add dummy entries for promoted feature sets
        used_links = set(fs.link for fs in feature_set_prefs)

        for info in feature_set_list.PROMOTED_FEATURE_SETS:
            if info["link"] not in used_links:
                fs = feature_set_prefs.add()
                fs.module_name = ""
                fs.name = info["name"]
                fs.link = info["link"]

    @staticmethod
    def update_external_rigs():
        """Get external feature sets"""

        set_list = feature_set_list.get_enabled_modules_names()

        # Reload rigs
        print('Reloading external rigs...')
        rig_lists.get_external_rigs(set_list)

        # Reload metarigs
        print('Reloading external metarigs...')
        metarig_menu.get_external_metarigs(set_list)

        # Re-register rig parameters
        register_rig_parameters()

    rigify_feature_sets: bpy.props.CollectionProperty(type=RigifyFeatureSets)
    active_feature_set_index: IntProperty()

    def draw(self, context: bpy.types.Context):
        layout: bpy.types.UILayout = self.layout

        layout.label(text="Feature Sets:")

        layout.operator("wm.rigify_add_feature_set", text="Install Feature Set from File...", icon='FILEBROWSER')

        row = layout.row()
        row.template_list(
            'RIGIFY_UL_FeatureSets',
            '',
            self, "rigify_feature_sets",
            self, 'active_feature_set_index'
        )

        # Clamp active index to ensure it is in bounds.
        self.active_feature_set_index = max(0, min(self.active_feature_set_index, len(self.rigify_feature_sets) - 1))

        if len(self.rigify_feature_sets) > 0:
            active_fs = self.rigify_feature_sets[self.active_feature_set_index]

            if active_fs:
                draw_feature_set_prefs(layout, context, active_fs)


def check_feature_set_error(_feature_set: RigifyFeatureSets, info: dict, layout: bpy.types.UILayout | None):
    split_factor = 0.15
    error = False

    if 'blender' in info and info['blender'] > bpy.app.version:
        error = True
        if layout:
            split = layout.row().split(factor=split_factor)
            split.label(text="Error:")
            sub = split.row()
            sub.alert = True
            text = (
                rpt_("This feature set requires Blender {:s} or newer to work properly.")
                .format(".".join(str(x) for x in info['blender']))
            )
            sub.label(icon='ERROR', text=text, translate=False)

    for dep_link in info.get("dependencies", []):
        if not feature_set_list.get_module_by_link_safe(dep_link):
            error = True
            if layout:
                split = layout.row().split(factor=split_factor)
                split.label(text="Error:")
                col = split.column()
                sub = col.row()
                sub.alert = True
                sub.label(
                    text="This feature set depends on the following feature set to work properly:",
                    icon='ERROR'
                )
                sub_split = col.split(factor=0.8)
                sub = sub_split.row()
                sub.alert = True
                sub.label(text=dep_link, translate=False, icon='BLANK1')
                op = sub_split.operator('wm.url_open', text="Repository", icon='URL')
                op.url = dep_link

    return error


def draw_feature_set_prefs(layout: bpy.types.UILayout, _context: bpy.types.Context, feature_set: RigifyFeatureSets):
    if feature_set.module_name:
        info = feature_set_list.get_info_dict(feature_set.module_name)
    else:
        info = {}
        for item in feature_set_list.PROMOTED_FEATURE_SETS:
            if item["link"] == feature_set.link:
                info = item
                break

    description = feature_set.name
    if 'description' in info:
        description = info['description']

    col = layout.column()
    split_factor = 0.15

    check_feature_set_error(feature_set, info, col)

    if feature_set.has_exceptions:
        split = col.row().split(factor=split_factor)
        split.label(text="Error:")
        sub = split.row()
        sub.alert = True
        sub.label(text="This feature set failed to load correctly.", icon='ERROR')

    split = col.row().split(factor=split_factor)
    split.label(text="Description:")
    col_desc = split.column()
    for description_line in description.split("\n"):
        col_desc.label(text=description_line)

    if 'author' in info:
        split = col.row().split(factor=split_factor)
        split.label(text="Author:")
        split.label(text=info["author"], translate=False)

    if 'version' in info:
        split = col.row().split(factor=split_factor)
        split.label(text="Version:")
        split.label(text=".".join(str(x) for x in info['version']), translate=False)

    if 'warning' in info:
        split = col.row().split(factor=split_factor)
        split.label(text="Warning:")
        split.label(text="  " + info['warning'], icon='ERROR')

    split = col.row().split(factor=split_factor)
    split.label(text="Internet:")
    row = split.row()
    if 'link' in info:
        op = row.operator('wm.url_open', text="Repository", icon='URL')
        op.url = info['link']
    if 'doc_url' in info:
        op = row.operator('wm.url_open', text="Documentation", icon='HELP')
        op.url = info['doc_url']
    if 'tracker_url' in info:
        op = row.operator('wm.url_open', text="Report a Bug", icon='URL')
        op.url = info['tracker_url']

    if feature_set.module_name:
        mod = feature_set_list.get_module_safe(feature_set.module_name)
        if mod:
            split = col.row().split(factor=split_factor)
            split.label(text="File:")
            split.label(text=mod.__file__, translate=False)

        split = col.row().split(factor=split_factor)
        split.label(text="")
        split.operator("wm.rigify_remove_feature_set", text="Remove", icon='CANCEL')


class RigifyName(bpy.types.PropertyGroup):
    name: StringProperty()


class RigifyColorSet(bpy.types.PropertyGroup):
    name: StringProperty(name="Color Set", default=" ")
    active: FloatVectorProperty(
        name="object_color",
        subtype='COLOR',
        default=(1.0, 1.0, 1.0),
        min=0.0, max=1.0,
        description="Color picker"
    )
    normal: FloatVectorProperty(
        name="object_color",
        subtype='COLOR',
        default=(1.0, 1.0, 1.0),
        min=0.0, max=1.0,
        description="Color picker"
    )
    select: FloatVectorProperty(
        name="object_color",
        subtype='COLOR',
        default=(1.0, 1.0, 1.0),
        min=0.0, max=1.0,
        description="Color picker"
    )
    standard_colors_lock: BoolProperty(default=True)

    def apply(self, color: bpy.types.BoneColor):
        color.palette = 'CUSTOM'
        color.custom.normal = utils.misc.gamma_correct(self.normal)
        color.custom.select = utils.misc.gamma_correct(self.select)
        color.custom.active = utils.misc.gamma_correct(self.active)


class RigifySelectionColors(bpy.types.PropertyGroup):
    select: FloatVectorProperty(
        name="object_color",
        subtype='COLOR',
        default=(0.314, 0.784, 1.0),
        min=0.0, max=1.0,
        description="color picker"
    )

    active: FloatVectorProperty(
        name="object_color",
        subtype='COLOR',
        default=(0.549, 1.0, 1.0),
        min=0.0, max=1.0,
        description="color picker"
    )


class RigifyParameters(bpy.types.PropertyGroup):
    name: StringProperty()

    # NOTE: parameters are dynamically added to this PropertyGroup.
    # Check `ControlLayersOption` in `layers.py`.


class RigifyBoneCollectionReference(bpy.types.PropertyGroup):
    """Reference from a RigifyParameters field to a bone collection."""

    uid: IntProperty(name="Unique ID", default=-1)

    def find_collection(self, *, update=False, raise_error=False) -> bpy.types.BoneCollection | None:
        if self.uid < 0:
            return None
        return utils.layers.resolve_collection_reference(self.id_data, self, update=update, raise_error=raise_error)

    def set_collection(self, coll: bpy.types.BoneCollection | None):
        if coll is None:
            self.uid = -1
            self["name"] = ""
        else:
            self.uid = utils.layers.ensure_collection_uid(coll)
            self["name"] = coll.name

    def _name_get(self):
        if coll := self.find_collection(update=False):
            return coll.name

        if self.uid >= 0:
            return self.get('name') or '?'

        return ""

    def _name_set(self, new_val):
        if not new_val:
            self.set_collection(None)
            return

        arm = self.id_data.data

        if new_coll := arm.collections_all.get(new_val):
            self.set_collection(new_coll)
        else:
            self.find_collection(update=True)

    def _name_search(self, _context, _edit):
        arm = self.id_data.data
        return [coll.name for coll in utils.misc.flatten_children(arm.collections)]

    name: StringProperty(
        name="Collection Name", description="Name of the referenced bone collection",
        get=_name_get, set=_name_set, search=_name_search
    )


# Parameter update callback

in_update = False


def update_callback(prop_name):
    from .utils.rig import get_rigify_type

    def callback(params, context):
        global in_update
        # Do not recursively call if the callback updates other parameters
        if not in_update:
            try:
                in_update = True
                bone = context.active_pose_bone

                if bone and bone.rigify_parameters == params:
                    rig_info = rig_lists.rigs.get(get_rigify_type(bone), None)
                    if rig_info:
                        rig_cb = getattr(rig_info["module"].Rig, 'on_parameter_update', None)
                        if rig_cb:
                            rig_cb(context, bone, params, prop_name)
            finally:
                in_update = False

    return callback


# Remember the initial property set
RIGIFY_PARAMETERS_BASE_DIR = set(dir(RigifyParameters))
RIGIFY_PARAMETER_TABLE = {'name': ('DEFAULT', StringProperty())}


def clear_rigify_parameters():
    for name in list(dir(RigifyParameters)):
        if name not in RIGIFY_PARAMETERS_BASE_DIR:
            delattr(RigifyParameters, name)
            if name in RIGIFY_PARAMETER_TABLE:
                del RIGIFY_PARAMETER_TABLE[name]


def format_property_spec(spec):
    """Turns the return value of bpy.props.SomeProperty(...) into a readable string."""
    callback, params = spec
    param_str = ["%s=%r" % (k, v) for k, v in params.items()]
    return "%s(%s)" % (callback.__name__, ', '.join(param_str))


class RigifyParameterValidator(object):
    """
    A wrapper around RigifyParameters that verifies properties
    defined from rigs for incompatible redefinitions using a table.

    Relies on the implementation details of bpy.props.* return values:
    specifically, they just return a tuple containing the real define
    function, and a dictionary with parameters. This allows comparing
    parameters before the property is actually defined.
    """
    __params = None
    __rig_name = ''
    __prop_table = {}

    def __init__(self, params, rig_name, prop_table):
        self.__params = params
        self.__rig_name = rig_name
        self.__prop_table = prop_table

    def __getattr__(self, name):
        return getattr(self.__params, name)

    def __setattr__(self, name, val_original):
        # allow __init__ to work correctly
        if hasattr(RigifyParameterValidator, name):
            return object.__setattr__(self, name, val_original)

        if not isinstance(val_original, bpy.props._PropertyDeferred):  # noqa
            print(f"!!! RIGIFY RIG {self.__rig_name}: "
                  f"INVALID DEFINITION FOR RIG PARAMETER {name}: {repr(val_original)}\n")
            return

        # actually defining the property modifies the dictionary with new parameters, so copy it now
        val = (val_original.function, val_original.keywords)
        new_def = (val[0], val[1].copy())

        if 'poll' in new_def[1]:
            del new_def[1]['poll']

        if name in self.__prop_table:
            cur_rig, cur_info = self.__prop_table[name]
            if new_def != cur_info:
                print(f"!!! RIGIFY RIG {self.__rig_name}: REDEFINING PARAMETER {name} AS:\n\n"
                      f"    {format_property_spec(val)}\n"
                      f"!!! PREVIOUS DEFINITION BY {cur_rig}:\n\n"
                      f"    {format_property_spec(cur_info)}\n")

        # inject a generic update callback that calls the appropriate rig class method
        if val[0] != bpy.props.CollectionProperty:
            val[1]['update'] = update_callback(name)

        setattr(self.__params, name, val_original)
        self.__prop_table[name] = (self.__rig_name, new_def)


####################
# REGISTER

classes = (
    RigifyName,
    RigifyParameters,
    RigifyBoneCollectionReference,
    RigifyColorSet,
    RigifySelectionColors,
    RIGIFY_UL_FeatureSets,
    RigifyFeatureSets,
    RigifyPreferences,
)


def register():
    from bpy.utils import register_class

    # Sub-modules.
    ui.register()
    feature_set_list.register()
    metarig_menu.register()
    operators.register()

    # Classes.
    for cls in classes:
        register_class(cls)

    register_rna_properties()

    prefs = RigifyPreferences.get_instance()
    prefs.register_feature_sets(True)
    prefs.update_external_rigs()

    # Add rig parameters
    register_rig_parameters()


def register_rig_parameters():
    for rig in rig_lists.rigs:
        rig_module = rig_lists.rigs[rig]['module']
        rig_class = rig_module.Rig
        rig_def = rig_class if hasattr(rig_class, 'add_parameters') else rig_module
        # noinspection PyBroadException
        try:
            if hasattr(rig_def, 'add_parameters'):
                validator = RigifyParameterValidator(RigifyParameters, rig, RIGIFY_PARAMETER_TABLE)
                rig_def.add_parameters(validator)
        except Exception:
            import traceback
            traceback.print_exc()


def unregister():
    from bpy.utils import unregister_class

    prefs = RigifyPreferences.get_instance()
    prefs.register_feature_sets(False)

    unregister_rna_properties()

    # Classes.
    for cls in classes:
        unregister_class(cls)

    clear_rigify_parameters()

    # Sub-modules.
    operators.unregister()
    metarig_menu.unregister()
    ui.unregister()
    feature_set_list.unregister()


def register_rna_properties() -> None:
    bpy.types.Armature.active_feature_set = EnumProperty(
        items=feature_set_list.feature_set_items,
        name="Feature Set",
        description="Restrict the rig list to a specific custom feature set"
    )

    bpy.types.PoseBone.rigify_type = StringProperty(name="Rigify Type", description="Rig type for this bone")
    bpy.types.PoseBone.rigify_parameters = PointerProperty(type=RigifyParameters)

    bpy.types.Armature.rigify_colors = CollectionProperty(type=RigifyColorSet)

    bpy.types.Armature.rigify_selection_colors = PointerProperty(type=RigifySelectionColors)

    bpy.types.Armature.rigify_colors_index = IntProperty(default=-1)
    bpy.types.Armature.rigify_colors_lock = BoolProperty(default=True)
    bpy.types.Armature.rigify_theme_to_add = EnumProperty(items=(
        ('THEME01', 'THEME01', ''),
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

    id_store = bpy.types.WindowManager
    id_store.rigify_collection = EnumProperty(
        items=(("All", "All", "All"),), default="All",
        name="Rigify Active Collection",
        description="The selected rig collection")

    id_store.rigify_widgets = CollectionProperty(type=RigifyName)
    id_store.rigify_types = CollectionProperty(type=RigifyName)
    id_store.rigify_active_type = IntProperty(name="Rigify Active Type",
                                              description="The selected rig type")

    bpy.types.Armature.rigify_force_widget_update = BoolProperty(
        name="Overwrite Widget Meshes",
        description="Forces Rigify to delete and rebuild all of the rig widget objects. By "
                    "default, already existing widgets are reused as-is to facilitate manual "
                    "editing",
        default=False)

    bpy.types.Armature.rigify_mirror_widgets = BoolProperty(
        name="Mirror Widgets",
        description="Make widgets for left and right side bones linked duplicates with negative "
                    "X scale for the right side, based on bone name symmetry",
        default=True)

    bpy.types.Armature.rigify_widgets_collection = PointerProperty(
        type=bpy.types.Collection,
        name="Widgets Collection",
        description="Defines which collection to place widget objects in. If unset, a new one "
                    "will be created based on the name of the rig")

    bpy.types.Armature.rigify_rig_basename = StringProperty(
        name="Rigify Rig Name",
        description="Optional. If specified, this name will be used for the newly generated rig, "
                    "widget collection and script. Otherwise, a name is generated based on the "
                    "name of the metarig object by replacing 'metarig' with 'rig', 'META' with "
                    "'RIG', or prefixing with 'RIG-'. When updating an already generated rig its "
                    "name is never changed",
        default="")

    bpy.types.Armature.rigify_target_rig = PointerProperty(
        type=bpy.types.Object,
        name="Rigify Target Rig",
        description="Defines which rig to overwrite. If unset, a new one will be created with "
                    "name based on the Rig Name option or the name of the metarig",
        poll=lambda self, obj: obj.type == 'ARMATURE' and obj.data is not self)

    bpy.types.Armature.rigify_rig_ui = PointerProperty(
        type=bpy.types.Text,
        name="Rigify Target Rig UI",
        description="Defines the UI to overwrite. If unset, a new one will be created and named "
                    "based on the name of the rig")

    bpy.types.Armature.rigify_finalize_script = PointerProperty(
        type=bpy.types.Text,
        name="Finalize Script",
        description="Run this script after generation to apply user-specific changes")

    id_store.rigify_transfer_only_selected = BoolProperty(
        name="Transfer Only Selected",
        description="Transfer selected bones only", default=True)

    # BoneCollection properties
    coll_store = bpy.types.BoneCollection

    coll_store.rigify_uid = IntProperty(name="Unique ID", default=-1)
    coll_store.rigify_ui_row = IntProperty(
        name="UI Row", default=0, min=0,
        description="If not zero, row of the UI panel where the button for this collection is shown")
    coll_store.rigify_ui_title = StringProperty(
        name="UI Title", description="Text to use on the UI panel button instead of the collection name")
    coll_store.rigify_sel_set = BoolProperty(
        name="Add Selection Set", default=False, description='Add Selection Set for this collection')
    coll_store.rigify_color_set_id = IntProperty(name="Color Set ID", default=0, min=0)

    def ui_title_get(coll):
        return coll.rigify_ui_title or coll.name

    def ui_title_set(coll, new_val):
        coll.rigify_ui_title = "" if new_val == coll.name else new_val

    coll_store.rigify_ui_title_name = StringProperty(
        name="UI Title", description="Text to use on the UI panel button (does not edit the collection name)",
        get=ui_title_get, set=ui_title_set
    )

    def color_set_get(coll):
        idx = coll.rigify_color_set_id
        if idx <= 0:
            return ""

        sets = utils.rig.get_rigify_colors(coll.id_data)
        return sets[idx - 1].name if idx <= len(sets) else f"? {idx}"

    def color_set_set(coll, new_val):
        if new_val == "":
            coll.rigify_color_set_id = 0
        else:
            sets = utils.rig.get_rigify_colors(coll.id_data)
            for i, cset in enumerate(sets):
                if cset.name == new_val:
                    coll.rigify_color_set_id = i + 1
                    break

    def color_set_search(coll, _ctx, _edit):
        return [cset.name for cset in utils.rig.get_rigify_colors(coll.id_data)]

    coll_store.rigify_color_set_name = StringProperty(
        name="Color Set", description="Color set specifying bone colors for this group",
        get=color_set_get, set=color_set_set, search=color_set_search
    )

    # Object properties
    obj_store = bpy.types.Object

    obj_store.rigify_owner_rig = PointerProperty(
        type=bpy.types.Object,
        name="Rigify Owner Rig",
        description="Rig that owns this object and may delete or overwrite it upon re-generation")

    # 5.0: Version metarigs to new Action Slot selector properties on file load.
    from .utils.action_layers import versioning_5_0
    bpy.app.handlers.load_post.append(versioning_5_0)


def unregister_rna_properties() -> None:
    # Properties on PoseBones and Armature. (Annotated to suppress unknown attribute warnings.)
    pose_bone: typing.Any = bpy.types.PoseBone

    del pose_bone.rigify_type
    del pose_bone.rigify_parameters

    arm_store: typing.Any = bpy.types.Armature

    del arm_store.active_feature_set
    del arm_store.rigify_colors
    del arm_store.rigify_selection_colors
    del arm_store.rigify_colors_index
    del arm_store.rigify_colors_lock
    del arm_store.rigify_theme_to_add
    del arm_store.rigify_force_widget_update
    del arm_store.rigify_target_rig
    del arm_store.rigify_rig_ui

    id_store: typing.Any = bpy.types.WindowManager

    del id_store.rigify_collection
    del id_store.rigify_types
    del id_store.rigify_active_type
    del id_store.rigify_transfer_only_selected

    coll_store: typing.Any = bpy.types.BoneCollection

    del coll_store.rigify_uid
    del coll_store.rigify_ui_row
    del coll_store.rigify_ui_title
    del coll_store.rigify_ui_title_name
    del coll_store.rigify_sel_set
    del coll_store.rigify_color_set_id
    del coll_store.rigify_color_set_name

    obj_store: typing.Any = bpy.types.Object

    del obj_store.rigify_owner_rig
