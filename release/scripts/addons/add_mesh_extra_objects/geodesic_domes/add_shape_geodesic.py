import bpy
import mathutils


def reset_transform(ob):
    m = mathutils.Matrix()
    ob.matrix_local = m


def func_add_corrective_pose_shape_fast(source, target):
    result = ""
    reset_transform(target)
    # If target object doesn't have Basis shape key, create it.
    try:
        num_keys = len(target.data.shape_keys.key_blocks)
    except:
        basis = target.shape_key_add()
        basis.name = "Basis"
        target.data.update()
    key_index = target.active_shape_key_index
    if key_index == 0:
        # Insert new shape key
        new_shapekey = target.shape_key_add()
        new_shapekey.name = "Shape_" + source.name
        new_shapekey_name = new_shapekey.name
        key_index = len(target.data.shape_keys.key_blocks) - 1
        target.active_shape_key_index = key_index
    # else, the active shape will be used (updated)
    target.show_only_shape_key = True
    shape_key_verts = target.data.shape_keys.key_blocks[key_index].data
    try:
        vgroup = target.active_shape_key.vertex_group
        target.active_shape_key.vertex_group = ''
    except:
        print("blub")
        result = "***ERROR*** blub"
        pass
    # copy the local vertex positions to the new shape
    verts = source.data.vertices
    try:
        for n in range(len(verts)):
            shape_key_verts[n].co = verts[n].co
        # go to all armature modifies and unpose the shape
    except:
        message = "***ERROR***, meshes have different number of vertices"
        result = message
    for n in target.modifiers:
        if n.type == 'ARMATURE' and n.show_viewport:
            # print("got one")
            n.use_bone_envelopes = False
            n.use_deform_preserve_volume = False
            n.use_vertex_groups = True
            armature = n.object
            unposeMesh(shape_key_verts, target, armature)
            break

    # set the new shape key value to 1.0, so we see the result instantly
    target.data.shape_keys.key_blocks[ target.active_shape_key_index].value = 1.0
    try:
        target.active_shape_key.vertex_group = vgroup
    except:
        print("bluba")
        result = result + "bluba"
        pass
    target.show_only_shape_key = False
    target.data.update()
    return result


class add_corrective_pose_shape_fast(bpy.types.Operator):
    bl_idname = "object.add_corrective_pose_shape_fast"
    bl_label = "Add object as corrective shape faster"
    bl_description = "Adds 1st object as shape to 2nd object as pose shape (only 1 armature)"

    @classmethod
    def poll(cls, context):
        return context.active_object is not None

    def execute(self, context):

        if len(context.selected_objects) > 2:
            print("Select source and target objects please")
            return {'FINISHED'}

        selection = context.selected_objects
        target = context.active_object
        if context.active_object == selection[0]:
            source = selection[1]
        else:
            source = selection[0]
        print(source)
        print(target)
        func_add_corrective_pose_shape_fast(source, target)

        return {'FINISHED'}


def register():
    bpy.utils.register_module(__name__)


def unregister():
    bpy.utils.unregister_module(__name__)

if __name__ == "__main__":
    register()
