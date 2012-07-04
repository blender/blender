import bpy


def main(context):
    obj = context.active_object
    mesh = obj.data

    is_editmode = (obj.mode == 'EDIT')
    if is_editmode:
        bpy.ops.object.mode_set(mode='OBJECT', toggle=False)

    if not mesh.uv_textures:
        uvtex = bpy.ops.mesh.uv_texture_add()
    else:
        uvtex = mesh.uv_textures.active

    # adjust UVs
    for i, uv in enumerate(uvtex.data):
        uvs = uv.uv1, uv.uv2, uv.uv3, uv.uv4
        for j, v_idx in enumerate(mesh.faces[i].vertices):
            if uv.select_uv[j]:
                # apply the location of the vertex as a UV
                uvs[j][:] = mesh.vertices[v_idx].co.xy

    if is_editmode:
        bpy.ops.object.mode_set(mode='EDIT', toggle=False)


class UvOperator(bpy.types.Operator):
    """UV Operator description"""
    bl_idname = "uv.simple_operator"
    bl_label = "Simple UV Operator"

    @classmethod
    def poll(cls, context):
        obj = context.active_object
        return (obj and obj.type == 'MESH')

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
