"""
USD Hook Example
++++++++++++++++

This example shows an implementation of ``USDHook`` to extend USD
export and import functionalty.

One may optionally define any or all of the following callback functions
in the ``USDHook`` subclass.

Hook function ``on_export()`` is called before the USD export finalizes,
allowing modifications to the USD stage immediately before it is
saved.  This function takes as an argument an instance of an
internally defined class ``USDSceneExportContext`` which provides the
following accessors to the scene data:

- ``get_stage()`` returns the USD stage to be saved.
- ``get_depsgraph()`` returns the Blender scene dependency graph.

Hook function ``on_material_export()`` is called for each material that is exported,
allowing modifications to the USD material, such as shader generation.
It is called with three arguments:

-``export_context``: An instance of the internally defined type ``USDMaterialExportContext``.
-``bl_material``: The source Blender material.
-``usd_material``: The target USD material to be exported.

``USDMaterialExportContext`` implements a ``get_stage()`` function which returns the
USD stage to be saved.

Note that the target USD material might already have connected shaders created by the USD exporter or
by other material export hooks.

Hook function ``on_import()`` is called after the USD import finalizes. This function takes
as an argument an instance of an internally defined class ``USDSceneImportContext`` which provides the
following accessors to the scene data:

- ``get_stage()`` returns the USD stage which was imported.

The hook functions should return ``True`` on success or ``False`` if the operation was bypassed or
otherwise failed to complete.  Exceptions raised by these functions will be reported in Blender, with
the exception details printed to the console.

The ``USDHookExample`` class in this example impements the fllowing functions:

- ``on_export()`` function to add custom data to the stage's root layer.
- ``on_material_export()`` function to create a simple ``MaterialX`` shader on the givne USD material.
- ``on_import()`` function to create a text object to display the stage's custom layer data.

"""

import bpy
import bpy.types
import pxr.Gf as Gf
import pxr.Sdf as Sdf
import pxr.Usd as Usd
import pxr.UsdShade as UsdShade
import textwrap


class USDHookExample(bpy.types.USDHook):
    """Example implementation of USD IO hooks"""
    bl_idname = "usd_hook_example"
    bl_label = "Example"

    @staticmethod
    def on_export(export_context):
        """ Include the Blender filepath in the root layer custom data.
        """

        stage = export_context.get_stage()

        if stage is None:
            return False
        data = bpy.data
        if data is None:
            return False

        # Set the custom data.
        rootLayer = stage.GetRootLayer()
        customData = rootLayer.customLayerData
        customData["blenderFilepath"] = data.filepath
        rootLayer.customLayerData = customData

        return True

    @staticmethod
    def on_material_export(export_context, bl_material, usd_material):
        """ Create a simple MaterialX shader on the exported material.
        """

        stage = export_context.get_stage()

        # Create a MaterialX standard surface shader
        mtl_path = usd_material.GetPrim().GetPath()
        shader = UsdShade.Shader.Define(stage, mtl_path.AppendPath("mtlxstandard_surface"))
        shader.CreateIdAttr("ND_standard_surface_surfaceshader")

        # Connect the shader.  MaterialX materials use "mtlx" renderContext
        usd_material.CreateSurfaceOutput("mtlx").ConnectToSource(shader.ConnectableAPI(), "out")

        # Set the color to the Blender material's viewport display color.
        col = bl_material.diffuse_color
        shader.CreateInput("base_color", Sdf.ValueTypeNames.Color3f).Set(Gf.Vec3f(col[0], col[1], col[2]))

        return True

    @staticmethod
    def on_import(import_context):
        """ Create a text object to display the stage's custom data.
        """
        stage = import_context.get_stage()

        if stage is None:
            return False

        # Get the custom data.
        rootLayer = stage.GetRootLayer()
        customData = rootLayer.customLayerData

        # Create a text object to display the stage path
        # and custom data dictionary entries.

        bpy.ops.object.text_add()
        ob = bpy.context.view_layer.objects.active

        if (ob is None) or (ob.data is None):
            return False

        ob.name = "layer_data"
        ob.data.name = "layer_data"

        # The stage root path is the first line.
        text = rootLayer.realPath

        # Append key/value strings, enforcing text wrapping.
        for item in customData.items():
            print(item)
            text += '\n'
            line = str(item[0]) + ': ' + str(item[1])
            text += textwrap.fill(line, width=80)

        ob.data.body = text

        return True


def register():
    bpy.utils.register_class(USDHookExample)


def unregister():
    bpy.utils.unregister_class(USDHookExample)


if __name__ == "__main__":
    register()
