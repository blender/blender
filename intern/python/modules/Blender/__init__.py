# 
# The Blender main module wrapper
# (c) 06/2001, NaN // strubi@blender.nl

__all__ = ["Object", "Image", "NMesh", "Window", "Mesh", "sys",
           "Lamp", "Scene", "Draw", "Camera", "Material", "Types", "Ipo",
           "BGL"]

import _Blender

Get = _Blender.Get
Redraw = _Blender.Redraw
link = _Blender.link
bylink = _Blender.bylink

import Object, Image, Mesh, Window, sys, Lamp, Scene, Draw, Camera
import Material, NMesh, BGL, Types, Ipo, Text

deg = lambda x: 0.0174532925199 * x  # conversion from degrees to radians

import __builtin__
__builtin__.deg = deg

