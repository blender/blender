import bpy


def main(operator, context):
    space = context.space_data
    node_tree = space.node_tree
    node_active = context.active_node
    node_selected = context.selected_nodes

    # now we have the context, perform a simple operation
    if node_active in node_selected:
        node_selected.remove(node_active)
    if len(node_selected) != 1:
        operator.report({'ERROR'}, "2 nodes must be selected")
        return

    node_other, = node_selected

    # now we have 2 nodes to operate on
    if not node_active.inputs:
        operator.report({'ERROR'}, "Active node has no inputs")
        return

    if not node_other.outputs:
        operator.report({'ERROR'}, "Selected node has no outputs")
        return

    socket_in = node_active.inputs[0]
    socket_out = node_other.outputs[0]

    # add a link between the two nodes
    node_link = node_tree.links.new(socket_in, socket_out)


class NodeOperator(bpy.types.Operator):
    """Tooltip"""
    bl_idname = "node.simple_operator"
    bl_label = "Simple Node Operator"

    @classmethod
    def poll(cls, context):
        space = context.space_data
        return space.type == 'NODE_EDITOR'

    def execute(self, context):
        main(context)
        return {'FINISHED'}


def register():
    bpy.utils.register_class(NodeOperator)


def unregister():
    bpy.utils.unregister_class(NodeOperator)


if __name__ == "__main__":
    register()
