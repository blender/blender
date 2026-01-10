"""
Explicitly Assigning Action Slots
+++++++++++++++++++++++++++++++++
An action slot is compatible with a data-block if the slot's ``target_id_type`` matches the data-block's type.
If there are multiple slots on the Action, and you want to just pick the first one that's
compatible, use the following code. ``anim_data.action_suitable_slots`` can be used `after` the
Action has been assigned; it is a list of action slots of that Action, but only the ones that
are actually compatible with the owner of anim_data (in this case, Suzanne).

"""
# If there are multiple slots on the Action, pick the first one that's compatible
anim_data = suzanne.animation_data_create()
anim_data.action = action
assert anim_data.action_suitable_slots, "expecting at least one suitable slot"
anim_data.action_slot = anim_data.action_suitable_slots[0]
