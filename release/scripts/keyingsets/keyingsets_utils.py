# This file defines a set of methods that are useful for various 
# Relative Keying Set (RKS) related operations, such as: callbacks
# for polling, iterator callbacks, and also generate callbacks. 
# All of these can be used in conjunction with the others. 

import bpy

###########################
# General Utilities

# Append the specified property name on the the existing path
def path_add_property(path, prop):
    if len(path):
        return path + "." + prop;
    else:
        return prop;

###########################
# Poll Callbacks

# selected objects
def RKS_POLL_selected_objects(ksi, context):
    return context.active_object or len(context.selected_objects);
    
# selected bones
def RKS_POLL_selected_bones(ksi, context):
    # we must be in Pose Mode, and there must be some bones selected 
    if (context.active_object) and (context.active_object.mode == 'POSE'):
        if context.active_pose_bone or len(context.select_pose_bones):
            return True;
    
    # nothing selected 
    return False;


# selected bones or objects
def RKS_POLL_selected_items(ksi, context):
    return RKS_POLL_selected_bones(ksi, context) or RKS_POLL_selected_objects(ksi, context);

###########################
# Iterator Callbacks

# all selected objects or pose bones, depending on which we've got
def RKS_ITER_selected_item(ksi, context, ks):
    if (context.active_object) and (context.active_object.mode == 'POSE'):
        for bone in context.selected_pose_bones:
            ksi.generate(context, ks, bone)
    else:
        for ob in context.selected_objects:
            ksi.generate(context, ks, ob)

###########################
# Generate Callbacks

# 'Available' F-Curves
def RKS_GEN_available(ksi, context, ks, data):
    # try to get the animation data associated with the closest 
    # ID-block to the data (neither of which may exist/be easy to find)
    id_block = data.id_data
    adt = getattr(id_block, "animation_data", None)

    # there must also be an active action...
    if adt is None or adt.action is None:
        return;
        
    # for each F-Curve, include an path to key it
    # NOTE: we don't need to set the group settings here
    for fcu in adt.action.fcurves:
        ks.paths.add(id_block, fcu.data_path, index=fcu.array_index)
    
# ------

# get ID block and based ID path for transform generators
def get_transform_generators_base_info(data):
    # ID-block for the data 
    id_block = data.id_data
    
    # get base path and grouping method/name
    if isinstance(data, bpy.types.ID):
        # no path in this case
        path = ""
        
        # data on ID-blocks directly should get grouped by the KeyingSet
        grouping = None
    else:
        # get the path to the ID-block
        path = data.path_to_id()

        # try to use the name of the data element to group the F-Curve
        # else fallback on the KeyingSet name
        grouping = getattr(data, "name", None)
        
    # return the ID-block and the path
    return id_block, path, grouping

# Location 
def RKS_GEN_location(ksi, context, ks, data):
    # get id-block and path info
    id_block, base_path, grouping= get_transform_generators_base_info(data)
    
    # add the property name to the base path
    path = path_add_property(base_path, "location")
    
    # add Keying Set entry for this...
    if grouping:
        ks.paths.add(id_block, path, grouping_method='NAMED', group_name=grouping)
    else:
        ks.paths.add(id_block, path)

# Rotation 
def RKS_GEN_rotation(ksi, context, ks, data):
    # get id-block and path info
    id_block, base_path, grouping= get_transform_generators_base_info(data)
    
    # add the property name to the base path
    #   rotation mode affects the property used
    if data.rotation_mode == 'QUATERNION':
        path = path_add_property(base_path, "rotation_quaternion")
    elif data.rotation_mode == 'AXISANGLE':
        path = path_add_property(base_path, "rotation_axis_angle")
    else:
        path = path_add_property(base_path, "rotation_euler")
    
    # add Keying Set entry for this...
    if grouping:
        ks.paths.add(id_block, path, grouping_method='NAMED', group_name=grouping)
    else:
        ks.paths.add(id_block, path)

# Scaling 
def RKS_GEN_scaling(ksi, context, ks, data):
    # get id-block and path info
    id_block, base_path, grouping= get_transform_generators_base_info(data)
    
    # add the property name to the base path
    path = path_add_property(base_path, "scale")
    
    # add Keying Set entry for this...
    if grouping:
        ks.paths.add(id_block, path, grouping_method='NAMED', group_name=grouping)
    else:
        ks.paths.add(id_block, path)

###########################
# Un-needed stuff which is here to just shut up the warnings...

classes = []

def register():
    register = bpy.types.register
    for cls in classes:
        register(cls)


def unregister():
    unregister = bpy.types.unregister
    for cls in classes:
        unregister(cls)

if __name__ == "__main__":
    register()

###########################
