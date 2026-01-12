"""
Manually Create an Action Slot
++++++++++++++++++++++++++++++
If required you can also manually create Action Slots on an Action. Note the ``target_id_type``
that matches the data-block type. Identifiers start with a prefix based on the ID type,
e.g. "OB" for objects, followed by the name. There can be identifiers like ``OBSuzanne``
and ``MESuzanne`` and the name (``Suzanne``) can be shared between them. This is intentional,
so that the slots and the datablocks can have the same name.

"""
# Actions creation.
action = bpy.data.actions.new("SuzanneAction")

# Creation of slots requires an ID type and a name.
slot = action.slots.new(id_type='OBJECT', name="Suzanne")
print(f"slot type={slot.target_id_type!r} "
      f"name={slot.name_display!r} "
      f"identifier={slot.identifier!r}")

# Output:
#   slot type=OBJECT name=Suzanne identifier=OBSuzanne
