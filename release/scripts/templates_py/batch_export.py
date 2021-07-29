# exports each selected object into its own file

import bpy
import os

# export to blend file location
basedir = os.path.dirname(bpy.data.filepath)

if not basedir:
    raise Exception("Blend file is not saved")

scene = bpy.context.scene

obj_active = scene.objects.active
selection = bpy.context.selected_objects

bpy.ops.object.select_all(action='DESELECT')

for obj in selection:

    obj.select = True

    # some exporters only use the active object
    scene.objects.active = obj

    name = bpy.path.clean_name(obj.name)
    fn = os.path.join(basedir, name)

    bpy.ops.export_scene.fbx(filepath=fn + ".fbx", use_selection=True)

    ## Can be used for multiple formats
    # bpy.ops.export_scene.x3d(filepath=fn + ".x3d", use_selection=True)

    obj.select = False

    print("written:", fn)


scene.objects.active = obj_active

for obj in selection:
    obj.select = True
