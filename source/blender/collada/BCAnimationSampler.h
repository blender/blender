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
 */

#ifndef __BC_ANIMATION_CURVE_CONTAINER_H__
#define __BC_ANIMATION_CURVE_CONTAINER_H__

#include "BCAnimationCurve.h"
#include "BCSampleData.h"
#include "collada_utils.h"

extern "C" {
#include "BKE_action.h"
#include "BKE_library.h"
#include "BLI_math_rotation.h"
#include "DNA_action_types.h"
}

/* Collection of animation curves */
class BCAnimation {
 private:
  Object *reference = NULL;
  bContext *mContext;

 public:
  BCFrameSet frame_set;
  BCAnimationCurveMap curve_map;

  BCAnimation(bContext *C, Object *ob) : mContext(C)
  {
    Main *bmain = CTX_data_main(mContext);
    reference = BKE_object_copy(bmain, ob);
  }

  ~BCAnimation()
  {
    BCAnimationCurveMap::iterator it;
    for (it = curve_map.begin(); it != curve_map.end(); ++it) {
      delete it->second;
    }

    if (reference && reference->id.us == 0) {
      Main *bmain = CTX_data_main(mContext);
      BKE_id_delete(bmain, &reference->id);
    }
    curve_map.clear();
  }

  Object *get_reference()
  {
    return reference;
  }
};

typedef std::map<Object *, BCAnimation *> BCAnimationObjectMap;

class BCSampleFrame {

  /* Each frame on the timeline that needs to be sampled will have
   * one BCSampleFrame where we collect sample information about all objects
   * that need to be sampled for that frame. */

 private:
  BCSampleMap sampleMap;

 public:
  ~BCSampleFrame()
  {
    BCSampleMap::iterator it;
    for (it = sampleMap.begin(); it != sampleMap.end(); ++it) {
      BCSample *sample = it->second;
      delete sample;
    }
    sampleMap.clear();
  }

  BCSample &add(Object *ob);

  /* Following methods return NULL if object is not in the sampleMap*/
  const BCSample *get_sample(Object *ob) const;
  const BCMatrix *get_sample_matrix(Object *ob) const;
  const BCMatrix *get_sample_matrix(Object *ob, Bone *bone) const;

  const bool has_sample_for(Object *ob) const;
  const bool has_sample_for(Object *ob, Bone *bone) const;
};

typedef std::map<int, BCSampleFrame> BCSampleFrameMap;

class BCSampleFrameContainer {

  /*
   * The BCSampleFrameContainer stores a map of BCSampleFrame objects
   * with the timeline frame as key.
   *
   * Some details on the purpose:
   * An Animation is made of multiple FCurves where each FCurve can
   * have multiple keyframes. When we want to export the animation we
   * also can decide whether we want to export the keyframes or a set
   * of sample frames at equidistant locations (sample period).
   * In any case we must resample first need to resample it fully
   * to resolve things like:
   *
   * - animations by constraints
   * - animations by drivers
   *
   * For this purpose we need to step through the entire animation and
   * then sample each frame that contains at least one keyFrame or
   * sampleFrame. Then for each frame we have to store the transform
   * information for all exported objects in a BCSampleframe
   *
   * The entire set of BCSampleframes is finally collected into
   * a BCSampleframneContainer
   */

 private:
  BCSampleFrameMap sample_frames;

 public:
  ~BCSampleFrameContainer()
  {
  }

  BCSample &add(Object *ob, int frame_index);
  BCSampleFrame *get_frame(int frame_index);  // returns NULL if frame does not exist

  const int get_frames(std::vector<int> &frames) const;
  const int get_frames(Object *ob, BCFrames &frames) const;
  const int get_frames(Object *ob, Bone *bone, BCFrames &frames) const;

  const int get_samples(Object *ob, BCFrameSampleMap &samples) const;
  const int get_matrices(Object *ob, BCMatrixSampleMap &matrices) const;
  const int get_matrices(Object *ob, Bone *bone, BCMatrixSampleMap &bones) const;
};

class BCAnimationSampler {
 private:
  BCExportSettings &export_settings;
  BCSampleFrameContainer sample_data;
  BCAnimationObjectMap objects;

  void generate_transform(Object *ob, const BCCurveKey &key, BCAnimationCurveMap &curves);
  void generate_transforms(Object *ob,
                           const std::string prep,
                           const BC_animation_type type,
                           BCAnimationCurveMap &curves);
  void generate_transforms(Object *ob, Bone *bone, BCAnimationCurveMap &curves);

  void initialize_curves(BCAnimationCurveMap &curves, Object *ob);
  void initialize_keyframes(BCFrameSet &frameset, Object *ob);
  BCSample &sample_object(Object *ob, int frame_index, bool for_opensim);
  void update_animation_curves(BCAnimation &animation,
                               BCSample &sample,
                               Object *ob,
                               int frame_index);
  void check_property_is_animated(
      BCAnimation &animation, float *ref, float *val, std::string data_path, int length);

 public:
  BCAnimationSampler(BCExportSettings &export_settings, BCObjectSet &animated_subset);
  ~BCAnimationSampler();

  void add_object(Object *ob);

  void sample_scene(BCExportSettings &export_settings, bool keyframe_at_end);

  BCAnimationCurveMap *get_curves(Object *ob);
  void get_object_frames(BCFrames &frames, Object *ob);
  bool get_object_samples(BCMatrixSampleMap &samples, Object *ob);
  void get_bone_frames(BCFrames &frames, Object *ob, Bone *bone);
  bool get_bone_samples(BCMatrixSampleMap &samples, Object *ob, Bone *bone);

  static void get_animated_from_export_set(std::set<Object *> &animated_objects,
                                           LinkNode &export_set);
  static void find_depending_animated(std::set<Object *> &animated_objects,
                                      std::set<Object *> &candidates);
  static bool is_animated_by_constraint(Object *ob,
                                        ListBase *conlist,
                                        std::set<Object *> &animated_objects);
};

#endif
