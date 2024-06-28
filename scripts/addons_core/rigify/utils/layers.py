# SPDX-FileCopyrightText: 2019-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
import random
import re
import zlib

from collections import defaultdict
from typing import TYPE_CHECKING, Sequence, Optional, Mapping, Iterable, Any

from bpy.types import bpy_prop_collection  # noqa
from bpy.types import Bone, UILayout, Object, PoseBone, Armature, BoneCollection, EditBone
from idprop.types import IDPropertyGroup
from rna_prop_ui import rna_idprop_value_to_python

from .errors import MetarigError
from .misc import ArmatureObject
from .naming import mirror_name_fuzzy

if TYPE_CHECKING:
    from ..base_rig import BaseRig
    from .. import RigifyBoneCollectionReference


ROOT_COLLECTION = "Root"
DEF_COLLECTION = "DEF"
ORG_COLLECTION = "ORG"
MCH_COLLECTION = "MCH"

SPECIAL_COLLECTIONS = (ROOT_COLLECTION, DEF_COLLECTION, MCH_COLLECTION, ORG_COLLECTION)

REFS_TOGGLE_SUFFIX = '_layers_extra'
REFS_LIST_SUFFIX = "_coll_refs"


def set_bone_layers(bone: Bone | EditBone, layers: Sequence[BoneCollection], *, combine=False):
    if not layers:
        return

    if not combine:
        for coll in list(bone.collections):
            coll.unassign(bone)

    for coll in layers:
        coll.assign(bone)


def union_layer_lists(lists: Iterable[Iterable[BoneCollection | None] | None]) -> list[BoneCollection]:
    all_collections = dict()

    for lst in lists:
        if lst is not None:
            all_collections.update({coll.name: coll for coll in lst if coll is not None})

    return list(all_collections.values())


def find_used_collections(obj: ArmatureObject) -> dict[str, BoneCollection]:
    bcoll_map = {}

    for bone in obj.data.bones:
        bcoll_map.update({coll.name: coll for coll in bone.collections})

    return bcoll_map


def is_collection_ref_list_prop(param: Any) -> bool:
    from .. import RigifyBoneCollectionReference

    return (isinstance(param, bpy_prop_collection) and
            all(isinstance(item, RigifyBoneCollectionReference) for item in param))


def copy_ref_list(to_ref_list, from_ref_list, *, mirror=False):
    """Copy collection references between two RigifyBoneCollectionReference lists."""
    to_ref_list.clear()

    for ref in from_ref_list:
        to_ref = to_ref_list.add()
        to_ref['uid'] = ref['uid']
        to_ref['name'] = ref['name']

        if mirror:
            to_ref.name = mirror_name_fuzzy(ref.name)


def ensure_collection_uid(bcoll: BoneCollection):
    """Retrieve the uid of the given bone collection, assigning a new one if necessary."""
    uid = bcoll.rigify_uid
    if uid >= 0:
        return uid

    # Choose the initial uid value
    max_uid = 0x7fffffff

    if re.fullmatch(r"Bones(\.\d+)?", bcoll.name):
        # Use random numbers for collections with the default name
        uid = random.randint(0, max_uid)
    else:
        uid = zlib.adler32(bcoll.name.encode("utf-8")) & max_uid

    # Ensure the uid is unique within the armature
    used_ids = set(coll.rigify_uid for coll in bcoll.id_data.collections_all)

    while uid in used_ids:
        uid = random.randint(0, max_uid)

    assert uid >= 0
    bcoll.rigify_uid = uid
    return uid


def resolve_collection_reference(obj: ArmatureObject, ref: Any, *,
                                 update=False, raise_error=False) -> bpy.types.BoneCollection | None:
    """
    Find the bone collection referenced by the given reference.
    The reference should be RigifyBoneCollectionReference, either typed or as a raw idproperty.
    """

    uid = ref["uid"]
    if uid < 0:
        return None

    arm = obj.data

    name = ref.get("name", "")
    name_coll = arm.collections_all.get(name) if name else None

    # First try an exact match of both name and uid
    if name_coll and name_coll.rigify_uid == uid:
        return name_coll

    # Then try searching by the uid
    for coll in arm.collections_all:
        if coll.rigify_uid == uid:
            if update:
                ref["name"] = coll.name
            return coll

    # Fallback to lookup by name only if possible
    if name_coll:
        if update:
            ref["uid"] = ensure_collection_uid(name_coll)
        return name_coll

    if raise_error:
        raise MetarigError(f"Broken bone collection reference: {name} #{uid}")

    return None


def validate_collection_references(obj: ArmatureObject):
    # Scan and update all references. This uses raw idprop access
    # to avoid depending on valid rig component definitions.
    refs = defaultdict(list)
    warnings = []

    for pose_bone in obj.pose.bones:
        params = pose_bone.get("rigify_parameters")
        if not params:
            continue

        for prop_name, prop_value in params.items():
            prop_name: str

            # Filter for reference list properties
            if not prop_name.endswith(REFS_LIST_SUFFIX):
                continue

            value = rna_idprop_value_to_python(prop_value)
            if not isinstance(value, list):
                continue

            for item in value:
                # Scan valid reference items
                if not isinstance(item, IDPropertyGroup):
                    continue

                name = item.get("name")
                if not name or item.get("uid", -1) < 0:
                    continue

                ref_coll = resolve_collection_reference(obj, item, update=True)

                if ref_coll:
                    refs[ref_coll.name].append(item)
                else:
                    stem = prop_name[:-len(REFS_LIST_SUFFIX)].replace("_", " ").title()
                    warnings.append(f"bone {pose_bone.name} has a broken reference to {stem} collection '{name}'")
                    print(f"RIGIFY: {warnings[-1]}")

    # Ensure uids are unique
    known_uids = dict()

    for bcoll in obj.data.collections_all:
        uid = bcoll.rigify_uid
        if uid < 0:
            continue

        prev_use = known_uids.get(uid)

        if prev_use is not None:
            warnings.append(f"collection {bcoll.name} has the same uid {uid} as {prev_use}")
            print(f"RIGIFY: {warnings[-1]}")

            # Replace the uid
            bcoll.rigify_uid = -1
            uid = ensure_collection_uid(bcoll)

            for ref in refs[bcoll.name]:
                ref["uid"] = uid

        known_uids[uid] = bcoll.name

    return warnings


##############################################
# UI utilities
##############################################


class ControlLayersOption:
    def __init__(self, name: str,
                 toggle_name: Optional[str] = None,
                 toggle_default=True, description="Set of bone collections"):
        self.name = name
        self.toggle_default = toggle_default
        self.description = description

        self.toggle_option = self.name + REFS_TOGGLE_SUFFIX
        self.refs_option = self.name + REFS_LIST_SUFFIX

        if toggle_name:
            self.toggle_name = toggle_name
        else:
            self.toggle_name = "Assign " + self.name.title() + " Collections"

    def get(self, params) -> Optional[list[BoneCollection]]:
        if getattr(params, self.toggle_option):
            result = []

            for ref in getattr(params, self.refs_option):
                coll = ref.find_collection(update=True, raise_error=True)

                if coll:
                    result.append(coll)

            if not result:
                bones = [pbone for pbone in params.id_data.pose.bones if pbone.rigify_parameters == params]
                print(f"RIGIFY: empty {self.name} layer list on bone {bones[0].name if bones else '?'}")

            return result
        else:
            return None

    def set(self, params, layers):
        if self.refs_option in params:
            del params[self.refs_option]

        setattr(params, self.toggle_option, layers is not None)

        if layers:
            items = getattr(params, self.refs_option)

            for coll in layers:
                item: RigifyBoneCollectionReference = items.add()
                item.set_collection(coll)

    def assign(self, params,
               bone_set: Object | Mapping[str, Bone | PoseBone],
               bone_list: Sequence[str], *,
               combine=False):
        layers = self.get(params)

        if isinstance(bone_set, Object):
            assert isinstance(bone_set.data, Armature)
            bone_set = bone_set.data.bones

        if layers:
            for name in bone_list:
                bone = bone_set[name]
                if isinstance(bone, PoseBone):
                    bone = bone.bone

                set_bone_layers(bone, layers, combine=combine)

    def assign_rig(self, rig: 'BaseRig', bone_list: Sequence[str], *, combine=False, priority=None):
        layers = self.get(rig.params)
        bone_set = rig.obj.data.bones

        if layers:
            for name in bone_list:
                set_bone_layers(bone_set[name], layers, combine=combine)

                if priority is not None:
                    rig.generator.set_layer_group_priority(name, layers, priority)

    def add_parameters(self, params):
        from .. import RigifyBoneCollectionReference

        prop_toggle = bpy.props.BoolProperty(
            name=self.toggle_name,
            default=self.toggle_default,
            description=""
        )

        setattr(params, self.toggle_option, prop_toggle)

        prop_coll_refs = bpy.props.CollectionProperty(
            type=RigifyBoneCollectionReference,
            description=self.description,
        )

        setattr(params, self.refs_option, prop_coll_refs)

    def parameters_ui(self, layout: UILayout, params):
        box = layout.box()

        row = box.row()
        row.prop(params, self.toggle_option)

        active = getattr(params, self.toggle_option)

        from ..operators.copy_mirror_parameters import make_copy_parameter_button
        from ..base_rig import BaseRig

        make_copy_parameter_button(row, self.refs_option, base_class=BaseRig, mirror_bone=True)

        if not active:
            return

        props = row.operator(operator="pose.rigify_collection_ref_add", text="", icon="ADD")
        props.prop_name = self.refs_option

        refs = getattr(params, self.refs_option)

        if len(refs):
            col = box.column(align=True)

            for i, ref in enumerate(refs):
                row = col.row(align=True)
                row.alert = ref.uid >= 0 and not ref.find_collection()
                row.prop(ref, "name", text="")
                row.alert = False

                props = row.operator(operator="pose.rigify_collection_ref_remove", text="", icon="REMOVE")
                props.prop_name = self.refs_option
                props.index = i
        else:
            box.label(text="Use the plus button to add list entries", icon="INFO")

    # Declarations for auto-completion
    FK: 'ControlLayersOption'
    TWEAK: 'ControlLayersOption'
    EXTRA_IK: 'ControlLayersOption'
    FACE_PRIMARY: 'ControlLayersOption'
    FACE_SECONDARY: 'ControlLayersOption'
    SKIN_PRIMARY: 'ControlLayersOption'
    SKIN_SECONDARY: 'ControlLayersOption'


ControlLayersOption.FK = ControlLayersOption(
    'fk', description="Layers for the FK controls to be on")
ControlLayersOption.TWEAK = ControlLayersOption(
    'tweak', description="Layers for the tweak controls to be on")

ControlLayersOption.EXTRA_IK = ControlLayersOption(
    'extra_ik', toggle_default=False,
    toggle_name="Extra IK Layers",
    description="Layers for the optional IK controls to be on",
)

# Layer parameters used by the super_face rig.
ControlLayersOption.FACE_PRIMARY = ControlLayersOption(
    'primary', description="Layers for the primary controls to be on")
ControlLayersOption.FACE_SECONDARY = ControlLayersOption(
    'secondary', description="Layers for the secondary controls to be on")

# Layer parameters used by the skin rigs
ControlLayersOption.SKIN_PRIMARY = ControlLayersOption(
    'skin_primary', toggle_default=False,
    toggle_name="Primary Control Layers",
    description="Layers for the primary controls to be on",
)

ControlLayersOption.SKIN_SECONDARY = ControlLayersOption(
    'skin_secondary', toggle_default=False,
    toggle_name="Secondary Control Layers",
    description="Layers for the secondary controls to be on",
)
