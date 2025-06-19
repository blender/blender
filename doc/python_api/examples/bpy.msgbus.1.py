"""
The message bus system can be used to receive notifications when properties of
Blender datablocks are changed via the data API.


Limitations
-----------

The message bus system is triggered by updates via the RNA system. This means
that the following updates will result in a notification on the message bus:

- Changes via the Python API, for example ``some_object.location.x += 3``.
- Changes via the sliders, fields, and buttons in the user interface.

The following updates do **not** trigger message bus notifications:

- Moving objects in the 3D Viewport.
- Changes performed by the animation system.

Changes done from ``msgbus`` callbacks are not included in related undo steps,
so users can easily skip their effects by using Undo followed by Redo.

Unlike properties ``update`` callbacks, message bus update callbacks are postponed
until all operators have finished executing.
Additionally, for each property the callback is only triggered once per update cycle,
even if the property was changed multiple times during that period.

Example Use
-----------

Below is an example of subscription to changes in the active object's location.
"""

import bpy

# Any Python object can act as the subscription's owner.
owner = object()

subscribe_to = bpy.context.object.location


def msgbus_callback(*args):
    # This will print:
    # Something changed! (1, 2, 3)
    print("Something changed!", args)


bpy.msgbus.subscribe_rna(
    key=subscribe_to,
    owner=owner,
    args=(1, 2, 3),
    notify=msgbus_callback,
)
