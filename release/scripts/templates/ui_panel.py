import bpy


class LayoutDemoPanel(bpy.types.Panel):
    """Creates a Panel in the scene context of the properties editor"""
    bl_label = "Layout Demo"
    bl_idname = "SCENE_PT_layout"
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "scene"

    def draw(self, context):
        layout = self.layout

        sc = context.scene
        
        #Create a simple row.
        layout.label(text=" Simple Row:")
        
        row = layout.row()
        row.prop(sc, "frame_start")
        row.prop(sc, "frame_end")
        
        #Create an row where the buttons are aligned to each other.
        layout.label(text=" Aligned Row")
        
        row = layout.row(align=True)
        row.prop(sc, "frame_start")
        row.prop(sc, "frame_end")
        
        #Create two columns, by using a split layout.
        split = layout.split()
        
        # First column
        col = split.column()
        col.label(text="Column One:")
        col.prop(sc, "frame_end")
        col.prop(sc, "frame_start")
        
        # Second column, aligned
        col = split.column(align=True)
        col.label(text="Column Two")
        col.prop(sc, "frame_start")
        col.prop(sc, "frame_end")


def register():
    bpy.utils.register_class(LayoutDemoPanel)


def unregister():
    bpy.utils.unregister_class(LayoutDemoPanel)


if __name__ == "__main__":
    register()
