/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <functional>

#include <Alembic/Abc/ISampleSelector.h>

#include "BLI_set.hh"
#include "BLI_vector.hh"

#include "RNA_path.hh"

namespace blender {

namespace animrig {
class Channelbag;
struct FCurveDescriptor;
}  // namespace animrig

struct bAction;
struct FCurve;
struct ID;
struct Main;
struct Scene;

namespace io::alembic {

struct TimeInfo;

struct FrameSampleInfo {
  Alembic::Abc::ISampleSelector selector{};
  int64_t sample_index = 0;
  float frame = 0.0f;
};

/* Base class for creating FCurves and setting their samples for each frame.
 * The actual FCurve creation is delegated to derived classes. */
class FCurveCreationHelper {
 protected:
  ID *id_ = nullptr;
  bAction *action_ = nullptr;
  animrig::Channelbag *channelbag = nullptr;

 public:
  FCurveCreationHelper(ID *id) : id_(id) {}

  virtual ~FCurveCreationHelper();

  void ensure_action_data(Main *bmain, const int sample_count);

  /* Called every frame. Derived classes should set the sample for every FCurve that they have
   * created. */
  virtual void set_fcurves_sample(const FrameSampleInfo &sample_info) = 0;

  void finish();

 protected:
  FCurve *create_fcurve(const animrig::FCurveDescriptor &fcurve_descriptor,
                        const int sample_count);

  /* This is where derived classes should create FCurves for every property that they want to see
   * key-framed. FCurves should be created using the #create_fcurve method above. */
  virtual void create_fcurves(const int sample_count) = 0;

  /* Derived classes can implement this to remove any FCurve for any property which was not
   * actually animated. */
  virtual void remove_unnecessary_fcurves() {}
};

/* Create keyframes for the entire range of the supplied #TimeInfo. */
void create_keyframes(Main *bmain,
                      Scene *scene,
                      Span<std::unique_ptr<FCurveCreationHelper>> helpers,
                      const TimeInfo time_info);

void set_fcurve_sample(FCurve *fcu, int64_t sample_index, const float frame, const float value);

}  // namespace io::alembic
}  // namespace blender
