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

# not used, defined for completeness
METARIG_NAMES = ("pelvis", "ribcage")

def metarig_template():
    bpy.ops.object.mode_set(mode='EDIT')
    obj = bpy.context.object
    arm = obj.data
    bone = arm.edit_bones.new('pelvis')
    bone.head[:] = -0.0000, -0.2559, 0.8673
    bone.tail[:] = -0.0000, -0.2559, -0.1327
    bone.roll = 0.0000
    bone.connected = False
    bone = arm.edit_bones.new('rib_cage')
    bone.head[:] = -0.0000, -0.2559, 0.8673
    bone.tail[:] = -0.0000, -0.2559, 1.8673
    bone.roll = -0.0000
    bone.connected = False
    bone.parent = arm.edit_bones['pelvis']
    bone = arm.edit_bones.new('spine.01')
    bone.head[:] = -0.0000, -0.0000, 0.0000
    bone.tail[:] = -0.0000, -0.2559, 0.8673
    bone.roll = -0.0000
    bone.connected = False
    bone.parent = arm.edit_bones['rib_cage']
    bone = arm.edit_bones.new('spine.02')
    bone.head[:] = -0.0000, -0.2559, 0.8673
    bone.tail[:] = -0.0000, -0.3321, 1.7080
    bone.roll = -0.0000
    bone.connected = True
    bone.parent = arm.edit_bones['spine.01']
    bone = arm.edit_bones.new('spine.03')
    bone.head[:] = -0.0000, -0.3321, 1.7080
    bone.tail[:] = -0.0000, -0.0787, 2.4160
    bone.roll = 0.0000
    bone.connected = True
    bone.parent = arm.edit_bones['spine.02']
    bone = arm.edit_bones.new('spine.04')
    bone.head[:] = -0.0000, -0.0787, 2.4160
    bone.tail[:] = -0.0000, 0.2797, 3.0016
    bone.roll = 0.0000
    bone.connected = True
    bone.parent = arm.edit_bones['spine.03']
    bone = arm.edit_bones.new('spine.05')
    bone.head[:] = -0.0000, 0.2797, 3.0016
    bone.tail[:] = -0.0000, 0.4633, 3.6135
    bone.roll = 0.0000
    bone.connected = True
    bone.parent = arm.edit_bones['spine.04']
    bone = arm.edit_bones.new('spine.06')
    bone.head[:] = -0.0000, 0.4633, 3.6135
    bone.tail[:] = -0.0000, 0.3671, 4.3477
    bone.roll = -0.0000
    bone.connected = True
    bone.parent = arm.edit_bones['spine.05']
    bone = arm.edit_bones.new('spine.07')
    bone.head[:] = -0.0000, 0.3671, 4.3477
    bone.tail[:] = -0.0000, 0.0175, 5.0033
    bone.roll = -0.0000
    bone.connected = True
    bone.parent = arm.edit_bones['spine.06']

    bpy.ops.object.mode_set(mode='OBJECT')
    pbone = obj.pose.bones['rib_cage']
    pbone['type'] = 'spine'


def metarig_definition(obj, orig_bone_name):
    '''
    The bone given is the second in a chain.
    Expects at least 1 parent and a chain of children withe the same basename
    eg.
        pelvis -> rib_cage -> spine.01 -> spine.02 -> spine.03

    note: same as neck.
    '''
    arm = obj.data
    ribcage = arm.bones[orig_bone_name]
    pelvis = ribcage.parent
    
    children = ribcage.children
    if len(children) != 1:
        print("expected the ribcage to have only 1 child.")

    child = children[0]
    bone_definition = [pelvis.name, ribcage.name, child.name]
    bone_definition.extend([child.name for child in child.children_recursive_basename])
    return bone_definition

def fk(*args):
    main(*args)

def main(obj, bone_definition, base_names):
    from Mathutils import Vector, Matrix, RotationMatrix
    from math import radians, pi

    arm = obj.data

    # Initialize container classes for convenience
    mt = bone_class_instance(obj, ["pelvis", "ribcage"]) # meta
    mt.pelvis = bone_definition[0]
    mt.ribcage = bone_definition[1]
    mt.update()

    spine_chain_orig = tuple(bone_definition[2:])
    spine_chain = [arm.edit_bones[child_name] for child_name in spine_chain_orig]
    spine_chain_basename = base_names[spine_chain[0].name].rsplit(".", 1)[0] # probably 'ORG-spine.01' -> 'spine'
    spine_chain_len = len(spine_chain_orig)
    print(spine_chain_orig)
    print(spine_chain_len)

    '''
    children = mt.ribcage_e.children
    child = children[0] # validate checks for 1 only.
    spine_chain_basename = child.basename # probably 'spine'
    spine_chain_segment_length = child.length
    spine_chain = [child] + child.children_recursive_basename
    spine_chain_orig = [child.name for child in spine_chain]
    '''
    
    child = spine_chain[0]
    spine_chain_segment_length = child.length
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

    spine_chain = [child.name for child in spine_chain]

    # We have 3 spine chains
    # - original (ORG_*)
    # - copy (*use original name*)
    # - reverse (MCH-rev_*)
    spine_chain_attrs = [("spine_%.2d" % (i + 1)) for i in range(spine_chain_len)]

    mt_chain = bone_class_instance(obj, spine_chain_attrs) # ORG_*
    rv_chain = bone_class_instance(obj, spine_chain_attrs) # *
    ex_chain = bone_class_instance(obj, spine_chain_attrs) # MCH-rev_*
    del spine_chain_attrs
    
    for i, child_name in enumerate(spine_chain):
        child_name_orig = base_names[spine_chain_orig[i]]

        attr = mt_chain.attr_names[i] # eg. spine_04

        setattr(mt_chain, attr, spine_chain_orig[i]) # the original bone

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
        attr = ex_chain.attr_names[i] + "_e"
        
        if i == 0:
            getattr(ex_chain, attr).parent = mt.pelvis_e
        else:
            attr_parent = ex_chain.attr_names[i-1] + "_e"
            getattr(ex_chain, attr).parent = getattr(ex_chain, attr_parent)
        
        # intentional! get the parent from the other paralelle chain member
        getattr(rv_chain, attr).parent = getattr(ex_chain, attr)
    
    
    # ex_chain needs to interlace bones!
    # Note, skip the first bone
    for i in range(1, spine_chain_len): # similar to neck
        child_name_orig = spine_chain_orig[i]
        spine_e = getattr(mt_chain, mt_chain.attr_names[i] + "_e")
        
        # dont store parent names, re-reference as each chain bones parent.
        spine_e_parent = arm.edit_bones.new("MCH-rot_%s" % child_name_orig)
        spine_e_parent.head = spine_e.head
        spine_e_parent.tail = spine_e.head + Vector(0.0, 0.0, spine_chain_segment_length / 2.0)
        spine_e_parent.roll = 0.0
        
        spine_e = getattr(ex_chain, ex_chain.attr_names[i] + "_e")
        orig_parent = spine_e.parent
        spine_e.connected = False
        spine_e.parent = spine_e_parent
        spine_e_parent.connected = False

        spine_e_parent.parent = orig_parent
        

    # Rotate the rev chain 180 about the by the first bones center point
    pivot = (rv_chain.spine_01_e.head + rv_chain.spine_01_e.tail) * 0.5
    matrix = RotationMatrix(radians(180), 3, 'X')
    for i, attr in enumerate(rv_chain.attr_names): # similar to neck
        spine_e = getattr(rv_chain, attr + "_e")
        # use the first bone as the pivot

        spine_e.head = ((spine_e.head - pivot) * matrix) + pivot
        spine_e.tail = ((spine_e.tail - pivot) * matrix) + pivot
        spine_e.roll += pi # 180d roll
        del spine_e
    
    
    bpy.ops.object.mode_set(mode='OBJECT')
    
    # refresh pose bones
    mt.update()
    ex.update()
    df.update()
    mt_chain.update()
    ex_chain.update()
    rv_chain.update()
    
    # df.pelvis_p / DEF-wgt_pelvis
    con = df.pelvis_p.constraints.new('COPY_LOCATION')
    con.target = obj
    con.subtarget = ex.pelvis
    con.owner_space = 'LOCAL'
    con.target_space = 'LOCAL'
    
    con = df.pelvis_p.constraints.new('COPY_ROTATION')
    con.target = obj
    con.subtarget = ex.pelvis
    con.owner_space = 'LOCAL'
    con.target_space = 'LOCAL'
    
    # df.ribcage_p / DEF-wgt_rib_cage
    con = df.ribcage_p.constraints.new('COPY_ROTATION')
    con.target = obj
    con.subtarget = ex.ribcage
    con.owner_space = 'LOCAL'
    con.target_space = 'LOCAL'
    
    con = df.ribcage_p.constraints.new('COPY_LOCATION')
    con.target = obj
    con.subtarget = ex.ribcage
    con.owner_space = 'LOCAL'
    con.target_space = 'LOCAL'
    
    con = ex.ribcage_hinge_p.constraints.new('COPY_ROTATION')
    con.name = "hinge"
    con.target = obj
    con.subtarget = mt.pelvis
    
    # add driver
    hinge_driver_path = mt.ribcage_p.path_to_id() + '["hinge"]'
    
    fcurve = con.driver_add("influence", 0)
    driver = fcurve.driver
    tar = driver.targets.new()
    driver.type = 'AVERAGE'
    tar.name = "var"
    tar.id_type = 'OBJECT'
    tar.id = obj
    tar.rna_path = hinge_driver_path
    
    mod = fcurve.modifiers[0]
    mod.poly_order = 1
    mod.coefficients[0] = 1.0
    mod.coefficients[1] = -1.0
    
    
    
    con = ex.spine_rotate_p.constraints.new('COPY_ROTATION')
    con.target = obj
    con.subtarget = mt.ribcage


    # ex.pelvis_p / MCH-wgt_pelvis
    con = ex.pelvis_p.constraints.new('COPY_LOCATION')
    con.target = obj
    con.subtarget = mt_chain.spine_01
    con.owner_space = 'WORLD'
    con.target_space = 'WORLD'

    con = ex.pelvis_p.constraints.new('COPY_ROTATION')
    con.target = obj
    con.subtarget = mt_chain.spine_01
    con.owner_space = 'WORLD'
    con.target_space = 'WORLD'
    
    # ex.ribcage_p / MCH-wgt_rib_cage
    con = ex.ribcage_p.constraints.new('COPY_LOCATION')
    con.target = obj
    con.subtarget = getattr(mt_chain, mt_chain.attr_names[-1])
    con.head_tail = 0.0

    con = ex.ribcage_p.constraints.new('COPY_ROTATION')
    con.target = obj
    con.subtarget = getattr(mt_chain, mt_chain.attr_names[-1])    
    
    # mt.pelvis_p / rib_cage
    con = mt.ribcage_p.constraints.new('COPY_LOCATION')
    con.target = obj
    con.subtarget = mt.pelvis
    con.head_tail = 0.0
    
    # This stores all important ID props
    prop = rna_idprop_ui_prop_get(mt.ribcage_p, "hinge", create=True)
    mt.ribcage_p["hinge"] = 1.0
    prop["soft_min"] = 0.0
    prop["soft_max"] = 1.0
    
    prop = rna_idprop_ui_prop_get(mt.ribcage_p, "pivot_slide", create=True)
    mt.ribcage_p["pivot_slide"] = 0.5
    prop["soft_min"] = 1.0 / spine_chain_len
    prop["soft_max"] = 1.0
    
    for i in range(spine_chain_len - 1):
        prop_name = "bend_%.2d" % (i + 1)
        prop = rna_idprop_ui_prop_get(mt.ribcage_p, prop_name, create=True)
        mt.ribcage_p[prop_name] = 1.0
        prop["soft_min"] = 0.0
        prop["soft_max"] = 1.0
    
    # Create a fake connected parent/child relationship with bone location constraints
    # positioned at the tip.
    
    # reverse bones / MCH-rev_spine.##
    for i in range(1, spine_chain_len):
        spine_p = getattr(rv_chain, rv_chain.attr_names[i] + "_p")
        spine_fake_parent_name = getattr(rv_chain, rv_chain.attr_names[i - 1])
        
        con = spine_p.constraints.new('COPY_LOCATION')
        con.target = obj
        con.subtarget = spine_fake_parent_name
        con.head_tail = 1.0
        del spine_p, spine_fake_parent_name, con
    
    
    # Constrain 'inbetween' bones
    
    # b01/max(0.001,b01+b02+b03+b04+b05)
    target_names = [("b%.2d" % (i + 1)) for i in range(spine_chain_len - 1)]
    expression_suffix = "/max(0.001,%s)" % "+".join(target_names)
    
    rib_driver_path = mt.ribcage_p.path_to_id()
    
    for i in range(1, spine_chain_len):
        
        spine_p = getattr(ex_chain, ex_chain.attr_names[i] + "_p")
        spine_p_parent = spine_p.parent # interlaced bone

        con = spine_p_parent.constraints.new('COPY_ROTATION')
        con.target = obj
        con.subtarget = ex.spine_rotate
        con.owner_space = 'LOCAL'
        con.target_space = 'LOCAL'
        del spine_p
        
        # add driver
        fcurve = con.driver_add("influence", 0)
        driver = fcurve.driver
        driver.type = 'SCRIPTED'
        # b01/max(0.001,b01+b02+b03+b04+b05)
        driver.expression = target_names[i - 1] + expression_suffix
        fcurve.modifiers.remove(0) # grr dont need a modifier

        for j in range(spine_chain_len - 1):
            tar = driver.targets.new()
            tar.name = target_names[j]
            tar.id_type = 'OBJECT'
            tar.id = obj
            tar.rna_path = rib_driver_path + ('["bend_%.2d"]' % (j + 1))
    
    
    # original bone drivers
    # note: the first bone has a lot more constraints, but also this simple one is first.
    for i, attr in enumerate(mt_chain.attr_names):
        spine_p = getattr(mt_chain, attr + "_p")
        
        con = spine_p.constraints.new('COPY_ROTATION')
        con.target = obj
        con.subtarget = getattr(ex_chain, attr) # lock to the copy's rotation
        del spine_p
    
    # pivot slide: - lots of copy location constraints.
    
    con = mt_chain.spine_01_p.constraints.new('COPY_LOCATION')
    con.name = "base"
    con.target = obj
    con.subtarget = rv_chain.spine_01 # lock to the reverse location
    
    for i in range(1, spine_chain_len + 1):
        con = mt_chain.spine_01_p.constraints.new('COPY_LOCATION')
        con.name = "slide_%d" % i
        con.target = obj
        
        if i == spine_chain_len:
            attr = mt_chain.attr_names[i - 1]
        else:
            attr = mt_chain.attr_names[i]

        con.subtarget = getattr(rv_chain, attr) # lock to the reverse location
        
        if i == spine_chain_len:
            con.head_tail = 1.0

        fcurve = con.driver_add("influence", 0)
        driver = fcurve.driver
        tar = driver.targets.new()
        driver.type = 'AVERAGE'
        tar.name = "var"
        tar.id_type = 'OBJECT'
        tar.id = obj
        tar.rna_path = rib_driver_path + '["pivot_slide"]'
        
        mod = fcurve.modifiers[0]
        mod.poly_order = 1
        mod.coefficients[0] = - (i - 1)
        mod.coefficients[1] = spine_chain_len
    
    # no support for blending chains
    return None

