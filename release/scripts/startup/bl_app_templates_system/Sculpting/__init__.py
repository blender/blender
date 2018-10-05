
import bpy
from bpy.app.handlers import persistent


@persistent
def load_handler(dummy):
    import bpy
    if bpy.data.filepath == "":
        # Apply subdivision modifier on startup
        bpy.ops.object.mode_set(mode='OBJECT')
        bpy.ops.object.modifier_apply(modifier="Subdivision")
        bpy.ops.object.mode_set(mode='EDIT')
        bpy.ops.transform.tosphere(value=1.0)
        bpy.ops.object.mode_set(mode='SCULPT')

def register():
    import bpy
    bpy.app.handlers.load_post.append(load_handler)

def unregister():
    import bpy
    bpy.app.handlers.load_post.remove(load_handler)
