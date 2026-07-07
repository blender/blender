# SPDX-FileCopyrightText: 2010-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy.types import UIList, UILayout, Armature
from bpy.props import (
    BoolProperty,
    IntProperty,
    EnumProperty,
    StringProperty
)
from bpy.app.translations import (
    pgettext_n as n_,
    pgettext_iface as iface_,
    pgettext_rpt as rpt_,
    contexts as i18n_contexts,
)
from bpy_extras import anim_utils

from collections import defaultdict
from typing import TYPE_CHECKING, Callable, Any
from mathutils import Color

from .utils.errors import MetarigError
from .utils.layers import ROOT_COLLECTION, SPECIAL_COLLECTIONS, validate_collection_references
from .utils.rig import write_metarig, get_rigify_type, get_rigify_target_rig, \
    get_rigify_colors, get_rigify_params
from .utils.widgets import write_widget
from .utils.naming import unique_name
from .utils.rig import upgrade_metarig_types, outdated_types, upgrade_metarig_layers, \
    is_valid_metarig, metarig_needs_upgrade
from .utils.misc import verify_armature_obj, ArmatureObject, IdPropSequence, flatten_children

from .rigs.utils import get_limb_generated_names

from .utils.animation import get_keyed_frames_in_range, bones_in_frame, overwrite_prop_animation
from .utils.animation import RIGIFY_OT_get_frame_range

from .utils.animation import register as animation_register
from .utils.animation import unregister as animation_unregister

from . import base_rig
from . import rig_lists
from . import generate
from . import rot_mode
from . import feature_set_list

if TYPE_CHECKING:
    from . import RigifyName, RigifySelectionColors


def get_rigify_types(id_store: bpy.types.WindowManager) -> IdPropSequence['RigifyName']:
    return id_store.rigify_types  # noqa


def get_transfer_only_selected(id_store: bpy.types.WindowManager) -> bool:
    return id_store.rigify_transfer_only_selected  # noqa


def get_selection_colors(armature: bpy.types.Armature) -> 'RigifySelectionColors':
    return armature.rigify_selection_colors  # noqa


def get_colors_lock(armature: bpy.types.Armature) -> bool:
    return armature.rigify_colors_lock  # noqa


def get_colors_index(armature: bpy.types.Armature) -> int:
    return armature.rigify_colors_index  # noqa


def get_theme_to_add(armature: bpy.types.Armature) -> str:
    return armature.rigify_theme_to_add  # noqa


def build_type_list(context, rigify_types: IdPropSequence['RigifyName']):
    rigify_types.clear()

    for r in sorted(rig_lists.rigs):
        if (context.object.data.active_feature_set in ('all', rig_lists.rigs[r]['feature_set'])
                or len(feature_set_list.get_enabled_modules_names()) == 0):
            a = rigify_types.add()
            a.name = r


# noinspection PyPep8Naming
class DATA_PT_rigify(bpy.types.Panel):
    bl_label = "Rigify"
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "data"

    @classmethod
    def poll(cls, context):
        return is_valid_metarig(context, allow_needs_upgrade=True)

    def draw(self, context):
        C = context
        layout = self.layout
        obj = verify_armature_obj(C.object)

        if metarig_needs_upgrade(obj):
            layout.label(text="This metarig requires upgrading to Bone Collections", icon='ERROR')
            layout.operator("armature.rigify_upgrade_layers", text="Upgrade Metarig")
            return

        WARNING = n_("Warning: Some features may change after generation")
        show_warning = False
        show_update_metarig = False
        show_not_updatable = False
        show_upgrade_face = False

        check_props = ['IK_follow', 'root/parent', 'FK_limb_follow', 'IK_Stretch']

        for pose_bone in obj.pose.bones:
            bone = pose_bone.bone
            if not bone:
                # If we are in edit mode and the bone was just created,
                # a pose bone won't exist yet.
                continue
            if list(set(pose_bone.keys()) & set(check_props)):  # bone.layers[30] and
                show_warning = True
                break

        old_rig = ''
        old_bone = ''

        for b in obj.pose.bones:
            old_rig = get_rigify_type(b)
            if old_rig in outdated_types:
                old_bone = b.name
                if outdated_types[old_rig]:
                    show_update_metarig = True
                else:
                    show_update_metarig = False
                    show_not_updatable = True
                    break
            elif old_rig == 'faces.super_face':
                show_upgrade_face = True

        if show_warning:
            layout.label(text=WARNING, icon='ERROR')

        enable_generate = not (show_not_updatable or show_update_metarig)

        if show_not_updatable:
            layout.label(text="WARNING: This metarig contains deprecated Rigify rig-types and "
                              "cannot be upgraded automatically.", icon='ERROR')
            text = iface_("({:s} on bone {:s})").format(old_rig, old_bone)
            layout.label(text=text, translate=False)
        elif show_update_metarig:
            layout.label(text="This metarig contains old rig-types that can be automatically "
                              "upgraded to benefit from new rigify features.", icon='ERROR')
            text = iface_("({:s} on bone {:s})").format(old_rig, old_bone)
            layout.label(text=text, translate=False)
            layout.operator("pose.rigify_upgrade_types", text="Upgrade Metarig")
        elif show_upgrade_face:
            layout.label(text="This metarig uses the old face rig.", icon='INFO')
            layout.operator("pose.rigify_upgrade_face")

        # Rig type field

        col = layout.column(align=True)
        col.active = ('rig_id' not in C.object.data)

        col.separator()
        row = col.row()
        text = (
            n_("Re-Generate Rig", i18n_contexts.operator_default) if get_rigify_target_rig(obj.data)
            else n_("Generate Rig", i18n_contexts.operator_default)
        )
        row.operator("pose.rigify_generate", text=text, icon='POSE_HLT')
        row.enabled = enable_generate


# noinspection PyPep8Naming
class DATA_PT_rigify_advanced(bpy.types.Panel):
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "data"
    bl_label = "Advanced"
    bl_parent_id = 'DATA_PT_rigify'
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        return is_valid_metarig(context)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        armature_id_store = verify_armature_obj(context.object).data

        col = layout.column()

        row = col.row()
        row.active = not get_rigify_target_rig(armature_id_store)
        row.prop(armature_id_store, "rigify_rig_basename", text="Rig Name")

        col.separator()

        col2 = col.box().column()
        col2.label(text="Overwrite Existing:")
        col2.row().prop(armature_id_store, "rigify_target_rig", text="Target Rig")
        col2.row().prop(armature_id_store, "rigify_rig_ui", text="Rig UI Script")
        col2.row().prop(armature_id_store, "rigify_widgets_collection")

        col.separator()
        col.row().prop(armature_id_store, "rigify_force_widget_update")
        col.row().prop(armature_id_store, "rigify_mirror_widgets")
        col.separator()
        col.row().prop(armature_id_store, "rigify_finalize_script", text="Run Script")


# noinspection PyPep8Naming
class DATA_PT_rigify_samples(bpy.types.Panel):
    bl_label = "Samples"
    bl_translation_context = i18n_contexts.id_armature
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "data"
    bl_parent_id = "DATA_PT_rigify"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        return is_valid_metarig(context) and context.object.mode == 'EDIT'

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False
        id_store = context.window_manager

        # Build types list
        rigify_types = get_rigify_types(id_store)
        build_type_list(context, rigify_types)

        if id_store.rigify_active_type > len(rigify_types):
            id_store.rigify_active_type = 0

        # Rig type list
        if len(feature_set_list.get_enabled_modules_names()) > 0:
            row = layout.row()
            row.prop(context.object.data, "active_feature_set")
        row = layout.row()
        row.template_list("UI_UL_list", "rigify_types", id_store, "rigify_types", id_store, 'rigify_active_type')

        props = layout.operator("armature.metarig_sample_add", text="Add sample")
        props.metarig_type = rigify_types[id_store.rigify_active_type].name


# noinspection SpellCheckingInspection
# noinspection PyPep8Naming
class DATA_UL_rigify_bone_collections(UIList):
    def filter_items(self, _context, data, propname):
        assert propname == 'collections_all'
        collections = data.collections_all
        flags = []

        # Filtering by name
        if self.filter_name:
            print(self.filter_name, self.use_filter_invert)
            flags = bpy.types.UI_UL_list.filter_items_by_name(
                self.filter_name, self.bitflag_filter_item, collections, "name")
        if not flags:
            flags = [self.bitflag_filter_item] * len(collections)

        # Reorder by name.
        if self.use_filter_sort_alpha:
            indices = bpy.types.UI_UL_list.sort_items_by_name(collections, "name")
        # Sort by tree order
        else:
            index_map = {c.name: i for i, c in enumerate(flatten_children(data.collections))}
            indices = [index_map[c.name] for c in collections]

        return flags, indices

    def draw_item(self, _context, layout, armature, bcoll, _icon, _active_data,
                  _active_prop_name, _index=0, _flt_flag=0):
        active_bone = armature.edit_bones.active or armature.bones.active
        has_active_bone = active_bone and bcoll.name in active_bone.collections

        split = layout.split(factor=0.7)

        split.prop(bcoll, "name", text="", emboss=False,
                   icon='DOT' if has_active_bone else 'BLANK1')

        if cset := bcoll.rigify_color_set_name:
            split.label(text=cset, icon="COLOR", translate=False)

        icons = layout.row(align=True)

        icons.prop(bcoll, "rigify_sel_set", text="", toggle=True, emboss=False,
                   icon='RADIOBUT_ON' if bcoll.rigify_sel_set else 'RADIOBUT_OFF')
        icons.label(text="", icon='RESTRICT_SELECT_OFF' if bcoll.rigify_ui_row > 0 else 'RESTRICT_SELECT_ON')
        icons.prop(bcoll, "is_visible", text="", emboss=False,
                   icon='HIDE_OFF' if bcoll.is_visible else 'HIDE_ON')


# noinspection PyPep8Naming
class DATA_PT_rigify_collection_list(bpy.types.Panel):
    bl_label = "Bone Collection UI"
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "data"
    bl_options = {'DEFAULT_CLOSED'}
    bl_parent_id = "DATA_PT_rigify"

    @classmethod
    def poll(cls, context):
        return is_valid_metarig(context)

    def draw(self, context):
        layout = self.layout
        obj = verify_armature_obj(context.object)
        arm = obj.data

        # Copy the bone collection list
        active_coll = arm.collections.active

        row = layout.row()

        row.template_list(
            "DATA_UL_rigify_bone_collections",
            "",
            arm,
            "collections_all",
            arm.collections,
            "active_index",
            rows=(4 if active_coll else 1),
        )

        col = row.column(align=True)
        col.operator("armature.collection_add", icon='ADD', text="")
        col.operator("armature.collection_remove", icon='REMOVE', text="")
        if active_coll:
            col.separator()
            col.operator("armature.collection_move", icon='TRIA_UP', text="").direction = 'UP'
            col.operator("armature.collection_move", icon='TRIA_DOWN', text="").direction = 'DOWN'

        layout.operator(operator='armature.rigify_validate_layers')

        if active_coll:
            col = layout.column()
            col.use_property_split = True
            col.use_property_decorate = False

            col.prop(active_coll, "rigify_color_set_name", icon="COLOR")
            col.prop(active_coll, "rigify_sel_set")
            col.separator()

            col.prop(active_coll, "rigify_ui_row", )
            row = col.row()
            row.active = active_coll.rigify_ui_row > 0  # noqa
            row.prop(active_coll, "rigify_ui_title")

        if ROOT_COLLECTION not in arm.collections_all:
            text = iface_("The '{:s}' collection will be added upon generation").format(ROOT_COLLECTION)
            layout.label(text=text, translate=False, icon='INFO')


# noinspection PyPep8Naming
class DATA_PT_rigify_collection_ui(bpy.types.Panel):
    bl_label = "UI Layout"
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "data"
    bl_options = set()
    bl_parent_id = "DATA_PT_rigify_collection_list"

    @classmethod
    def poll(cls, context):
        return is_valid_metarig(context) and len(verify_armature_obj(context.object).data.collections_all)

    @staticmethod
    def draw_btn_block(arm: Armature, parent: UILayout, bcoll_id: int, loose=False):
        bcoll = arm.collections_all[bcoll_id]
        block = parent.row(align=True)

        if bcoll == arm.collections.active:
            block.prop(bcoll, "rigify_ui_title_name", text="", emboss=True)

            if not loose:
                props = block.operator(text="", icon="X", operator="armature.rigify_collection_set_ui_row")
                props.index = bcoll_id
                props.row = 0
        else:
            props = block.operator(text=bcoll.rigify_ui_title_name, operator="armature.rigify_collection_select")
            props.index = bcoll_id

    def draw(self, context):
        layout = self.layout
        obj = verify_armature_obj(context.object)
        arm = obj.data

        # Sort into button rows
        row_table = defaultdict(list)
        has_buttons = False

        index_map = {c.name: i for i, c in enumerate(arm.collections_all)}

        for bcoll in flatten_children(arm.collections):
            row_table[bcoll.rigify_ui_row].append(index_map[bcoll.name])

            if bcoll.rigify_ui_row > 0:
                has_buttons = True

        active_bcoll_idx = arm.collections.active_index

        if active_bcoll_idx < 0:
            layout.label(text="Click a button to select a collection:", icon="INFO")

        box = layout.box()
        last_row = max(row_table.keys())

        for row_id in range(1, last_row + 2):
            row = box.row()
            row_items = row_table[row_id]

            if row_id == 1 and not has_buttons:
                row.label(text="Click to assign the button here:", icon="INFO")

            grid = row.grid_flow(row_major=True, columns=len(row_items), even_columns=True)
            for bcoll_id in row_items:
                self.draw_btn_block(arm, grid, bcoll_id)

            btn_row = row.row(align=True)

            if active_bcoll_idx >= 0:
                props = btn_row.operator(text="", icon="TRIA_LEFT", operator="armature.rigify_collection_set_ui_row")
                props.index = active_bcoll_idx
                props.row = row_id

            if row_id < last_row + 1:
                props = btn_row.operator(text="", icon="ADD", operator="armature.rigify_collection_add_ui_row")
                props.row = row_id
                props.add = True
            else:
                btn_row.label(text="", icon="BLANK1")

            if row_id < last_row:
                props = btn_row.operator(text="", icon="REMOVE", operator="armature.rigify_collection_add_ui_row")
                props.row = row_id + 1
                props.add = False
            else:
                btn_row.label(text="", icon="BLANK1")

        if 0 in row_table:
            box = layout.box()
            box.label(text="Permanently hidden collections:")

            grid = box.grid_flow(row_major=True, columns=2, even_columns=True)

            for i, bcoll_id in enumerate(row_table[0]):
                self.draw_btn_block(arm, grid, bcoll_id, loose=True)


# noinspection PyPep8Naming
class DATA_OT_rigify_collection_select(bpy.types.Operator):
    bl_idname = "armature.rigify_collection_select"
    bl_label = "Make Collection Active"
    bl_description = "Make this collection active"
    bl_options = {'UNDO_GROUPED'}

    index: IntProperty(name="Index")

    @staticmethod
    def button(layout, *, index, **kwargs):
        props = layout.operator(**kwargs)
        props.index = index

    @classmethod
    def poll(cls, context):
        return context.object and context.object.type == 'ARMATURE'

    def execute(self, context):
        obj = verify_armature_obj(context.object)
        obj.data.collections.active_index = self.index
        return {'FINISHED'}


# noinspection PyPep8Naming
class DATA_OT_rigify_collection_set_ui_row(bpy.types.Operator):
    bl_idname = "armature.rigify_collection_set_ui_row"
    bl_label = "Move Between UI Rows"
    bl_options = {'UNDO'}

    index: IntProperty(name="Index")
    row: IntProperty(name="Row")
    select: BoolProperty(name="Select")

    @classmethod
    def poll(cls, context):
        return context.object and context.object.type == 'ARMATURE'

    @classmethod
    def description(cls, context, properties: Any):
        if properties.row == 0:
            return "Remove this button from the UI panel"
        else:
            return "Move the active button to this UI panel row"

    def execute(self, context):
        obj = verify_armature_obj(context.object)
        if self.select:
            obj.data.collections.active_index = self.index
        obj.data.collections_all[self.index].rigify_ui_row = self.row
        return {'FINISHED'}


# noinspection PyPep8Naming
class DATA_OT_rigify_collection_add_ui_row(bpy.types.Operator):
    bl_idname = "armature.rigify_collection_add_ui_row"
    bl_label = "Add/Remove UI Rows"
    bl_options = {'UNDO'}

    row: IntProperty(name="Row")
    add: BoolProperty(name="Add")

    @classmethod
    def poll(cls, context):
        return context.object and context.object.type == 'ARMATURE'

    @classmethod
    def description(cls, context, properties: Any):
        if properties.add:
            return "Insert a new row before this one, shifting buttons down"
        else:
            return "Remove this row, shifting buttons up"

    def execute(self, context):
        obj = verify_armature_obj(context.object)
        for coll in obj.data.collections_all:
            if coll.rigify_ui_row >= self.row:
                coll.rigify_ui_row += (1 if self.add else -1)
        return {'FINISHED'}


# noinspection PyPep8Naming
class DATA_OT_rigify_add_color_sets(bpy.types.Operator):
    bl_idname = "armature.rigify_add_color_sets"
    bl_label = "Rigify Add Standard Color Sets"
    bl_options = {'UNDO'}

    @classmethod
    def poll(cls, context):
        return context.object and context.object.type == 'ARMATURE'

    def execute(self, context):
        obj = verify_armature_obj(context.object)
        armature = obj.data

        if not hasattr(armature, 'rigify_colors'):
            return {'FINISHED'}

        rigify_colors = get_rigify_colors(armature)
        groups = ['Root', 'IK', 'Special', 'Tweak', 'FK', 'Extra']

        for g in groups:
            if g in rigify_colors:
                continue

            color = rigify_colors.add()
            color.name = g

            color.select = Color((0.3140000104904175, 0.7839999794960022, 1.0))
            color.active = Color((0.5490000247955322, 1.0, 1.0))
            color.standard_colors_lock = True

            if g == "Root":
                color.normal = Color((0.43529415130615234, 0.18431372940540314, 0.41568630933761597))
            if g == "IK":
                color.normal = Color((0.6039215922355652, 0.0, 0.0))
            if g == "Special":
                color.normal = Color((0.9568628072738647, 0.7882353663444519, 0.0470588281750679))
            if g == "Tweak":
                color.normal = Color((0.03921568766236305, 0.21176472306251526, 0.5803921818733215))
            if g == "FK":
                color.normal = Color((0.11764706671237946, 0.5686274766921997, 0.03529411926865578))
            if g == "Extra":
                color.normal = Color((0.9686275124549866, 0.250980406999588, 0.0941176563501358))

        return {'FINISHED'}


# noinspection PyPep8Naming
class DATA_OT_rigify_use_standard_colors(bpy.types.Operator):
    bl_idname = "armature.rigify_use_standard_colors"
    bl_label = "Rigify Get active/select colors from current theme"
    bl_options = {'UNDO'}

    @classmethod
    def poll(cls, context):
        return context.object and context.object.type == 'ARMATURE'

    def execute(self, context):
        obj = verify_armature_obj(context.object)
        armature = obj.data
        if not hasattr(armature, 'rigify_colors'):
            return {'FINISHED'}

        current_theme = bpy.context.preferences.themes.items()[0][0]
        theme = bpy.context.preferences.themes[current_theme]

        selection_colors = get_selection_colors(armature)
        selection_colors.select = theme.view_3d.bone_pose
        selection_colors.active = theme.view_3d.bone_pose_active

        # for col in armature.rigify_colors:
        #     col.select = theme.view_3d.bone_pose
        #     col.active = theme.view_3d.bone_pose_active

        return {'FINISHED'}


# noinspection PyPep8Naming
class DATA_OT_rigify_apply_selection_colors(bpy.types.Operator):
    bl_idname = "armature.rigify_apply_selection_colors"
    bl_label = "Rigify Apply user defined active/select colors"
    bl_options = {'UNDO'}

    @classmethod
    def poll(cls, context):
        return context.object and context.object.type == 'ARMATURE'

    def execute(self, context):
        obj = verify_armature_obj(context.object)
        armature = obj.data

        if not hasattr(armature, 'rigify_colors'):
            return {'FINISHED'}

        # current_theme = bpy.context.preferences.themes.items()[0][0]
        # theme = bpy.context.preferences.themes[current_theme]

        rigify_colors = get_rigify_colors(armature)
        selection_colors = get_selection_colors(armature)

        for col in rigify_colors:
            col.select = selection_colors.select
            col.active = selection_colors.active

        return {'FINISHED'}


# noinspection PyPep8Naming
class DATA_OT_rigify_color_set_add(bpy.types.Operator):
    bl_idname = "armature.rigify_color_set_add"
    bl_label = "Rigify Add Color Set"
    bl_options = {'UNDO'}

    @classmethod
    def poll(cls, context):
        return context.object and context.object.type == 'ARMATURE'

    def execute(self, context):
        obj = context.object
        armature = obj.data

        if hasattr(armature, 'rigify_colors'):
            armature.rigify_colors.add()
            armature.rigify_colors[-1].name = unique_name(armature.rigify_colors, 'Group')

            current_theme = bpy.context.preferences.themes.items()[0][0]
            theme = bpy.context.preferences.themes[current_theme]

            armature.rigify_colors[-1].normal = theme.view_3d.wire
            armature.rigify_colors[-1].normal.hsv = theme.view_3d.wire.hsv
            armature.rigify_colors[-1].select = theme.view_3d.bone_pose
            armature.rigify_colors[-1].select.hsv = theme.view_3d.bone_pose.hsv
            armature.rigify_colors[-1].active = theme.view_3d.bone_pose_active
            armature.rigify_colors[-1].active.hsv = theme.view_3d.bone_pose_active.hsv

        return {'FINISHED'}


# noinspection PyPep8Naming
class DATA_OT_rigify_color_set_add_theme(bpy.types.Operator):
    bl_idname = "armature.rigify_color_set_add_theme"
    bl_label = "Rigify Add Color Set from Theme"
    bl_options = {"REGISTER", "UNDO"}

    theme: EnumProperty(items=(
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
    ),
        name='Theme')

    @classmethod
    def poll(cls, context):
        return context.object and context.object.type == 'ARMATURE'

    def execute(self, context):
        obj = verify_armature_obj(context.object)
        armature = obj.data

        if hasattr(armature, 'rigify_colors'):
            rigify_colors = get_rigify_colors(armature)

            if self.theme in rigify_colors.keys():
                return {'FINISHED'}

            rigify_colors.add()
            rigify_colors[-1].name = self.theme

            color_id = int(self.theme[-2:]) - 1

            theme_color_set = bpy.context.preferences.themes[0].bone_color_sets[color_id]

            rigify_colors[-1].normal = theme_color_set.normal
            rigify_colors[-1].select = theme_color_set.select
            rigify_colors[-1].active = theme_color_set.active

        return {'FINISHED'}


# noinspection PyPep8Naming
class DATA_OT_rigify_color_set_remove(bpy.types.Operator):
    bl_idname = "armature.rigify_color_set_remove"
    bl_label = "Rigify Remove Color Set"
    bl_options = {'UNDO'}

    idx: IntProperty()

    @classmethod
    def poll(cls, context):
        return context.object and context.object.type == 'ARMATURE'

    def execute(self, context):
        obj = verify_armature_obj(context.object)

        rigify_colors = get_rigify_colors(obj.data)
        rigify_colors.remove(self.idx)

        # set layers references to 0
        for coll in obj.data.collections_all:
            idx = coll.rigify_color_set_id

            if idx == self.idx + 1:
                coll.rigify_color_set_id = 0
            elif idx > self.idx + 1:
                coll.rigify_color_set_id = idx - 1

        return {'FINISHED'}


# noinspection PyPep8Naming
class DATA_OT_rigify_color_set_remove_all(bpy.types.Operator):
    bl_idname = "armature.rigify_color_set_remove_all"
    bl_label = "Rigify Remove All Color Sets"
    bl_options = {'UNDO'}

    @classmethod
    def poll(cls, context):
        return context.object and context.object.type == 'ARMATURE'

    def execute(self, context):
        obj = verify_armature_obj(context.object)

        rigify_colors = get_rigify_colors(obj.data)
        while len(rigify_colors) > 0:
            rigify_colors.remove(0)

        # set layers references to 0
        for coll in obj.data.collections_all:
            coll.rigify_color_set_id = 0

        return {'FINISHED'}


# noinspection PyPep8Naming
class DATA_UL_rigify_color_sets(bpy.types.UIList):
    def draw_item(self, context, layout, data, item, icon, active_data, active_prop_name, index=0, flt_flag=0):
        row = layout.row(align=True)
        row = row.split(factor=0.1)
        row.label(text=str(index + 1))
        row = row.split(factor=0.7)
        row.prop(item, "name", text='', emboss=False)
        row = row.row(align=True)
        # icon = 'LOCKED' if item.standard_colors_lock else 'UNLOCKED'
        # row.prop(item, "standard_colors_lock", text='', icon=icon)
        row.prop(item, "normal", text='')
        row2 = row.row(align=True)
        row2.prop(item, "select", text='')
        row2.prop(item, "active", text='')
        # row2.enabled = not item.standard_colors_lock
        arm = verify_armature_obj(context.object).data
        row2.enabled = not get_colors_lock(arm)


# noinspection PyPep8Naming
class DATA_MT_rigify_color_sets_context_menu(bpy.types.Menu):
    bl_label = 'Rigify Color Sets Specials'

    def draw(self, context):
        layout = self.layout

        layout.operator('armature.rigify_color_set_remove_all')


# noinspection SpellCheckingInspection
# noinspection PyPep8Naming
class DATA_PT_rigify_color_sets(bpy.types.Panel):
    bl_label = "Color Sets"
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "data"
    bl_options = {'DEFAULT_CLOSED'}
    bl_parent_id = "DATA_PT_rigify"

    @classmethod
    def poll(cls, context):
        return is_valid_metarig(context)

    def draw(self, context):
        obj = verify_armature_obj(context.object)
        armature = obj.data
        idx = get_colors_index(armature)
        selection_colors = get_selection_colors(armature)
        is_locked = get_colors_lock(armature)
        theme = get_theme_to_add(armature)

        layout = self.layout
        row = layout.row()
        row.operator("armature.rigify_use_standard_colors", icon='FILE_REFRESH', text='')
        row = row.row(align=True)
        row.prop(selection_colors, 'select', text='')
        row.prop(selection_colors, 'active', text='')
        row = layout.row(align=True)
        icon = 'LOCKED' if is_locked else 'UNLOCKED'
        row.prop(armature, 'rigify_colors_lock', text='Unified select/active colors', icon=icon)
        row.operator("armature.rigify_apply_selection_colors", icon='FILE_REFRESH', text='Apply')
        row = layout.row()
        row.template_list("DATA_UL_rigify_color_sets", "", obj.data, "rigify_colors", obj.data, "rigify_colors_index")

        col = row.column(align=True)
        col.operator("armature.rigify_color_set_add", icon='ADD', text="")
        col.operator("armature.rigify_color_set_remove", icon='REMOVE', text="").idx = idx
        col.menu("DATA_MT_rigify_color_sets_context_menu", icon='DOWNARROW_HLT', text="")
        row = layout.row()
        row.prop(armature, 'rigify_theme_to_add', text='Theme')
        op = row.operator("armature.rigify_color_set_add_theme", text="Add From Theme")
        op.theme = theme
        row = layout.row()
        row.operator("armature.rigify_add_color_sets", text="Add Standard")


# noinspection PyPep8Naming
class BONE_PT_rigify_buttons(bpy.types.Panel):
    bl_label = "Rigify Type"
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "bone"
    # bl_options = {'DEFAULT_OPEN'}

    @classmethod
    def poll(cls, context):
        return is_valid_metarig(context) and context.active_pose_bone

    def draw(self, context):
        C = context
        id_store = C.window_manager
        bone = context.active_pose_bone
        rig_name = get_rigify_type(bone)

        layout = self.layout
        rig_types = get_rigify_types(id_store)

        # Build types list
        build_type_list(context, rig_types)

        # Rig type field
        if len(feature_set_list.get_enabled_modules_names()) > 0:
            row = layout.row()
            row.prop(context.object.data, "active_feature_set")
        row = layout.row()
        row.prop_search(bone, "rigify_type", id_store, "rigify_types", text="Rig type")

        # Rig type parameters / Rig type non-exist alert
        if rig_name != "":
            try:
                rig = rig_lists.rigs[rig_name]['module']
            except (ImportError, AttributeError, KeyError):
                row = layout.row()
                box = row.box()
                text = rpt_("ERROR: type \"{:s}\" does not exist!").format(rig_name)
                box.label(text=text, icon='ERROR', translate=False)
            else:
                if hasattr(rig.Rig, 'parameters_ui'):
                    rig = rig.Rig

                try:
                    param_cb = rig.parameters_ui

                    # Ignore the known empty base method
                    if getattr(param_cb, '__func__', None) == \
                            getattr(base_rig.BaseRig.parameters_ui, '__func__'):
                        param_cb = None
                except AttributeError:
                    param_cb = None

                if param_cb is None:
                    col = layout.column()
                    col.label(text="No options")
                else:
                    col = layout.column()
                    col.label(text="Options:")
                    box = layout.box()
                    param_cb(box, get_rigify_params(bone))


# noinspection PyPep8Naming
class VIEW3D_PT_tools_rigify_dev(bpy.types.Panel):
    bl_label = "Rigify Dev Tools"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "Rigify"

    @classmethod
    def poll(cls, context):
        return context.mode in ['EDIT_ARMATURE', 'EDIT_MESH']

    def draw(self, context):
        obj = context.active_object
        if obj is not None:
            if context.mode == 'EDIT_ARMATURE':
                r = self.layout.row()
                r.operator("armature.rigify_encode_metarig", text="Encode Metarig to Python")
                r = self.layout.row()
                r.operator("armature.rigify_encode_metarig_sample", text="Encode Sample to Python")

            if context.mode == 'EDIT_MESH':
                r = self.layout.row()
                r.operator("mesh.rigify_encode_mesh_widget", text="Encode Mesh Widget to Python")


# noinspection PyPep8Naming
class VIEW3D_PT_rigify_animation_tools(bpy.types.Panel):
    bl_label = "Rigify Animation Tools"
    bl_context = "posemode"  # noqa
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "Rigify"

    @classmethod
    def poll(cls, context):
        obj = context.active_object
        if not obj or obj.type != 'ARMATURE':
            return False

        rig_id = obj.data.get("rig_id", "")
        if not rig_id:
            return False

        has_arm = hasattr(bpy.types, f'POSE_OT_rigify_arm_ik2fk_{rig_id}')
        has_leg = hasattr(bpy.types, f'POSE_OT_rigify_leg_ik2fk_{rig_id}')
        return has_arm or has_leg

    def draw(self, context):
        obj = context.active_object
        id_store = context.window_manager
        if obj is not None:
            row = self.layout.row()

            only_selected = get_transfer_only_selected(id_store)

            if only_selected:
                icon = 'OUTLINER_DATA_ARMATURE'
            else:
                icon = 'ARMATURE_DATA'

            row.prop(id_store, 'rigify_transfer_only_selected', toggle=True, icon=icon)

            row = self.layout.row(align=True)
            row.operator("rigify.ik2fk", text='IK2FK Pose', icon='SNAP_ON')
            row.operator("rigify.fk2ik", text='FK2IK Pose', icon='SNAP_ON')

            row = self.layout.row(align=True)
            row.operator("rigify.transfer_fk_to_ik", text='IK2FK Action', icon='ACTION_TWEAK')
            row.operator("rigify.transfer_ik_to_fk", text='FK2IK Action', icon='ACTION_TWEAK')

            row = self.layout.row(align=True)
            row.operator("rigify.clear_animation", text="Clear IK Action", icon='CANCEL').anim_type = "IK"
            row.operator("rigify.clear_animation", text="Clear FK Action", icon='CANCEL').anim_type = "FK"

            row = self.layout.row(align=True)
            op = row.operator("rigify.rotation_pole", icon='FORCE_HARMONIC', text='Switch to pole')
            op.value = True
            op.toggle = False
            op.bake = True
            op = row.operator("rigify.rotation_pole", icon='FORCE_MAGNETIC', text='Switch to rotation')
            op.value = False
            op.toggle = False
            op.bake = True
            RIGIFY_OT_get_frame_range.draw_range_ui(context, self.layout)


def rigify_report_exception(operator, exception):
    import traceback
    import sys
    import os
    # find the non-utils module name where the error happened
    # hint, this is the metarig type!
    _exception_type, _exception_value, exception_traceback = sys.exc_info()
    fns = [item.filename for item in traceback.extract_tb(exception_traceback)]
    fns_rig = [fn for fn in fns if os.path.basename(os.path.dirname(fn)) != 'utils']
    fn = fns_rig[-1]
    fn = os.path.basename(fn)
    fn = os.path.splitext(fn)[0]
    message = []
    if fn.startswith("__"):
        message.append(rpt_("Incorrect armature..."))
    else:
        message.append(rpt_("Incorrect armature for type '{:s}'").format(fn))
    message.append(exception.message)

    message.reverse()  # XXX - stupid! menu's are upside down!

    operator.report({'ERROR'}, '\n'.join(message))


def is_metarig(obj):
    if not (obj and obj.data and obj.type == 'ARMATURE'):
        return False
    if 'rig_id' in obj.data:
        return False
    for b in obj.pose.bones:
        if b.rigify_type != "":
            return True
    return False


class Generate(bpy.types.Operator):
    bl_idname = "pose.rigify_generate"
    bl_label = "Rigify Generate Rig"
    bl_options = {'UNDO'}
    bl_description = "Generate a rig from the active metarig armature"

    @classmethod
    def poll(cls, context):
        return is_metarig(context.object)

    def execute(self, context):
        metarig = verify_armature_obj(context.object)

        for bcoll in metarig.data.collections_all:
            if bcoll.rigify_ui_row > 0 and bcoll.name not in SPECIAL_COLLECTIONS:
                break
        else:
            self.report(
                {'ERROR'},
                'No bone collections have UI buttons assigned - please check the Bone Collections UI sub-panel'
            )
            return {'CANCELLED'}

        try:
            generate.generate_rig(context, metarig)
        except MetarigError as rig_exception:
            import traceback
            traceback.print_exc()

            rigify_report_exception(self, rig_exception)
        except Exception as rig_exception:
            import traceback
            traceback.print_exc()

            message = rpt_("Generation has thrown an exception: ") + str(rig_exception)
            self.report({'ERROR'}, message)
        else:
            target_rig = get_rigify_target_rig(metarig.data)
            message = rpt_('Successfully generated: "{:s}"').format(target_rig.name)
            self.report({'INFO'}, message)
        finally:
            bpy.ops.object.mode_set(mode='OBJECT')

        return {'FINISHED'}


class UpgradeMetarigTypes(bpy.types.Operator):
    bl_idname = "pose.rigify_upgrade_types"
    bl_label = "Rigify Upgrade Metarig Types"
    bl_description = "Upgrade the Rigify types on the active metarig armature"
    bl_options = {'UNDO'}

    def execute(self, context):
        upgrade_metarig_types(verify_armature_obj(context.active_object))
        return {'FINISHED'}


class UpgradeMetarigLayers(bpy.types.Operator):
    """Upgrades the metarig from bone layers to bone collections"""

    bl_idname = "armature.rigify_upgrade_layers"
    bl_label = "Rigify Upgrade Metarig Layers"
    bl_description = "Upgrade the metarig from bone layers to bone collections"
    bl_options = {'UNDO'}

    def execute(self, context):
        upgrade_metarig_layers(verify_armature_obj(context.active_object))
        return {'FINISHED'}


class ValidateMetarigLayers(bpy.types.Operator):
    """Validates references from rig component settings to bone collections"""

    bl_idname = "armature.rigify_validate_layers"
    bl_label = "Validate Collection References"
    bl_description = (
        "Validate references from rig component settings to bone collections.\n"
        "Always run this both before and after joining two metarig armature objects into one to avoid glitches"
    )
    bl_options = {'UNDO'}

    @classmethod
    def poll(cls, context):
        return is_valid_metarig(context) and context.object.mode != 'EDIT'

    def execute(self, context):
        obj = verify_armature_obj(context.object)
        messages = validate_collection_references(obj)
        for msg in messages:
            self.report({'WARNING'}, msg)
        if not messages:
            self.report({'INFO'}, "No issues detected")
        return {'FINISHED'}


class Sample(bpy.types.Operator):
    """Create a sample metarig to be modified before generating the final rig"""

    bl_idname = "armature.metarig_sample_add"
    bl_label = "Add Metarig Sample"
    bl_options = {'UNDO'}

    metarig_type: StringProperty(
        name="Type",
        description="Name of the rig type to generate a sample of",
        maxlen=128,
        options={'SKIP_SAVE'}
    )

    @classmethod
    def poll(cls, context):
        return context.mode == 'EDIT_ARMATURE'

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False
        col = layout.column()
        build_type_list(context, get_rigify_types(context.window_manager))
        col.prop(context.object.data, "active_feature_set")
        col.prop_search(self, "metarig_type", context.window_manager, "rigify_types")

    def invoke(self, context, event):
        if self.metarig_type == "":
            return context.window_manager.invoke_props_dialog(self)
        return self.execute(context)

    def execute(self, context):
        if self.metarig_type == "":
            self.report({'ERROR'}, "You must select a rig type to create a sample of")
            return {'CANCELLED'}
        try:
            rig = rig_lists.rigs[self.metarig_type]["module"]
            create_sample = rig.create_sample
        except (ImportError, AttributeError, KeyError):
            raise Exception("rig type '" + self.metarig_type + "' has no sample.")
        else:
            create_sample(context.active_object)
        finally:
            bpy.ops.object.mode_set(mode='EDIT')

        return {'FINISHED'}


class EncodeMetarig(bpy.types.Operator):
    """Create Python code that will generate the selected metarig"""
    bl_idname = "armature.rigify_encode_metarig"
    bl_label = "Rigify Encode Metarig"
    bl_options = {'UNDO'}

    @classmethod
    def poll(cls, context):
        return context.mode == 'EDIT_ARMATURE' and is_metarig(context.object)

    def execute(self, context):
        name = "metarig.py"

        if name in bpy.data.texts:
            text_block = bpy.data.texts[name]
            text_block.clear()
        else:
            text_block = bpy.data.texts.new(name)

        obj = verify_armature_obj(context.active_object)
        text = write_metarig(obj, layers=True, func_name="create", groups=True, widgets=True)
        text_block.write(text)
        bpy.ops.object.mode_set(mode='EDIT')
        message = rpt_("Metarig written to text datablock {:s}").format(text_block.name)
        self.report({'INFO'}, message)
        return {'FINISHED'}


class EncodeMetarigSample(bpy.types.Operator):
    """Create Python code that will generate the selected metarig as a sample"""
    bl_idname = "armature.rigify_encode_metarig_sample"
    bl_label = "Rigify Encode Metarig Sample"
    bl_options = {'UNDO'}

    @classmethod
    def poll(cls, context):
        return context.mode == 'EDIT_ARMATURE' and is_metarig(context.object)

    def execute(self, context):
        name = "metarig_sample.py"

        if name in bpy.data.texts:
            text_block = bpy.data.texts[name]
            text_block.clear()
        else:
            text_block = bpy.data.texts.new(name)

        obj = verify_armature_obj(context.active_object)
        text = write_metarig(obj, layers=False, func_name="create_sample")
        text_block.write(text)
        bpy.ops.object.mode_set(mode='EDIT')

        message = rpt_("Metarig Sample written to text datablock {:s}").format(text_block.name)
        self.report({'INFO'}, message)
        return {'FINISHED'}


# noinspection PyPep8Naming
class VIEW3D_MT_rigify(bpy.types.Menu):
    bl_label = "Rigify"
    bl_idname = "VIEW3D_MT_rigify"

    append: Callable
    remove: Callable

    def draw(self, context):
        layout = self.layout
        obj = verify_armature_obj(context.object)
        target_rig = get_rigify_target_rig(obj.data)

        text = (
            n_("Re-Generate Rig", i18n_contexts.operator_default) if target_rig
            else n_("Generate Rig", i18n_contexts.operator_default)
        )
        layout.operator(Generate.bl_idname, text=text)

        if context.mode == 'EDIT_ARMATURE':
            layout.separator()
            layout.operator(Sample.bl_idname)
            layout.separator()
            layout.operator(EncodeMetarig.bl_idname, text="Encode Metarig")
            layout.operator(EncodeMetarigSample.bl_idname, text="Encode Metarig Sample")


def draw_rigify_menu(self, context):
    if is_metarig(context.object):
        self.layout.menu(VIEW3D_MT_rigify.bl_idname)


class EncodeWidget(bpy.types.Operator):
    """Create Python code that will generate the selected metarig"""
    bl_idname = "mesh.rigify_encode_mesh_widget"
    bl_label = "Rigify Encode Widget"
    bl_options = {'UNDO'}

    @classmethod
    def poll(cls, context):
        return context.mode == 'EDIT_MESH'

    def execute(self, context):
        name = "widget.py"

        if name in bpy.data.texts:
            text_block = bpy.data.texts[name]
            text_block.clear()
        else:
            text_block = bpy.data.texts.new(name)

        text = write_widget(context.active_object)
        text_block.write(text)
        bpy.ops.object.mode_set(mode='EDIT')

        return {'FINISHED'}


def draw_mesh_edit_menu(self, _context: bpy.types.Context):
    self.layout.operator(EncodeWidget.bl_idname)
    self.layout.separator()


def fk_to_ik(rig: ArmatureObject, window='ALL'):
    scn = bpy.context.scene
    id_store = bpy.context.window_manager

    rig_id = rig.data['rig_id']
    leg_ik2fk = eval('bpy.ops.pose.rigify_leg_ik2fk_' + rig_id)
    arm_ik2fk = eval('bpy.ops.pose.rigify_arm_ik2fk_' + rig_id)
    limb_generated_names = get_limb_generated_names(rig)

    if window == 'ALL':
        frames = get_keyed_frames_in_range(bpy.context, rig)
    elif window == 'CURRENT':
        frames = [scn.frame_current]
    else:
        frames = [scn.frame_current]

    only_selected = get_transfer_only_selected(id_store)

    if not only_selected:
        pose_bones = rig.pose.bones
        bpy.ops.pose.select_all(action='DESELECT')
    else:
        pose_bones = bpy.context.selected_pose_bones
        bpy.ops.pose.select_all(action='DESELECT')

    for b in pose_bones:
        for group in limb_generated_names:
            if b.name in limb_generated_names[group].values() or b.name in limb_generated_names[group]['controls']\
                    or b.name in limb_generated_names[group]['ik_ctrl']:
                names = limb_generated_names[group]
                if names['limb_type'] == 'arm':
                    func = arm_ik2fk
                    controls = names['controls']
                    ik_ctrl = names['ik_ctrl']
                    # fk_ctrl = names['fk_ctrl']
                    parent = names['parent']
                    pole = names['pole']
                    rig.pose.bones[controls[0]].select = True
                    rig.pose.bones[controls[4]].select = True
                    rig.pose.bones[pole].select = True
                    rig.pose.bones[parent].select = True
                    kwargs = {'uarm_fk': controls[1], 'farm_fk': controls[2], 'hand_fk': controls[3],
                              'uarm_ik': controls[0], 'farm_ik': ik_ctrl[1], 'hand_ik': controls[4],
                              'pole': pole, 'main_parent': parent}
                    args = (controls[0], controls[1], controls[2], controls[3],
                            controls[4], pole, parent)
                else:
                    func = leg_ik2fk
                    controls = names['controls']
                    ik_ctrl = names['ik_ctrl']
                    # fk_ctrl = names['fk_ctrl']
                    parent = names['parent']
                    pole = names['pole']
                    rig.pose.bones[controls[0]].select = True
                    rig.pose.bones[controls[6]].select = True
                    rig.pose.bones[controls[5]].select = True
                    rig.pose.bones[pole].select = True
                    rig.pose.bones[parent].select = True
                    # noinspection SpellCheckingInspection
                    kwargs = {'thigh_fk': controls[1], 'shin_fk': controls[2], 'foot_fk': controls[3],
                              'mfoot_fk': controls[7], 'thigh_ik': controls[0], 'shin_ik': ik_ctrl[1],
                              'foot_ik': controls[6], 'pole': pole, 'footroll': controls[5], 'mfoot_ik': ik_ctrl[2],
                              'main_parent': parent}
                    args = (controls[0], controls[1], controls[2], controls[3],
                            controls[6], controls[5], pole, parent)

                for f in frames:
                    if not bones_in_frame(f, rig, *args):
                        continue
                    scn.frame_set(f)
                    func(**kwargs)
                    bpy.ops.anim.keyframe_insert_menu(type='BUILTIN_KSI_VisualLocRot')
                    bpy.ops.anim.keyframe_insert_menu(type='Scaling')

                bpy.ops.pose.select_all(action='DESELECT')
                limb_generated_names.pop(group)
                break


def ik_to_fk(rig: ArmatureObject, window='ALL'):
    scn = bpy.context.scene
    id_store = bpy.context.window_manager

    rig_id = rig.data['rig_id']
    leg_fk2ik = eval('bpy.ops.pose.rigify_leg_fk2ik_' + rig_id)
    arm_fk2ik = eval('bpy.ops.pose.rigify_arm_fk2ik_' + rig_id)
    limb_generated_names = get_limb_generated_names(rig)

    if window == 'ALL':
        frames = get_keyed_frames_in_range(bpy.context, rig)
    elif window == 'CURRENT':
        frames = [scn.frame_current]
    else:
        frames = [scn.frame_current]

    only_selected = get_transfer_only_selected(id_store)

    if not only_selected:
        bpy.ops.pose.select_all(action='DESELECT')
        pose_bones = rig.pose.bones
    else:
        pose_bones = bpy.context.selected_pose_bones
        bpy.ops.pose.select_all(action='DESELECT')

    for b in pose_bones:
        for group in limb_generated_names:
            if b.name in limb_generated_names[group].values() or b.name in limb_generated_names[group]['controls']\
                    or b.name in limb_generated_names[group]['ik_ctrl']:
                names = limb_generated_names[group]
                if names['limb_type'] == 'arm':
                    func = arm_fk2ik
                    controls = names['controls']
                    ik_ctrl = names['ik_ctrl']
                    # fk_ctrl = names['fk_ctrl']
                    parent = names['parent']
                    pole = names['pole']
                    rig.pose.bones[controls[1]].select = True
                    rig.pose.bones[controls[2]].select = True
                    rig.pose.bones[controls[3]].select = True
                    kwargs = {'uarm_fk': controls[1], 'farm_fk': controls[2], 'hand_fk': controls[3],
                              'uarm_ik': controls[0], 'farm_ik': ik_ctrl[1],
                              'hand_ik': controls[4]}
                    args = (controls[0], controls[1], controls[2], controls[3],
                            controls[4], pole, parent)
                else:
                    func = leg_fk2ik
                    controls = names['controls']
                    ik_ctrl = names['ik_ctrl']
                    # fk_ctrl = names['fk_ctrl']
                    parent = names['parent']
                    pole = names['pole']
                    rig.pose.bones[controls[1]].select = True
                    rig.pose.bones[controls[2]].select = True
                    rig.pose.bones[controls[3]].select = True
                    # noinspection SpellCheckingInspection
                    kwargs = {'thigh_fk': controls[1], 'shin_fk': controls[2], 'foot_fk': controls[3],
                              'mfoot_fk': controls[7], 'thigh_ik': controls[0], 'shin_ik': ik_ctrl[1],
                              'foot_ik': ik_ctrl[2], 'mfoot_ik': ik_ctrl[2]}
                    args = (controls[0], controls[1], controls[2], controls[3],
                            controls[6], controls[5], pole, parent)

                for f in frames:
                    if not bones_in_frame(f, rig, *args):
                        continue
                    scn.frame_set(f)
                    func(**kwargs)
                    bpy.ops.anim.keyframe_insert_menu(type='BUILTIN_KSI_VisualLocRot')
                    bpy.ops.anim.keyframe_insert_menu(type='Scaling')

                bpy.ops.pose.select_all(action='DESELECT')
                limb_generated_names.pop(group)
                break


def clear_animation(channelbag, anim_type, names):
    bones = []
    for group in names:
        if names[group]['limb_type'] == 'arm':
            if anim_type == 'IK':
                bones.extend([names[group]['controls'][0], names[group]['controls'][4]])
            elif anim_type == 'FK':
                bones.extend([names[group]['controls'][1], names[group]['controls'][2], names[group]['controls'][3]])
        else:
            if anim_type == 'IK':
                bones.extend([names[group]['controls'][0], names[group]['controls'][6], names[group]['controls'][5],
                              names[group]['controls'][4]])
            elif anim_type == 'FK':
                bones.extend([names[group]['controls'][1], names[group]['controls'][2], names[group]['controls'][3],
                              names[group]['controls'][4]])
    f_curves = []
    for fcu in channelbag.fcurves:
        words = fcu.data_path.split('"')
        if words[0] == "pose.bones[" and words[1] in bones:
            f_curves.append(fcu)

    if not f_curves:
        return

    for fcu in f_curves:
        channelbag.fcurves.remove(fcu)

    # Put cleared bones back to rest pose
    bpy.ops.pose.loc_clear()
    bpy.ops.pose.rot_clear()
    bpy.ops.pose.scale_clear()

    # updateView3D()


def rot_pole_toggle(rig: ArmatureObject, window='ALL', value=False, toggle=False, bake=False):
    scn = bpy.context.scene
    id_store = bpy.context.window_manager

    rig_id = rig.data['rig_id']
    leg_fk2ik = eval('bpy.ops.pose.rigify_leg_fk2ik_' + rig_id)
    arm_fk2ik = eval('bpy.ops.pose.rigify_arm_fk2ik_' + rig_id)
    leg_ik2fk = eval('bpy.ops.pose.rigify_leg_ik2fk_' + rig_id)
    arm_ik2fk = eval('bpy.ops.pose.rigify_arm_ik2fk_' + rig_id)
    limb_generated_names = get_limb_generated_names(rig)

    if window == 'ALL':
        frames = get_keyed_frames_in_range(bpy.context, rig)
    elif window == 'CURRENT':
        frames = [scn.frame_current]
    else:
        frames = [scn.frame_current]

    only_selected = get_transfer_only_selected(id_store)

    if not only_selected:
        bpy.ops.pose.select_all(action='DESELECT')
        pose_bones = rig.pose.bones
    else:
        pose_bones = bpy.context.selected_pose_bones
        bpy.ops.pose.select_all(action='DESELECT')

    for b in pose_bones:
        for group in limb_generated_names:
            names = limb_generated_names[group]

            if toggle:
                new_pole_vector_value = not rig.pose.bones[names['parent']]['pole_vector']
            else:
                new_pole_vector_value = value

            if b.name in names.values() or b.name in names['controls'] or b.name in names['ik_ctrl']:
                if names['limb_type'] == 'arm':
                    func1 = arm_fk2ik
                    func2 = arm_ik2fk
                    controls = names['controls']
                    ik_ctrl = names['ik_ctrl']
                    # fk_ctrl = names['fk_ctrl']
                    parent = names['parent']
                    pole = names['pole']
                    rig.pose.bones[controls[0]].select = not new_pole_vector_value
                    rig.pose.bones[controls[4]].select = not new_pole_vector_value
                    rig.pose.bones[parent].select = not new_pole_vector_value
                    rig.pose.bones[pole].select = new_pole_vector_value

                    kwargs1 = {'uarm_fk': controls[1], 'farm_fk': controls[2], 'hand_fk': controls[3],
                               'uarm_ik': controls[0], 'farm_ik': ik_ctrl[1],
                               'hand_ik': controls[4]}
                    kwargs2 = {'uarm_fk': controls[1], 'farm_fk': controls[2], 'hand_fk': controls[3],
                               'uarm_ik': controls[0], 'farm_ik': ik_ctrl[1], 'hand_ik': controls[4],
                               'pole': pole, 'main_parent': parent}
                    args = (controls[0], controls[4], pole, parent)
                else:
                    func1 = leg_fk2ik
                    func2 = leg_ik2fk
                    controls = names['controls']
                    ik_ctrl = names['ik_ctrl']
                    # fk_ctrl = names['fk_ctrl']
                    parent = names['parent']
                    pole = names['pole']
                    rig.pose.bones[controls[0]].select = not new_pole_vector_value
                    rig.pose.bones[controls[6]].select = not new_pole_vector_value
                    rig.pose.bones[controls[5]].select = not new_pole_vector_value
                    rig.pose.bones[parent].select = not new_pole_vector_value
                    rig.pose.bones[pole].select = new_pole_vector_value

                    # noinspection SpellCheckingInspection
                    kwargs1 = {'thigh_fk': controls[1], 'shin_fk': controls[2], 'foot_fk': controls[3],
                               'mfoot_fk': controls[7], 'thigh_ik': controls[0], 'shin_ik': ik_ctrl[1],
                               'foot_ik': ik_ctrl[2], 'mfoot_ik': ik_ctrl[2]}
                    # noinspection SpellCheckingInspection
                    kwargs2 = {'thigh_fk': controls[1], 'shin_fk': controls[2], 'foot_fk': controls[3],
                               'mfoot_fk': controls[7], 'thigh_ik': controls[0], 'shin_ik': ik_ctrl[1],
                               'foot_ik': controls[6], 'pole': pole, 'footroll': controls[5], 'mfoot_ik': ik_ctrl[2],
                               'main_parent': parent}
                    args = (controls[0], controls[6], controls[5], pole, parent)

                for f in frames:
                    if bake and not bones_in_frame(f, rig, *args):
                        continue
                    scn.frame_set(f)
                    func1(**kwargs1)
                    rig.pose.bones[names['parent']]['pole_vector'] = new_pole_vector_value
                    func2(**kwargs2)
                    if bake:
                        bpy.ops.anim.keyframe_insert_menu(type='BUILTIN_KSI_VisualLocRot')
                        bpy.ops.anim.keyframe_insert_menu(type='Scaling')
                        overwrite_prop_animation(rig, rig.pose.bones[parent], 'pole_vector', new_pole_vector_value, [f])

                bpy.ops.pose.select_all(action='DESELECT')
                limb_generated_names.pop(group)
                break
    scn.frame_set(0)


# noinspection PyPep8Naming
class OBJECT_OT_IK2FK(bpy.types.Operator):
    """ Snaps IK limb on FK limb at current frame"""
    bl_idname = "rigify.ik2fk"
    bl_label = "IK2FK"
    bl_description = "Snaps IK limb on FK"
    bl_options = {'INTERNAL'}

    def execute(self, context):
        rig = verify_armature_obj(context.object)

        fk_to_ik(rig, window='CURRENT')

        return {'FINISHED'}


# noinspection PyPep8Naming
class OBJECT_OT_FK2IK(bpy.types.Operator):
    """ Snaps FK limb on IK limb at current frame"""
    bl_idname = "rigify.fk2ik"
    bl_label = "FK2IK"
    bl_description = "Snaps FK limb on IK"
    bl_options = {'INTERNAL'}

    def execute(self, context):
        rig = verify_armature_obj(context.object)

        ik_to_fk(rig, window='CURRENT')

        return {'FINISHED'}


# noinspection PyPep8Naming
class OBJECT_OT_TransferFKtoIK(bpy.types.Operator):
    """Transfers FK animation to IK"""
    bl_idname = "rigify.transfer_fk_to_ik"
    bl_label = "Transfer FK anim to IK"
    bl_description = "Transfer FK animation to IK bones"
    bl_options = {'INTERNAL'}

    def execute(self, context):
        rig = verify_armature_obj(context.object)

        fk_to_ik(rig)

        return {'FINISHED'}


# noinspection PyPep8Naming
class OBJECT_OT_TransferIKtoFK(bpy.types.Operator):
    """Transfers FK animation to IK"""
    bl_idname = "rigify.transfer_ik_to_fk"
    bl_label = "Transfer IK anim to FK"
    bl_description = "Transfer IK animation to FK bones"
    bl_options = {'INTERNAL'}

    def execute(self, context):
        rig = verify_armature_obj(context.object)

        ik_to_fk(rig)

        return {'FINISHED'}


# noinspection PyPep8Naming
class OBJECT_OT_ClearAnimation(bpy.types.Operator):
    bl_idname = "rigify.clear_animation"
    bl_label = "Clear Animation"
    bl_description = "Clear animation for FK or IK bones"
    bl_options = {'INTERNAL', 'UNDO'}

    anim_type: StringProperty()

    def execute(self, context):
        rig = verify_armature_obj(context.object)

        if not rig.animation_data:
            return {'CANCELLED'}

        channelbag = anim_utils.action_get_channelbag_for_slot(
            rig.animation_data.action, rig.animation_data.action_slot)
        if not channelbag:
            return {'CANCELLED'}

        clear_animation(channelbag, self.anim_type, names=get_limb_generated_names(rig))
        return {'FINISHED'}


# noinspection PyPep8Naming
class OBJECT_OT_Rot2Pole(bpy.types.Operator):
    bl_idname = "rigify.rotation_pole"
    bl_label = "Rotation - Pole toggle"
    bl_description = "Toggles IK chain between rotation and pole target"
    bl_options = {'INTERNAL'}

    bone_name: StringProperty(default='')
    window: StringProperty(default='ALL')
    toggle: BoolProperty(default=True)
    value: BoolProperty(default=True)
    bake: BoolProperty(default=True)

    def execute(self, context):
        rig = verify_armature_obj(context.object)

        if self.bone_name:
            bpy.ops.pose.select_all(action='DESELECT')
            rig.pose.bones[self.bone_name].select = True

        rot_pole_toggle(rig, window=self.window, toggle=self.toggle, value=self.value, bake=self.bake)
        return {'FINISHED'}


# noinspection PyPep8Naming
class POSE_OT_rigify_collection_ref_add(bpy.types.Operator):
    bl_idname = "pose.rigify_collection_ref_add"
    bl_label = "Add Bone Collection Reference"
    bl_description = "Add a new row to the bone collection reference list"
    bl_options = {'UNDO'}

    prop_name: StringProperty(name="Property Name")

    @classmethod
    def poll(cls, context):
        return is_valid_metarig(context) and context.active_pose_bone

    def execute(self, context):
        params = get_rigify_params(context.active_pose_bone)
        getattr(params, self.prop_name).add()
        return {'FINISHED'}


# noinspection PyPep8Naming
class POSE_OT_rigify_collection_ref_remove(bpy.types.Operator):
    bl_idname = "pose.rigify_collection_ref_remove"
    bl_label = "Remove Bone Collection Reference"
    bl_description = "Remove this row from the bone collection reference list"
    bl_options = {'UNDO'}

    prop_name: StringProperty(name="Property Name")
    index: IntProperty(name="Entry Index")

    @classmethod
    def poll(cls, context):
        return is_valid_metarig(context) and context.active_pose_bone

    def execute(self, context):
        params = get_rigify_params(context.active_pose_bone)
        getattr(params, self.prop_name).remove(self.index)
        return {'FINISHED'}


###############
# Registering

classes = (
    DATA_OT_rigify_add_color_sets,
    DATA_OT_rigify_use_standard_colors,
    DATA_OT_rigify_apply_selection_colors,
    DATA_OT_rigify_color_set_add,
    DATA_OT_rigify_color_set_add_theme,
    DATA_OT_rigify_color_set_remove,
    DATA_OT_rigify_color_set_remove_all,
    DATA_UL_rigify_color_sets,
    DATA_MT_rigify_color_sets_context_menu,
    DATA_PT_rigify,
    DATA_PT_rigify_advanced,
    DATA_PT_rigify_color_sets,
    DATA_UL_rigify_bone_collections,
    DATA_PT_rigify_collection_list,
    DATA_PT_rigify_collection_ui,
    DATA_OT_rigify_collection_select,
    DATA_OT_rigify_collection_set_ui_row,
    DATA_OT_rigify_collection_add_ui_row,
    DATA_PT_rigify_samples,
    BONE_PT_rigify_buttons,
    VIEW3D_PT_rigify_animation_tools,
    VIEW3D_PT_tools_rigify_dev,
    Generate,
    UpgradeMetarigTypes,
    UpgradeMetarigLayers,
    ValidateMetarigLayers,
    Sample,
    VIEW3D_MT_rigify,
    EncodeMetarig,
    EncodeMetarigSample,
    EncodeWidget,
    OBJECT_OT_FK2IK,
    OBJECT_OT_IK2FK,
    OBJECT_OT_TransferFKtoIK,
    OBJECT_OT_TransferIKtoFK,
    OBJECT_OT_ClearAnimation,
    OBJECT_OT_Rot2Pole,
    POSE_OT_rigify_collection_ref_add,
    POSE_OT_rigify_collection_ref_remove,
)


def register():
    from bpy.utils import register_class

    animation_register()

    # Classes.
    for cls in classes:
        register_class(cls)

    bpy.types.VIEW3D_MT_editor_menus.append(draw_rigify_menu)
    bpy.types.VIEW3D_MT_edit_mesh.prepend(draw_mesh_edit_menu)

    # Sub-modules.
    rot_mode.register()


def unregister():
    from bpy.utils import unregister_class

    # Sub-modules.
    rot_mode.unregister()

    # Classes.
    for cls in classes:
        unregister_class(cls)

    bpy.types.VIEW3D_MT_editor_menus.remove(draw_rigify_menu)
    bpy.types.VIEW3D_MT_edit_mesh.remove(draw_mesh_edit_menu)

    animation_unregister()
