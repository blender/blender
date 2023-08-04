"""
Base class for integrating USD Hydra based renderers.

USD Hydra Based Renderer
++++++++++++++++++++++++
"""

import bpy


class CustomHydraRenderEngine(bpy.types.HydraRenderEngine):
    # Identifier and name in the user interface.
    bl_idname = "CUSTOM_HYDRA_RENDERER"
    bl_label = "Custom Hydra Renderer"

    # Name of the render plugin.
    bl_delegate_id = "HdCustomRendererPlugin"

    # Register path to plugin.
    @classmethod
    def register(cls):
        super().register()

        import pxr
        pxr.Plug.Registry().RegisterPlugins(['/path/to/plugin'])

    # Render settings that will be passed to the delegate.
    def get_render_settings(self, engine_type):
        return {
            'myBoolean': True,
            'myValue': 8,
            'aovToken:Depth': "depth",
        }

    # RenderEngine methods for update, render and draw are implemented in
    # HydraRenderEngine. Optionally extra work can be done before or after
    # by implementing the methods like this.
    def update(self, data, depsgraph):
        super().update(data, depsgraph)
        # Do extra work here

    def update_render_passes(self, scene, render_layer):
        if render_layer.use_pass_z:
            self.register_pass(scene, render_layer, 'Depth', 1, 'Z', 'VALUE')


# Registration
def register():
    bpy.utils.register_class(CustomHydraRenderEngine)


def unregister():
    bpy.utils.unregister_class(CustomHydraRenderEngine)


if __name__ == "__main__":
    register()
