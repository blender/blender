# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy.types import Operator
from bpy.props import (
    IntProperty,
    StringProperty,
)


class TEXT_OT_jump_to_file_at_point(Operator):
    bl_idname = "text.jump_to_file_at_point"
    bl_label = "Open Text File at point"
    bl_description = "Edit text file in external text editor"

    filepath: StringProperty(name="filepath")
    line: IntProperty(name="line")
    column: IntProperty(name="column")

    def execute(self, context):
        import shlex
        import subprocess
        from string import Template

        if not self.properties.is_property_set("filepath"):
            text = context.space_data.text
            if not text:
                return {'CANCELLED'}
            self.filepath = text.filepath
            self.line = text.current_line_index
            self.column = text.current_character

        text_editor = context.preferences.filepaths.text_editor
        text_editor_args = context.preferences.filepaths.text_editor_args

        # Use the internal text editor.
        if not text_editor:
            return bpy.ops.text.jump_to_file_at_point_internal(
                filepath=self.filepath,
                line=self.line,
                column=self.column,
            )

        if not text_editor_args:
            self.report(
                {'ERROR_INVALID_INPUT'},
                "Provide text editor argument format in File Paths/Applications Preferences, "
                "see input field tool-tip for more information.",
            )
            return {'CANCELLED'}

        if "$filepath" not in text_editor_args:
            self.report({'ERROR_INVALID_INPUT'}, "Text Editor Args Format must contain $filepath")
            return {'CANCELLED'}

        args = [text_editor]
        template_vars = {
            "filepath": self.filepath,
            "line": self.line + 1,
            "column": self.column + 1,
            "line0": self.line,
            "column0": self.column,
        }

        try:
            args.extend([Template(arg).substitute(**template_vars) for arg in shlex.split(text_editor_args)])
        except Exception as ex:
            self.report({'ERROR'}, "Exception parsing template: %r" % ex)
            return {'CANCELLED'}

        try:
            # With `check=True` if `process.returncode != 0` an exception will be raised.
            subprocess.run(args, check=True)
        except Exception as ex:
            self.report({'ERROR'}, "Exception running external editor: %r" % ex)
            return {'CANCELLED'}

        return {'FINISHED'}


classes = (
    TEXT_OT_jump_to_file_at_point,
)
