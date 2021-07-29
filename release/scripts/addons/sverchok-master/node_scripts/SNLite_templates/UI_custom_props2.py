"""
in objects_in o d=[[]] n=0
out stuff     v
"""

def ui(self, context, layout):
    scn = context.scene
    if not hasattr(scn, 'some_scriptnode_props'):
        return
    layout.prop(scn.some_scriptnode_props, 'custom_1')
        
scn = bpy.context.scene
if hasattr(scn, 'some_scriptnode_props'):
    print(scn.some_scriptnode_props.custom_1)


stuff = []