# This stub runs a python script relative to the currently open
# blend file, useful when editing scripts externally.

import bpy
import os

# Use your own script name here:
filename = "my_script.py"

filepath = os.path.join(os.path.dirname(bpy.data.filepath), filename)
global_namespace = {"__file__": filepath, "__name__": "__main__"}
with open(filepath, 'rb') as file:
    exec(compile(file.read(), filepath, 'exec'), global_namespace)

