/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_anim_defaults.h"
#include "DNA_anim_types.h"
#include "DNA_array_utils.hh"
#include "DNA_defaults.h"

#include "BLI_listbase.h"
#include "BLI_listbase_wrapper.hh"
#include "BLI_math_base.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_string_utils.hh"

#include "BKE_anim_data.hh"
#include "BKE_animation.hh"
#include "BKE_fcurve.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"

#include "ED_keyframing.hh"

#include "MEM_guardedalloc.h"

#include "atomic_ops.h"

#include "ANIM_animation.hh"
#include "ANIM_fcurve.hh"

#include <cstdio>
#include <cstring>

namespace blender::animrig {

static animrig::Layer &animationlayer_alloc()
{
  AnimationLayer *layer = DNA_struct_default_alloc(AnimationLayer);
  return layer->wrap();
}
static animrig::Strip &animationstrip_alloc_infinite(const Strip::Type type)
{
  AnimationStrip *strip = nullptr;
  switch (type) {
    case Strip::Type::Keyframe: {
      KeyframeAnimationStrip *key_strip = MEM_new<KeyframeAnimationStrip>(__func__);
      strip = &key_strip->strip;
      break;
    }
  }

  BLI_assert_msg(strip, "unsupported strip type");

  /* Copy the default AnimationStrip fields into the allocated data-block. */
  memcpy(strip, DNA_struct_default_get(AnimationStrip), sizeof(*strip));
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

blender::Span<const Layer *> Animation::layers() const
{
  return blender::Span<Layer *>{reinterpret_cast<Layer **>(this->layer_array),
                                this->layer_array_num};
}
blender::MutableSpan<Layer *> Animation::layers()
{
  return blender::MutableSpan<Layer *>{reinterpret_cast<Layer **>(this->layer_array),
                                       this->layer_array_num};
}
const Layer *Animation::layer(const int64_t index) const
{
  return &this->layer_array[index]->wrap();
}
Layer *Animation::layer(const int64_t index)
{
  return &this->layer_array[index]->wrap();
}

Layer &Animation::layer_add(const StringRefNull name)
{
  using namespace blender::animrig;

  Layer &new_layer = animationlayer_alloc();
  STRNCPY_UTF8(new_layer.name, name.c_str());

  grow_array_and_append<::AnimationLayer *>(
      &this->layer_array, &this->layer_array_num, &new_layer);
  this->layer_active_index = this->layer_array_num - 1;

  return new_layer;
}

static void layer_ptr_destructor(AnimationLayer **dna_layer_ptr)
{
  Layer &layer = (*dna_layer_ptr)->wrap();
  MEM_delete(&layer);
};

bool Animation::layer_remove(Layer &layer_to_remove)
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

int64_t Animation::find_layer_index(const Layer &layer) const
{
  for (const int64_t layer_index : this->layers().index_range()) {
    const Layer *visit_layer = this->layer(layer_index);
    if (visit_layer == &layer) {
      return layer_index;
    }
  }
  return -1;
}

blender::Span<const Binding *> Animation::bindings() const
{
  return blender::Span<Binding *>{reinterpret_cast<Binding **>(this->binding_array),
                                  this->binding_array_num};
}
blender::MutableSpan<Binding *> Animation::bindings()
{
  return blender::MutableSpan<Binding *>{reinterpret_cast<Binding **>(this->binding_array),
                                         this->binding_array_num};
}
const Binding *Animation::binding(const int64_t index) const
{
  return &this->binding_array[index]->wrap();
}
Binding *Animation::binding(const int64_t index)
{
  return &this->binding_array[index]->wrap();
}

Binding *Animation::binding_for_handle(const binding_handle_t handle)
{
  const Binding *binding = const_cast<const Animation *>(this)->binding_for_handle(handle);
  return const_cast<Binding *>(binding);
}

const Binding *Animation::binding_for_handle(const binding_handle_t handle) const
{
  /* TODO: implement hash-map lookup. */
  for (const Binding *binding : bindings()) {
    if (binding->handle == handle) {
      return binding;
    }
  }
  return nullptr;
}

static void anim_binding_name_ensure_unique(Animation &animation, Binding &binding)
{
  /* Cannot capture parameters by reference in the lambda, as that would change its signature
   * and no longer be compatible with BLI_uniquename_cb(). That's why this struct is necessary. */
  struct DupNameCheckData {
    Animation &anim;
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

void Animation::binding_name_set(Main &bmain, Binding &binding, const StringRefNull new_name)
{
  this->binding_name_define(binding, new_name);
  this->binding_name_propagate(bmain, binding);
}

void Animation::binding_name_define(Binding &binding, const StringRefNull new_name)
{
  STRNCPY_UTF8(binding.name, new_name.c_str());
  anim_binding_name_ensure_unique(*this, binding);
}

void Animation::binding_name_propagate(Main &bmain, const Binding &binding)
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
      if (!adt || adt->animation != this) {
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

Binding *Animation::binding_find_by_name(const StringRefNull binding_name)
{
  for (Binding *binding : bindings()) {
    if (STREQ(binding->name, binding_name.c_str())) {
      return binding;
    }
  }
  return nullptr;
}

Binding *Animation::binding_for_id(const ID &animated_id)
{
  const Binding *binding = const_cast<const Animation *>(this)->binding_for_id(animated_id);
  return const_cast<Binding *>(binding);
}

const Binding *Animation::binding_for_id(const ID &animated_id) const
{
  const AnimData *adt = BKE_animdata_from_id(&animated_id);

  /* Note that there is no check that `adt->animation` is actually `this`. */

  const Binding *binding = this->binding_for_handle(adt->binding_handle);
  if (!binding) {
    return nullptr;
  }
  if (!binding->is_suitable_for(animated_id)) {
    return nullptr;
  }
  return binding;
}

Binding &Animation::binding_allocate()
{
  Binding &binding = MEM_new<AnimationBinding>(__func__)->wrap();
  this->last_binding_handle++;
  BLI_assert_msg(this->last_binding_handle > 0, "Animation Binding handle overflow");
  binding.handle = this->last_binding_handle;
  return binding;
}

Binding &Animation::binding_add()
{
  Binding &binding = this->binding_allocate();

  /* Append the Binding to the animation data-block. */
  grow_array_and_append<::AnimationBinding *>(
      &this->binding_array, &this->binding_array_num, &binding);

  return binding;
}

Binding *Animation::find_suitable_binding_for(const ID &animated_id)
{
  AnimData *adt = BKE_animdata_from_id(&animated_id);

  /* The binding handle is only valid when this animation has already been
   * assigned. Otherwise it's meaningless. */
  if (adt && adt->animation == this) {
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

bool Animation::is_binding_animated(const binding_handle_t binding_handle) const
{
  if (binding_handle == Binding::unassigned) {
    return false;
  }

  Span<const FCurve *> fcurves = fcurves_for_animation(*this, binding_handle);
  return !fcurves.is_empty();
}

void Animation::free_data()
{
  /* Free layers. */
  for (Layer *layer : this->layers()) {
    MEM_delete(layer);
  }
  MEM_SAFE_FREE(this->layer_array);
  this->layer_array_num = 0;

  /* Free bindings. */
  for (Binding *binding : this->bindings()) {
    MEM_delete(binding);
  }
  MEM_SAFE_FREE(this->binding_array);
  this->binding_array_num = 0;
}

bool Animation::assign_id(Binding *binding, ID &animated_id)
{
  AnimData *adt = BKE_animdata_ensure_id(&animated_id);
  if (!adt) {
    return false;
  }

  if (adt->animation) {
    /* Unassign the ID from its existing animation first, or use the top-level
     * function `assign_animation(anim, ID)`. */
    return false;
  }

  if (binding) {
    if (!binding->connect_id(animated_id)) {
      return false;
    }

    /* If the binding is not yet named, use the ID name. */
    if (binding->name[0] == '\0') {
      this->binding_name_define(*binding, animated_id.name);
    }
    /* Always make sure the ID's binding name matches the assigned binding. */
    STRNCPY_UTF8(adt->binding_name, binding->name);
  }
  else {
    adt->binding_handle = Binding::unassigned;
    /* Keep adt->binding_name untouched, as A) it's not necessary to erase it
     * because `adt->binding_handle = 0` already indicates "no binding yet",
     * and B) it would erase information that can later be used when trying to
     * identify which binding this was once attached to.  */
  }

  adt->animation = this;
  id_us_plus(&this->id);

  return true;
}

void Animation::unassign_id(ID &animated_id)
{
  AnimData *adt = BKE_animdata_from_id(&animated_id);
  BLI_assert_msg(adt, "ID is not animated at all");
  BLI_assert_msg(adt->animation == this, "ID is not assigned to this Animation");

  /* Before unassigning, make sure that the stored Binding name is up to date. The binding name
   * might have changed in a way that wasn't copied into the ADT yet (for example when the
   * Animation data-block is linked from another file), so better copy the name to be sure that it
   * can be transparently reassigned later.
   *
   * TODO: Replace this with a BLI_assert() that the name is as expected, and "simply" ensure this
   * name is always correct. */
  const Binding *binding = this->binding_for_handle(adt->binding_handle);
  if (binding) {
    STRNCPY_UTF8(adt->binding_name, binding->name);
  }

  id_us_min(&this->id);
  adt->animation = nullptr;
}

/* ----- AnimationLayer implementation ----------- */

Layer::Layer(const Layer &other)
{
  memcpy(this, &other, sizeof(*this));

  /* Strips. */
  this->strip_array = MEM_cnew_array<AnimationStrip *>(other.strip_array_num, __func__);
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
  Strip &strip = animationstrip_alloc_infinite(strip_type);

  /* Add the new strip to the strip array. */
  grow_array_and_append<::AnimationStrip *>(&this->strip_array, &this->strip_array_num, &strip);

  return strip;
}

static void strip_ptr_destructor(AnimationStrip **dna_strip_ptr)
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

/* ----- AnimationBinding implementation ----------- */
bool Binding::connect_id(ID &animated_id)
{
  if (!this->is_suitable_for(animated_id)) {
    return false;
  }

  AnimData *adt = BKE_animdata_ensure_id(&animated_id);
  if (!adt) {
    return false;
  }

  if (this->idtype == 0) {
    this->idtype = GS(animated_id.name);
  }

  adt->binding_handle = this->handle;
  return true;
}

bool Binding::is_suitable_for(const ID &animated_id) const
{
  /* Check that the ID type is compatible with this binding. */
  const int animated_idtype = GS(animated_id.name);
  return this->idtype == 0 || this->idtype == animated_idtype;
}

bool assign_animation(Animation &anim, ID &animated_id)
{
  unassign_animation(animated_id);

  Binding *binding = anim.find_suitable_binding_for(animated_id);
  return anim.assign_id(binding, animated_id);
}

void unassign_animation(ID &animated_id)
{
  Animation *anim = get_animation(animated_id);
  if (!anim) {
    return;
  }
  anim->unassign_id(animated_id);
}

Animation *get_animation(ID &animated_id)
{
  AnimData *adt = BKE_animdata_from_id(&animated_id);
  if (!adt) {
    return nullptr;
  }
  if (!adt->animation) {
    return nullptr;
  }
  return &adt->animation->wrap();
}

/* ----- AnimationStrip implementation ----------- */

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

/* ----- KeyframeAnimationStrip implementation ----------- */

KeyframeStrip::KeyframeStrip(const KeyframeStrip &other)
{
  memcpy(this, &other, sizeof(*this));

  this->channelbags_array = MEM_cnew_array<AnimationChannelBag *>(other.channelbags_array_num,
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
  return this->type() == Type::Keyframe;
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

  ChannelBag &channels = MEM_new<AnimationChannelBag>(__func__)->wrap();
  channels.binding_handle = binding.handle;

  grow_array_and_append<AnimationChannelBag *>(
      &this->channelbags_array, &this->channelbags_array_num, &channels);

  return channels;
}

FCurve *KeyframeStrip::fcurve_find(const Binding &binding,
                                   const StringRefNull rna_path,
                                   const int array_index)
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
    if (fcu->array_index == array_index && fcu->rna_path && StringRef(fcu->rna_path) == rna_path) {
      return fcu;
    }
  }
  return nullptr;
}

FCurve &KeyframeStrip::fcurve_find_or_create(const Binding &binding,
                                             const StringRefNull rna_path,
                                             const int array_index)
{
  if (FCurve *existing_fcurve = this->fcurve_find(binding, rna_path, array_index)) {
    return *existing_fcurve;
  }

  FCurve *new_fcurve = create_fcurve_for_channel(rna_path.c_str(), array_index);

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

FCurve *KeyframeStrip::keyframe_insert(const Binding &binding,
                                       const StringRefNull rna_path,
                                       const int array_index,
                                       const float2 time_value,
                                       const KeyframeSettings &settings)
{
  FCurve &fcurve = this->fcurve_find_or_create(binding, rna_path, array_index);

  if (!BKE_fcurve_is_keyframable(&fcurve)) {
    /* TODO: handle this properly, in a way that can be communicated to the user. */
    std::fprintf(stderr,
                 "FCurve %s[%d] for binding %s doesn't allow inserting keys.\n",
                 rna_path.c_str(),
                 array_index,
                 binding.name);
    return nullptr;
  }

  /* TODO: Handle the eInsertKeyFlags. */
  const int index = insert_vert_fcurve(&fcurve, time_value, settings, eInsertKeyFlags(0));
  if (index < 0) {
    std::fprintf(stderr,
                 "Could not insert key into FCurve %s[%d] for binding %s.\n",
                 rna_path.c_str(),
                 array_index,
                 binding.name);
    return nullptr;
  }

  return &fcurve;
}

/* AnimationChannelBag implementation. */

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

static const animrig::ChannelBag *channelbag_for_animation(const Animation &anim,
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

static animrig::ChannelBag *channelbag_for_animation(Animation &anim,
                                                     const binding_handle_t binding_handle)
{
  const animrig::ChannelBag *const_bag = channelbag_for_animation(
      const_cast<const Animation &>(anim), binding_handle);
  return const_cast<animrig::ChannelBag *>(const_bag);
}

Span<FCurve *> fcurves_for_animation(Animation &anim, const binding_handle_t binding_handle)
{
  animrig::ChannelBag *bag = channelbag_for_animation(anim, binding_handle);
  if (!bag) {
    return {};
  }
  return bag->fcurves();
}

Span<const FCurve *> fcurves_for_animation(const Animation &anim,
                                           const binding_handle_t binding_handle)
{
  const animrig::ChannelBag *bag = channelbag_for_animation(anim, binding_handle);
  if (!bag) {
    return {};
  }
  return bag->fcurves();
}

}  // namespace blender::animrig
