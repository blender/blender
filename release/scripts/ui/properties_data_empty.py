
import bpy

class DataButtonsPanel(bpy.types.Panel):
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "data"

    def poll(self, context):
        return (context.object and context.object.type == 'EMPTY')

class DATA_PT_empty(DataButtonsPanel):
    bl_label = "Empty"

    def draw(self, context):
        layout = self.layout

        ob = context.object

        layout.itemR(ob, "empty_draw_type", text="Display")
        layout.itemR(ob, "empty_draw_size", text="Size")

bpy.types.register(DATA_PT_empty)
