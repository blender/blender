# SPDX-FileCopyrightText: 2019-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
import importlib
import importlib.util
import re

from itertools import count
from collections import defaultdict
from typing import TYPE_CHECKING, Any, Optional
from bpy.types import bpy_struct, Constraint, Object, PoseBone, Bone, Armature

from bpy.types import bpy_prop_array, bpy_prop_collection  # noqa
from idprop.types import IDPropertyArray
from mathutils import Vector

from .misc import ArmatureObject, wrap_list_to_lines, IdPropSequence, find_index, flatten_children

if TYPE_CHECKING:
    from ..base_rig import BaseRig
    from .. import RigifyColorSet

RIG_DIR = "rigs"  # Name of the directory where rig types are kept
METARIG_DIR = "metarigs"  # Name of the directory where metarigs are kept
TEMPLATE_DIR = "ui_templates"  # Name of the directory where ui templates are kept

# noinspection SpellCheckingInspection
outdated_types = {"pitchipoy.limbs.super_limb": "limbs.super_limb",
                  "pitchipoy.limbs.super_arm": "limbs.super_limb",
                  "pitchipoy.limbs.super_leg": "limbs.super_limb",
                  "pitchipoy.limbs.super_front_paw": "limbs.super_limb",
                  "pitchipoy.limbs.super_rear_paw": "limbs.super_limb",
                  "pitchipoy.limbs.super_finger": "limbs.super_finger",
                  "pitchipoy.super_torso_turbo": "spines.super_spine",
                  "pitchipoy.simple_tentacle": "limbs.simple_tentacle",
                  "pitchipoy.super_face": "faces.super_face",
                  "pitchipoy.super_palm": "limbs.super_palm",
                  "pitchipoy.super_copy": "basic.super_copy",
                  "pitchipoy.tentacle": "",
                  "palm": "limbs.super_palm",
                  "basic.copy": "basic.super_copy",
                  "biped.arm": "",
                  "biped.leg": "",
                  "finger": "",
                  "neck_short": "",
                  "misc.delta": "",
                  "spine": ""
                  }


def get_rigify_type(pose_bone: PoseBone) -> str:
    rigify_type = pose_bone.rigify_type  # noqa
    return rigify_type.replace(" ", "")


def get_rigify_params(pose_bone: PoseBone) -> Any:
    return pose_bone.rigify_parameters  # noqa


def get_rigify_colors(arm: Armature) -> IdPropSequence['RigifyColorSet']:
    return arm.rigify_colors  # noqa


def get_rigify_target_rig(arm: Armature) -> Optional[ArmatureObject]:
    return arm.rigify_target_rig  # noqa


def get_rigify_rig_basename(arm: Armature) -> str:
    return arm.rigify_rig_basename  # noqa


def get_rigify_mirror_widgets(arm: Armature) -> bool:
    return arm.rigify_mirror_widgets  # noqa


def get_rigify_force_widget_update(arm: Armature) -> bool:
    return arm.rigify_force_widget_update  # noqa


def get_rigify_finalize_script(arm: Armature) -> Optional[bpy.types.Text]:
    return arm.rigify_finalize_script  # noqa


def is_rig_base_bone(obj: Object, name):
    return bool(get_rigify_type(obj.pose.bones[name]))


def metarig_needs_upgrade(obj):
    return bool(obj.data.get("rigify_layers"))


def is_valid_metarig(context, *, allow_needs_upgrade=False):
    obj = context.object
    if not context.object:
        return False
    if obj.type != 'ARMATURE' or obj.data.get("rig_id") is not None:
        return False
    return allow_needs_upgrade or not metarig_needs_upgrade(context.object)


def upgrade_metarig_types(metarig: Object, revert=False):
    """
    Replaces rigify_type properties from old versions with their current names.

    metarig: rig to update.
    revert: revert types to previous version (if old type available)
    """

    if revert:
        vals = list(outdated_types.values())
        rig_defs = {v: k for k, v in outdated_types.items() if vals.count(v) == 1}
    else:
        rig_defs = outdated_types

    for bone in metarig.pose.bones:
        rig_type = bone.rigify_type
        if rig_type in rig_defs:
            bone.rigify_type = rig_defs[rig_type]

            parameters = get_rigify_params(bone)

            if 'leg' in rig_type:
                parameters.limb_type = 'leg'
            if 'arm' in rig_type:
                parameters.limb_type = 'arm'
            if 'paw' in rig_type:
                parameters.limb_type = 'paw'
            if rig_type == "basic.copy":
                parameters.make_widget = False


def resolve_layer_names(layers):
    """Combine full layer names if some buttons use fragments with parentheses only."""

    ui_rows = defaultdict(list)
    name_counts = defaultdict(int)

    for i, layer in enumerate(layers):
        if name := layer.get("name", "").strip():
            name_counts[name] += 1
            ui_rows[layer.get("row", 1)].append(name)

    def needs_rename(n):
        return name_counts[n] > 1 or n.startswith("(")

    def clean_stem(raw_name):
        return re.sub(r"\s*\(.*\)$", "", raw_name)

    def search_left(my_row, col_idx):
        while col_idx > 0:
            col_idx -= 1
            if not needs_rename(my_row[col_idx]):
                return clean_stem(my_row[col_idx])

    def search_up(my_row, row_idx, col_idx):
        while row_idx > 1:
            row_idx -= 1
            prev_row = ui_rows[row_idx]
            if len(prev_row) != len(my_row):
                return None
            if not needs_rename(prev_row[col_idx]):
                return clean_stem(prev_row[col_idx])

    names = []

    for i, layer in enumerate(layers):
        name: str = layer.get("name", "").strip()

        if name and needs_rename(name):
            row = layer.get("row", 1)

            cur_row = ui_rows[row]
            cur_col = cur_row.index(name)

            if stem := search_left(cur_row, cur_col):
                name = stem + " " + name
            elif stem := search_up(cur_row, row, cur_col):
                name = stem + " " + name

        names.append(name)

    return names


def upgrade_metarig_layers(metarig: ArmatureObject):
    from .layers import (REFS_LIST_SUFFIX, DEF_COLLECTION, MCH_COLLECTION, ORG_COLLECTION, ROOT_COLLECTION,
                         ensure_collection_uid)

    arm = metarig.data

    # Find layer collections
    coll_table = {}

    for coll in arm.collections_all:
        if m := re.match(r'^Layer (\d+)', coll.name):
            coll_table[int(m[1]) - 1] = coll

    # Assign names to special layers if they exist
    special_layers = {28: ROOT_COLLECTION, 29: DEF_COLLECTION, 30: MCH_COLLECTION, 31: ORG_COLLECTION}

    for idx, name in special_layers.items():
        if coll := coll_table.get(idx):
            coll.name = name

    # Apply existing layer metadata
    if layers := arm.get("rigify_layers"):
        names = resolve_layer_names(layers)

        # Enforce the special names
        for idx, name in special_layers.items():
            if idx < len(names) and names[idx]:
                names[idx] = name

        cur_idx = 0

        for i, layer in enumerate(layers):
            coll = coll_table.get(i)

            old_name = layer.get("name", "").strip()
            new_name = names[i]

            if new_name:
                if not coll:
                    coll = arm.collections.new(new_name)
                    coll_table[i] = coll
                else:
                    coll.name = new_name

            if coll:
                coll_idx = find_index(arm.collections_all, coll)
                arm.collections.move(coll_idx, cur_idx)
                cur_idx += 1

                coll.rigify_ui_row = layer.get("row", 1)

                if old_name and old_name != coll.name:
                    coll.rigify_ui_title = old_name

                coll.rigify_sel_set = layer.get("selset", False)
                coll.rigify_color_set_id = layer.get("group_prop", 0)

        del arm["rigify_layers"]

    arm.collections.active_index = 0

    # Remove empty rows, and ensure the root button position is at the bottom
    root_bcoll = coll_table.get(28)

    used_rows = set()
    for bcoll in arm.collections_all:
        if bcoll != root_bcoll and bcoll.rigify_ui_row > 0:
            used_rows.add(bcoll.rigify_ui_row)

    row_map = {}
    for i in range(1, max(used_rows) + 1):
        if i in used_rows:
            row_map[i] = len(row_map) + 1

    for bcoll in arm.collections_all:
        if bcoll == root_bcoll:
            bcoll.rigify_ui_row = len(row_map) + 3
        elif bcoll.rigify_ui_row > 0:
            bcoll.rigify_ui_row = row_map[bcoll.rigify_ui_row]

    # Convert the layer references in rig component parameters
    default_layers = [i == 1 for i in range(32)]
    default_map = {
        'faces.super_face': ['primary', 'secondary'],
        'limbs.arm': ['fk', 'tweak'],
        'limbs.front_paw': ['fk', 'tweak'],
        'limbs.leg': ['fk', 'tweak'],
        'limbs.paw': ['fk', 'tweak'],
        'limbs.rear_paw': ['fk', 'tweak'],
        'limbs.simple_tentacle': ['tweak'],
        'limbs.super_finger': ['tweak'],
        'limbs.super_limb': ['fk', 'tweak'],
        'spines.basic_spine': ['fk', 'tweak'],
    }

    for pose_bone in metarig.pose.bones:
        params = get_rigify_params(pose_bone)

        # Work around the stupid legacy default where one layer is implicitly selected
        for name_stem in default_map.get(get_rigify_type(pose_bone), []):
            prop_name = name_stem + "_layers"
            if prop_name not in params and name_stem + REFS_LIST_SUFFIX not in params:
                params[prop_name] = default_layers

        for prop_name, prop_value in list(params.items()):
            if prop_name.endswith("_layers") and isinstance(prop_value, IDPropertyArray) and len(prop_value) == 32:
                entries = []

                for i, show in enumerate(prop_value.to_list()):
                    if show:
                        coll = coll_table.get(i)
                        uid = ensure_collection_uid(coll) if coll else i
                        name = coll.name if coll else f"Layer {i+1}"
                        entries.append({"uid": uid, "name": name})

                params[prop_name[:-7] + REFS_LIST_SUFFIX] = entries

                del params[prop_name]


##############################################
# Misc
##############################################

def rig_is_child(rig: 'BaseRig', parent: Optional['BaseRig'], *, strict=False):
    """
    Checks if the rig is a child of the parent.
    Unless strict is True, returns true if the rig and parent are the same.
    """
    if parent is None:
        return True

    if rig and strict:
        rig = rig.rigify_parent

    while rig:
        if rig is parent:
            return True

        rig = rig.rigify_parent

    return False


def get_parent_rigs(rig: 'BaseRig') -> list['BaseRig']:
    """Returns a list containing the rig and all of its parents."""
    result = []
    while rig:
        result.append(rig)
        rig = rig.rigify_parent
    return result


def get_resource(resource_name):
    """ Fetches a rig module by name, and returns it.
    """
    module = importlib.import_module(resource_name)
    importlib.reload(module)
    return module


def connected_children_names(obj: ArmatureObject, bone_name: str) -> list[str]:
    """ Returns a list of bone names (in order) of the bones that form a single
        connected chain starting with the given bone as a parent.
        If there is a connected branch, the list stops there.
    """
    bone = obj.data.bones[bone_name]
    names = []

    while True:
        connects = 0
        con_name = ""

        for child in bone.children:
            if child.use_connect:
                connects += 1
                con_name = child.name

        if connects == 1:
            names += [con_name]
            bone = obj.data.bones[con_name]
        else:
            break

    return names


def has_connected_children(bone: Bone):
    """ Returns true/false whether a bone has connected children or not.
    """
    t = False
    for b in bone.children:
        t = t or b.use_connect
    return t


def _list_bone_names_depth_first_sorted_rec(result_list: list[str], bone: Bone):
    result_list.append(bone.name)

    for child in sorted(list(bone.children), key=lambda b: b.name):
        _list_bone_names_depth_first_sorted_rec(result_list, child)


def list_bone_names_depth_first_sorted(obj: ArmatureObject):
    """Returns a list of bone names in depth first name sorted order."""
    result_list = []

    for bone in sorted(list(obj.data.bones), key=lambda b: b.name):
        if bone.parent is None:
            _list_bone_names_depth_first_sorted_rec(result_list, bone)

    return result_list


def _get_property_value(obj, name: str):
    """Retrieve the attribute value, converting from Blender to python types."""
    value = getattr(obj, name, None)
    if isinstance(value, bpy_prop_array):
        value = tuple(value)
    return value


def _format_property_value(prefix: str, value: Any, *, limit=90, indent=4) -> list[str]:
    """Format a property value assignment to lines, wrapping if too long."""

    if isinstance(value, tuple):
        return wrap_list_to_lines(prefix, '()', map(repr, value), limit=limit, indent=indent)

    if isinstance(value, list):
        return wrap_list_to_lines(prefix, '[]', map(repr, value), limit=limit, indent=indent)

    return [prefix + repr(value)]


def _generate_properties(lines, prefix, obj: bpy_struct, base_class: type, *,
                         defaults: Optional[dict[str, Any]] = None,
                         objects: Optional[dict[Any, str]] = None):
    obj_rna: bpy.types.Struct = type(obj).bl_rna  # noqa
    base_rna: bpy.types.Struct = base_class.bl_rna  # noqa

    defaults = defaults or {}
    block_props = set(prop.identifier for prop in base_rna.properties) - set(defaults.keys())

    for prop in obj_rna.properties:
        if prop.identifier not in block_props and not prop.is_readonly:
            cur_value = _get_property_value(obj, prop.identifier)

            if prop.identifier in defaults:
                if cur_value == defaults[prop.identifier]:
                    continue

            if isinstance(cur_value, bpy_struct):
                if objects and cur_value in objects:
                    lines.append('%s.%s = %s' % (prefix, prop.identifier, objects[cur_value]))
            else:
                lines += _format_property_value('%s.%s = ' % (prefix, prop.identifier), cur_value)


def write_metarig_widgets(obj: Object):
    from .widgets import write_widget

    widget_set = set()

    for pbone in obj.pose.bones:
        if pbone.custom_shape:
            widget_set.add(pbone.custom_shape)

    id_set = set()
    widget_map = {}
    code = []

    for widget_obj in widget_set:
        ident = re.sub("[^0-9a-zA-Z_]+", "_", widget_obj.name)

        if ident in id_set:
            for i in count(1):
                if ident+'_'+str(i) not in id_set:
                    break

        id_set.add(ident)
        widget_map[widget_obj] = ident

        code.append(write_widget(widget_obj, name=ident, use_size=False))

    return widget_map, code


def write_metarig(obj: ArmatureObject, layers=False, func_name="create",
                  groups=False, widgets=False):
    """
    Write a metarig as a python script, this rig is to have all info needed for
    generating the real rig with rigify.
    """
    from .layers import REFS_LIST_SUFFIX, is_collection_ref_list_prop

    code = [
        "import bpy\n",
        "from rna_prop_ui import rna_idprop_ui_create\n",
        "from mathutils import Color\n\n",
    ]

    # Widget object creation functions if requested
    if widgets:
        widget_map, widget_code = write_metarig_widgets(obj)

        if widget_map:
            code.append("from rigify.utils.widgets import widget_generator\n\n")
            code += widget_code
    else:
        widget_map = {}

    # Start of the metarig function
    code.append("def %s(obj):  # noqa" % func_name)
    code.append("    # generated by rigify.utils.write_metarig")
    bpy.ops.object.mode_set(mode='EDIT')
    code.append("    bpy.ops.object.mode_set(mode='EDIT')")
    code.append("    arm = obj.data")

    arm = obj.data

    # Rigify bone group colors info
    rigify_colors = get_rigify_colors(arm)

    if groups and len(rigify_colors) > 0:
        code.append("\n    for i in range(" + str(len(rigify_colors)) + "):")
        code.append("        arm.rigify_colors.add()\n")

        for i in range(len(rigify_colors)):
            name = rigify_colors[i].name
            active = rigify_colors[i].active
            normal = rigify_colors[i].normal
            select = rigify_colors[i].select
            standard_colors_lock = rigify_colors[i].standard_colors_lock
            code.append('    arm.rigify_colors[' + str(i) + '].name = "' + name + '"')
            code.append('    arm.rigify_colors[' + str(i)
                        + '].active = Color((%.4f, %.4f, %.4f))' % tuple(active[:]))
            code.append('    arm.rigify_colors[' + str(i)
                        + '].normal = Color((%.4f, %.4f, %.4f))' % tuple(normal[:]))
            code.append('    arm.rigify_colors[' + str(i)
                        + '].select = Color((%.4f, %.4f, %.4f))' % tuple(select[:]))
            code.append('    arm.rigify_colors[' + str(i)
                        + '].standard_colors_lock = ' + str(standard_colors_lock))

    # Rigify collection layout info
    if layers:
        collection_attrs = {
            'ui_row': 0, 'ui_title': '', 'sel_set': False, 'color_set_id': 0
        }

        code.append('\n    bone_collections = {}')

        code.append('\n    for bcoll in list(arm.collections_all):'
                    '\n        arm.collections.remove(bcoll)\n')

        args = ', '.join(f'{k}={repr(v)}' for k, v in collection_attrs.items())

        code.append(f"    def add_bone_collection(name, *, parent=None, {args}):")
        code.append(f"        new_bcoll = arm.collections.new(name, parent=bone_collections.get(parent))")
        for k, _v in collection_attrs.items():
            code.append(f"        new_bcoll.rigify_{k} = {k}")
        code.append("        bone_collections[name] = new_bcoll")

        code.append("""
    def assign_bone_collections(pose_bone, *coll_names):
        assert not len(pose_bone.bone.collections)
        for name in coll_names:
            bone_collections[name].assign(pose_bone)

    def assign_bone_collection_refs(params, attr_name, *coll_names):
        ref_list = getattr(params, attr_name + '_coll_refs', None)
        if ref_list is not None:
            for name in coll_names:
                ref_list.add().set_collection(bone_collections[name])
""")

        for bcoll in flatten_children(arm.collections):
            args = [repr(bcoll.name)]
            if bcoll.parent:
                args.append(f"parent={bcoll.parent.name!r}")
            for k, v in collection_attrs.items():
                value = getattr(bcoll, "rigify_" + k)
                if value != v:
                    args.append(f"{k}={repr(value)}")
            code.append(f"    add_bone_collection({', '.join(args)})")

    # write parents first
    bones = [(len(bone.parent_recursive), bone.name) for bone in arm.edit_bones]
    bones.sort(key=lambda item: item[0])
    bones = [item[1] for item in bones]

    code.append("\n    bones = {}\n")

    # noinspection SpellCheckingInspection
    extra_props = {
        'bbone_segments': 1,
        'bbone_mapping_mode': 'STRAIGHT',
        'bbone_easein': 1, 'bbone_easeout': 1,
        'bbone_rollin': 0, 'bbone_rollout': 0,
        'bbone_curveinx': 0, 'bbone_curveinz': 0,
        'bbone_curveoutx': 0, 'bbone_curveoutz': 0,
        'bbone_scalein': Vector((1, 1, 1)),
        'bbone_scaleout': Vector((1, 1, 1)),
    }

    for bone_name in bones:
        bone = arm.edit_bones[bone_name]
        code.append("    bone = arm.edit_bones.new(%r)" % bone.name)
        code.append("    bone.head = %.4f, %.4f, %.4f" % bone.head.to_tuple(4))
        code.append("    bone.tail = %.4f, %.4f, %.4f" % bone.tail.to_tuple(4))
        code.append("    bone.roll = %.4f" % bone.roll)
        code.append("    bone.use_connect = %s" % str(bone.use_connect))
        if bone.inherit_scale != 'FULL':
            code.append("    bone.inherit_scale = %r" % str(bone.inherit_scale))
        if bone.parent:
            code.append("    bone.parent = arm.edit_bones[bones[%r]]" % bone.parent.name)
        for prop, default in extra_props.items():
            value = getattr(bone, prop)
            if value != default:
                code.append(f"    bone.{prop} = {value!r}")
        code.append("    bones[%r] = bone.name" % bone.name)

    bpy.ops.object.mode_set(mode='OBJECT')
    code.append("")
    code.append("    bpy.ops.object.mode_set(mode='OBJECT')")

    if widgets and widget_map:
        code.append("    widget_map = {}")

    # Rig type and other pose properties
    for bone_name in bones:
        pbone = obj.pose.bones[bone_name]

        rigify_type = get_rigify_type(pbone)
        rigify_parameters = get_rigify_params(pbone)

        code.append("    pbone = obj.pose.bones[bones[%r]]" % bone_name)
        code.append("    pbone.rigify_type = %r" % rigify_type)
        code.append("    pbone.lock_location = %s" % str(tuple(pbone.lock_location)))
        code.append("    pbone.lock_rotation = %s" % str(tuple(pbone.lock_rotation)))
        code.append("    pbone.lock_rotation_w = %s" % str(pbone.lock_rotation_w))
        code.append("    pbone.lock_scale = %s" % str(tuple(pbone.lock_scale)))
        code.append("    pbone.rotation_mode = %r" % pbone.rotation_mode)
        if layers and len(pbone.bone.collections):
            args = ', '.join(f"'{bcoll.name}'" for bcoll in pbone.bone.collections)
            code.append(f"    assign_bone_collections(pbone, {args})")

        # Rig type parameters
        for param_name in rigify_parameters.keys():
            param = _get_property_value(rigify_parameters, param_name)

            if isinstance(param, bpy_prop_collection):
                if layers and param_name.endswith(REFS_LIST_SUFFIX) and is_collection_ref_list_prop(param):
                    bcoll_set = [item.find_collection() for item in param]
                    bcoll_set = [bcoll for bcoll in bcoll_set if bcoll is not None]
                    if len(bcoll_set) > 0:
                        args = ', '.join(f"'{bcoll.name}'" for bcoll in bcoll_set)
                        code.append(f"    assign_bone_collection_refs("
                                    f"pbone.rigify_parameters, '{param_name[:-10]}', {args})")
                continue

            if param is not None:
                code.append("    try:")
                code += _format_property_value(
                    f"        pbone.rigify_parameters.{param_name} = ", param)
                code.append("    except AttributeError:")
                code.append("        pass")

        # Custom properties
        custom_properties = {
            property_name: value for property_name, value in pbone.items()
            if property_name not in pbone.bl_rna.properties.keys()
            and type(pbone[property_name]) in (float, int)
        }

        if custom_properties:
            code.append('    # custom properties')

        for custom_property, current_value in custom_properties.items():
            props_data = pbone.id_properties_ui(custom_property).as_dict()
            code.append(f"    rna_idprop_ui_create(")
            code.append(f"        pbone,")
            code.append(f"        {custom_property!r},")
            code.append(f"        default={props_data['default']!r},")
            if 'min' in props_data:
                code.append(f"        min={props_data['min']},")
            if 'max' in props_data:
                code.append(f"        max={props_data['max']},")
            if 'soft_min' in props_data:
                code.append(f"        soft_min={props_data['soft_min']},")
            if 'soft_max' in props_data:
                code.append(f"        soft_max={props_data['soft_max']},")
            if 'subtype' in props_data:
                code.append(f"        subtype={props_data['subtype']!r},")
            if 'description' in props_data:
                code.append(f"        description={props_data['description']!r},")
            if 'precision' in props_data:
                code.append(f"        precision={props_data['precision']},")
            if 'step' in props_data:
                code.append(f"        step={props_data['step']},")
            code.append(f"    )")
            if props_data['default'] != current_value:
                code.append(f"    pbone[{custom_property!r}] = {current_value}")

        # Constraints
        for con in pbone.constraints:
            code.append("    con = pbone.constraints.new(%r)" % con.type)
            code.append("    con.name = %r" % con.name)
            # Add target first because of target_space handling
            if con.type == 'ARMATURE':
                for tgt in con.targets:
                    code.append("    tgt = con.targets.new()")
                    code.append("    tgt.target = obj")
                    code.append("    tgt.subtarget = %r" % tgt.subtarget)
                    code.append("    tgt.weight = %.3f" % tgt.weight)
            elif getattr(con, 'target', None) == obj:
                code.append("    con.target = obj")
            # Generic properties
            _generate_properties(
                code, "    con", con, Constraint,
                defaults={
                    'owner_space': 'WORLD', 'target_space': 'WORLD',
                    'mute': False, 'influence': 1.0,
                    'target': obj,
                },
                objects={obj: 'obj'},
            )
        # Custom widgets
        if widgets and pbone.custom_shape:
            widget_id = widget_map[pbone.custom_shape]
            code.append("    if %r not in widget_map:" % widget_id)
            code.append(("        widget_map[%r] = create_%s_widget(obj, pbone.name, "
                         "widget_name=%r, widget_force_new=True)")
                        % (widget_id, widget_id, pbone.custom_shape.name))
            code.append("    pbone.custom_shape = widget_map[%r]" % widget_id)

    code.append("\n    bpy.ops.object.mode_set(mode='EDIT')")
    code.append("    for bone in arm.edit_bones:")
    code.append("        bone.select = False")
    code.append("        bone.select_head = False")
    code.append("        bone.select_tail = False")

    code.append("    for b in bones:")
    code.append("        bone = arm.edit_bones[bones[b]]")
    code.append("        bone.select = True")
    code.append("        bone.select_head = True")
    code.append("        bone.select_tail = True")
    code.append("        bone.bbone_x = bone.bbone_z = bone.length * 0.05")
    code.append("        arm.edit_bones.active = bone")

    if not layers:
        code.append("        if bcoll := arm.collections.active:")
        code.append("            bcoll.assign(bone)")
    else:
        code.append("\n    arm.collections.active_index = 0")

    code.append("\n    return bones")

    code.append('\n\nif __name__ == "__main__":')
    code.append("    " + func_name + "(bpy.context.active_object)\n")

    return "\n".join(code)
