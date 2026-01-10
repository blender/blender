"""
Finding Action Slot Users
+++++++++++++++++++++++++
To return a list of the data-blocks that are animated by a specific slot of an Action, use the ``users()`` method of the ActionSlot.

"""
# Iterate through all actions in the Blender data.
print("Action & slot users:")
for action in bpy.data.actions:
    for slot in action.slots:
        # Return the data-blocks that are animated by this slot of this action
        users = slot.users()
        print(f"{action.name:20} slot={slot.identifier:12s} users: {users}")
