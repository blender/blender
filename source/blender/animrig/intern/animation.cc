/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_anim_defaults.h"
#include "DNA_anim_types.h"
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

/* ----- AnimationBinding implementation ----------- */

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

}  // namespace blender::animrig
