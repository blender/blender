import bpy


class SimpleCustomMenu(bpy.types.Menu):
    bl_label = "Simple Custom Menu"
    bl_idname = "OBJECT_MT_simple_custom_menu"

    def draw(self, context):
        layout = self.layout

        layout.operator("wm.open_mainfile")
        layout.operator("wm.save_as_mainfile")


def register():
    bpy.utils.register_class(SimpleCustomMenu)


def unregister():
    bpy.utils.unregister_class(SimpleCustomMenu)


if __name__ == "__main__":
    register()

    # The menu can also be called from scripts
    bpy.ops.wm.call_menu(name=SimpleCustomMenu.bl_idname)
