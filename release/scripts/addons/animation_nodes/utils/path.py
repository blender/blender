import bpy
import os

def getAbsolutePathOfSound(sound):
    return toAbsolutePath(sound.filepath, library = sound.library)

def toAbsolutePath(path, start = None, library = None):
    absPath = bpy.path.abspath(path, start, library)
    return os.path.normpath(absPath)

def toIDPropertyPath(name):
    return '["' + name + '"]'
