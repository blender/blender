import bpy
from sverchok.node_tree import SverchCustomTreeNode

class ToolsNode(bpy.types.Node, SverchCustomTreeNode):
    ''' NOT USED '''
    bl_idname = 'ToolsNode'
    bl_label = 'Tools node'
    bl_icon = 'OUTLINER_OB_EMPTY'
    #bl_height_default = 110
    #bl_width_min = 20
    #color = (1,1,1)
    color_ = bpy.types.ColorRamp

    def draw_buttons(self, context, layout):
        col = layout.column()
        col.scale_y = 15
        col.template_color_picker
        u = "Update "
        #col.operator(SverchokUpdateAll.bl_idname, text=u)
        op = col.operator(SverchokUpdateCurrent.bl_idname, text=u+self.id_data.name)
        op.node_group = self.id_data.name
        #box = layout.box()

        # add back if you need

        node_count = len(self.id_data.nodes)
        tex = str(node_count) + ' | ' + str(self.id_data.name)
        layout.label(text=tex)
        #layout.template_color_ramp(self, 'color_', expand=True)

    def update(self):
        self.use_custom_color = True
        self.color = (1.0, 0.0, 0.0)
        

