/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup animrig
 */

#include "DNA_action_defaults.h"
#include "DNA_action_types.h"
#include "DNA_anim_types.h"
#include "DNA_array_utils.hh"
#include "DNA_defaults.h"

#include "BLI_listbase.h"
#include "BLI_listbase_wrapper.hh"
#include "BLI_math_base.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_string_utils.hh"

#include "BKE_action.h"
#include "BKE_action.hh"
#include "BKE_anim_data.hh"
#include "BKE_fcurve.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_preview_image.hh"

#include "RNA_access.hh"
#include "RNA_path.hh"
#include "RNA_prototypes.hh"

#include "ED_keyframing.hh"

#include "MEM_guardedalloc.h"

#include "BLT_translation.hh"

#include "DEG_depsgraph_build.hh"

#include "ANIM_action.hh"
#include "ANIM_animdata.hh"
#include "ANIM_fcurve.hh"
#include "action_runtime.hh"

#include "atomic_ops.h"

#include <cstdio>
#include <cstring>

namespace blender::animrig {

namespace {
/**
 * Default name for action slots. The first two characters in the name indicate the ID type
 * of whatever is animated by it.
 *
 * Since the ID type may not be determined when the slot is created, the prefix starts out at
 * XX. Note that no code should use this XX value; use Slot::has_idtype() instead.
 */
constexpr const char *slot_default_name = "Slot";
constexpr const char *slot_unbound_prefix = "XX";

constexpr const char *layer_default_name = "Layer";

}  // namespace

static animrig::Layer &ActionLayer_alloc()
{
  ActionLayer *layer = DNA_struct_default_alloc(ActionLayer);
  return layer->wrap();
}
static animrig::Strip &ActionStrip_alloc_infinite(const Strip::Type type)
{
  ActionStrip *strip = nullptr;
  switch (type) {
    case Strip::Type::Keyframe: {
      KeyframeActionStrip *key_strip = MEM_new<KeyframeActionStrip>(__func__);
      strip = &key_strip->strip;
      break;
    }
  }

  BLI_assert_msg(strip, "unsupported strip type");

  /* Copy the default ActionStrip fields into the allocated data-block. */
  memcpy(strip, DNA_struct_default_get(ActionStrip), sizeof(*strip));
  return strip->wrap();
}

/* Copied from source/blender/blenkernel/intern/grease_pencil.cc.
 * Keep an eye on DNA_array_utils.hh; we may want to move these functions in there. */
template<typename T> static void grow_array(T **array, int *num, const int add_num)
{
  BLI_assert(add_num > 0);
  const int new_array_num = *num + add_num;
  T *new_array = reinterpret_cast<T *>(
      MEM_cnew_array<T *>(new_array_num, "animrig::action/grow_array"));

  blender::uninitialized_relocate_n(*array, *num, new_array);
  MEM_SAFE_FREE(*array);

  *array = new_array;
  *num = new_array_num;
}

template<typename T> static void grow_array_and_append(T **array, int *num, T item)
{
  grow_array(array, num, 1);
  (*array)[*num - 1] = item;
}

template<typename T> static void shrink_array(T **array, int *num, const int shrink_num)
{
  BLI_assert(shrink_num > 0);
  const int new_array_num = *num - shrink_num;
  T *new_array = reinterpret_cast<T *>(MEM_cnew_array<T *>(new_array_num, __func__));

  blender::uninitialized_move_n(*array, new_array_num, new_array);
  MEM_freeN(*array);

  *array = new_array;
  *num = new_array_num;
}

/* ----- Action implementation ----------- */

bool Action::is_empty() const
{
  /* The check for emptiness has to include the check for an empty `groups` ListBase because of the
   * animation filtering code. With the functions `rearrange_action_channels` and
   * `join_groups_action_temp` the ownership of FCurves is temporarily transferred to the `groups`
   * ListBase leaving `curves` potentially empty. */
  return this->layer_array_num == 0 && this->slot_array_num == 0 &&
         BLI_listbase_is_empty(&this->curves) && BLI_listbase_is_empty(&this->groups);
}
bool Action::is_action_legacy() const
{
  /* This is a valid legacy Action only if there is no layered info. */
  return this->layer_array_num == 0 && this->slot_array_num == 0;
}
bool Action::is_action_layered() const
{
  /* This is a valid layered Action if there is ANY layered info (because that
   * takes precedence) or when there is no legacy info. */
  return this->layer_array_num > 0 || this->slot_array_num > 0 ||
         (BLI_listbase_is_empty(&this->curves) && BLI_listbase_is_empty(&this->groups));
}

blender::Span<const Layer *> Action::layers() const
{
  return blender::Span<Layer *>{reinterpret_cast<Layer **>(this->layer_array),
                                this->layer_array_num};
}
blender::MutableSpan<Layer *> Action::layers()
{
  return blender::MutableSpan<Layer *>{reinterpret_cast<Layer **>(this->layer_array),
                                       this->layer_array_num};
}
const Layer *Action::layer(const int64_t index) const
{
  return &this->layer_array[index]->wrap();
}
Layer *Action::layer(const int64_t index)
{
  return &this->layer_array[index]->wrap();
}

Layer &Action::layer_add(const StringRefNull name)
{
  Layer &new_layer = ActionLayer_alloc();
  STRNCPY_UTF8(new_layer.name, name.c_str());

  grow_array_and_append<::ActionLayer *>(&this->layer_array, &this->layer_array_num, &new_layer);
  this->layer_active_index = this->layer_array_num - 1;

  /* If this is the first layer in this Action, it means that it could have been
   * used as a legacy Action before. As a result, this->idroot may be non-zero
   * while it should be zero for layered Actions.
   *
   * And since setting this to 0 when it is already supposed to be 0 is fine,
   * there is no check for whether this is actually the first layer. */
  this->idroot = 0;

  return new_layer;
}

static void layer_ptr_destructor(ActionLayer **dna_layer_ptr)
{
  Layer &layer = (*dna_layer_ptr)->wrap();
  MEM_delete(&layer);
};

bool Action::layer_remove(Layer &layer_to_remove)
{
  const int64_t layer_index = this->find_layer_index(layer_to_remove);
  if (layer_index < 0) {
    return false;
  }

  dna::array::remove_index(&this->layer_array,
                           &this->layer_array_num,
                           &this->layer_active_index,
                           layer_index,
                           layer_ptr_destructor);
  return true;
}

void Action::layer_ensure_at_least_one()
{
  if (!this->layers().is_empty()) {
    return;
  }

  Layer &layer = this->layer_add(DATA_(layer_default_name));
  layer.strip_add(Strip::Type::Keyframe);
}

int64_t Action::find_layer_index(const Layer &layer) const
{
  for (const int64_t layer_index : this->layers().index_range()) {
    const Layer *visit_layer = this->layer(layer_index);
    if (visit_layer == &layer) {
      return layer_index;
    }
  }
  return -1;
}

blender::Span<const Slot *> Action::slots() const
{
  return blender::Span<Slot *>{reinterpret_cast<Slot **>(this->slot_array), this->slot_array_num};
}
blender::MutableSpan<Slot *> Action::slots()
{
  return blender::MutableSpan<Slot *>{reinterpret_cast<Slot **>(this->slot_array),
                                      this->slot_array_num};
}
const Slot *Action::slot(const int64_t index) const
{
  return &this->slot_array[index]->wrap();
}
Slot *Action::slot(const int64_t index)
{
  return &this->slot_array[index]->wrap();
}

Slot *Action::slot_for_handle(const slot_handle_t handle)
{
  const Slot *slot = const_cast<const Action *>(this)->slot_for_handle(handle);
  return const_cast<Slot *>(slot);
}

const Slot *Action::slot_for_handle(const slot_handle_t handle) const
{
  if (handle == Slot::unassigned) {
    return nullptr;
  }

  /* TODO: implement hash-map lookup. */
  for (const Slot *slot : slots()) {
    if (slot->handle == handle) {
      return slot;
    }
  }
  return nullptr;
}

static void slot_name_ensure_unique(Action &action, Slot &slot)
{
  /* Cannot capture parameters by reference in the lambda, as that would change its signature
   * and no longer be compatible with BLI_uniquename_cb(). That's why this struct is necessary. */
  struct DupNameCheckData {
    Action &action;
    Slot &slot;
  };
  DupNameCheckData check_data = {action, slot};

  auto check_name_is_used = [](void *arg, const char *name) -> bool {
    DupNameCheckData *data = static_cast<DupNameCheckData *>(arg);
    for (const Slot *slot : data->action.slots()) {
      if (slot == &data->slot) {
        /* Don't compare against the slot that's being renamed. */
        continue;
      }
      if (STREQ(slot->name, name)) {
        return true;
      }
    }
    return false;
  };

  BLI_uniquename_cb(check_name_is_used, &check_data, "", '.', slot.name, sizeof(slot.name));
}

/* TODO: maybe this function should only set the 'name without prefix' aka the 'display name'. That
 * way only `this->id_type` is responsible for the prefix. I (Sybren) think that's easier to
 * determine when the code is a bit more mature, and we can see what the majority of the calls to
 * this function actually do/need. */
void Action::slot_name_set(Main &bmain, Slot &slot, const StringRefNull new_name)
{
  this->slot_name_define(slot, new_name);
  this->slot_name_propagate(bmain, slot);
}

void Action::slot_name_define(Slot &slot, const StringRefNull new_name)
{
  BLI_assert_msg(StringRef(new_name).size() >= Slot::name_length_min,
                 "Action Slots must be large enough for a 2-letter ID code + the display name");
  STRNCPY_UTF8(slot.name, new_name.c_str());
  slot_name_ensure_unique(*this, slot);
}

void Action::slot_name_propagate(Main &bmain, const Slot &slot)
{
  /* Just loop over all animatable IDs in the main database. */
  ListBase *lb;
  ID *id;
  FOREACH_MAIN_LISTBASE_BEGIN (&bmain, lb) {
    FOREACH_MAIN_LISTBASE_ID_BEGIN (lb, id) {
      if (!id_can_have_animdata(id)) {
        /* This ID type cannot have any animation, so ignore all and continue to
         * the next ID type. */
        break;
      }

      AnimData *adt = BKE_animdata_from_id(id);
      if (!adt || adt->action != this) {
        /* Not animated by this Action. */
        continue;
      }
      if (adt->slot_handle != slot.handle) {
        /* Not animated by this Slot. */
        continue;
      }

      /* Ensure the Slot name on the AnimData is correct. */
      STRNCPY_UTF8(adt->slot_name, slot.name);
    }
    FOREACH_MAIN_LISTBASE_ID_END;
  }
  FOREACH_MAIN_LISTBASE_END;
}

Slot *Action::slot_find_by_name(const StringRefNull slot_name)
{
  for (Slot *slot : slots()) {
    if (STREQ(slot->name, slot_name.c_str())) {
      return slot;
    }
  }
  return nullptr;
}

Slot &Action::slot_allocate()
{
  Slot &slot = *MEM_new<Slot>(__func__);
  this->last_slot_handle++;
  BLI_assert_msg(this->last_slot_handle > 0, "Action Slot handle overflow");
  slot.handle = this->last_slot_handle;

  /* Set the default flags. These cannot be set via the 'DNA defaults' system,
   * as that would require knowing which bit corresponds with which flag. That's
   * only known to the C++ wrapper code. */
  slot.set_expanded(true);
  return slot;
}

Slot &Action::slot_add()
{
  Slot &slot = this->slot_allocate();

  /* Assign the default name and the 'unbound' name prefix. */
  STRNCPY_UTF8(slot.name, slot_unbound_prefix);
  BLI_strncpy_utf8(slot.name + 2, DATA_(slot_default_name), ARRAY_SIZE(slot.name) - 2);

  /* Append the Slot to the Action. */
  grow_array_and_append<::ActionSlot *>(&this->slot_array, &this->slot_array_num, &slot);

  slot_name_ensure_unique(*this, slot);

  /* If this is the first slot in this Action, it means that it could have
   * been used as a legacy Action before. As a result, this->idroot may be
   * non-zero while it should be zero for layered Actions.
   *
   * And since setting this to 0 when it is already supposed to be 0 is fine,
   * there is no check for whether this is actually the first layer. */
  this->idroot = 0;

  return slot;
}

Slot &Action::slot_add_for_id(const ID &animated_id)
{
  Slot &slot = this->slot_add();

  slot.idtype = GS(animated_id.name);
  this->slot_name_define(slot, animated_id.name);

  /* No need to call anim.slot_name_propagate() as nothing will be using
   * this brand new Slot yet. */

  return slot;
}

Slot &Action::slot_ensure_for_id(const ID &animated_id)
{
  if (Slot *slot = this->find_suitable_slot_for(animated_id)) {
    return *slot;
  }

  return this->slot_add_for_id(animated_id);
}

void Action::slot_active_set(const slot_handle_t slot_handle)
{
  for (Slot *slot : slots()) {
    slot->set_active(slot->handle == slot_handle);
  }
}

Slot *Action::slot_active_get()
{
  for (Slot *slot : slots()) {
    if (slot->is_active()) {
      return slot;
    }
  }
  return nullptr;
}

Slot *Action::find_suitable_slot_for(const ID &animated_id)
{
  AnimData *adt = BKE_animdata_from_id(&animated_id);

  /* The slot handle is only valid when this action has already been
   * assigned. Otherwise it's meaningless. */
  if (adt && adt->action == this) {
    Slot *slot = this->slot_for_handle(adt->slot_handle);
    if (slot && slot->is_suitable_for(animated_id)) {
      return slot;
    }
  }

  /* Try the slot name from the AnimData, if it is set. */
  if (adt && adt->slot_name[0]) {
    Slot *slot = this->slot_find_by_name(adt->slot_name);
    if (slot && slot->is_suitable_for(animated_id)) {
      return slot;
    }
  }

  /* As a last resort, search for the ID name. */
  Slot *slot = this->slot_find_by_name(animated_id.name);
  if (slot && slot->is_suitable_for(animated_id)) {
    return slot;
  }

  return nullptr;
}

bool Action::is_slot_animated(const slot_handle_t slot_handle) const
{
  if (slot_handle == Slot::unassigned) {
    return false;
  }

  Span<const FCurve *> fcurves = fcurves_for_action_slot(*this, slot_handle);
  return !fcurves.is_empty();
}

Layer *Action::get_layer_for_keyframing()
{
  assert_baklava_phase_1_invariants(*this);

  if (this->layers().is_empty()) {
    return nullptr;
  }

  return this->layer(0);
}

bool Action::assign_id(Slot *slot, ID &animated_id)
{
  AnimData *adt = BKE_animdata_ensure_id(&animated_id);
  if (!adt) {
    return false;
  }

  if (adt->action && adt->action != this) {
    /* The caller should unassign the ID from its existing animation first, or
     * use the top-level function `assign_action(anim, ID)`. */
    return false;
  }

  /* Check that the new Slot is suitable, before changing `adt`. */
  if (slot && !slot->is_suitable_for(animated_id)) {
    return false;
  }

  /* Unassign any previously-assigned Slot. */
  Slot *slot_to_unassign = this->slot_for_handle(adt->slot_handle);
  if (slot_to_unassign) {
    slot_to_unassign->users_remove(animated_id);

    /* Before unassigning, make sure that the stored Slot name is up to date. The slot name
     * might have changed in a way that wasn't copied into the ADT yet (for example when the
     * Action is linked from another file), so better copy the name to be sure that it can be
     * transparently reassigned later.
     *
     * TODO: Replace this with a BLI_assert() that the name is as expected, and "simply" ensure
     * this name is always correct. */
    STRNCPY_UTF8(adt->slot_name, slot_to_unassign->name);
  }

  /* Assign the Action itself. */
  if (!adt->action) {
    /* Due to the precondition check above, we know that adt->action is either 'this' (in which
     * case the user count is already correct) or `nullptr` (in which case this is a new
     * reference, and the user count should be increased). */
    id_us_plus(&this->id);
    adt->action = this;
  }

  /* Assign the Slot. */
  if (slot) {
    this->slot_setup_for_id(*slot, animated_id);
    adt->slot_handle = slot->handle;
    slot->users_add(animated_id);

    /* Always make sure the ID's slot name matches the assigned slot. */
    STRNCPY_UTF8(adt->slot_name, slot->name);
  }
  else {
    adt->slot_handle = Slot::unassigned;
  }

  return true;
}

void Action::slot_name_ensure_prefix(Slot &slot)
{
  slot.name_ensure_prefix();
  slot_name_ensure_unique(*this, slot);
}

void Action::slot_setup_for_id(Slot &slot, const ID &animated_id)
{
  if (slot.has_idtype()) {
    BLI_assert(slot.idtype == GS(animated_id.name));
    return;
  }

  slot.idtype = GS(animated_id.name);
  this->slot_name_ensure_prefix(slot);
}

void Action::unassign_id(ID &animated_id)
{
  AnimData *adt = BKE_animdata_from_id(&animated_id);
  BLI_assert_msg(adt, "ID is not animated at all");
  BLI_assert_msg(adt->action == this, "ID is not assigned to this Animation");

  /* Unassign the Slot first. */
  this->assign_id(nullptr, animated_id);

  /* Unassign the Action itself. */
  id_us_min(&this->id);
  adt->action = nullptr;
}

/* ----- ActionLayer implementation ----------- */

Layer::Layer(const Layer &other)
{
  memcpy(this, &other, sizeof(*this));

  /* Strips. */
  this->strip_array = MEM_cnew_array<ActionStrip *>(other.strip_array_num, __func__);
  for (int i : other.strips().index_range()) {
    this->strip_array[i] = other.strip(i)->duplicate(__func__);
  }
}

Layer::~Layer()
{
  for (Strip *strip : this->strips()) {
    MEM_delete(strip);
  }
  MEM_SAFE_FREE(this->strip_array);
  this->strip_array_num = 0;
}

blender::Span<const Strip *> Layer::strips() const
{
  return blender::Span<Strip *>{reinterpret_cast<Strip **>(this->strip_array),
                                this->strip_array_num};
}
blender::MutableSpan<Strip *> Layer::strips()
{
  return blender::MutableSpan<Strip *>{reinterpret_cast<Strip **>(this->strip_array),
                                       this->strip_array_num};
}
const Strip *Layer::strip(const int64_t index) const
{
  return &this->strip_array[index]->wrap();
}
Strip *Layer::strip(const int64_t index)
{
  return &this->strip_array[index]->wrap();
}

Strip &Layer::strip_add(const Strip::Type strip_type)
{
  Strip &strip = ActionStrip_alloc_infinite(strip_type);

  /* Add the new strip to the strip array. */
  grow_array_and_append<::ActionStrip *>(&this->strip_array, &this->strip_array_num, &strip);

  return strip;
}

static void strip_ptr_destructor(ActionStrip **dna_strip_ptr)
{
  Strip &strip = (*dna_strip_ptr)->wrap();
  MEM_delete(&strip);
};

bool Layer::strip_remove(Strip &strip_to_remove)
{
  const int64_t strip_index = this->find_strip_index(strip_to_remove);
  if (strip_index < 0) {
    return false;
  }

  dna::array::remove_index(
      &this->strip_array, &this->strip_array_num, nullptr, strip_index, strip_ptr_destructor);

  return true;
}

int64_t Layer::find_strip_index(const Strip &strip) const
{
  for (const int64_t strip_index : this->strips().index_range()) {
    const Strip *visit_strip = this->strip(strip_index);
    if (visit_strip == &strip) {
      return strip_index;
    }
  }
  return -1;
}

/* ----- ActionSlot implementation ----------- */

Slot::Slot()
{
  memset(this, 0, sizeof(*this));
  this->runtime = MEM_new<SlotRuntime>(__func__);
}

Slot::Slot(const Slot &other)
{
  memset(this, 0, sizeof(*this));
  STRNCPY(this->name, other.name);
  this->idtype = other.idtype;
  this->handle = other.handle;
  this->runtime = MEM_new<SlotRuntime>(__func__);
}

Slot::~Slot()
{
  MEM_delete(this->runtime);
}

void Slot::blend_read_post()
{
  BLI_assert(!this->runtime);
  this->runtime = MEM_new<SlotRuntime>(__func__);
}

bool Slot::is_suitable_for(const ID &animated_id) const
{
  if (!this->has_idtype()) {
    /* Without specific ID type set, this Slot can animate any ID. */
    return true;
  }

  /* Check that the ID type is compatible with this slot. */
  const int animated_idtype = GS(animated_id.name);
  return this->idtype == animated_idtype;
}

bool Slot::has_idtype() const
{
  return this->idtype != 0;
}

Slot::Flags Slot::flags() const
{
  return static_cast<Slot::Flags>(this->slot_flags);
}
bool Slot::is_expanded() const
{
  return this->slot_flags & uint8_t(Flags::Expanded);
}
void Slot::set_expanded(const bool expanded)
{
  if (expanded) {
    this->slot_flags |= uint8_t(Flags::Expanded);
  }
  else {
    this->slot_flags &= ~(uint8_t(Flags::Expanded));
  }
}

bool Slot::is_selected() const
{
  return this->slot_flags & uint8_t(Flags::Selected);
}
void Slot::set_selected(const bool selected)
{
  if (selected) {
    this->slot_flags |= uint8_t(Flags::Selected);
  }
  else {
    this->slot_flags &= ~(uint8_t(Flags::Selected));
  }
}

bool Slot::is_active() const
{
  return this->slot_flags & uint8_t(Flags::Active);
}
void Slot::set_active(const bool active)
{
  if (active) {
    this->slot_flags |= uint8_t(Flags::Active);
  }
  else {
    this->slot_flags &= ~(uint8_t(Flags::Active));
  }
}

Span<ID *> Slot::users(Main &bmain) const
{
  if (bmain.is_action_slot_to_id_map_dirty) {
    internal::rebuild_slot_user_cache(bmain);
  }
  BLI_assert(this->runtime);
  return this->runtime->users.as_span();
}

Vector<ID *> Slot::runtime_users()
{
  BLI_assert_msg(this->runtime, "Slot::runtime should always be allocated");
  return this->runtime->users;
}

void Slot::users_add(ID &animated_id)
{
  BLI_assert(this->runtime);
  this->runtime->users.append_non_duplicates(&animated_id);
}

void Slot::users_remove(ID &animated_id)
{
  BLI_assert(this->runtime);
  Vector<ID *> &users = this->runtime->users;

  const int64_t vector_index = users.first_index_of_try(&animated_id);
  if (vector_index < 0) {
    return;
  }

  users.remove_and_reorder(vector_index);
}

void Slot::users_invalidate(Main &bmain)
{
  bmain.is_action_slot_to_id_map_dirty = true;
}

std::string Slot::name_prefix_for_idtype() const
{
  if (!this->has_idtype()) {
    return slot_unbound_prefix;
  }

  char name[3] = {0};
  *reinterpret_cast<short *>(name) = this->idtype;
  return name;
}

StringRefNull Slot::name_without_prefix() const
{
  BLI_assert(StringRef(this->name).size() >= name_length_min);

  /* Avoid accessing an uninitialized part of the string accidentally. */
  if (this->name[0] == '\0' || this->name[1] == '\0') {
    return "";
  }
  return this->name + 2;
}

void Slot::name_ensure_prefix()
{
  BLI_assert(StringRef(this->name).size() >= name_length_min);

  if (StringRef(this->name).size() < 2) {
    /* The code below would overwrite the trailing 0-byte. */
    this->name[2] = '\0';
  }

  if (!this->has_idtype()) {
    /* A zero idtype is not going to convert to a two-character string, so we
     * need to explicitly assign the default prefix. */
    this->name[0] = slot_unbound_prefix[0];
    this->name[1] = slot_unbound_prefix[1];
    return;
  }

  *reinterpret_cast<short *>(this->name) = this->idtype;
}

/* ----- Functions  ----------- */

bool assign_action(Action &action, ID &animated_id)
{
  unassign_action(animated_id);

  Slot *slot = action.find_suitable_slot_for(animated_id);
  return action.assign_id(slot, animated_id);
}

bool is_action_assignable_to(const bAction *dna_action, const ID_Type id_code)
{
  if (!dna_action) {
    /* Clearing the Action is always possible. */
    return true;
  }

  if (dna_action->idroot == 0) {
    /* This is either a never-assigned legacy action, or a layered action. In
     * any case, it can be assigned to any ID. */
    return true;
  }

  const animrig::Action &action = dna_action->wrap();
  if (!action.is_action_layered()) {
    /* Legacy Actions can only be assigned if their idroot matches. Empty
     * Actions are considered both 'layered' and 'legacy' at the same time,
     * hence this condition checks for 'not layered' rather than 'legacy'. */
    return action.idroot == id_code;
  }

  return true;
}

void unassign_action(ID &animated_id)
{
  Action *action = get_action(animated_id);
  if (!action) {
    return;
  }
  action->unassign_id(animated_id);
}

void unassign_slot(ID &animated_id)
{
  AnimData *adt = BKE_animdata_from_id(&animated_id);
  BLI_assert_msg(adt, "Cannot unassign an Action Slot from a non-animated ID.");
  if (!adt) {
    return;
  }

  if (!adt->action) {
    /* Nothing assigned. */
    BLI_assert_msg(adt->slot_handle == Slot::unassigned,
                   "Slot handle should be 'unassigned' when no Action is assigned");
    return;
  }

  /* Assign the 'nullptr' slot, effectively unassigning it. */
  Action &action = adt->action->wrap();
  action.assign_id(nullptr, animated_id);
}

/* TODO: rename to get_action(). */
Action *get_action(ID &animated_id)
{
  AnimData *adt = BKE_animdata_from_id(&animated_id);
  if (!adt) {
    return nullptr;
  }
  if (!adt->action) {
    return nullptr;
  }
  return &adt->action->wrap();
}

std::optional<std::pair<Action *, Slot *>> get_action_slot_pair(ID &animated_id)
{
  AnimData *adt = BKE_animdata_from_id(&animated_id);
  if (!adt || !adt->action) {
    /* Not animated by any Action. */
    return std::nullopt;
  }

  Action &action = adt->action->wrap();
  Slot *slot = action.slot_for_handle(adt->slot_handle);
  if (!slot) {
    /* Will not receive any animation from this Action. */
    return std::nullopt;
  }

  return std::make_pair(&action, slot);
}

/* ----- ActionStrip implementation ----------- */

Strip *Strip::duplicate(const StringRefNull allocation_name) const
{
  switch (this->type()) {
    case Type::Keyframe: {
      const KeyframeStrip &source = this->as<KeyframeStrip>();
      KeyframeStrip *copy = MEM_new<KeyframeStrip>(allocation_name.c_str(), source);
      return &copy->strip.wrap();
    }
  }
  BLI_assert_unreachable();
  return nullptr;
}

Strip::~Strip()
{
  switch (this->type()) {
    case Type::Keyframe:
      this->as<KeyframeStrip>().~KeyframeStrip();
      return;
  }
  BLI_assert_unreachable();
}

bool Strip::is_infinite() const
{
  return this->frame_start == -std::numeric_limits<float>::infinity() &&
         this->frame_end == std::numeric_limits<float>::infinity();
}

bool Strip::contains_frame(const float frame_time) const
{
  return this->frame_start <= frame_time && frame_time <= this->frame_end;
}

bool Strip::is_last_frame(const float frame_time) const
{
  /* Maybe this needs a more advanced equality check. Implement that when
   * we have an actual example case that breaks. */
  return this->frame_end == frame_time;
}

void Strip::resize(const float frame_start, const float frame_end)
{
  BLI_assert(frame_start <= frame_end);
  BLI_assert_msg(frame_start < std::numeric_limits<float>::infinity(),
                 "only the end frame can be at positive infinity");
  BLI_assert_msg(frame_end > -std::numeric_limits<float>::infinity(),
                 "only the start frame can be at negative infinity");
  this->frame_start = frame_start;
  this->frame_end = frame_end;
}

/* ----- KeyframeActionStrip implementation ----------- */

KeyframeStrip::KeyframeStrip(const KeyframeStrip &other)
{
  memcpy(this, &other, sizeof(*this));

  this->channelbag_array = MEM_cnew_array<ActionChannelBag *>(other.channelbag_array_num,
                                                              __func__);
  Span<const ChannelBag *> channelbags_src = other.channelbags();
  for (int i : channelbags_src.index_range()) {
    this->channelbag_array[i] = MEM_new<animrig::ChannelBag>(__func__, *other.channelbag(i));
  }
}

KeyframeStrip::~KeyframeStrip()
{
  for (ChannelBag *channelbag_for_slot : this->channelbags()) {
    MEM_delete(channelbag_for_slot);
  }
  MEM_SAFE_FREE(this->channelbag_array);
  this->channelbag_array_num = 0;
}

template<> bool Strip::is<KeyframeStrip>() const
{
  return this->type() == KeyframeStrip::TYPE;
}

template<> KeyframeStrip &Strip::as<KeyframeStrip>()
{
  BLI_assert_msg(this->is<KeyframeStrip>(), "Strip is not a KeyframeStrip");
  return *reinterpret_cast<KeyframeStrip *>(this);
}

template<> const KeyframeStrip &Strip::as<KeyframeStrip>() const
{
  BLI_assert_msg(this->is<KeyframeStrip>(), "Strip is not a KeyframeStrip");
  return *reinterpret_cast<const KeyframeStrip *>(this);
}

KeyframeStrip::operator Strip &()
{
  return this->strip.wrap();
}

blender::Span<const ChannelBag *> KeyframeStrip::channelbags() const
{
  return blender::Span<ChannelBag *>{reinterpret_cast<ChannelBag **>(this->channelbag_array),
                                     this->channelbag_array_num};
}
blender::MutableSpan<ChannelBag *> KeyframeStrip::channelbags()
{
  return blender::MutableSpan<ChannelBag *>{
      reinterpret_cast<ChannelBag **>(this->channelbag_array), this->channelbag_array_num};
}
const ChannelBag *KeyframeStrip::channelbag(const int64_t index) const
{
  return &this->channelbag_array[index]->wrap();
}
ChannelBag *KeyframeStrip::channelbag(const int64_t index)
{
  return &this->channelbag_array[index]->wrap();
}
const ChannelBag *KeyframeStrip::channelbag_for_slot(const slot_handle_t slot_handle) const
{
  for (const ChannelBag *channels : this->channelbags()) {
    if (channels->slot_handle == slot_handle) {
      return channels;
    }
  }
  return nullptr;
}
int64_t KeyframeStrip::find_channelbag_index(const ChannelBag &channelbag_to_remove) const
{
  for (int64_t index = 0; index < this->channelbag_array_num; index++) {
    if (this->channelbag(index) == &channelbag_to_remove) {
      return index;
    }
  }
  return -1;
}
ChannelBag *KeyframeStrip::channelbag_for_slot(const slot_handle_t slot_handle)
{
  const auto *const_this = const_cast<const KeyframeStrip *>(this);
  const auto *const_channels = const_this->channelbag_for_slot(slot_handle);
  return const_cast<ChannelBag *>(const_channels);
}
const ChannelBag *KeyframeStrip::channelbag_for_slot(const Slot &slot) const
{
  return this->channelbag_for_slot(slot.handle);
}
ChannelBag *KeyframeStrip::channelbag_for_slot(const Slot &slot)
{
  return this->channelbag_for_slot(slot.handle);
}

ChannelBag &KeyframeStrip::channelbag_for_slot_add(const Slot &slot)
{
  BLI_assert_msg(channelbag_for_slot(slot) == nullptr,
                 "Cannot add chans-for-slot for already-registered slot");

  ChannelBag &channels = MEM_new<ActionChannelBag>(__func__)->wrap();
  channels.slot_handle = slot.handle;

  grow_array_and_append<ActionChannelBag *>(
      &this->channelbag_array, &this->channelbag_array_num, &channels);

  return channels;
}

ChannelBag &KeyframeStrip::channelbag_for_slot_ensure(const Slot &slot)
{
  ChannelBag *channel_bag = this->channelbag_for_slot(slot);
  if (channel_bag != nullptr) {
    return *channel_bag;
  }
  return this->channelbag_for_slot_add(slot);
}

static void channelbag_ptr_destructor(ActionChannelBag **dna_channelbag_ptr)
{
  ChannelBag &channelbag = (*dna_channelbag_ptr)->wrap();
  MEM_delete(&channelbag);
};

bool KeyframeStrip::channelbag_remove(ChannelBag &channelbag_to_remove)
{
  const int64_t channelbag_index = this->find_channelbag_index(channelbag_to_remove);
  if (channelbag_index < 0) {
    return false;
  }

  dna::array::remove_index(&this->channelbag_array,
                           &this->channelbag_array_num,
                           nullptr,
                           channelbag_index,
                           channelbag_ptr_destructor);

  return true;
}

const FCurve *ChannelBag::fcurve_find(const FCurveDescriptor fcurve_descriptor) const
{
  return animrig::fcurve_find(this->fcurves(), fcurve_descriptor);
}

FCurve *ChannelBag::fcurve_find(const FCurveDescriptor fcurve_descriptor)
{
  /* Intermediate variable needed to disambiguate const/non-const overloads. */
  Span<FCurve *> fcurves = this->fcurves();
  return animrig::fcurve_find(fcurves, fcurve_descriptor);
}

FCurve &ChannelBag::fcurve_ensure(const FCurveDescriptor fcurve_descriptor)
{
  if (FCurve *existing_fcurve = this->fcurve_find(fcurve_descriptor)) {
    return *existing_fcurve;
  }
  return this->fcurve_create(fcurve_descriptor);
}

FCurve *ChannelBag::fcurve_create_unique(FCurveDescriptor fcurve_descriptor)
{
  if (this->fcurve_find(fcurve_descriptor)) {
    return nullptr;
  }
  return &this->fcurve_create(fcurve_descriptor);
}

FCurve &ChannelBag::fcurve_create(FCurveDescriptor fcurve_descriptor)
{
  FCurve *new_fcurve = create_fcurve_for_channel(fcurve_descriptor);

  if (this->fcurve_array_num == 0) {
    new_fcurve->flag |= FCURVE_ACTIVE; /* First curve is added active. */
  }

  grow_array_and_append(&this->fcurve_array, &this->fcurve_array_num, new_fcurve);
  return *new_fcurve;
}

static void fcurve_ptr_destructor(FCurve **fcurve_ptr)
{
  BKE_fcurve_free(*fcurve_ptr);
};

bool ChannelBag::fcurve_remove(FCurve &fcurve_to_remove)
{
  const int64_t fcurve_index = this->fcurves().as_span().first_index_try(&fcurve_to_remove);
  if (fcurve_index < 0) {
    return false;
  }

  dna::array::remove_index(
      &this->fcurve_array, &this->fcurve_array_num, nullptr, fcurve_index, fcurve_ptr_destructor);

  return true;
}

void ChannelBag::fcurves_clear()
{
  dna::array::clear(&this->fcurve_array, &this->fcurve_array_num, nullptr, fcurve_ptr_destructor);
}

SingleKeyingResult KeyframeStrip::keyframe_insert(const Slot &slot,
                                                  const FCurveDescriptor fcurve_descriptor,
                                                  const float2 time_value,
                                                  const KeyframeSettings &settings,
                                                  const eInsertKeyFlags insert_key_flags)
{
  /* Get the fcurve, or create one if it doesn't exist and the keying flags
   * allow. */
  FCurve *fcurve = nullptr;
  if (key_insertion_may_create_fcurve(insert_key_flags)) {
    fcurve = &this->channelbag_for_slot_ensure(slot).fcurve_ensure(fcurve_descriptor);
  }
  else {
    ChannelBag *channels = this->channelbag_for_slot(slot);
    if (channels != nullptr) {
      fcurve = channels->fcurve_find(fcurve_descriptor);
    }
  }

  if (!fcurve) {
    std::fprintf(stderr,
                 "FCurve %s[%d] for slot %s was not created due to either the Only Insert "
                 "Available setting or Replace keyframing mode.\n",
                 fcurve_descriptor.rna_path.c_str(),
                 fcurve_descriptor.array_index,
                 slot.name);
    return SingleKeyingResult::CANNOT_CREATE_FCURVE;
  }

  if (!BKE_fcurve_is_keyframable(fcurve)) {
    /* TODO: handle this properly, in a way that can be communicated to the user. */
    std::fprintf(stderr,
                 "FCurve %s[%d] for slot %s doesn't allow inserting keys.\n",
                 fcurve_descriptor.rna_path.c_str(),
                 fcurve_descriptor.array_index,
                 slot.name);
    return SingleKeyingResult::FCURVE_NOT_KEYFRAMEABLE;
  }

  const SingleKeyingResult insert_vert_result = insert_vert_fcurve(
      fcurve, time_value, settings, insert_key_flags);

  if (insert_vert_result != SingleKeyingResult::SUCCESS) {
    std::fprintf(stderr,
                 "Could not insert key into FCurve %s[%d] for slot %s.\n",
                 fcurve_descriptor.rna_path.c_str(),
                 fcurve_descriptor.array_index,
                 slot.name);
    return insert_vert_result;
  }

  return SingleKeyingResult::SUCCESS;
}

/* ActionChannelBag implementation. */

ChannelBag::ChannelBag(const ChannelBag &other)
{
  this->slot_handle = other.slot_handle;
  this->fcurve_array_num = other.fcurve_array_num;

  this->fcurve_array = MEM_cnew_array<FCurve *>(other.fcurve_array_num, __func__);
  for (int i = 0; i < other.fcurve_array_num; i++) {
    const FCurve *fcu_src = other.fcurve_array[i];
    this->fcurve_array[i] = BKE_fcurve_copy(fcu_src);
  }
}

ChannelBag::~ChannelBag()
{
  for (FCurve *fcu : this->fcurves()) {
    BKE_fcurve_free(fcu);
  }
  MEM_SAFE_FREE(this->fcurve_array);
  this->fcurve_array_num = 0;
}

blender::Span<const FCurve *> ChannelBag::fcurves() const
{
  return blender::Span<FCurve *>{this->fcurve_array, this->fcurve_array_num};
}
blender::MutableSpan<FCurve *> ChannelBag::fcurves()
{
  return blender::MutableSpan<FCurve *>{this->fcurve_array, this->fcurve_array_num};
}
const FCurve *ChannelBag::fcurve(const int64_t index) const
{
  return this->fcurve_array[index];
}
FCurve *ChannelBag::fcurve(const int64_t index)
{
  return this->fcurve_array[index];
}

/* Utility function implementations. */

static const animrig::ChannelBag *channelbag_for_action_slot(const Action &action,
                                                             const slot_handle_t slot_handle)
{
  if (slot_handle == Slot::unassigned) {
    return nullptr;
  }

  for (const animrig::Layer *layer : action.layers()) {
    for (const animrig::Strip *strip : layer->strips()) {
      switch (strip->type()) {
        case animrig::Strip::Type::Keyframe: {
          const animrig::KeyframeStrip &key_strip = strip->as<animrig::KeyframeStrip>();
          const animrig::ChannelBag *bag = key_strip.channelbag_for_slot(slot_handle);
          if (bag) {
            return bag;
          }
        }
      }
    }
  }

  return nullptr;
}

static animrig::ChannelBag *channelbag_for_action_slot(Action &action,
                                                       const slot_handle_t slot_handle)
{
  const animrig::ChannelBag *const_bag = channelbag_for_action_slot(
      const_cast<const Action &>(action), slot_handle);
  return const_cast<animrig::ChannelBag *>(const_bag);
}

Span<FCurve *> fcurves_for_action_slot(Action &action, const slot_handle_t slot_handle)
{
  assert_baklava_phase_1_invariants(action);
  animrig::ChannelBag *bag = channelbag_for_action_slot(action, slot_handle);
  if (!bag) {
    return {};
  }
  return bag->fcurves();
}

Span<const FCurve *> fcurves_for_action_slot(const Action &action, const slot_handle_t slot_handle)
{
  assert_baklava_phase_1_invariants(action);
  const animrig::ChannelBag *bag = channelbag_for_action_slot(action, slot_handle);
  if (!bag) {
    return {};
  }
  return bag->fcurves();
}

/* Lots of template args to support transparent non-const and const versions. */
template<typename ActionType,
         typename FCurveType,
         typename LayerType,
         typename StripType,
         typename KeyframeStripType,
         typename ChannelBagType>
static Vector<FCurveType *> fcurves_all_into(ActionType &action)
{
  /* Empty means Empty. */
  if (action.is_empty()) {
    return {};
  }

  /* Legacy Action. */
  if (action.is_action_legacy()) {
    Vector<FCurveType *> legacy_fcurves;
    LISTBASE_FOREACH (FCurveType *, fcurve, &action.curves) {
      legacy_fcurves.append(fcurve);
    }
    return legacy_fcurves;
  }

  /* Layered Action. */
  BLI_assert(action.is_action_layered());

  Vector<FCurveType *> all_fcurves;
  for (LayerType *layer : action.layers()) {
    for (StripType *strip : layer->strips()) {
      switch (strip->type()) {
        case Strip::Type::Keyframe: {
          KeyframeStripType &key_strip = strip->template as<KeyframeStrip>();
          for (ChannelBagType *bag : key_strip.channelbags()) {
            for (FCurveType *fcurve : bag->fcurves()) {
              all_fcurves.append(fcurve);
            }
          }
        }
      }
    }
  }
  return all_fcurves;
}

Vector<FCurve *> fcurves_all(Action &action)
{
  return fcurves_all_into<Action, FCurve, Layer, Strip, KeyframeStrip, ChannelBag>(action);
}

Vector<const FCurve *> fcurves_all(const Action &action)
{
  return fcurves_all_into<const Action,
                          const FCurve,
                          const Layer,
                          const Strip,
                          const KeyframeStrip,
                          const ChannelBag>(action);
}

FCurve *action_fcurve_find(bAction *act, FCurveDescriptor fcurve_descriptor)
{
  if (act == nullptr) {
    return nullptr;
  }
  return BKE_fcurve_find(
      &act->curves, fcurve_descriptor.rna_path.c_str(), fcurve_descriptor.array_index);
}

FCurve *action_fcurve_ensure(Main *bmain,
                             bAction *act,
                             const char group[],
                             PointerRNA *ptr,
                             FCurveDescriptor fcurve_descriptor)
{
  if (act == nullptr) {
    return nullptr;
  }
  Action &action = act->wrap();

  if (USER_EXPERIMENTAL_TEST(&U, use_animation_baklava) && action.is_action_layered()) {
    /* NOTE: for layered actions we require the following:
     *
     * - `ptr` is non-null.
     * - `ptr` has an `owner_id` that already uses `act`.
     *
     * This isn't for any principled reason, but rather is because adding
     * support for layered actions to this function was a fix to make Follow
     * Path animation work properly with layered actions (see PR #124353), and
     * those are the requirements the Follow Path code conveniently met.
     * Moreover those requirements were also already met by the other call sites
     * that potentially call this function with layered actions.
     *
     * Trying to puzzle out what "should" happen when these requirements don't
     * hold, or if this is even the best place to handle the layered action
     * cases at all, was leading to discussion of larger changes than made sense
     * to tackle at that point. */
    BLI_assert(ptr != nullptr);
    if (ptr == nullptr) {
      return nullptr;
    }
    AnimData *adt = BKE_animdata_from_id(ptr->owner_id);
    BLI_assert(adt != nullptr && adt->action == act);
    if (adt == nullptr || adt->action != act) {
      return nullptr;
    }

    /* Ensure the id has an assigned slot. */
    Slot &slot = action.slot_ensure_for_id(*ptr->owner_id);
    action.assign_id(&slot, *ptr->owner_id);

    action.layer_ensure_at_least_one();

    assert_baklava_phase_1_invariants(action);
    KeyframeStrip &strip = action.layer(0)->strip(0)->as<KeyframeStrip>();

    return &strip.channelbag_for_slot_ensure(slot).fcurve_ensure(fcurve_descriptor);
  }

  /* Try to find f-curve matching for this setting.
   * - add if not found and allowed to add one
   *   TODO: add auto-grouping support? how this works will need to be resolved
   */
  FCurve *fcu = BKE_fcurve_find(
      &act->curves, fcurve_descriptor.rna_path.c_str(), fcurve_descriptor.array_index);

  if (fcu != nullptr) {
    return fcu;
  }

  /* Determine the property subtype if we can. */
  std::optional<PropertySubType> prop_subtype = std::nullopt;
  if (ptr != nullptr) {
    PropertyRNA *resolved_prop;
    PointerRNA resolved_ptr;
    PointerRNA id_ptr = RNA_id_pointer_create(ptr->owner_id);
    const bool resolved = RNA_path_resolve_property(
        &id_ptr, fcurve_descriptor.rna_path.c_str(), &resolved_ptr, &resolved_prop);
    if (resolved) {
      prop_subtype = RNA_property_subtype(resolved_prop);
    }
  }

  BLI_assert_msg(!fcurve_descriptor.prop_subtype.has_value(),
                 "Did not expect a prop_subtype to be passed in. This is fine, but does need some "
                 "changes to action_fcurve_ensure() to deal with it");
  fcu = create_fcurve_for_channel(
      {fcurve_descriptor.rna_path, fcurve_descriptor.array_index, prop_subtype});

  if (BLI_listbase_is_empty(&act->curves)) {
    fcu->flag |= FCURVE_ACTIVE;
  }

  if (group) {
    bActionGroup *agrp = BKE_action_group_find_name(act, group);

    if (agrp == nullptr) {
      agrp = action_groups_add_new(act, group);

      /* Sync bone group colors if applicable. */
      if (ptr && (ptr->type == &RNA_PoseBone) && ptr->data) {
        const bPoseChannel *pchan = static_cast<const bPoseChannel *>(ptr->data);
        action_group_colors_set_from_posebone(agrp, pchan);
      }
    }

    action_groups_add_channel(act, agrp, fcu);
  }
  else {
    BLI_addtail(&act->curves, fcu);
  }

  /* New f-curve was added, meaning it's possible that it affects
   * dependency graph component which wasn't previously animated.
   */
  DEG_relations_tag_update(bmain);

  return fcu;
}

ID *action_slot_get_id_for_keying(Main &bmain,
                                  Action &action,
                                  const slot_handle_t slot_handle,
                                  ID *primary_id)
{
  if (action.is_action_legacy()) {
    if (primary_id && get_action(*primary_id) == &action) {
      return primary_id;
    }
    return nullptr;
  }

  Slot *slot = action.slot_for_handle(slot_handle);
  if (slot == nullptr) {
    return nullptr;
  }

  blender::Span<ID *> users = slot->users(bmain);
  if (users.size() == 1) {
    /* We only do this for `users.size() == 1` and not `users.size() >= 1`
     * because when there's more than one user it's ambiguous which user we
     * should return, and that would be unpredictable for end users of Blender.
     * We also expect that to be a corner case anyway.  So instead we let that
     * case either get disambiguated by the primary ID in the case below, or
     * return null. */
    return users[0];
  }
  if (users.contains(primary_id)) {
    return primary_id;
  }

  return nullptr;
}

ID *action_slot_get_id_best_guess(Main &bmain, Slot &slot, ID *primary_id)
{
  blender::Span<ID *> users = slot.users(bmain);
  if (users.is_empty()) {
    return 0;
  }
  if (users.contains(primary_id)) {
    return primary_id;
  }
  return users[0];
}

void assert_baklava_phase_1_invariants(const Action &action)
{
  if (action.is_action_legacy()) {
    return;
  }
  if (action.layers().is_empty()) {
    return;
  }
  BLI_assert(action.layers().size() == 1);

  assert_baklava_phase_1_invariants(*action.layer(0));
}

void assert_baklava_phase_1_invariants(const Layer &layer)
{
  if (layer.strips().is_empty()) {
    return;
  }
  BLI_assert(layer.strips().size() == 1);

  assert_baklava_phase_1_invariants(*layer.strip(0));
}

void assert_baklava_phase_1_invariants(const Strip &strip)
{
  UNUSED_VARS_NDEBUG(strip);
  BLI_assert(strip.type() == Strip::Type::Keyframe);
  BLI_assert(strip.is_infinite());
  BLI_assert(strip.frame_offset == 0.0);
}

Action *convert_to_layered_action(Main &bmain, const Action &legacy_action)
{
  if (!legacy_action.is_action_legacy()) {
    return nullptr;
  }

  std::string suffix = "_layered";
  /* In case the legacy action has a long name it is shortened to make space for the suffix. */
  char legacy_name[MAX_ID_NAME - 10];
  /* Offsetting the id.name to remove the ID prefix (AC) which gets added back later. */
  STRNCPY_UTF8(legacy_name, legacy_action.id.name + 2);

  const std::string layered_action_name = std::string(legacy_name) + suffix;
  bAction *dna_action = BKE_action_add(&bmain, layered_action_name.c_str());

  Action &converted_action = dna_action->wrap();
  Slot &slot = converted_action.slot_add();
  Layer &layer = converted_action.layer_add(legacy_action.id.name);
  KeyframeStrip &strip = layer.strip_add<KeyframeStrip>();
  BLI_assert(strip.channelbag_array_num == 0);
  ChannelBag *bag = &strip.channelbag_for_slot_add(slot);

  const int fcu_count = BLI_listbase_count(&legacy_action.curves);
  bag->fcurve_array = MEM_cnew_array<FCurve *>(fcu_count, "Convert to layered action");
  bag->fcurve_array_num = fcu_count;

  int i = 0;
  LISTBASE_FOREACH_INDEX (FCurve *, fcu, &legacy_action.curves, i) {
    bag->fcurve_array[i] = BKE_fcurve_copy(fcu);
  }

  return &converted_action;
}

}  // namespace blender::animrig
