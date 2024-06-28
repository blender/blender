# SPDX-FileCopyrightText: 2021-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy

from math import radians
from functools import partial
from mathutils import Vector

from ..utils.errors import MetarigError
from ..utils.rig import get_rigify_type
from ..utils.node_merger import NodeMerger
from ..utils.layers import ControlLayersOption, set_bone_layers, union_layer_lists


def find_face_bone(obj):
    pbone = obj.pose.bones.get('face')
    if pbone and get_rigify_type(pbone) == 'faces.super_face':
        return pbone.name


def process_all(process, name_map):
    process('face', layer='*', rig='')

    process('nose', rig='skin.stretchy_chain', connect_ends='next', priority=1)
    process('nose.001')
    process('nose.002', parent='nose_master',
            rig='skin.stretchy_chain', connect_ends=True, priority=1)
    process('nose.003')
    process('nose.004', parent='face', rig='skin.basic_chain', connect_ends='prev')

    process('lip.T.L', parent='jaw_master', layer=1, rig='skin.stretchy_chain', sharpen=(0, 90),
            falloff=(0.7, 1, 0.1), spherical=(True, False, True), scale=True)
    process('lip.T.L.001', bbone_mapping_mode='CURVED')

    process('lip.B.L', parent='jaw_master', layer=1, rig='skin.stretchy_chain', sharpen=(0, 90),
            falloff=(0.7, 1, 0.1), spherical=(True, False, True), scale=True)
    process('lip.B.L.001', bbone_mapping_mode='CURVED')

    process('jaw', parent='jaw_master', layer=1, rig='skin.basic_chain', connect_ends='next')
    process('chin', parent='jaw_master', rig='skin.stretchy_chain', connect_ends='prev')
    process('chin.001')

    process('ear.L', layer=0, rig='basic.super_copy', params={'super_copy_widget_type': 'sphere'})

    process('ear.L.001', parent='ear.L', rig='skin.basic_chain', connect_ends=True)
    process('ear.L.002', parent='ear.L', rig='skin.stretchy_chain', connect_ends=True)
    process('ear.L.003')
    process('ear.L.004', parent='ear.L', rig='skin.basic_chain', connect_ends=True)

    process('ear.R', layer=0, rig='basic.super_copy', params={'super_copy_widget_type': 'sphere'})

    process('ear.R.001', parent='ear.R', rig='skin.basic_chain', connect_ends=True)
    process('ear.R.002', parent='ear.R', rig='skin.stretchy_chain', connect_ends=True)
    process('ear.R.003')
    process('ear.R.004', parent='ear.R', rig='skin.basic_chain', connect_ends=True)

    process('lip.T.R', parent='jaw_master', layer=1, rig='skin.stretchy_chain', sharpen=(0, 90),
            falloff=(0.7, 1, 0.1), spherical=(True, False, True), scale=True)
    process('lip.T.R.001', bbone_mapping_mode='CURVED')

    process('lip.B.R', parent='jaw_master', layer=1, rig='skin.stretchy_chain', sharpen=(0, 90),
            falloff=(0.7, 1, 0.1), spherical=(True, False, True), scale=True)
    process('lip.B.R.001', bbone_mapping_mode='CURVED')

    process('brow.B.L', rig='skin.stretchy_chain', middle=2, connect_ends='next')
    process('brow.B.L.001')
    process('brow.B.L.002')
    process('brow.B.L.003')

    # ,connect_ends=True,sharpen=(120,120))
    process('lid.T.L', parent='eye.L', sec_layer=1, rig='skin.stretchy_chain', middle=2,
            spherical=(False, True, False))
    process('lid.T.L.001')
    process('lid.T.L.002')
    process('lid.T.L.003')
    # ,connect_ends=True,sharpen=(120,120))
    process('lid.B.L', parent='eye.L', sec_layer=1, rig='skin.stretchy_chain', middle=2)
    process('lid.B.L.001')
    process('lid.B.L.002')
    process('lid.B.L.003')

    process('brow.B.R', rig='skin.stretchy_chain', middle=2, connect_ends='next')
    process('brow.B.R.001')
    process('brow.B.R.002')
    process('brow.B.R.003')

    # ,connect_ends=True,sharpen=(120,120))
    process('lid.T.R', parent='eye.R', sec_layer=1, rig='skin.stretchy_chain', middle=2,
            spherical=(False, True, False))
    process('lid.T.R.001')
    process('lid.T.R.002')
    process('lid.T.R.003')
    # ,connect_ends=True,sharpen=(120,120))
    process('lid.B.R', parent='eye.R', sec_layer=1, rig='skin.stretchy_chain', middle=2)
    process('lid.B.R.001')
    process('lid.B.R.002')
    process('lid.B.R.003')

    process('forehead.L', parent='face', rig='skin.basic_chain')
    process('forehead.L.001', parent='face', rig='skin.basic_chain')
    process('forehead.L.002', parent='face', rig='skin.basic_chain')

    process('temple.L', parent='face', rig='skin.basic_chain', connect_ends=False, priority=1)
    process('jaw.L', parent='jaw_master', rig='skin.basic_chain', connect_ends='prev')
    process('jaw.L.001')
    process('chin.L')
    process('cheek.B.L', parent='face', layer=1, rig='skin.stretchy_chain', connect_ends='next')
    process('cheek.B.L.001')
    process('brow.T.L', parent='face', rig='skin.basic_chain', connect_ends=True)
    process('brow.T.L.001', parent='face', layer=1, rig='skin.stretchy_chain', connect_ends=True)
    process('brow.T.L.002')
    process('brow.T.L.003', parent='face', rig='skin.basic_chain', connect_ends='prev')

    process('forehead.R', parent='face', rig='skin.basic_chain')
    process('forehead.R.001', parent='face', rig='skin.basic_chain')
    process('forehead.R.002', parent='face', rig='skin.basic_chain')

    process('temple.R', parent='face', rig='skin.basic_chain', connect_ends=False, priority=1)
    process('jaw.R', parent='jaw_master', rig='skin.basic_chain', connect_ends='prev')
    process('jaw.R.001')
    process('chin.R')
    process('cheek.B.R', parent='face', layer=1, rig='skin.stretchy_chain', connect_ends='next')
    process('cheek.B.R.001')
    process('brow.T.R', parent='face', rig='skin.basic_chain', connect_ends=True)
    process('brow.T.R.001', parent='face', layer=1, rig='skin.stretchy_chain', connect_ends=True)
    process('brow.T.R.002')
    process('brow.T.R.003', parent='face', rig='skin.basic_chain', connect_ends='prev')

    process('eye.L', layer=0, rig='face.skin_eye')
    process('eye.R', layer=0, rig='face.skin_eye')

    process('cheek.T.L', rig='skin.basic_chain')
    process('cheek.T.L.001')

    process('nose.L', parent='brow.B.L.004', connect=True)
    process('nose.L.001', parent='nose_master', rig='skin.basic_chain', layer=1)

    process('cheek.T.R', rig='skin.basic_chain')
    process('cheek.T.R.001')

    process('nose.R', parent='brow.B.R.004', connect=True)
    process('nose.R.001', parent='nose_master', rig='skin.basic_chain', layer=1)

    process('teeth.T', layer=0, rig='basic.super_copy', params={'super_copy_widget_type': 'teeth'})
    process('teeth.B', layer=0, parent='jaw_master', rig='basic.super_copy',
            params={'super_copy_widget_type': 'teeth'})

    process('tongue', pri_layer=0, parent='jaw_master', rig='face.basic_tongue')
    process('tongue.001')
    process('tongue.002')

    # New bones
    process('jaw_master', layer=0, parent='face', rig='face.skin_jaw',
            params={'jaw_mouth_influence': 1.0})
    process('nose_master', layer=0, parent='face', rig='basic.super_copy',
            params={'super_copy_widget_type': 'diamond', 'make_deform': False})

    process('brow.B.L.004', parent='face', rig='skin.stretchy_chain',
            connect_ends='prev', falloff=(-10, 1, 0))
    process('brow.B.R.004', parent='face', rig='skin.stretchy_chain',
            connect_ends='prev', falloff=(-10, 1, 0))

    process('brow_glue.B.L.002', parent='face', rig='skin.glue', glue_copy=0.25, glue_reparent=True)
    process('brow_glue.B.R.002', parent='face', rig='skin.glue', glue_copy=0.25, glue_reparent=True)

    process('lid_glue.B.L.002', parent='face', rig='skin.glue', glue_copy=0.1)
    process('lid_glue.B.R.002', parent='face', rig='skin.glue', glue_copy=0.1)

    process('cheek_glue.T.L.001', parent='face',
            rig='skin.glue', glue_copy=0.5, glue_reparent=True)
    process('cheek_glue.T.R.001', parent='face',
            rig='skin.glue', glue_copy=0.5, glue_reparent=True)

    process('nose_glue.L.001', parent='face', rig='skin.glue', glue_copy=0.2, glue_reparent=True)
    process('nose_glue.R.001', parent='face', rig='skin.glue', glue_copy=0.2, glue_reparent=True)

    process('nose_glue.004', parent='face', rig='skin.glue', glue_copy=0.2, glue_reparent=True)

    if 'nose_end_glue.004' in name_map:
        process('nose_end_glue.004', parent='face', rig='skin.glue', glue_copy=0.5, glue_reparent=True)

    process('chin_end_glue.001', parent='jaw_master', rig='skin.glue', glue_copy=0.5, glue_reparent=True)


def make_new_bones(obj, name_map):
    eb = obj.data.edit_bones
    # face_bone = name_map['face']

    bone = eb.new(name='jaw_master')
    bone.head = (eb['jaw.R'].head + eb['jaw.L'].head) / 2
    bone.tail = eb['jaw'].tail
    bone.roll = 0
    name_map['jaw_master'] = bone.name

    bone = eb.new(name='nose_master')
    bone.head = (eb['nose.L.001'].head + eb['nose.R.001'].head) / 2
    nose_width = (eb['nose.L.001'].head - eb['nose.R.001'].head).length
    nose_length = (eb['nose.001'].tail - bone.head).length
    bone.tail = bone.head + Vector((0, -max(1.5 * nose_width, 2 * nose_length), 0))
    bone.roll = 0
    name_map['nose_master'] = bone.name

    def align_bones(bones):
        prev_mat = eb[bones[0]].matrix

        for bone_name in bones[1:]:
            edit_bone = eb[bone_name]
            _, angle = (prev_mat.inverted() @ edit_bone.matrix).to_quaternion().to_swing_twist('Y')
            edit_bone.roll -= angle
            prev_mat = edit_bone.matrix

    align_bones(['ear.L', 'ear.L.001', 'ear.L.002', 'ear.L.003', 'ear.L.004'])
    align_bones(['ear.R', 'ear.R.001', 'ear.R.002', 'ear.R.003', 'ear.R.004'])

    align_bones(['cheek.B.L', 'cheek.B.L.001', 'brow.T.L',
                 'brow.T.L.001', 'brow.T.L.002', 'brow.T.L.003'])
    align_bones(['cheek.B.R', 'cheek.B.R.001', 'brow.T.R',
                 'brow.T.R.001', 'brow.T.R.002', 'brow.T.R.003'])

    align_bones(['cheek.T.L', 'cheek.T.L.001'])
    align_bones(['cheek.T.R', 'cheek.T.R.001'])

    align_bones(['temple.L', 'jaw.L', 'jaw.L.001', 'chin.L'])
    align_bones(['temple.R', 'jaw.R', 'jaw.R.001', 'chin.R'])

    align_bones(['brow.B.L', 'brow.B.L.001', 'brow.B.L.002', 'brow.B.L.003', 'nose.L'])
    align_bones(['brow.B.R', 'brow.B.R.001', 'brow.B.R.002', 'brow.B.R.003', 'nose.R'])

    def is_same_pos(from_name, from_end, to_name, to_end):
        head = getattr(eb[from_name], from_end)
        tail = getattr(eb[to_name], to_end)
        return (head - tail).length < 2 * NodeMerger.epsilon

    def bridge(name, from_name, from_end, to_name, to_end, roll: str | int = 0):
        if is_same_pos(from_name, from_end, to_name, to_end):
            raise MetarigError(f"Locations of {from_name} {from_end} and {to_name} {to_end} overlap.")

        edit_bone = eb.new(name=name)
        edit_bone.head = getattr(eb[from_name], from_end)
        edit_bone.tail = getattr(eb[to_name], to_end)
        edit_bone.roll = (eb[from_name].roll + eb[to_name].roll) / 2 if roll == 'mix' else radians(roll)
        name_map[name] = edit_bone.name

    def bridge_glue(name, from_name, to_name):
        bridge(name, from_name, 'head', to_name, 'head', roll=-45 if 'R' in name else 45)

    bridge('brow.B.L.004', 'brow.B.L.003', 'tail', 'nose.L', 'head', roll='mix')
    bridge('brow.B.R.004', 'brow.B.R.003', 'tail', 'nose.R', 'head', roll='mix')

    bridge_glue('brow_glue.B.L.002', 'brow.B.L.002', 'brow.T.L.002')
    bridge_glue('brow_glue.B.R.002', 'brow.B.R.002', 'brow.T.R.002')

    bridge_glue('lid_glue.B.L.002', 'lid.B.L.002', 'cheek.T.L.001')
    bridge_glue('lid_glue.B.R.002', 'lid.B.R.002', 'cheek.T.R.001')

    bridge_glue('cheek_glue.T.L.001', 'cheek.T.L.001', 'cheek.B.L.001')
    bridge_glue('cheek_glue.T.R.001', 'cheek.T.R.001', 'cheek.B.R.001')

    bridge_glue('nose_glue.L.001', 'nose.L.001', 'lip.T.L.001')
    bridge_glue('nose_glue.R.001', 'nose.R.001', 'lip.T.R.001')

    bridge('nose_glue.004', 'nose.004', 'head', 'lip.T.L', 'head', roll=45)

    if not is_same_pos('nose.004', 'tail', 'lip.T.L', 'head'):
        bridge('nose_end_glue.004', 'nose.004', 'tail', 'lip.T.L', 'head', roll=45)
    else:
        eb['nose.004'].tail = eb['lip.T.L'].head

    bridge('chin_end_glue.001', 'chin.001', 'tail', 'lip.B.L', 'head', roll=45)


def check_bone(obj, name_map, bone, **_kwargs):
    bone = name_map.get(bone, bone)
    if bone not in obj.pose.bones:
        raise MetarigError(f"Bone '{bone}' not found")


def parent_bone(obj, name_map, bone, parent=None, connect=False, **_kwargs):
    if parent is not None:
        bone = name_map.get(bone, bone)
        parent = name_map.get(parent, parent)

        edit_bone = obj.data.edit_bones[bone]
        edit_bone.use_connect = connect
        edit_bone.parent = obj.data.edit_bones[parent]


def set_layers(obj, name_map, layer_table, bone, layer=2, pri_layer=None, sec_layer=None, **_kwargs):
    bone = name_map.get(bone, bone)
    pbone = obj.pose.bones[bone]
    main_layers = layer_table[layer]
    set_bone_layers(pbone.bone, main_layers)

    if pri_layer is not None:
        pri_layers = layer_table[pri_layer]
        ControlLayersOption.SKIN_PRIMARY.set(
            pbone.rigify_parameters,
            pri_layers if pri_layers != main_layers else None
        )

    if sec_layer is not None:
        sec_layers = layer_table[sec_layer]
        ControlLayersOption.SKIN_SECONDARY.set(
            pbone.rigify_parameters,
            sec_layers if sec_layers != main_layers else None
        )


connect_ends_map = {
    'prev': (True, False),
    'next': (False, True),
    True: (True, True),
}


def set_rig(
    obj, name_map, bone, rig=None,
    connect_ends=None, priority=0, middle=0, sharpen=None,
    falloff=None, spherical=None, falloff_length=False, scale=False,
    glue_copy=None, glue_reparent=False,
    bbone_mapping_mode=None,
    params=None, **_kwargs
):
    bone = name_map.get(bone, bone)
    if rig is not None:
        pbone = obj.pose.bones[bone]
        pbone.rigify_type = rig

        if rig in ('skin.basic_chain', 'skin.stretchy_chain', 'skin.anchor'):
            pbone.rigify_parameters.skin_control_orientation_bone = name_map['face']

        if priority:
            pbone.rigify_parameters.skin_chain_priority = priority

        if middle:
            pbone.rigify_parameters.skin_chain_pivot_pos = middle

        if connect_ends:
            pbone.rigify_parameters.skin_chain_connect_ends = connect_ends_map[connect_ends]

        if falloff:
            pbone.rigify_parameters.skin_chain_falloff = falloff

        if spherical:
            pbone.rigify_parameters.skin_chain_falloff_spherical = spherical

        if falloff_length:
            pbone.rigify_parameters.skin_chain_falloff_length = True

        if sharpen:
            pbone.rigify_parameters.skin_chain_connect_sharp_angle = tuple(map(radians, sharpen))

        if scale:
            if rig == 'skin.stretchy_chain':
                pbone.rigify_parameters.skin_chain_falloff_scale = True
            pbone.rigify_parameters.skin_chain_use_scale = (True, True, True, True)

        if glue_copy:
            pbone.rigify_parameters.relink_constraints = True
            pbone.rigify_parameters.skin_glue_use_tail = True
            pbone.rigify_parameters.skin_glue_tail_reparent = glue_reparent
            pbone.rigify_parameters.skin_glue_add_constraint = 'COPY_LOCATION_OWNER'
            pbone.rigify_parameters.skin_glue_add_constraint_influence = glue_copy

        if params:
            for k, v in params.items():
                setattr(pbone.rigify_parameters, k, v)

    if bbone_mapping_mode is not None:
        obj.pose.bones[bone].bone.bbone_mapping_mode = bbone_mapping_mode


def update_face_rig(obj):
    assert obj.type == 'ARMATURE'

    bpy.ops.object.mode_set(mode='OBJECT')

    face_bone = 'face'
    name_map = {'face': face_bone}

    # Find the layer settings
    face_pbone = obj.pose.bones[face_bone]
    main_layers = list(face_pbone.bone.collections)

    primary_layers = ControlLayersOption.FACE_PRIMARY.get(face_pbone.rigify_parameters) or main_layers
    secondary_layers = ControlLayersOption.FACE_SECONDARY.get(face_pbone.rigify_parameters) or main_layers

    # Edit mode changes
    bpy.ops.object.mode_set(mode='EDIT')

    make_new_bones(obj, name_map)

    process_all(partial(parent_bone, obj, name_map), name_map)

    # Check all bones exist
    bpy.ops.object.mode_set(mode='OBJECT')

    process_all(partial(check_bone, obj, name_map), name_map)

    # Set bone layers
    layer_table = {
        0: main_layers, 1: primary_layers, 2: secondary_layers,
        '*': union_layer_lists([main_layers, primary_layers, secondary_layers]),
    }

    process_all(partial(set_rig, obj, name_map), name_map)
    process_all(partial(set_layers, obj, name_map, layer_table), name_map)

    for bcoll in layer_table['*']:
        bcoll.is_visible = True


# noinspection PyPep8Naming
class POSE_OT_rigify_upgrade_face(bpy.types.Operator):
    """Upgrade the legacy super_face rig type to new modular face"""

    bl_idname = "pose.rigify_upgrade_face"
    bl_label = "Upgrade Face Rig"
    bl_description = 'Upgrades the legacy super_face rig type to the new modular face. This '\
                     'preserves compatibility with existing weight painting, but not animation'
    bl_options = {'UNDO'}

    @classmethod
    def poll(cls, context):
        obj = context.object
        return obj and obj.type == 'ARMATURE' and find_face_bone(obj)

    def invoke(self, context, event):
        return context.window_manager.invoke_confirm(self, event)

    def execute(self, context):
        mode = context.object.mode
        update_face_rig(context.object)
        bpy.ops.object.mode_set(mode=mode)
        return {'FINISHED'}


# =============================================
# Registration

classes = (
    POSE_OT_rigify_upgrade_face,
)


def register():
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)


def unregister():
    from bpy.utils import unregister_class
    for cls in classes:
        unregister_class(cls)
