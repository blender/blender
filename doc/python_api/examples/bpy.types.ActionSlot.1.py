"""
Action Slots organize animation data within an action. Each action has slots with specific animation
data. An animated data-block specifies an action and a slot, determining the animation data it uses.
See the `Blender Manual <https://docs.blender.org/manual/en/5.1/animation/actions.html#action-slots>`_
for how Action Slots are used, or the `technical documentation <https://developer.blender.org/docs/features/animation/>`_
for details on the animation system's architecture.

Create & Access an Action Slot
++++++++++++++++++++++++++++++
To get started with Action Slots, you can easily create them by inserting a keyframe on an object. When you do this,
Blender automatically creates an Action & Slot for that data-block.

"""
import bpy

# Assume Suzanne mesh is present in the scene.
suzanne = bpy.data.objects["Suzanne"]

# Create animation data and an action for Suzanne:
# Slot will be automatically created.
suzanne.keyframe_insert("location", index=0)

# Action slots can be accessed like this:
action = suzanne.animation_data.action
for slot in action.slots:
    print(f"Slot Identifier {slot.identifier!r} "
          f"with name {slot.name_display!r} "
          f"targets ID type {slot.target_id_type!r}")
