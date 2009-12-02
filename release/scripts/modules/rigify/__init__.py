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
from Mathutils import Vector

# TODO, have these in a more general module
from rna_prop_ui import rna_idprop_ui_get, rna_idprop_ui_prop_get

empty_layer = [False] * 32

def auto_class(slots, name="ContainerClass"):
    return type(name, (object,), {"__slots__":tuple(slots)})
    
def auto_class_instance(slots, name="ContainerClass"):
    return auto_class(slots, name)()

def bone_class_instance(obj, slots, name="BoneContainer"):
    slots = slots[:] # dont modify the original
    for i in range(len(slots)):
        member = slots[i]
        slots.append(member + "_b") # bone bone
        slots.append(member + "_p") # pose bone
        slots.append(member + "_e") # edit bone
    
    slots.extend(["obj", "update"])
    
    instance = auto_class_instance(slots, name)
    
    def update():
        '''
        Re-Assigns bones from the blender data
        '''
        arm = obj.data
        
        bbones = arm.bones
        pbones = obj.pose.bones
        ebones = arm.edit_bones
        
        for member in slots:
            
            if member in ("update", "obj"):
                continue
            
            if not member[-2] == "_":
                name = getattr(instance, member, None)
                if name is not None:
                    setattr(instance, member + "_b", bbones.get(name, None))
                    setattr(instance, member + "_p", pbones.get(name, None))
                    setattr(instance, member + "_e", ebones.get(name, None))
        
    instance.update = update
    
    return instance

def gen_none(obj, orig_bone_name):
    pass

def get_bone_data(obj, bone_name):
    arm = obj.data
    pbone = obj.pose.bones[bone_name]
    if obj.mode == 'EDIT':
        bone = arm.edit_bones[bone_name]
    else:
        bone = arm.bones[bone_name]
    
    return arm, pbone, bone

def copy_bone_simple(arm, from_bone, name, parent=False):
    ebone = arm.edit_bones[from_bone]
    ebone_new = arm.edit_bones.new(name)
    
    if parent:
        ebone_new.connected = ebone.connected
        ebone_new.parent = ebone.parent
    
    ebone_new.head = ebone.head
    ebone_new.tail = ebone.tail
    ebone_new.roll = ebone.roll
    return ebone_new


def add_stretch_to(obj, from_name, to_name, name):
    '''
    Adds a bone that stretches from one to another
    '''
    
    mode_orig = obj.mode
    bpy.ops.object.mode_set(mode='EDIT')

    arm = obj.data
    stretch_ebone = arm.edit_bones.new(name)
    stretch_name = stretch_ebone.name
    del name
    
    head = stretch_ebone.head = arm.edit_bones[from_name].head.copy()
    #tail = stretch_ebone.tail = arm.edit_bones[to_name].head.copy()
    
    # annoying exception for zero length bones, since its using stretch_to the rest pose doesnt really matter
    #if (head - tail).length < 0.1:
    if 1:
        tail = stretch_ebone.tail = arm.edit_bones[from_name].tail.copy()


    # Now for the constraint
    bpy.ops.object.mode_set(mode='OBJECT')
    from_pbone = obj.pose.bones[from_name]
    to_pbone = obj.pose.bones[to_name]
    
    stretch_pbone = obj.pose.bones[stretch_name]
    
    con = stretch_pbone.constraints.new('COPY_LOCATION')
    con.target = obj
    con.subtarget = from_name
    
    con = stretch_pbone.constraints.new('STRETCH_TO')
    con.target = obj
    con.subtarget = to_name   
    con.original_length = (head - tail).length
    con.keep_axis = 'PLANE_X'
    con.volume = 'NO_VOLUME'

    bpy.ops.object.mode_set(mode=mode_orig)


def add_pole_target_bone(obj, base_name, name, mode='CROSS'):
    '''
    Does not actually create a poll target, just the bone to use as a poll target
    '''
    mode_orig = obj.mode
    bpy.ops.object.mode_set(mode='EDIT')
    
    arm = obj.data
    
    poll_ebone = arm.edit_bones.new(base_name + "_poll")
    base_ebone = arm.edit_bones[base_name]
    poll_name = poll_ebone.name
    parent_ebone = base_ebone.parent
    
    base_head = base_ebone.head.copy()
    base_tail = base_ebone.tail.copy()
    base_dir = base_head - base_tail
    
    parent_head = parent_ebone.head.copy()
    parent_tail = parent_ebone.tail.copy()
    parent_dir = parent_head - parent_tail
    
    distance = (base_dir.length + parent_dir.length)
    
    if mode == 'CROSS':
        offset = base_dir.copy().normalize() - parent_dir.copy().normalize()
        offset.length = distance
    else:
        offset = Vector(0,0,0)
        if mode[0]=="+":
            val = distance
        else:
            val = -distance
        
        setattr(offset, mode[1].lower(), val)
    
    poll_ebone.head = base_head + offset
    poll_ebone.tail = base_head + (offset * (1.0 - (1.0 / 4.0)))
    
    bpy.ops.object.mode_set(mode=mode_orig)
    
    return poll_name


def generate_rig(context, ob):
    
    global_undo = context.user_preferences.edit.global_undo
    context.user_preferences.edit.global_undo = False
    
    # add_stretch_to(ob, "a", "b", "c")

    bpy.ops.object.mode_set(mode='OBJECT')
    
    
    # copy object and data
    ob.selected = False
    ob_new = ob.copy()
    ob_new.data = ob.data.copy()
    scene = context.scene
    scene.objects.link(ob_new)
    scene.objects.active = ob_new
    ob_new.selected = True
    
    # enter armature editmode
    
    
    for pbone_name in ob_new.pose.bones.keys():
        bone_type = ob_new.pose.bones[pbone_name].get("type", "")

        if bone_type == "":
        	continue

        # submodule = getattr(self, bone_type)
        # exec("from rigify import %s as submodule")
        submodule = __import__(name="%s.%s" % (__package__, bone_type), fromlist=[bone_type])

        reload(submodule) # XXX, dev only

        
        # Toggle editmode so the pose data is always up to date
        bpy.ops.object.mode_set(mode='EDIT')
        submodule.main(ob_new, pbone_name)
        bpy.ops.object.mode_set(mode='OBJECT')
    
    # needed to update driver deps
    # context.scene.update()
    
    # Only for demo'ing
    ob.restrict_view = True
    ob_new.data.draw_axes = False
    
    context.user_preferences.edit.global_undo = global_undo

if __name__ == "__main__":
    generate_rig(bpy.context, bpy.context.object)
