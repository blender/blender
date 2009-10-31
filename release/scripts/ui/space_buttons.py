
import bpy

class Buttons_HT_header(bpy.types.Header):
    bl_space_type = 'PROPERTIES'

    def draw(self, context):
        layout = self.layout

        so = context.space_data
        scene = context.scene

        row= layout.row(align=True)
        row.template_header()

        if context.area.show_menus:
            sub = row.row(align=True)
            sub.itemM("Buttons_MT_view", text="View")

        row = layout.row()
        row.itemR(so, "buttons_context", expand=True, text="")
        row.itemR(scene, "current_frame")

class Buttons_MT_view(bpy.types.Menu):
    bl_label = "View"

    def draw(self, context):
        layout = self.layout
        so = context.space_data

        col = layout.column()
        col.itemR(so, "panel_alignment", expand=True)

bpy.types.register(Buttons_HT_header)
bpy.types.register(Buttons_MT_view)
