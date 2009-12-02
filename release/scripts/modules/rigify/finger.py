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
from rigify import get_bone_data, empty_layer
from rna_prop_ui import rna_idprop_ui_get, rna_idprop_ui_prop_get
from functools import reduce


def metarig_template():
    bpy.ops.object.mode_set(mode='EDIT')
    obj = bpy.context.object
    arm = obj.data
    bone = arm.edit_bones.new('finger.01')
    bone.head[:] = 0.0000, 0.0000, 0.0000
    bone.tail[:] = 0.8788, -0.4584, -0.1327
    bone.roll = -2.8722
    bone.connected = False
    bone = arm.edit_bones.new('finger.02')
    bone.head[:] = 0.8788, -0.4584, -0.1327
    bone.tail[:] = 1.7483, -0.9059, -0.3643
    bone.roll = -2.7099
    bone.connected = True
    bone.parent = arm.edit_bones['finger.01']
    bone = arm.edit_bones.new('finger.03')
    bone.head[:] = 1.7483, -0.9059, -0.3643
    bone.tail[:] = 2.2478, -1.1483, -0.7408
    bone.roll = -2.1709
    bone.connected = True
    bone.parent = arm.edit_bones['finger.02']

    bpy.ops.object.mode_set(mode='OBJECT')
    pbone = obj.pose.bones['finger.01']
    pbone['type'] = 'finger'


def main(obj, orig_bone_name):
    
    # *** EDITMODE
    
    # get assosiated data 
    arm, orig_pbone, orig_ebone = get_bone_data(obj, orig_bone_name)
    
    obj.animation_data_create() # needed if its a new armature with no keys
    
    arm.layer[0] = arm.layer[8] = True
    
    children = orig_pbone.children_recursive
    tot_len = reduce(lambda f, pbone: f + pbone.bone.length, children, orig_pbone.bone.length)
    
    base_name = orig_pbone.basename
    
    # first make a new bone at the location of the finger
    control_ebone = arm.edit_bones.new(base_name)
    control_bone_name = control_ebone.name # we dont know if we get the name requested
    
    control_ebone.connected = orig_ebone.connected
    control_ebone.parent = orig_ebone.parent
    
    # Place the finger bone
    head = orig_ebone.head.copy()
    tail = orig_ebone.tail.copy()
    
    control_ebone.head = head
    control_ebone.tail = head + ((tail - head).normalize() * tot_len)
    control_ebone.roll = orig_ebone.roll
    
    # now add bones inbetween this and its children recursively
    
    # switching modes so store names only!
    children = [pbone.name for pbone in children]

    # set an alternate layer for driver bones
    other_layer = empty_layer[:]
    other_layer[8] = True
    

    driver_bone_pairs = []

    for child_bone_name in children:
        arm, pbone_child, child_ebone = get_bone_data(obj, child_bone_name)
        
        # finger.02 --> finger_driver.02
        driver_bone_name = child_bone_name.split('.')
        driver_bone_name = driver_bone_name[0] + "_driver." + ".".join(driver_bone_name[1:])
        
        driver_ebone = arm.edit_bones.new(driver_bone_name)
        driver_bone_name = driver_ebone.name # cant be too sure!
        driver_ebone.layer = other_layer
        
        new_len = pbone_child.bone.length / 2.0

        head = child_ebone.head.copy()
        tail = child_ebone.tail.copy()
        
        driver_ebone.head = head
        driver_ebone.tail = head + ((tail - head).normalize() * new_len)
        driver_ebone.roll = child_ebone.roll
        
        # Insert driver_ebone in the chain without connected parents
        driver_ebone.connected = False
        driver_ebone.parent = child_ebone.parent
        
        child_ebone.connected = False
        child_ebone.parent = driver_ebone
        
        # Add the drivers to these when in posemode.
        driver_bone_pairs.append((child_bone_name, driver_bone_name))
    
    del control_ebone
    

    # *** POSEMODE
    bpy.ops.object.mode_set(mode='OBJECT')
    
    
    arm, orig_pbone, orig_bone = get_bone_data(obj, orig_bone_name)
    arm, control_pbone, control_bone= get_bone_data(obj, control_bone_name)
    
    
    # only allow Y scale
    control_pbone.lock_scale = (True, False, True)
    
    control_pbone["bend_ratio"] = 0.4
    prop = rna_idprop_ui_prop_get(control_pbone, "bend_ratio", create=True)
    prop["soft_min"] = 0.0
    prop["soft_max"] = 1.0
    
    con = orig_pbone.constraints.new('COPY_LOCATION')
    con.target = obj
    con.subtarget = control_bone_name
    
    con = orig_pbone.constraints.new('COPY_ROTATION')
    con.target = obj
    con.subtarget = control_bone_name
    
    
    
    # setup child drivers on each new smaller bone added. assume 2 for now.
    
    # drives the bones
    controller_path = control_pbone.path_to_id() # 'pose.bones["%s"]' % control_bone_name

    i = 0
    for child_bone_name, driver_bone_name in driver_bone_pairs:

        # XXX - todo, any number
        if i==2:
            break
        
        arm, driver_pbone, driver_bone = get_bone_data(obj, driver_bone_name)
        
        driver_pbone.rotation_mode = 'YZX'
        fcurve_driver = driver_pbone.driver_add("rotation_euler", 0)
        
        #obj.driver_add('pose.bones["%s"].scale', 1)
        #obj.animation_data.drivers[-1] # XXX, WATCH THIS
        driver = fcurve_driver.driver
        
        # scale target
        tar = driver.targets.new()
        tar.name = "scale"
        tar.id_type = 'OBJECT'
        tar.id = obj
        tar.rna_path = controller_path + '.scale[1]'

        # bend target
        tar = driver.targets.new()
        tar.name = "br"
        tar.id_type = 'OBJECT'
        tar.id = obj
        tar.rna_path = controller_path + '["bend_ratio"]'

        # XXX - todo, any number
        if i==0:
            driver.expression = '(-scale+1.0)*pi*2.0*(1.0-br)'
        elif i==1:
            driver.expression = '(-scale+1.0)*pi*2.0*br'
        
        arm, child_pbone, child_bone = get_bone_data(obj, child_bone_name)

        # only allow X rotation
        driver_pbone.lock_rotation = child_pbone.lock_rotation = (False, True, True)
        
        i += 1

