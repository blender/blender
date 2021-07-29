import bpy



class TestOp(bpy.types.Operator):
    bl_idname = "node.sverchok_debug_test"
    bl_label = "Debug"

    def execute(self, context):
        for i in range(1000):
            bpy.ops.node.add_node(type="ObjectsNodeMK2")
            node = context.active_node
            bpy.ops.node.add_node(type="SvBmeshViewerNodeMK2")
            node.select = True
            bpy.ops.node.delete()
            print(i)
        return {"FINISHED"}


# def register():
#    bpy.utils.register_class(TestOp)

# test