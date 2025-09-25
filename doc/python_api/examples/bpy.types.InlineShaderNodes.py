"""
Inline Shader Nodes
+++++++++++++++++++
"""
import bpy

# The materials should be retrieved from the evaluated object to make sure that
# e.g. edits of Geometry Nodes are applied.
depsgraph = bpy.context.view_layer.depsgraph
ob = bpy.context.active_object
ob_eval = depsgraph.id_eval_get(ob)
material_eval = ob_eval.material_slots[0].material

# Compute the inlined shader nodes.
# Important: Do not loose the reference to this object while accessing the inlined
#   node tree. Otherwise there will be a crash due to a dangling pointer.
inline_shader_nodes = material_eval.inline_shader_nodes()

# Get the actual inlined `bpy.types.NodeTree`.
tree = inline_shader_nodes.node_tree

for node in tree.nodes:
    print(node.name)
