"""
**Logging Context Member Access**

Context members can be logged by calling ``logging_set(True)`` on the "with" target of a temporary override.
This will log the members that are being accessed during the operation and may
assist in debugging when it is unclear which members need to be overridden.

In the event an operator fails to execute because of a missing context member, logging may help
identify which member is required.

This example shows how to log which context members are being accessed.
Log statements are printed to your system's console.

.. important::

   Not all operators rely on Context Members and therefore will not be affected by
   :class:`bpy.types.Context.temp_override`, use logging to what members if any are accessed.
"""

import bpy
from bpy import context

my_objects = [context.scene.camera]

with context.temp_override(selected_objects=my_objects) as override:
    override.logging_set(True)  # Enable logging.
    bpy.ops.object.delete()
