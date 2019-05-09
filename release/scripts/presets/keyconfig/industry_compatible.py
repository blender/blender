import os
import bpy

# ------------------------------------------------------------------------------
# Operators needed by this keymap to function

# Selection Modes

class IC_KEYMAP_OT_mesh_select_mode(bpy.types.Operator):
    bl_idname = "ic_keymap.mesh_select_mode"
    bl_label = "Switch to Vertex, Edge or Face Mode from any mode"
    bl_options = {'UNDO'}

    type: bpy.props.EnumProperty(
        name="Mode",
        items=(
            ('VERT', "Vertex", "Switcth to Vertex Mode From any Mode"),
            ('EDGE', "Edge", "Switcth to Edge Mode From any Mode"),
            ('FACE', "Face", "Switcth to Face Mode From any Mode"),
        ),
    )

    @classmethod
    def poll(cls, context):
        return (context.active_object is not None) and (context.object.type == 'MESH')

    def execute(self, context):
        bpy.ops.object.mode_set(mode='EDIT')
        bpy.ops.mesh.select_mode(type=self.type)

        return{'FINISHED'}


classes = (
    IC_KEYMAP_OT_mesh_select_mode,
)


# ------------------------------------------------------------------------------
# Keymap

dirname, filename = os.path.split(__file__)
idname = os.path.splitext(filename)[0]

def update_fn(_self, _context):
    load()


industry_compatible = bpy.utils.execfile(os.path.join(dirname, "keymap_data", "industry_compatible_data.py"))


def load():
    from sys import platform
    from bl_keymap_utils.io import keyconfig_init_from_data

    prefs = bpy.context.preferences

    kc = bpy.context.window_manager.keyconfigs.new(idname)
    params = industry_compatible.Params(use_mouse_emulate_3_button=prefs.inputs.use_mouse_emulate_3_button)
    keyconfig_data = industry_compatible.generate_keymaps(params)

    if platform == 'darwin':
        from bl_keymap_utils.platform_helpers import keyconfig_data_oskey_from_ctrl_for_macos
        keyconfig_data = keyconfig_data_oskey_from_ctrl_for_macos(keyconfig_data)

    keyconfig_init_from_data(kc, keyconfig_data)

if __name__ == "__main__":
    # XXX, no way to unregister
    for cls in classes:
        bpy.utils.register_class(cls)

    load()
