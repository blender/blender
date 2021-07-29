"""
in floats_in s .=[] n=0.0
out floats_out s
ui = my_temp_material, RGB Curves
"""

from sverchok.utils.snlite_utils import get_valid_evaluate_function as get_evaluator

# currently the node noame (here 'RGB Curves') must be uniqe per material.
# copying nodes within the same nodetree  does not automatically 'bump' the nodename
# to the next available. You must be aware of this current implementation limitation.
evaluate = get_evaluator('my_temp_material', 'RGB Curves')

self.width = 200
def ui(self, context, layout):
    m = bpy.data.materials.get('my_temp_material')
    if not m:
        return
    tnode = m.node_tree.nodes['RGB Curves']
    layout.template_curve_mapping(tnode, "mapping", type="NONE")


floats_out = []
for flist in floats_in:
    floats_out.append([evaluate(v) for v in flist])
