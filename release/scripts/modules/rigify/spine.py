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
#  Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# ##### END GPL LICENSE BLOCK #####

import bpy
from rigify import bone_class_instance, copy_bone_simple
from rna_prop_ui import rna_idprop_ui_prop_get

def validate(obj, orig_bone_name):
    '''
    The bone given is the second in a chain.
    Expects at least 1 parent and a chain of children withe the same basename
    eg.
        pelvis -> rib_cage -> spine.01 -> spine.02 -> spine.03
    '''
    orig_bone = obj.data.bones[orig_bone_name]
    if not orig_bone.parent:
        return "expected spine bone '%s' to have a parent" % orig_bone_name
    
    children = orig_bone.children

    if len(children) != 1:
        return "expected spine bone '%s' to have only 1 child for the sine chain" % orig_bone_name
    
    children_spine = children[0].children_recursive_basename

    if len(children_spine) == 0:
        return "expected '%s' to define a chain of children with its basename (2 or more)" % children[0]

    return ''

def main(obj, orig_bone_name):
    from Mathutils import Vector, Matrix, RotationMatrix
    from math import radians, pi
    
    arm = obj.data

    # Initialize container classes for convenience
    mt = bone_class_instance(obj, ["pelvis", "ribcage"]) # meta
    mt.ribcage = orig_bone_name
    mt.update()
    mt.pelvis = mt.ribcage_e.parent.name
    mt.update()

    children = mt.ribcage_e.children
    child = children[0] # validate checks for 1 only.
    spine_chain_basename = child.basename # probably 'spine'
    spine_chain_segment_length = child.length
    spine_chain = [child] + child.children_recursive_basename
    spine_chain_orig = [child.name for child in spine_chain]
    child.parent = mt.pelvis_e # was mt.ribcage
    
    # The first bone in the chain happens to be the basis of others, create them now
    ex = bone_class_instance(obj, ["pelvis", "ribcage", "ribcage_hinge", "spine_rotate"])
    df = bone_class_instance(obj, ["pelvis", "ribcage"]) # DEF-wgt_pelvis, DEF-wgt_rib_cage


    # copy the pelvis, offset to make MCH-spine_rotate and MCH-ribcage_hinge
    ex.ribcage_hinge_e = copy_bone_simple(arm, mt.pelvis, "MCH-%s_hinge" % mt.ribcage)
    ex.ribcage_hinge = ex.ribcage_hinge_e.name
    ex.ribcage_hinge_e.translate(Vector(0.0, spine_chain_segment_length / 4.0, 0.0))
    mt.ribcage_e.parent = ex.ribcage_hinge_e
    
    ex.spine_rotate_e = copy_bone_simple(arm, mt.pelvis, "MCH-%s_rotate" % spine_chain_basename)
    ex.spine_rotate = ex.spine_rotate_e.name
    ex.spine_rotate_e.translate(Vector(0.0, spine_chain_segment_length / 2.0, 0.0))
    # swap head/tail
    ex.spine_rotate_e.head, ex.spine_rotate_e.tail = ex.spine_rotate_e.tail.copy(), ex.spine_rotate_e.head.copy()
    ex.spine_rotate_e.parent = mt.pelvis_e
    
    df.pelvis_e = copy_bone_simple(arm, child.name, "DEF-wgt_%s" % mt.pelvis)
    df.pelvis = df.pelvis_e.name
    df.pelvis_e.translate(Vector(spine_chain_segment_length * 2.0, -spine_chain_segment_length, 0.0))

    ex.pelvis_e = copy_bone_simple(arm, child.name, "MCH-wgt_%s" % mt.pelvis)
    ex.pelvis = ex.pelvis_e.name
    ex.pelvis_e.translate(Vector(0.0, -spine_chain_segment_length, 0.0))
    ex.pelvis_e.parent = mt.pelvis_e

    # Copy the last bone now
    child = spine_chain[-1]
    
    df.ribcage_e = copy_bone_simple(arm, child.name, "DEF-wgt_%s" % mt.ribcage)
    df.ribcage = df.ribcage_e.name
    df.ribcage_e.translate(Vector(spine_chain_segment_length * 2.0, -df.ribcage_e.length / 2.0, 0.0))
    
    ex.ribcage_e = copy_bone_simple(arm, child.name, "MCH-wgt_%s" % mt.ribcage)
    ex.ribcage = ex.ribcage_e.name
    ex.ribcage_e.translate(Vector(0.0, -ex.ribcage_e.length / 2.0, 0.0))
    ex.ribcage_e.parent = mt.ribcage_e

    # rename!
    for child in spine_chain:
        child.name = "ORG-" + child.name

    spine_chain = [child.name for child in spine_chain]

    # We have 3 spine chains
    # - original (ORG_*)
    # - copy (*use original name*)
    # - reverse (MCH-rev_*)
    spine_chain_attrs = [("spine_%.2d" % (i + 1)) for i in range(len(spine_chain_orig))]
    mt_chain = bone_class_instance(obj, spine_chain_attrs) # ORG_*
    rv_chain = bone_class_instance(obj, spine_chain_attrs) # *
    ex_chain = bone_class_instance(obj, spine_chain_attrs) # MCH-rev_*
    
    for i, child_name in enumerate(spine_chain):
        child_name_orig = spine_chain_orig[i]

        attr = spine_chain_attrs[i] # eg. spine_04

        setattr(mt_chain, attr, spine_chain[i]) # use the new name

        ebone = copy_bone_simple(arm, child_name, child_name_orig) # use the original name
        setattr(ex_chain, attr, ebone.name)

        ebone = copy_bone_simple(arm, child_name, "MCH-rev_%s" % child_name_orig)
        setattr(rv_chain, attr, ebone.name)
        ebone.connected = False

    mt_chain.update()
    ex_chain.update()
    rv_chain.update()

    # Now we need to re-parent these chains
    for i, child_name in enumerate(spine_chain_orig):        
        attr = spine_chain_attrs[i] + "_e"
        
        if i == 0:
            getattr(ex_chain, attr).parent = mt.pelvis_e
        else:
            attr_parent = spine_chain_attrs[i-1] + "_e"
            getattr(ex_chain, attr).parent = getattr(ex_chain, attr_parent)
        
        # intentional! get the parent from the other paralelle chain member
        getattr(rv_chain, attr).parent = getattr(ex_chain, attr)
    
    
    # ex_chain needs to interlace bones!
    # Note, skip the first bone
    for i in range(1, len(spine_chain_orig)): # similar to neck
        child_name_orig = spine_chain_orig[i]
        spine_e = getattr(mt_chain, spine_chain_attrs[i] + "_e")
        
        # dont store parent names, re-reference as each chain bones parent.
        spine_e_parent = arm.edit_bones.new("MCH-rot_%s" % child_name_orig)
        spine_e_parent.head = spine_e.head
        spine_e_parent.tail = spine_e.head + Vector(0.0, 0.0, spine_chain_segment_length / 2.0)
        spine_e_parent.roll = 0.0
        
        spine_e = getattr(ex_chain, spine_chain_attrs[i] + "_e")
        orig_parent = spine_e.parent
        spine_e.connected = False
        spine_e.parent = spine_e_parent
        spine_e_parent.connected = False

        spine_e_parent.parent = orig_parent
        

    # Rotate the rev chain 180 about the by the first bones center point
    pivot = (rv_chain.spine_01_e.head + rv_chain.spine_01_e.tail) * 0.5
    matrix = RotationMatrix(radians(180), 3, 'X')
    for i in range(len(spine_chain_orig)): # similar to neck
        spine_e = getattr(rv_chain, spine_chain_attrs[i] + "_e")
        # use the first bone as the pivot

        spine_e.head = ((spine_e.head - pivot) * matrix) + pivot
        spine_e.tail = ((spine_e.tail - pivot) * matrix) + pivot
        spine_e.roll += pi # 180d roll

    # done with editmode
    # TODO, posemode
    