import bpy
import platform

filepaths = bpy.context.preferences.filepaths

filepaths.text_editor_args = "-g $filepath:$line:$column"

match platform.system():
    case "Windows":
        filepaths.text_editor = "code.cmd"
    case _:
        filepaths.text_editor = "code"
