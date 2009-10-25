import bpy
import os

def expandpath(path):
	if path.startswith("//"):
		return os.path.join(os.path.dirname(bpy.data.filename), path[2:])
	
	return path

import types
bpy.sys = types.ModuleType("bpy.sys")
bpy.sys.expandpath = expandpath
