# This example adds an object mode tool to the toolbar.
# This is just the circle-select tool.
import bpy
from bpy.utils.toolsystem import ToolDef

@ToolDef.from_fn
def my_tool():
    def draw_settings(context, layout, tool):
        props = tool.operator_properties("view3d.select_circle")
        layout.prop(props, "radius")
    return dict(
        text="My Circle Select",
        description=(
            "This is a tooltip\n"
            "with multiple lines"
        ),
        icon="ops.generic.select_circle",
        widget=None,
        keymap=(
            ("view3d.select_circle", dict(deselect=False), dict(type='ACTIONMOUSE', value='PRESS')),
            ("view3d.select_circle", dict(deselect=True), dict(type='ACTIONMOUSE', value='PRESS', ctrl=True)),
        ),
        draw_settings=draw_settings,
    )


def register():
    bpy.utils.register_tool('VIEW_3D', 'OBJECT', my_tool)


def unregister():
    bpy.utils.unregister_tool('VIEW_3D', 'OBJECT', my_tool)

if __name__ == "__main__":
    register()
