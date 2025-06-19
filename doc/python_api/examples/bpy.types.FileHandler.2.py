"""
FileHandler for Importing multiple files and exposing Operator options
----------------------------------------------------------------------

Operators which support being executed with multiple files from 'drag-and-drop' require the
following properties be defined:

.. code-block:: python

    directory: StringProperty(subtype='DIR_PATH', options={'SKIP_SAVE', 'HIDDEN'})
    files: CollectionProperty(type=OperatorFileListElement, options={'SKIP_SAVE', 'HIDDEN'})

These ``directory`` and ``files`` properties will be set with the necessary data from the
`drag-and-drop` operation.

Additionally, if the operator provides operator properties that need to be accessible to the user,
the :class:`ImportHelper.invoke_popup` method can be used to show a dialog leveraging the standard
:class:`Operator.draw` method for layout and display.

"""

import bpy
from bpy_extras.io_utils import ImportHelper
from mathutils import Vector


class ShaderScriptImport(bpy.types.Operator, ImportHelper):
    """
    Creates one or more Shader Script nodes from text files.
    """
    bl_idname = "shader.script_import"
    bl_label = "Import a text file as a script node"

    # This Operator supports processing multiple `.txt` files at a time. The following properties
    # must be defined.
    directory: bpy.props.StringProperty(subtype='DIR_PATH', options={'SKIP_SAVE', 'HIDDEN'})
    files: bpy.props.CollectionProperty(type=bpy.types.OperatorFileListElement, options={'SKIP_SAVE', 'HIDDEN'})

    # Allow the user to choose whether the node's label is set or not
    set_label: bpy.props.BoolProperty(name="Set Label", default=False)

    @classmethod
    def poll(cls, context):
        return (
            context.region and context.region.type == 'WINDOW' and
            context.area and context.area.ui_type == 'ShaderNodeTree' and
            context.object and context.object.type == 'MESH' and
            context.material
        )

    def execute(self, context):
        # The directory property must be set.
        if not self.directory:
            return {'CANCELLED'}

        x = 0.0
        y = 0.0
        for file in self.files:
            # Direct calls to this Operator may use unsupported file-paths. Ensure the incoming
            # files are ones that are supported.
            if file.name.endswith(".txt"):
                import os
                filepath = os.path.join(self.directory, file.name)

                node_tree = context.material.node_tree
                text_node = node_tree.nodes.new(type="ShaderNodeScript")
                text_node.mode = 'EXTERNAL'
                text_node.filepath = filepath
                text_node.location = Vector((x, y))

                # Set the node's title to the file name.
                if self.set_label:
                    text_node.label = file.name

                x += 20.0
                y -= 20.0

        return {'FINISHED'}

    # Use ImportHelper's invoke_popup() to handle the invocation so that this operator's properties
    # are shown in a popup. This allows the user to configure additional settings on the operator
    # like the `set_label` property. Consider having a draw() method on the operator in order to
    # layout the properties in the UI appropriately.
    #
    # If filepath information is not provided the file select window will be invoked instead.

    def invoke(self, context, event):
        return self.invoke_popup(context)


# Define a file handler that supports the following set of conditions:
#  - Execute the `shader.script_import` operator
#  - When `.txt` files are dropped in the Shader Editor
class SHADER_FH_script_import(bpy.types.FileHandler):
    bl_idname = "SHADER_FH_script_import"
    bl_label = "File handler for shader script node import"
    bl_import_operator = "shader.script_import"
    bl_file_extensions = ".txt"

    @classmethod
    def poll_drop(cls, context):
        return (
            context.region and context.region.type == 'WINDOW' and
            context.area and context.area.ui_type == 'ShaderNodeTree'
        )


bpy.utils.register_class(ShaderScriptImport)
bpy.utils.register_class(SHADER_FH_script_import)
