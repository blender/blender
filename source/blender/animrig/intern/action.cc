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
#include "RNA_prototypes.h"

#include "ED_keyframing.hh"

#include "MEM_guardedalloc.h"

#include "BLT_translation.hh"

#include "DEG_depsgraph_build.hh"

#include "ANIM_action.hh"
#include "ANIM_fcurve.hh"
#include "action_runtime.hh"

#include "atomic_ops.h"

#include <cstdio>
#include <cstring>

namespace blender::animrig {

namespace {
/**
 * Default name for animation bindings. The first two characters in the name indicate the ID type
 * of whatever is animated by it.
 *
 * Since the ID type may not be determined when the binding is created, the prefix starts out at
 * XX. Note that no code should use this XX value; use Binding::has_idtype() instead.
 */
constexpr const char *binding_default_name = "Binding";
constexpr const char *binding_unbound_prefix = "XX";

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
      MEM_cnew_array<T *>(new_array_num, "animrig::animation/grow_array"));

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

/* ----- Animation implementation ----------- */

bool Action::is_empty() const
{
  return this->layer_array_num == 0 && this->binding_array_num == 0 &&
         BLI_listbase_is_empty(&this->curves);
}
bool Action::is_action_legacy() const
{
  /* This is a valid legacy Action only if there is no layered info. */
  return this->layer_array_num == 0 && this->binding_array_num == 0;
}
bool Action::is_action_layered() const
{
  /* This is a valid layered Action if there is ANY layered info (because that
   * takes precedence) or when there is no legacy info. */
  return this->layer_array_num > 0 || this->binding_array_num > 0 ||
         BLI_listbase_is_empty(&this->curves);
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

blender::Span<const Binding *> Action::bindings() const
{
  return blender::Span<Binding *>{reinterpret_cast<Binding **>(this->binding_array),
                                  this->binding_array_num};
}
blender::MutableSpan<Binding *> Action::bindings()
{
  return blender::MutableSpan<Binding *>{reinterpret_cast<Binding **>(this->binding_array),
                                         this->binding_array_num};
}
const Binding *Action::binding(const int64_t index) const
{
  return &this->binding_array[index]->wrap();
}
Binding *Action::binding(const int64_t index)
{
  return &this->binding_array[index]->wrap();
}

Binding *Action::binding_for_handle(const binding_handle_t handle)
{
  const Binding *binding = const_cast<const Action *>(this)->binding_for_handle(handle);
  return const_cast<Binding *>(binding);
}

const Binding *Action::binding_for_handle(const binding_handle_t handle) const
{
  /* TODO: implement hash-map lookup. */
  for (const Binding *binding : bindings()) {
    if (binding->handle == handle) {
      return binding;
    }
  }
  return nullptr;
}

static void anim_binding_name_ensure_unique(Action &animation, Binding &binding)
{
  /* Cannot capture parameters by reference in the lambda, as that would change its signature
   * and no longer be compatible with BLI_uniquename_cb(). That's why this struct is necessary. */
  struct DupNameCheckData {
    Action &anim;
    Binding &binding;
  };
  DupNameCheckData check_data = {animation, binding};

  auto check_name_is_used = [](void *arg, const char *name) -> bool {
    DupNameCheckData *data = static_cast<DupNameCheckData *>(arg);
    for (const Binding *binding : data->anim.bindings()) {
      if (binding == &data->binding) {
        /* Don't compare against the binding that's being renamed. */
        continue;
      }
      if (STREQ(binding->name, name)) {
        return true;
      }
    }
    return false;
  };

  BLI_uniquename_cb(check_name_is_used, &check_data, "", '.', binding.name, sizeof(binding.name));
}

/* TODO: maybe this function should only set the 'name without prefix' aka the 'display name'. That
 * way only `this->id_type` is responsible for the prefix. I (Sybren) think that's easier to
 * determine when the code is a bit more mature, and we can see what the majority of the calls to
 * this function actually do/need. */
void Action::binding_name_set(Main &bmain, Binding &binding, const StringRefNull new_name)
{
  this->binding_name_define(binding, new_name);
  this->binding_name_propagate(bmain, binding);
}

void Action::binding_name_define(Binding &binding, const StringRefNull new_name)
{
  BLI_assert_msg(
      StringRef(new_name).size() >= Binding::name_length_min,
      "Animation Bindings must be large enough for a 2-letter ID code + the display name");
  STRNCPY_UTF8(binding.name, new_name.c_str());
  anim_binding_name_ensure_unique(*this, binding);
}

void Action::binding_name_propagate(Main &bmain, const Binding &binding)
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
        /* Not animated by this Animation. */
        continue;
      }
      if (adt->binding_handle != binding.handle) {
        /* Not animated by this Binding. */
        continue;
      }

      /* Ensure the Binding name on the AnimData is correct. */
      STRNCPY_UTF8(adt->binding_name, binding.name);
    }
    FOREACH_MAIN_LISTBASE_ID_END;
  }
  FOREACH_MAIN_LISTBASE_END;
}

Binding *Action::binding_find_by_name(const StringRefNull binding_name)
{
  for (Binding *binding : bindings()) {
    if (STREQ(binding->name, binding_name.c_str())) {
      return binding;
    }
  }
  return nullptr;
}

Binding &Action::binding_allocate()
{
  Binding &binding = *MEM_new<Binding>(__func__);
  this->last_binding_handle++;
  BLI_assert_msg(this->last_binding_handle > 0, "Animation Binding handle overflow");
  binding.handle = this->last_binding_handle;
  return binding;
}

Binding &Action::binding_add()
{
  Binding &binding = this->binding_allocate();

  /* Assign the default name and the 'unbound' name prefix. */
  STRNCPY_UTF8(binding.name, binding_unbound_prefix);
  BLI_strncpy_utf8(binding.name + 2, DATA_(binding_default_name), ARRAY_SIZE(binding.name) - 2);

  /* Append the Binding to the animation data-block. */
  grow_array_and_append<::ActionBinding *>(
      &this->binding_array, &this->binding_array_num, &binding);

  anim_binding_name_ensure_unique(*this, binding);

  /* If this is the first binding in this Action, it means that it could have
   * been used as a legacy Action before. As a result, this->idroot may be
   * non-zero while it should be zero for layered Actions.
   *
   * And since setting this to 0 when it is already supposed to be 0 is fine,
   * there is no check for whether this is actually the first layer. */
  this->idroot = 0;

  return binding;
}

Binding &Action::binding_add_for_id(const ID &animated_id)
{
  Binding &binding = this->binding_add();

  binding.idtype = GS(animated_id.name);
  this->binding_name_define(binding, animated_id.name);

  /* No need to call anim.binding_name_propagate() as nothing will be using
   * this brand new Binding yet. */

  return binding;
}

Binding *Action::find_suitable_binding_for(const ID &animated_id)
{
  AnimData *adt = BKE_animdata_from_id(&animated_id);

  /* The binding handle is only valid when this action has already been
   * assigned. Otherwise it's meaningless. */
  if (adt && adt->action == this) {
    Binding *binding = this->binding_for_handle(adt->binding_handle);
    if (binding && binding->is_suitable_for(animated_id)) {
      return binding;
    }
  }

  /* Try the binding name from the AnimData, if it is set. */
  if (adt && adt->binding_name[0]) {
    Binding *binding = this->binding_find_by_name(adt->binding_name);
    if (binding && binding->is_suitable_for(animated_id)) {
      return binding;
    }
  }

  /* As a last resort, search for the ID name. */
  Binding *binding = this->binding_find_by_name(animated_id.name);
  if (binding && binding->is_suitable_for(animated_id)) {
    return binding;
  }

  return nullptr;
}

bool Action::is_binding_animated(const binding_handle_t binding_handle) const
{
  if (binding_handle == Binding::unassigned) {
    return false;
  }

  Span<const FCurve *> fcurves = fcurves_for_animation(*this, binding_handle);
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

bool Action::assign_id(Binding *binding, ID &animated_id)
{
  AnimData *adt = BKE_animdata_ensure_id(&animated_id);
  if (!adt) {
    return false;
  }

  if (adt->action && adt->action != this) {
    /* The caller should unassign the ID from its existing animation first, or
     * use the top-level function `assign_animation(anim, ID)`. */
    return false;
  }

  /* Check that the new Binding is suitable, before changing `adt`. */
  if (binding && !binding->is_suitable_for(animated_id)) {
    return false;
  }

  /* Unassign any previously-assigned Binding. */
  Binding *binding_to_unassign = this->binding_for_handle(adt->binding_handle);
  if (binding_to_unassign) {
    binding_to_unassign->users_remove(animated_id);

    /* Before unassigning, make sure that the stored Binding name is up to date. The binding name
     * might have changed in a way that wasn't copied into the ADT yet (for example when the
     * Action is linked from another file), so better copy the name to be sure that it can be
     * transparently reassigned later.
     *
     * TODO: Replace this with a BLI_assert() that the name is as expected, and "simply" ensure
     * this name is always correct. */
    STRNCPY_UTF8(adt->binding_name, binding_to_unassign->name);
  }

  /* Assign the Action itself. */
  if (!adt->action) {
    /* Due to the precondition check above, we know that adt->action is either 'this' (in which
     * case the user count is already correct) or `nullptr` (in which case this is a new
     * reference, and the user count should be increased). */
    id_us_plus(&this->id);
    adt->action = this;
  }

  /* Assign the Binding. */
  if (binding) {
    this->binding_setup_for_id(*binding, animated_id);
    adt->binding_handle = binding->handle;
    binding->users_add(animated_id);

    /* Always make sure the ID's binding name matches the assigned binding. */
    STRNCPY_UTF8(adt->binding_name, binding->name);
  }
  else {
    adt->binding_handle = Binding::unassigned;
  }

  return true;
}

void Action::binding_name_ensure_prefix(Binding &binding)
{
  binding.name_ensure_prefix();
  anim_binding_name_ensure_unique(*this, binding);
}

void Action::binding_setup_for_id(Binding &binding, const ID &animated_id)
{
  if (binding.has_idtype()) {
    BLI_assert(binding.idtype == GS(animated_id.name));
    return;
  }

  binding.idtype = GS(animated_id.name);
  this->binding_name_ensure_prefix(binding);
}

void Action::unassign_id(ID &animated_id)
{
  AnimData *adt = BKE_animdata_from_id(&animated_id);
  BLI_assert_msg(adt, "ID is not animated at all");
  BLI_assert_msg(adt->action == this, "ID is not assigned to this Animation");

  /* Unassign the Binding first. */
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

/* ----- ActionBinding implementation ----------- */

Binding::Binding()
{
  memset(this, 0, sizeof(*this));
  this->runtime = MEM_new<BindingRuntime>(__func__);
}

Binding::Binding(const Binding &other)
{
  memset(this, 0, sizeof(*this));
  STRNCPY(this->name, other.name);
  this->idtype = other.idtype;
  this->handle = other.handle;
  this->runtime = MEM_new<BindingRuntime>(__func__);
}

Binding::~Binding()
{
  MEM_delete(this->runtime);
}

void Binding::blend_read_post()
{
  BLI_assert(!this->runtime);
  this->runtime = MEM_new<BindingRuntime>(__func__);
}

bool Binding::is_suitable_for(const ID &animated_id) const
{
  if (!this->has_idtype()) {
    /* Without specific ID type set, this Binding can animate any ID. */
    return true;
  }

  /* Check that the ID type is compatible with this binding. */
  const int animated_idtype = GS(animated_id.name);
  return this->idtype == animated_idtype;
}

bool Binding::has_idtype() const
{
  return this->idtype != 0;
}

Span<ID *> Binding::users(Main &bmain) const
{
  if (bmain.is_action_binding_to_id_map_dirty) {
    internal::rebuild_binding_user_cache(bmain);
  }
  BLI_assert(this->runtime);
  return this->runtime->users.as_span();
}

Vector<ID *> Binding::runtime_users()
{
  BLI_assert_msg(this->runtime, "Binding::runtime should always be allocated");
  return this->runtime->users;
}

void Binding::users_add(ID &animated_id)
{
  BLI_assert(this->runtime);
  this->runtime->users.append_non_duplicates(&animated_id);
}

void Binding::users_remove(ID &animated_id)
{
  BLI_assert(this->runtime);
  Vector<ID *> &users = this->runtime->users;

  const int64_t vector_index = users.first_index_of_try(&animated_id);
  if (vector_index < 0) {
    return;
  }

  users.remove_and_reorder(vector_index);
}

void Binding::users_invalidate(Main &bmain)
{
  bmain.is_action_binding_to_id_map_dirty = true;
}

/* ----- Functions  ----------- */

bool assign_animation(Action &anim, ID &animated_id)
{
  unassign_animation(animated_id);

  Binding *binding = anim.find_suitable_binding_for(animated_id);
  return anim.assign_id(binding, animated_id);
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

void unassign_animation(ID &animated_id)
{
  Action *anim = get_animation(animated_id);
  if (!anim) {
    return;
  }
  anim->unassign_id(animated_id);
}

void unassign_binding(ID &animated_id)
{
  AnimData *adt = BKE_animdata_from_id(&animated_id);
  BLI_assert_msg(adt, "Cannot unassign an Action Binding from a non-animated ID.");
  if (!adt) {
    return;
  }

  if (!adt->action) {
    /* Nothing assigned. */
    BLI_assert_msg(adt->binding_handle == Binding::unassigned,
                   "Binding handle should be 'unassigned' when no Action is assigned");
    return;
  }

  /* Assign the 'nullptr' binding, effectively unassigning it. */
  Action &action = adt->action->wrap();
  action.assign_id(nullptr, animated_id);
}

/* TODO: rename to get_action(). */
Action *get_animation(ID &animated_id)
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

std::optional<std::pair<Action *, Binding *>> get_action_binding_pair(ID &animated_id)
{
  AnimData *adt = BKE_animdata_from_id(&animated_id);
  if (!adt || !adt->action) {
    /* Not animated by any Action. */
    return std::nullopt;
  }

  Action &action = adt->action->wrap();
  Binding *binding = action.binding_for_handle(adt->binding_handle);
  if (!binding) {
    /* Will not receive any animation from this Action. */
    return std::nullopt;
  }

  return std::make_pair(&action, binding);
}

std::string Binding::name_prefix_for_idtype() const
{
  if (!this->has_idtype()) {
    return binding_unbound_prefix;
  }

  char name[3] = {0};
  *reinterpret_cast<short *>(name) = this->idtype;
  return name;
}

StringRefNull Binding::name_without_prefix() const
{
  BLI_assert(StringRef(this->name).size() >= name_length_min);

  /* Avoid accessing an uninitialized part of the string accidentally. */
  if (this->name[0] == '\0' || this->name[1] == '\0') {
    return "";
  }
  return this->name + 2;
}

void Binding::name_ensure_prefix()
{
  BLI_assert(StringRef(this->name).size() >= name_length_min);

  if (StringRef(this->name).size() < 2) {
    /* The code below would overwrite the trailing 0-byte. */
    this->name[2] = '\0';
  }

  if (!this->has_idtype()) {
    /* A zero idtype is not going to convert to a two-character string, so we
     * need to explicitly assign the default prefix. */
    this->name[0] = binding_unbound_prefix[0];
    this->name[1] = binding_unbound_prefix[1];
    return;
  }

  *reinterpret_cast<short *>(this->name) = this->idtype;
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

  this->channelbags_array = MEM_cnew_array<ActionChannelBag *>(other.channelbags_array_num,
                                                               __func__);
  Span<const ChannelBag *> channelbags_src = other.channelbags();
  for (int i : channelbags_src.index_range()) {
    this->channelbags_array[i] = MEM_new<animrig::ChannelBag>(__func__, *other.channelbag(i));
  }
}

KeyframeStrip::~KeyframeStrip()
{
  for (ChannelBag *channelbag_for_binding : this->channelbags()) {
    MEM_delete(channelbag_for_binding);
  }
  MEM_SAFE_FREE(this->channelbags_array);
  this->channelbags_array_num = 0;
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
  return blender::Span<ChannelBag *>{reinterpret_cast<ChannelBag **>(this->channelbags_array),
                                     this->channelbags_array_num};
}
blender::MutableSpan<ChannelBag *> KeyframeStrip::channelbags()
{
  return blender::MutableSpan<ChannelBag *>{
      reinterpret_cast<ChannelBag **>(this->channelbags_array), this->channelbags_array_num};
}
const ChannelBag *KeyframeStrip::channelbag(const int64_t index) const
{
  return &this->channelbags_array[index]->wrap();
}
ChannelBag *KeyframeStrip::channelbag(const int64_t index)
{
  return &this->channelbags_array[index]->wrap();
}
const ChannelBag *KeyframeStrip::channelbag_for_binding(
    const binding_handle_t binding_handle) const
{
  for (const ChannelBag *channels : this->channelbags()) {
    if (channels->binding_handle == binding_handle) {
      return channels;
    }
  }
  return nullptr;
}
ChannelBag *KeyframeStrip::channelbag_for_binding(const binding_handle_t binding_handle)
{
  const auto *const_this = const_cast<const KeyframeStrip *>(this);
  const auto *const_channels = const_this->channelbag_for_binding(binding_handle);
  return const_cast<ChannelBag *>(const_channels);
}
const ChannelBag *KeyframeStrip::channelbag_for_binding(const Binding &binding) const
{
  return this->channelbag_for_binding(binding.handle);
}
ChannelBag *KeyframeStrip::channelbag_for_binding(const Binding &binding)
{
  return this->channelbag_for_binding(binding.handle);
}

ChannelBag &KeyframeStrip::channelbag_for_binding_add(const Binding &binding)
{
  BLI_assert_msg(channelbag_for_binding(binding) == nullptr,
                 "Cannot add chans-for-binding for already-registered binding");

  ChannelBag &channels = MEM_new<ActionChannelBag>(__func__)->wrap();
  channels.binding_handle = binding.handle;

  grow_array_and_append<ActionChannelBag *>(
      &this->channelbags_array, &this->channelbags_array_num, &channels);

  return channels;
}

FCurve *KeyframeStrip::fcurve_find(const Binding &binding,
                                   const FCurveDescriptor fcurve_descriptor)
{
  ChannelBag *channels = this->channelbag_for_binding(binding);
  if (channels == nullptr) {
    return nullptr;
  }

  /* Copy of the logic in BKE_fcurve_find(), but then compatible with our array-of-FCurves
   * instead of ListBase. */

  for (FCurve *fcu : channels->fcurves()) {
    /* Check indices first, much cheaper than a string comparison. */
    /* Simple string-compare (this assumes that they have the same root...) */
    if (fcu->array_index == fcurve_descriptor.array_index && fcu->rna_path &&
        StringRef(fcu->rna_path) == fcurve_descriptor.rna_path)
    {
      return fcu;
    }
  }
  return nullptr;
}

FCurve &KeyframeStrip::fcurve_find_or_create(const Binding &binding,
                                             const FCurveDescriptor fcurve_descriptor)
{
  if (FCurve *existing_fcurve = this->fcurve_find(binding, fcurve_descriptor)) {
    return *existing_fcurve;
  }

  FCurve *new_fcurve = create_fcurve_for_channel(fcurve_descriptor);

  ChannelBag *channels = this->channelbag_for_binding(binding);
  if (channels == nullptr) {
    channels = &this->channelbag_for_binding_add(binding);
  }

  if (channels->fcurve_array_num == 0) {
    new_fcurve->flag |= FCURVE_ACTIVE; /* First curve is added active. */
  }

  grow_array_and_append(&channels->fcurve_array, &channels->fcurve_array_num, new_fcurve);
  return *new_fcurve;
}

SingleKeyingResult KeyframeStrip::keyframe_insert(const Binding &binding,
                                                  const FCurveDescriptor fcurve_descriptor,
                                                  const float2 time_value,
                                                  const KeyframeSettings &settings,
                                                  const eInsertKeyFlags insert_key_flags)
{
  /* Get the fcurve, or create one if it doesn't exist and the keying flags
   * allow. */
  FCurve *fcurve = key_insertion_may_create_fcurve(insert_key_flags) ?
                       &this->fcurve_find_or_create(binding, fcurve_descriptor) :
                       this->fcurve_find(binding, fcurve_descriptor);
  if (!fcurve) {
    std::fprintf(stderr,
                 "FCurve %s[%d] for binding %s was not created due to either the Only Insert "
                 "Available setting or Replace keyframing mode.\n",
                 fcurve_descriptor.rna_path.c_str(),
                 fcurve_descriptor.array_index,
                 binding.name);
    return SingleKeyingResult::CANNOT_CREATE_FCURVE;
  }

  if (!BKE_fcurve_is_keyframable(fcurve)) {
    /* TODO: handle this properly, in a way that can be communicated to the user. */
    std::fprintf(stderr,
                 "FCurve %s[%d] for binding %s doesn't allow inserting keys.\n",
                 fcurve_descriptor.rna_path.c_str(),
                 fcurve_descriptor.array_index,
                 binding.name);
    return SingleKeyingResult::FCURVE_NOT_KEYFRAMEABLE;
  }

  const SingleKeyingResult insert_vert_result = insert_vert_fcurve(
      fcurve, time_value, settings, insert_key_flags);

  if (insert_vert_result != SingleKeyingResult::SUCCESS) {
    std::fprintf(stderr,
                 "Could not insert key into FCurve %s[%d] for binding %s.\n",
                 fcurve_descriptor.rna_path.c_str(),
                 fcurve_descriptor.array_index,
                 binding.name);
    return insert_vert_result;
  }

  return SingleKeyingResult::SUCCESS;
}

/* ActionChannelBag implementation. */

ChannelBag::ChannelBag(const ChannelBag &other)
{
  this->binding_handle = other.binding_handle;
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

const FCurve *ChannelBag::fcurve_find(const StringRefNull rna_path, const int array_index) const
{
  for (const FCurve *fcu : this->fcurves()) {
    /* Check indices first, much cheaper than a string comparison. */
    if (fcu->array_index == array_index && fcu->rna_path && StringRef(fcu->rna_path) == rna_path) {
      return fcu;
    }
  }
  return nullptr;
}

/* Utility function implementations. */

static const animrig::ChannelBag *channelbag_for_animation(const Action &anim,
                                                           const binding_handle_t binding_handle)
{
  if (binding_handle == Binding::unassigned) {
    return nullptr;
  }

  for (const animrig::Layer *layer : anim.layers()) {
    for (const animrig::Strip *strip : layer->strips()) {
      switch (strip->type()) {
        case animrig::Strip::Type::Keyframe: {
          const animrig::KeyframeStrip &key_strip = strip->as<animrig::KeyframeStrip>();
          const animrig::ChannelBag *bag = key_strip.channelbag_for_binding(binding_handle);
          if (bag) {
            return bag;
          }
        }
      }
    }
  }

  return nullptr;
}

static animrig::ChannelBag *channelbag_for_animation(Action &anim,
                                                     const binding_handle_t binding_handle)
{
  const animrig::ChannelBag *const_bag = channelbag_for_animation(const_cast<const Action &>(anim),
                                                                  binding_handle);
  return const_cast<animrig::ChannelBag *>(const_bag);
}

Span<FCurve *> fcurves_for_animation(Action &anim, const binding_handle_t binding_handle)
{
  animrig::ChannelBag *bag = channelbag_for_animation(anim, binding_handle);
  if (!bag) {
    return {};
  }
  return bag->fcurves();
}

Span<const FCurve *> fcurves_for_animation(const Action &anim,
                                           const binding_handle_t binding_handle)
{
  const animrig::ChannelBag *bag = channelbag_for_animation(anim, binding_handle);
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

}  // namespace blender::animrig
