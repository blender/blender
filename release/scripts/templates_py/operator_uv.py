import bpy
import bmesh


def main(context):
    obj = context.active_object
    me = obj.data
    bm = bmesh.from_edit_mesh(me)

    uv_layer = bm.loops.layers.uv.verify()
    bm.faces.layers.tex.verify()  # currently blender needs both layers.

    # adjust UVs
    for f in bm.faces:
        for l in f.loops:
            luv = l[uv_layer]
            if luv.select:
                # apply the location of the vertex as a UV
                luv.uv = l.vert.co.xy

    bmesh.update_edit_mesh(me)


class UvOperator(bpy.types.Operator):
    """UV Operator description"""
    bl_idname = "uv.simple_operator"
    bl_label = "Simple UV Operator"

    @classmethod
    def poll(cls, context):
        return (context.mode == 'EDIT_MESH')

    def execute(self, context):
        main(context)
        return {'FINISHED'}


def register():
    bpy.utils.register_class(UvOperator)


def unregister():
    bpy.utils.unregister_class(UvOperator)


if __name__ == "__main__":
    register()

    # test call
    bpy.ops.uv.simple_operator()
