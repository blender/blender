"""
Basic FileHandler for Operator that imports multiple files
----------------------------------------------------------

Also operators can be invoked with multiple files from 'drag-and-drop',
but for this it is require to define the following properties:

.. code-block:: python

    directory: StringProperty(subtype='FILE_PATH')
    files: CollectionProperty(type=bpy.types.OperatorFileListElement)

This ``directory`` and ``files`` properties now will be used by the
``FileHandler`` to set 'drag-and-drop' filepath data.

"""

import bpy
from mathutils import Vector


class ShaderScriptImport(bpy.types.Operator):
    """Test importer that creates scripts nodes from .txt files"""
    bl_idname = "shader.script_import"
    bl_label = "Import a text file as a script node"

    """
    This Operator can import multiple .txt files, we need following directory and files
    properties that the file handler will use to set files path data
    """
    directory: bpy.props.StringProperty(subtype='FILE_PATH', options={'SKIP_SAVE'})
    files: bpy.props.CollectionProperty(type=bpy.types.OperatorFileListElement, options={'SKIP_SAVE'})

    @classmethod
    def poll(cls, context):
        return (context.region and context.region.type == 'WINDOW'
                and context.area and context.area.ui_type == 'ShaderNodeTree'
                and context.object and context.object.type == 'MESH'
                and context.material)

    def execute(self, context):
        """ The directory property need to be set. """
        if not self.directory:
            return {'CANCELLED'}
        x = 0.0
        y = 0.0
        for file in self.files:
            """
            Calls to the operator can set unfiltered file names,
            ensure the file extension is .txt
            """
            if file.name.endswith(".txt"):
                node_tree = context.material.node_tree
                text_node = node_tree.nodes.new(type="ShaderNodeScript")
                text_node.mode = 'EXTERNAL'
                import os
                filepath = os.path.join(self.directory, file.name)
                text_node.filepath = filepath
                text_node.location = Vector((x, y))
                x += 20.0
                y -= 20.0
        return {'FINISHED'}

    """
    By default the file handler invokes the operator with the directory and files properties set.
    In this example if this properties are set the operator is executed, if not the
    file select window is invoked.
    This depends on setting ``options={'SKIP_SAVE'}`` to the properties options to avoid
    to reuse filepath data between operator calls.
    """

    def invoke(self, context, event):
        if self.directory:
            return self.execute(context)
        context.window_manager.fileselect_add(self)
        return {'RUNNING_MODAL'}


class SHADER_FH_script_import(bpy.types.FileHandler):
    bl_idname = "SHADER_FH_script_import"
    bl_label = "File handler for shader script node import"
    bl_import_operator = "shader.script_import"
    bl_file_extensions = ".txt"

    @classmethod
    def poll_drop(cls, context):
        return (context.region and context.region.type == 'WINDOW'
                and context.area and context.area.ui_type == 'ShaderNodeTree')


bpy.utils.register_class(ShaderScriptImport)
bpy.utils.register_class(SHADER_FH_script_import)
