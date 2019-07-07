/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 */

#include <vector>
#include <map>
#include <algorithm>  // std::find

#include "ExportSettings.h"
#include "BCAnimationCurve.h"
#include "BCAnimationSampler.h"
#include "collada_utils.h"
#include "BCMath.h"

extern "C" {
#include "BKE_action.h"
#include "BKE_constraint.h"
#include "BKE_key.h"
#include "BKE_main.h"
#include "BKE_library.h"
#include "BKE_material.h"
#include "BLI_listbase.h"
#include "DNA_anim_types.h"
#include "DNA_scene_types.h"
#include "DNA_key_types.h"
#include "DNA_constraint_types.h"
#include "ED_object.h"
}

static std::string EMPTY_STRING;
static BCAnimationCurveMap BCEmptyAnimationCurves;

BCAnimationSampler::BCAnimationSampler(BCExportSettings &export_settings, BCObjectSet &object_set)
    : export_settings(export_settings)
{
  BCObjectSet::iterator it;
  for (it = object_set.begin(); it != object_set.end(); ++it) {
    Object *ob = *it;
    add_object(ob);
  }
}

BCAnimationSampler::~BCAnimationSampler()
{
  BCAnimationObjectMap::iterator it;
  for (it = objects.begin(); it != objects.end(); ++it) {
    BCAnimation *animation = it->second;
    delete animation;
  }
}

void BCAnimationSampler::add_object(Object *ob)
{
  BlenderContext blender_context = export_settings.get_blender_context();
  BCAnimation *animation = new BCAnimation(blender_context.get_context(), ob);
  objects[ob] = animation;

  initialize_keyframes(animation->frame_set, ob);
  initialize_curves(animation->curve_map, ob);
}

BCAnimationCurveMap *BCAnimationSampler::get_curves(Object *ob)
{
  BCAnimation &animation = *objects[ob];
  if (animation.curve_map.size() == 0) {
    initialize_curves(animation.curve_map, ob);
  }
  return &animation.curve_map;
}

static void get_sample_frames(BCFrameSet &sample_frames,
                              int sampling_rate,
                              bool keyframe_at_end,
                              Scene *scene)
{
  sample_frames.clear();

  if (sampling_rate < 1) {
    return;  // no sample frames in this case
  }

  float sfra = scene->r.sfra;
  float efra = scene->r.efra;

  int frame_index;
  for (frame_index = nearbyint(sfra); frame_index < efra; frame_index += sampling_rate) {
    sample_frames.insert(frame_index);
  }

  if (frame_index >= efra && keyframe_at_end) {
    sample_frames.insert(efra);
  }
}

static bool is_object_keyframe(Object *ob, int frame_index)
{
  return false;
}

static void add_keyframes_from(bAction *action, BCFrameSet &frameset)
{
  if (action) {
    FCurve *fcu = NULL;
    for (fcu = (FCurve *)action->curves.first; fcu; fcu = fcu->next) {
      BezTriple *bezt = fcu->bezt;
      for (int i = 0; i < fcu->totvert; bezt++, i++) {
        int frame_index = nearbyint(bezt->vec[1][0]);
        frameset.insert(frame_index);
      }
    }
  }
}

void BCAnimationSampler::check_property_is_animated(
    BCAnimation &animation, float *ref, float *val, std::string data_path, int length)
{
  for (int array_index = 0; array_index < length; ++array_index) {
    if (!bc_in_range(ref[length], val[length], 0.00001)) {
      BCCurveKey key(BC_ANIMATION_TYPE_OBJECT, data_path, array_index);
      BCAnimationCurveMap::iterator it = animation.curve_map.find(key);
      if (it == animation.curve_map.end()) {
        animation.curve_map[key] = new BCAnimationCurve(key, animation.get_reference());
      }
    }
  }
}

void BCAnimationSampler::update_animation_curves(BCAnimation &animation,
                                                 BCSample &sample,
                                                 Object *ob,
                                                 int frame)
{
  BCAnimationCurveMap::iterator it;
  for (it = animation.curve_map.begin(); it != animation.curve_map.end(); ++it) {
    BCAnimationCurve *curve = it->second;
    if (curve->is_transform_curve()) {
      curve->add_value_from_matrix(sample, frame);
    }
    else {
      curve->add_value_from_rna(frame);
    }
  }
}

BCSample &BCAnimationSampler::sample_object(Object *ob, int frame_index, bool for_opensim)
{
  BCSample &ob_sample = sample_data.add(ob, frame_index);
  // if (export_settings.get_apply_global_orientation()) {
  //  const BCMatrix &global_transform = export_settings.get_global_transform();
  //  ob_sample.get_matrix(global_transform);
  //}

  if (ob->type == OB_ARMATURE) {
    bPoseChannel *pchan;
    for (pchan = (bPoseChannel *)ob->pose->chanbase.first; pchan; pchan = pchan->next) {
      Bone *bone = pchan->bone;
      Matrix bmat;
      if (bc_bone_matrix_local_get(ob, bone, bmat, for_opensim)) {

        ob_sample.add_bone_matrix(bone, bmat);
      }
    }
  }
  return ob_sample;
}

void BCAnimationSampler::sample_scene(BCExportSettings &export_settings, bool keyframe_at_end)
{
  BlenderContext blender_context = export_settings.get_blender_context();
  int sampling_rate = export_settings.get_sampling_rate();
  bool for_opensim = export_settings.get_open_sim();
  bool keep_keyframes = export_settings.get_keep_keyframes();
  BC_export_animation_type export_animation_type = export_settings.get_export_animation_type();

  Scene *scene = blender_context.get_scene();
  BCFrameSet scene_sample_frames;
  get_sample_frames(scene_sample_frames, sampling_rate, keyframe_at_end, scene);
  BCFrameSet::iterator it;

  int startframe = scene->r.sfra;
  int endframe = scene->r.efra;

  for (int frame_index = startframe; frame_index <= endframe; ++frame_index) {
    /* Loop over all frames and decide for each frame if sampling is necessary */
    bool is_scene_sample_frame = false;
    bool needs_update = true;
    if (scene_sample_frames.find(frame_index) != scene_sample_frames.end()) {
      bc_update_scene(blender_context, frame_index);
      needs_update = false;
      is_scene_sample_frame = true;
    }

    bool needs_sampling = is_scene_sample_frame || keep_keyframes ||
                          export_animation_type == BC_ANIMATION_EXPORT_KEYS;
    if (!needs_sampling) {
      continue;
    }

    BCAnimationObjectMap::iterator obit;
    for (obit = objects.begin(); obit != objects.end(); ++obit) {
      Object *ob = obit->first;
      BCAnimation *animation = obit->second;
      BCFrameSet &object_keyframes = animation->frame_set;
      if (is_scene_sample_frame || object_keyframes.find(frame_index) != object_keyframes.end()) {

        if (needs_update) {
          bc_update_scene(blender_context, frame_index);
          needs_update = false;
        }

        BCSample &sample = sample_object(ob, frame_index, for_opensim);
        update_animation_curves(*animation, sample, ob, frame_index);
      }
    }
  }
}

bool BCAnimationSampler::is_animated_by_constraint(Object *ob,
                                                   ListBase *conlist,
                                                   std::set<Object *> &animated_objects)
{
  bConstraint *con;
  for (con = (bConstraint *)conlist->first; con; con = con->next) {
    ListBase targets = {NULL, NULL};

    const bConstraintTypeInfo *cti = BKE_constraint_typeinfo_get(con);

    if (!bc_validateConstraints(con)) {
      continue;
    }

    if (cti && cti->get_constraint_targets) {
      bConstraintTarget *ct;
      Object *obtar;
      cti->get_constraint_targets(con, &targets);
      for (ct = (bConstraintTarget *)targets.first; ct; ct = ct->next) {
        obtar = ct->tar;
        if (obtar) {
          if (animated_objects.find(obtar) != animated_objects.end()) {
            return true;
          }
        }
      }
    }
  }
  return false;
}

void BCAnimationSampler::find_depending_animated(std::set<Object *> &animated_objects,
                                                 std::set<Object *> &candidates)
{
  bool found_more;
  do {
    found_more = false;
    std::set<Object *>::iterator it;
    for (it = candidates.begin(); it != candidates.end(); ++it) {
      Object *cob = *it;
      ListBase *conlist = get_active_constraints(cob);
      if (is_animated_by_constraint(cob, conlist, animated_objects)) {
        animated_objects.insert(cob);
        candidates.erase(cob);
        found_more = true;
        break;
      }
    }
  } while (found_more && candidates.size() > 0);
}

void BCAnimationSampler::get_animated_from_export_set(std::set<Object *> &animated_objects,
                                                      LinkNode &export_set)
{
  /* Check if this object is animated. That is: Check if it has its own action, or:
   *
   * - Check if it has constraints to other objects.
   * - at least one of the other objects is animated as well.
   */

  animated_objects.clear();
  std::set<Object *> static_objects;
  std::set<Object *> candidates;

  LinkNode *node;
  for (node = &export_set; node; node = node->next) {
    Object *cob = (Object *)node->link;
    if (bc_has_animations(cob)) {
      animated_objects.insert(cob);
    }
    else {
      ListBase conlist = cob->constraints;
      if (conlist.first) {
        candidates.insert(cob);
      }
    }
  }
  find_depending_animated(animated_objects, candidates);
}

void BCAnimationSampler::get_object_frames(BCFrames &frames, Object *ob)
{
  sample_data.get_frames(ob, frames);
}

void BCAnimationSampler::get_bone_frames(BCFrames &frames, Object *ob, Bone *bone)
{
  sample_data.get_frames(ob, bone, frames);
}

bool BCAnimationSampler::get_bone_samples(BCMatrixSampleMap &samples, Object *ob, Bone *bone)
{
  sample_data.get_matrices(ob, bone, samples);
  return bc_is_animated(samples);
}

bool BCAnimationSampler::get_object_samples(BCMatrixSampleMap &samples, Object *ob)
{
  sample_data.get_matrices(ob, samples);
  return bc_is_animated(samples);
}

#if 0
/**
 * Add sampled values to #FCurve
 * If no #FCurve exists, create a temporary #FCurve;
 * \note The temporary #FCurve will later be removed when the
 * #BCAnimationSampler is removed (by its destructor).
 *
 * \param curve: The curve to which the data is added.
 * \param matrices: The set of matrix values from where the data is taken.
 * \param animation_type:
 * - #BC_ANIMATION_EXPORT_SAMPLES: Use all matrix data.
 * - #BC_ANIMATION_EXPORT_KEYS: Only take data from matrices for keyframes.
 */
void BCAnimationSampler::add_value_set(BCAnimationCurve &curve,
                                       BCFrameSampleMap &samples,
                                       BC_export_animation_type animation_type)
{
  int array_index = curve.get_array_index();
  const BC_animation_transform_type tm_type = curve.get_transform_type();

  BCFrameSampleMap::iterator it;
  for (it = samples.begin(); it != samples.end(); ++it) {
    const int frame_index = nearbyint(it->first);
    if (animation_type == BC_ANIMATION_EXPORT_SAMPLES || curve.is_keyframe(frame_index)) {

      const BCSample *sample = it->second;
      float val = 0;

      int subindex = curve.get_subindex();
      bool good;
      if (subindex == -1) {
        good = sample->get_value(tm_type, array_index, &val);
      }
      else {
        good = sample->get_value(tm_type, array_index, &val, subindex);
      }

      if (good) {
        curve.add_value(val, frame_index);
      }
    }
  }
  curve.remove_unused_keyframes();
  curve.calchandles();
}
#endif

void BCAnimationSampler::generate_transform(Object *ob,
                                            const BCCurveKey &key,
                                            BCAnimationCurveMap &curves)
{
  BCAnimationCurveMap::const_iterator it = curves.find(key);
  if (it == curves.end()) {
    curves[key] = new BCAnimationCurve(key, ob);
  }
}

void BCAnimationSampler::generate_transforms(Object *ob,
                                             const std::string prep,
                                             const BC_animation_type type,
                                             BCAnimationCurveMap &curves)
{
  generate_transform(ob, BCCurveKey(type, prep + "location", 0), curves);
  generate_transform(ob, BCCurveKey(type, prep + "location", 1), curves);
  generate_transform(ob, BCCurveKey(type, prep + "location", 2), curves);
  generate_transform(ob, BCCurveKey(type, prep + "rotation_euler", 0), curves);
  generate_transform(ob, BCCurveKey(type, prep + "rotation_euler", 1), curves);
  generate_transform(ob, BCCurveKey(type, prep + "rotation_euler", 2), curves);
  generate_transform(ob, BCCurveKey(type, prep + "scale", 0), curves);
  generate_transform(ob, BCCurveKey(type, prep + "scale", 1), curves);
  generate_transform(ob, BCCurveKey(type, prep + "scale", 2), curves);
}

void BCAnimationSampler::generate_transforms(Object *ob, Bone *bone, BCAnimationCurveMap &curves)
{
  std::string prep = "pose.bones[\"" + std::string(bone->name) + "\"].";
  generate_transforms(ob, prep, BC_ANIMATION_TYPE_BONE, curves);

  for (Bone *child = (Bone *)bone->childbase.first; child; child = child->next) {
    generate_transforms(ob, child, curves);
  }
}

/**
 * Collect all keyframes from all animation curves related to the object.
 * The bc_get... functions check for NULL and correct object type.
 * The #add_keyframes_from() function checks for NULL.
 */
void BCAnimationSampler::initialize_keyframes(BCFrameSet &frameset, Object *ob)
{
  frameset.clear();
  add_keyframes_from(bc_getSceneObjectAction(ob), frameset);
  add_keyframes_from(bc_getSceneCameraAction(ob), frameset);
  add_keyframes_from(bc_getSceneLightAction(ob), frameset);

  for (int a = 0; a < ob->totcol; a++) {
    Material *ma = give_current_material(ob, a + 1);
    add_keyframes_from(bc_getSceneMaterialAction(ma), frameset);
  }
}

void BCAnimationSampler::initialize_curves(BCAnimationCurveMap &curves, Object *ob)
{
  BC_animation_type object_type = BC_ANIMATION_TYPE_OBJECT;

  bAction *action = bc_getSceneObjectAction(ob);
  if (action) {
    FCurve *fcu = (FCurve *)action->curves.first;

    for (; fcu; fcu = fcu->next) {
      object_type = BC_ANIMATION_TYPE_OBJECT;
      if (ob->type == OB_ARMATURE) {
        char *boneName = BLI_str_quoted_substrN(fcu->rna_path, "pose.bones[");
        if (boneName) {
          object_type = BC_ANIMATION_TYPE_BONE;
        }
      }

      /* Adding action curves on object */
      BCCurveKey key(object_type, fcu->rna_path, fcu->array_index);
      curves[key] = new BCAnimationCurve(key, ob, fcu);
    }
  }

  /* Add missing curves */
  object_type = BC_ANIMATION_TYPE_OBJECT;
  generate_transforms(ob, EMPTY_STRING, object_type, curves);
  if (ob->type == OB_ARMATURE) {
    bArmature *arm = (bArmature *)ob->data;
    for (Bone *root_bone = (Bone *)arm->bonebase.first; root_bone; root_bone = root_bone->next) {
      generate_transforms(ob, root_bone, curves);
    }
  }

  /* Add curves on Object->data actions */
  action = NULL;
  if (ob->type == OB_CAMERA) {
    action = bc_getSceneCameraAction(ob);
    object_type = BC_ANIMATION_TYPE_CAMERA;
  }
  else if (ob->type == OB_LAMP) {
    action = bc_getSceneLightAction(ob);
    object_type = BC_ANIMATION_TYPE_LIGHT;
  }

  if (action) {
    /* Add light action or Camera action */
    FCurve *fcu = (FCurve *)action->curves.first;
    for (; fcu; fcu = fcu->next) {
      BCCurveKey key(object_type, fcu->rna_path, fcu->array_index);
      curves[key] = new BCAnimationCurve(key, ob, fcu);
    }
  }

  /* Add curves on Object->material actions*/
  object_type = BC_ANIMATION_TYPE_MATERIAL;
  for (int a = 0; a < ob->totcol; a++) {
    /* Export Material parameter animations. */
    Material *ma = give_current_material(ob, a + 1);
    if (ma) {
      action = bc_getSceneMaterialAction(ma);
      if (action) {
        /* isMatAnim = true; */
        FCurve *fcu = (FCurve *)action->curves.first;
        for (; fcu; fcu = fcu->next) {
          BCCurveKey key(object_type, fcu->rna_path, fcu->array_index, a);
          curves[key] = new BCAnimationCurve(key, ob, fcu);
        }
      }
    }
  }
}

/* ==================================================================== */

BCSample &BCSampleFrame::add(Object *ob)
{
  BCSample *sample = new BCSample(ob);
  sampleMap[ob] = sample;
  return *sample;
}

/* Get the matrix for the given key, returns Unity when the key does not exist */
const BCSample *BCSampleFrame::get_sample(Object *ob) const
{
  BCSampleMap::const_iterator it = sampleMap.find(ob);
  if (it == sampleMap.end()) {
    return NULL;
  }
  return it->second;
}

const BCMatrix *BCSampleFrame::get_sample_matrix(Object *ob) const
{
  BCSampleMap::const_iterator it = sampleMap.find(ob);
  if (it == sampleMap.end()) {
    return NULL;
  }
  BCSample *sample = it->second;
  return &sample->get_matrix();
}

/* Get the matrix for the given Bone, returns Unity when the Objewct is not sampled */
const BCMatrix *BCSampleFrame::get_sample_matrix(Object *ob, Bone *bone) const
{
  BCSampleMap::const_iterator it = sampleMap.find(ob);
  if (it == sampleMap.end()) {
    return NULL;
  }

  BCSample *sample = it->second;
  const BCMatrix *bc_bone = sample->get_matrix(bone);
  return bc_bone;
}

/* Check if the key is in this BCSampleFrame */
const bool BCSampleFrame::has_sample_for(Object *ob) const
{
  return sampleMap.find(ob) != sampleMap.end();
}

/* Check if the Bone is in this BCSampleFrame */
const bool BCSampleFrame::has_sample_for(Object *ob, Bone *bone) const
{
  const BCMatrix *bc_bone = get_sample_matrix(ob, bone);
  return (bc_bone);
}

/* ==================================================================== */

BCSample &BCSampleFrameContainer::add(Object *ob, int frame_index)
{
  BCSampleFrame &frame = sample_frames[frame_index];
  return frame.add(ob);
}

/* ====================================================== */
/* Below are the getters which we need to export the data */
/* ====================================================== */

/* Return either the BCSampleFrame or NULL if frame does not exist*/
BCSampleFrame *BCSampleFrameContainer::get_frame(int frame_index)
{
  BCSampleFrameMap::iterator it = sample_frames.find(frame_index);
  BCSampleFrame *frame = (it == sample_frames.end()) ? NULL : &it->second;
  return frame;
}

/* Return a list of all frames that need to be sampled */
const int BCSampleFrameContainer::get_frames(std::vector<int> &frames) const
{
  frames.clear();  // safety;
  BCSampleFrameMap::const_iterator it;
  for (it = sample_frames.begin(); it != sample_frames.end(); ++it) {
    frames.push_back(it->first);
  }
  return frames.size();
}

const int BCSampleFrameContainer::get_frames(Object *ob, BCFrames &frames) const
{
  frames.clear();  // safety;
  BCSampleFrameMap::const_iterator it;
  for (it = sample_frames.begin(); it != sample_frames.end(); ++it) {
    const BCSampleFrame &frame = it->second;
    if (frame.has_sample_for(ob)) {
      frames.push_back(it->first);
    }
  }
  return frames.size();
}

const int BCSampleFrameContainer::get_frames(Object *ob, Bone *bone, BCFrames &frames) const
{
  frames.clear();  // safety;
  BCSampleFrameMap::const_iterator it;
  for (it = sample_frames.begin(); it != sample_frames.end(); ++it) {
    const BCSampleFrame &frame = it->second;
    if (frame.has_sample_for(ob, bone)) {
      frames.push_back(it->first);
    }
  }
  return frames.size();
}

const int BCSampleFrameContainer::get_samples(Object *ob, BCFrameSampleMap &samples) const
{
  samples.clear();  // safety;
  BCSampleFrameMap::const_iterator it;
  for (it = sample_frames.begin(); it != sample_frames.end(); ++it) {
    const BCSampleFrame &frame = it->second;
    const BCSample *sample = frame.get_sample(ob);
    if (sample) {
      samples[it->first] = sample;
    }
  }
  return samples.size();
}

const int BCSampleFrameContainer::get_matrices(Object *ob, BCMatrixSampleMap &samples) const
{
  samples.clear();  // safety;
  BCSampleFrameMap::const_iterator it;
  for (it = sample_frames.begin(); it != sample_frames.end(); ++it) {
    const BCSampleFrame &frame = it->second;
    const BCMatrix *matrix = frame.get_sample_matrix(ob);
    if (matrix) {
      samples[it->first] = matrix;
    }
  }
  return samples.size();
}

const int BCSampleFrameContainer::get_matrices(Object *ob,
                                               Bone *bone,
                                               BCMatrixSampleMap &samples) const
{
  samples.clear();  // safety;
  BCSampleFrameMap::const_iterator it;
  for (it = sample_frames.begin(); it != sample_frames.end(); ++it) {
    const BCSampleFrame &frame = it->second;
    const BCMatrix *sample = frame.get_sample_matrix(ob, bone);
    if (sample) {
      samples[it->first] = sample;
    }
  }
  return samples.size();
}
