/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include <cstdlib>

#include "DNA_anim_types.h"

#include "ANIM_animation.hh"

#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "MEM_guardedalloc.h"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"

#include "rna_internal.hh"

#include "WM_api.hh"
#include "WM_types.hh"

using namespace blender;

const EnumPropertyItem rna_enum_layer_mix_mode_items[] = {
    {int(animrig::Layer::MixMode::Replace),
     "REPLACE",
     0,
     "Replace",
     "Channels in this layer override the same channels from underlying layers"},
    {int(animrig::Layer::MixMode::Offset),
     "OFFSET",
     0,
     "Offset",
     "Channels in this layer are added to underlying layers as sequential operations"},
    {int(animrig::Layer::MixMode::Add),
     "ADD",
     0,
     "Add",
     "Channels in this layer are added to underlying layers on a per-channel basis"},
    {int(animrig::Layer::MixMode::Subtract),
     "SUBTRACT",
     0,
     "Subtract",
     "Channels in this layer are subtracted to underlying layers on a per-channel basis"},
    {int(animrig::Layer::MixMode::Multiply),
     "MULTIPLY",
     0,
     "Multiply",
     "Channels in this layer are multiplied with underlying layers on a per-channel basis"},
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_strip_type_items[] = {
    {int(animrig::Strip::Type::Keyframe),
     "KEYFRAME",
     0,
     "Keyframe",
     "Strip containing keyframes on F-Curves"},
    {0, nullptr, 0, nullptr, nullptr},
};

#ifdef RNA_RUNTIME

#  include "ANIM_animation.hh"

#  include "DEG_depsgraph.hh"

#  include <fmt/format.h>

static animrig::Animation &rna_animation(const PointerRNA *ptr)
{
  return reinterpret_cast<Animation *>(ptr->owner_id)->wrap();
}

static animrig::Binding &rna_data_binding(const PointerRNA *ptr)
{
  return reinterpret_cast<AnimationBinding *>(ptr->data)->wrap();
}

static animrig::Layer &rna_data_layer(const PointerRNA *ptr)
{
  return reinterpret_cast<AnimationLayer *>(ptr->data)->wrap();
}

static animrig::Strip &rna_data_strip(const PointerRNA *ptr)
{
  return reinterpret_cast<AnimationStrip *>(ptr->data)->wrap();
}

static void rna_Animation_tag_animupdate(Main *, Scene *, PointerRNA *ptr)
{
  animrig::Animation &anim = rna_animation(ptr);
  DEG_id_tag_update(&anim.id, ID_RECALC_ANIMATION);
}

static animrig::KeyframeStrip &rna_data_keyframe_strip(const PointerRNA *ptr)
{
  animrig::Strip &strip = reinterpret_cast<AnimationStrip *>(ptr->data)->wrap();
  return strip.as<animrig::KeyframeStrip>();
}

static animrig::ChannelBag &rna_data_channelbag(const PointerRNA *ptr)
{
  return reinterpret_cast<AnimationChannelBag *>(ptr->data)->wrap();
}

template<typename T>
static void rna_iterator_array_begin(CollectionPropertyIterator *iter, Span<T *> items)
{
  rna_iterator_array_begin(iter, (void *)items.data(), sizeof(T *), items.size(), 0, nullptr);
}

template<typename T>
static void rna_iterator_array_begin(CollectionPropertyIterator *iter, MutableSpan<T *> items)
{
  rna_iterator_array_begin(iter, (void *)items.data(), sizeof(T *), items.size(), 0, nullptr);
}

static AnimationBinding *rna_Animation_bindings_new(Animation *anim_id,
                                                    bContext *C,
                                                    ReportList *reports,
                                                    ID *animated_id)
{
  if (animated_id == nullptr) {
    BKE_report(reports,
               RPT_ERROR,
               "A binding without animated ID cannot be created at the moment; if you need it, "
               "please file a bug report");
    return nullptr;
  }

  animrig::Animation &anim = anim_id->wrap();
  animrig::Binding &binding = anim.binding_add();
  /* TODO: actually set binding->idtype to this ID's type. */
  anim.binding_name_define(binding, animated_id->name);

  WM_event_add_notifier(C, NC_ANIMATION | ND_ANIMCHAN, nullptr);
  return &binding;
}

static void rna_iterator_animation_layers_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  animrig::Animation &anim = rna_animation(ptr);
  rna_iterator_array_begin(iter, anim.layers());
}

static int rna_iterator_animation_layers_length(PointerRNA *ptr)
{
  animrig::Animation &anim = rna_animation(ptr);
  return anim.layers().size();
}

static AnimationLayer *rna_Animation_layers_new(Animation *dna_animation,
                                                bContext *C,
                                                ReportList *reports,
                                                const char *name)
{
  animrig::Animation &anim = dna_animation->wrap();

  if (anim.layers().size() >= 1) {
    /* Not allowed to have more than one layer, for now. This limitation is in
     * place until working with multiple animated IDs is fleshed out better. */
    BKE_report(reports, RPT_ERROR, "An Animation may not have more than one layer");
    return nullptr;
  }

  animrig::Layer &layer = anim.layer_add(name);

  WM_event_add_notifier(C, NC_ANIMATION | ND_ANIMCHAN, nullptr);
  return &layer;
}

void rna_Animation_layers_remove(Animation *dna_animation,
                                 bContext *C,
                                 ReportList *reports,
                                 AnimationLayer *dna_layer)
{
  animrig::Animation &anim = dna_animation->wrap();
  animrig::Layer &layer = dna_layer->wrap();
  if (!anim.layer_remove(layer)) {
    BKE_report(reports, RPT_ERROR, "This layer does not belong to this animation");
    return;
  }

  WM_event_add_notifier(C, NC_ANIMATION | ND_ANIMCHAN, nullptr);
  DEG_id_tag_update(&anim.id, ID_RECALC_ANIMATION);
}

static void rna_iterator_animation_bindings_begin(CollectionPropertyIterator *iter,
                                                  PointerRNA *ptr)
{
  animrig::Animation &anim = rna_animation(ptr);
  rna_iterator_array_begin(iter, anim.bindings());
}

static int rna_iterator_animation_bindings_length(PointerRNA *ptr)
{
  animrig::Animation &anim = rna_animation(ptr);
  return anim.bindings().size();
}

static std::optional<std::string> rna_AnimationBinding_path(const PointerRNA *ptr)
{
  animrig::Binding &binding = rna_data_binding(ptr);

  char name_esc[sizeof(binding.name) * 2];
  BLI_str_escape(name_esc, binding.name, sizeof(name_esc));
  return fmt::format("bindings[\"{}\"]", name_esc);
}

static void rna_AnimationBinding_name_set(PointerRNA *ptr, const char *name)
{
  animrig::Animation &anim = rna_animation(ptr);
  animrig::Binding &binding = rna_data_binding(ptr);

  anim.binding_name_define(binding, name);
}

static void rna_AnimationBinding_name_update(Main *bmain, Scene *, PointerRNA *ptr)
{
  animrig::Animation &anim = rna_animation(ptr);
  animrig::Binding &binding = rna_data_binding(ptr);

  anim.binding_name_propagate(*bmain, binding);
}

static std::optional<std::string> rna_AnimationLayer_path(const PointerRNA *ptr)
{
  animrig::Layer &layer = rna_data_layer(ptr);

  char name_esc[sizeof(layer.name) * 2];
  BLI_str_escape(name_esc, layer.name, sizeof(name_esc));
  return fmt::format("layers[\"{}\"]", name_esc);
}

static void rna_iterator_animationlayer_strips_begin(CollectionPropertyIterator *iter,
                                                     PointerRNA *ptr)
{
  animrig::Layer &layer = rna_data_layer(ptr);
  rna_iterator_array_begin(iter, layer.strips());
}

static int rna_iterator_animationlayer_strips_length(PointerRNA *ptr)
{
  animrig::Layer &layer = rna_data_layer(ptr);
  return layer.strips().size();
}

AnimationStrip *rna_AnimationStrips_new(AnimationLayer *dna_layer,
                                        bContext *C,
                                        ReportList *reports,
                                        const int type)
{
  const animrig::Strip::Type strip_type = animrig::Strip::Type(type);

  animrig::Layer &layer = dna_layer->wrap();

  if (layer.strips().size() >= 1) {
    /* Not allowed to have more than one strip, for now. This limitation is in
     * place until working with layers is fleshed out better. */
    BKE_report(reports, RPT_ERROR, "A layer may not have more than one strip");
    return nullptr;
  }

  animrig::Strip &strip = layer.strip_add(strip_type);

  WM_event_add_notifier(C, NC_ANIMATION | ND_ANIMCHAN, nullptr);
  return &strip;
}

void rna_AnimationStrips_remove(ID *animation_id,
                                AnimationLayer *dna_layer,
                                bContext *C,
                                ReportList *reports,
                                AnimationStrip *dna_strip)
{
  animrig::Layer &layer = dna_layer->wrap();
  animrig::Strip &strip = dna_strip->wrap();
  if (!layer.strip_remove(strip)) {
    BKE_report(reports, RPT_ERROR, "This strip does not belong to this layer");
    return;
  }

  WM_event_add_notifier(C, NC_ANIMATION | ND_ANIMCHAN, nullptr);
  DEG_id_tag_update(animation_id, ID_RECALC_ANIMATION);
}

static StructRNA *rna_AnimationStrip_refine(PointerRNA *ptr)
{
  animrig::Strip &strip = rna_data_strip(ptr);
  switch (strip.type()) {
    case animrig::Strip::Type::Keyframe:
      return &RNA_KeyframeAnimationStrip;
  }
  return &RNA_UnknownType;
}

static std::optional<std::string> rna_AnimationStrip_path(const PointerRNA *ptr)
{
  animrig::Animation &anim = rna_animation(ptr);
  animrig::Strip &strip_to_find = rna_data_strip(ptr);

  for (animrig::Layer *layer : anim.layers()) {
    Span<animrig::Strip *> strips = layer->strips();
    const int index = strips.first_index_try(&strip_to_find);
    if (index < 0) {
      continue;
    }

    PointerRNA layer_ptr = RNA_pointer_create(&anim.id, &RNA_AnimationLayer, layer);
    const std::optional<std::string> layer_path = rna_AnimationLayer_path(&layer_ptr);
    BLI_assert_msg(layer_path, "Every animation layer should have a valid RNA path.");
    const std::string strip_path = fmt::format("{}.strips[{}]", *layer_path, index);
    return strip_path;
  }

  return std::nullopt;
}

static void rna_iterator_keyframestrip_channelbags_begin(CollectionPropertyIterator *iter,
                                                         PointerRNA *ptr)
{
  animrig::KeyframeStrip &key_strip = rna_data_keyframe_strip(ptr);
  rna_iterator_array_begin(iter, key_strip.channelbags());
}

static int rna_iterator_keyframestrip_channelbags_length(PointerRNA *ptr)
{
  animrig::KeyframeStrip &key_strip = rna_data_keyframe_strip(ptr);
  return key_strip.channelbags().size();
}

static void rna_iterator_ChannelBag_fcurves_begin(CollectionPropertyIterator *iter,
                                                  PointerRNA *ptr)
{
  animrig::ChannelBag &bag = rna_data_channelbag(ptr);
  rna_iterator_array_begin(iter, bag.fcurves());
}

static int rna_iterator_ChannelBag_fcurves_length(PointerRNA *ptr)
{
  animrig::ChannelBag &bag = rna_data_channelbag(ptr);
  return bag.fcurves().size();
}

static AnimationChannelBag *rna_KeyframeAnimationStrip_channels(
    KeyframeAnimationStrip *self, const animrig::binding_handle_t binding_handle)
{
  animrig::KeyframeStrip &key_strip = self->wrap();
  return key_strip.channelbag_for_binding(binding_handle);
}

#else

static void rna_def_animation_bindings(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;

  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "AnimationBindings");
  srna = RNA_def_struct(brna, "AnimationBindings", nullptr);
  RNA_def_struct_sdna(srna, "Animation");
  RNA_def_struct_ui_text(srna, "Animation Bindings", "Collection of animation bindings");

  /* Animation.bindings.new(...) */
  func = RNA_def_function(srna, "new", "rna_Animation_bindings_new");
  RNA_def_function_ui_description(func, "Add a binding to the animation");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT | FUNC_USE_REPORTS);
  parm = RNA_def_pointer(
      func, "animated_id", "ID", "Data-Block", "Data-block that will be animated by this binding");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_pointer(
      func, "binding", "AnimationBinding", "", "Newly created animation binding");
  RNA_def_function_return(func, parm);
}

static void rna_def_animation_layers(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;

  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "AnimationLayers");
  srna = RNA_def_struct(brna, "AnimationLayers", nullptr);
  RNA_def_struct_sdna(srna, "Animation");
  RNA_def_struct_ui_text(srna, "Animation Layers", "Collection of animation layers");

  /* Animation.layers.new(...) */
  func = RNA_def_function(srna, "new", "rna_Animation_layers_new");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT | FUNC_USE_REPORTS);
  RNA_def_function_ui_description(
      func,
      "Add a layer to the Animation. Currently an Animation can only have at most one layer");
  parm = RNA_def_string(func,
                        "name",
                        nullptr,
                        sizeof(AnimationLayer::name) - 1,
                        "Name",
                        "Name of the layer, will be made unique within the Animation data-block");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_pointer(func, "layer", "AnimationLayer", "", "Newly created animation layer");
  RNA_def_function_return(func, parm);

  /* Animation.layers.remove(layer) */
  func = RNA_def_function(srna, "remove", "rna_Animation_layers_remove");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT | FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "Remove the layer from the animation");
  parm = RNA_def_pointer(
      func, "anim_layer", "AnimationLayer", "Animation Layer", "The layer to remove");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
}

static void rna_def_animation(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "Animation", "ID");
  RNA_def_struct_sdna(srna, "Animation");
  RNA_def_struct_ui_text(srna, "Animation", "A collection of animation layers");
  RNA_def_struct_ui_icon(srna, ICON_ACTION);

  prop = RNA_def_property(srna, "last_binding_handle", PROP_INT, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  /* Collection properties .*/
  prop = RNA_def_property(srna, "bindings", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "AnimationBinding");
  RNA_def_property_collection_funcs(prop,
                                    "rna_iterator_animation_bindings_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_dereference_get",
                                    "rna_iterator_animation_bindings_length",
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_ui_text(prop, "Bindings", "The list of bindings in this animation data-block");
  rna_def_animation_bindings(brna, prop);

  prop = RNA_def_property(srna, "layers", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "AnimationLayer");
  RNA_def_property_collection_funcs(prop,
                                    "rna_iterator_animation_layers_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_dereference_get",
                                    "rna_iterator_animation_layers_length",
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_ui_text(prop, "Layers", "The list of layers that make up this Animation");
  rna_def_animation_layers(brna, prop);
}

static void rna_def_animation_binding(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "AnimationBinding", nullptr);
  RNA_def_struct_path_func(srna, "rna_AnimationBinding_path");
  RNA_def_struct_ui_text(
      srna,
      "Animation Binding",
      "Identifier for a set of channels in this Animation, that can be used by a data-block "
      "to specify what it gets animated by");

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_struct_name_property(srna, prop);
  RNA_def_property_string_funcs(prop, nullptr, nullptr, "rna_AnimationBinding_name_set");
  RNA_def_property_update(prop, NC_ANIMATION | ND_ANIMCHAN, "rna_AnimationBinding_name_update");
  RNA_def_struct_ui_text(
      srna,
      "Binding Name",
      "Used when connecting an Animation to a data-block, to find the correct binding handle");

  prop = RNA_def_property(srna, "handle", PROP_INT, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_struct_ui_text(srna,
                         "Binding Handle",
                         "Number specific to this Binding, unique within the Animation data-block"
                         "This is used, for example, on a KeyframeAnimationStrip to look up the "
                         "AnimationChannelBag for this Binding");
}

static void rna_def_animationlayer_strips(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;

  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "AnimationStrips");
  srna = RNA_def_struct(brna, "AnimationStrips", nullptr);
  RNA_def_struct_sdna(srna, "AnimationLayer");
  RNA_def_struct_ui_text(srna, "Animation Strips", "Collection of animation strips");

  /* Layer.strips.new(type='...') */
  func = RNA_def_function(srna, "new", "rna_AnimationStrips_new");
  RNA_def_function_ui_description(func,
                                  "Add a new strip to the layer. Currently a layer can only have "
                                  "one strip, with infinite boundaries");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT | FUNC_USE_REPORTS);
  parm = RNA_def_enum(func,
                      "type",
                      rna_enum_strip_type_items,
                      int(animrig::Strip::Type::Keyframe),
                      "Type",
                      "The type of strip to create");
  /* Return value. */
  parm = RNA_def_pointer(func, "strip", "AnimationStrip", "", "Newly created animation strip");
  RNA_def_function_return(func, parm);

  /* Layer.strips.remove(strip) */
  func = RNA_def_function(srna, "remove", "rna_AnimationStrips_remove");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_CONTEXT | FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "Remove the strip from the animation layer");
  parm = RNA_def_pointer(
      func, "anim_strip", "AnimationStrip", "Animation Strip", "The strip to remove");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
}

static void rna_def_animation_layer(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "AnimationLayer", nullptr);
  RNA_def_struct_ui_text(srna, "Animation Layer", "");
  RNA_def_struct_path_func(srna, "rna_AnimationLayer_path");

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_struct_name_property(srna, prop);

  prop = RNA_def_property(srna, "influence", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(
      prop, "Influence", "How much of this layer is used when blending into the lower layers");
  RNA_def_property_ui_range(prop, 0.0, 1.0, 3, 2);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_update(prop, NC_ANIMATION | ND_ANIMCHAN, "rna_Animation_tag_animupdate");

  prop = RNA_def_property(srna, "mix_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "layer_mix_mode");
  RNA_def_property_ui_text(
      prop, "Mix Mode", "How animation of this layer is blended into the lower layers");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_enum_items(prop, rna_enum_layer_mix_mode_items);
  RNA_def_property_update(prop, NC_ANIMATION | ND_ANIMCHAN, "rna_Animation_tag_animupdate");

  /* Collection properties .*/
  prop = RNA_def_property(srna, "strips", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "AnimationStrip");
  RNA_def_property_collection_funcs(prop,
                                    "rna_iterator_animationlayer_strips_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_dereference_get",
                                    "rna_iterator_animationlayer_strips_length",
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_ui_text(prop, "Strips", "The list of strips that are on this animation layer");

  rna_def_animationlayer_strips(brna, prop);
}

static void rna_def_keyframestrip_channelbags(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;

  RNA_def_property_srna(cprop, "AnimationChannelBags");
  srna = RNA_def_struct(brna, "AnimationChannelBags", nullptr);
  RNA_def_struct_sdna(srna, "KeyframeAnimationStrip");
  RNA_def_struct_ui_text(
      srna,
      "Animation Channels for Bindings",
      "For each animation binding, a list of animation channels that are meant for that binding");
}

static void rna_def_animation_keyframe_strip(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "KeyframeAnimationStrip", "AnimationStrip");
  RNA_def_struct_ui_text(
      srna, "Keyframe Animation Strip", "Strip with a set of F-Curves for each animation binding");

  prop = RNA_def_property(srna, "channelbags", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "AnimationChannelBag");
  RNA_def_property_collection_funcs(prop,
                                    "rna_iterator_keyframestrip_channelbags_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_dereference_get",
                                    "rna_iterator_keyframestrip_channelbags_length",
                                    nullptr,
                                    nullptr,
                                    nullptr);
  rna_def_keyframestrip_channelbags(brna, prop);

  {
    FunctionRNA *func;
    PropertyRNA *parm;

    /* KeyframeStrip.channels(...). */
    func = RNA_def_function(srna, "channels", "rna_KeyframeAnimationStrip_channels");
    RNA_def_function_ui_description(func, "Find the AnimationChannelBag for a specific Binding");
    parm = RNA_def_int(func,
                       "binding_handle",
                       0,
                       0,
                       INT_MAX,
                       "Binding Handle",
                       "Number that identifies a specific animation binding",
                       0,
                       INT_MAX);
    RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
    parm = RNA_def_pointer(func, "channels", "AnimationChannelBag", "Channels", "");
    RNA_def_function_return(func, parm);
  }
}

static void rna_def_animation_strip(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "AnimationStrip", nullptr);
  RNA_def_struct_ui_text(srna, "Animation Strip", "");
  RNA_def_struct_path_func(srna, "rna_AnimationStrip_path");
  RNA_def_struct_refine_func(srna, "rna_AnimationStrip_refine");

  static const EnumPropertyItem prop_type_items[] = {
      {int(animrig::Strip::Type::Keyframe),
       "KEYFRAME",
       0,
       "Keyframe",
       "Strip with a set of F-Curves for each animation binding"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "strip_type");
  RNA_def_property_enum_items(prop, prop_type_items);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  /* Define Strip subclasses. */
  rna_def_animation_keyframe_strip(brna);
}

static void rna_def_channelbag_for_binding_fcurves(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;

  RNA_def_property_srna(cprop, "AnimationChannelBagFCurves");
  srna = RNA_def_struct(brna, "AnimationChannelBagFCurves", nullptr);
  RNA_def_struct_sdna(srna, "bAnimationChannelBag");
  RNA_def_struct_ui_text(
      srna, "F-Curves", "Collection of F-Curves for a specific animation binding");
}

static void rna_def_animation_channelbag(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "AnimationChannelBag", nullptr);
  RNA_def_struct_ui_text(
      srna,
      "Animation Channel Bag",
      "Collection of animation channels, typically associated with an animation binding");

  prop = RNA_def_property(srna, "binding_handle", PROP_INT, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "fcurves", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_funcs(prop,
                                    "rna_iterator_ChannelBag_fcurves_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_dereference_get",
                                    "rna_iterator_ChannelBag_fcurves_length",
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_struct_type(prop, "FCurve");
  RNA_def_property_ui_text(prop, "F-Curves", "The individual F-Curves that animate the binding");
  rna_def_channelbag_for_binding_fcurves(brna, prop);
}

void RNA_def_animation_id(BlenderRNA *brna)
{
  rna_def_animation(brna);
  rna_def_animation_binding(brna);
  rna_def_animation_layer(brna);
  rna_def_animation_strip(brna);
  rna_def_animation_channelbag(brna);
}

#endif
