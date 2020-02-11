"""
Note on Altering Data
+++++++++++++++++++++

Altering data from handlers should be done carefully. While rendering the
``frame_change_pre`` and ``frame_change_post`` handlers are called from one
thread and the viewport updates from a different thread. If the handler changes
data that is accessed by the viewport, this can cause a crash of Blender. In
such cases, lock the interface (Render → Lock Interface or
:data:`bpy.types.RenderSettings.use_lock_interface`) before starting a render.

Below is an example of a mesh that is altered from a handler:
"""

def frame_change_pre(scene):
    # A triangle that shifts in the z direction
    zshift = scene.frame_current * 0.1
    vertices = [(-1, -1, zshift), (1, -1, zshift), (0, 1, zshift)]
    triangles = [(0, 1, 2)]

    object = bpy.data.objects["The Object"]
    object.data.clear_geometry()
    object.data.from_pydata(vertices, [], triangles)
