
def main(context):
    obj = context.active_object
    mesh = obj.data

    is_editmode = (obj.mode == 'EDIT')
    if is_editmode:
        bpy.ops.object.mode_set(mode='OBJECT', toggle=False)


    if not mesh.active_uv_texture:
        bpy.ops.mesh.uv_texture_add()

    # adjust UVs
    for i, uv in enumerate(mesh.active_uv_texture.data):
        uvs = uv.uv1, uv.uv2, uv.uv3, uv.uv4
        for j, v_idx in enumerate(mesh.faces[i].verts):
            if uv.uv_selected[j]:
                # apply the location of the vertex as a UV
                uvs[j][:] = mesh.verts[v_idx].co.xy       


    if is_editmode:
        bpy.ops.object.mode_set(mode='EDIT', toggle=False)

class UvOperator(bpy.types.Operator):
    ''''''
    bl_idname = "uv.simple_operator"
    bl_label = "Simple Object Operator"

    def poll(self, context):
        obj = context.active_object
        return (obj and obj.type == 'MESH')

    def execute(self, context):
        main(context)
        return {'FINISHED'}

bpy.types.register(UvOperator)

if __name__ == "__main__":
    bpy.ops.uv.simple_operator()
