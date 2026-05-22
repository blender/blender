/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "abc_keyframing.h"
#include "abc_reader_archive.h"

#include "DNA_scene_types.h"

#include "ANIM_action.hh"
#include "ANIM_animdata.hh"

#include "BKE_fcurve.hh"

using Alembic::Abc::ISampleSelector;

namespace blender {
namespace io::alembic {

/* Utility: create new fcurve and add it as a channel to a group. */
static FCurve *create_fcurve(animrig::Channelbag &channelbag,
                             const animrig::FCurveDescriptor &fcurve_descriptor,
                             const int sample_count)
{
  FCurve *fcurve = channelbag.fcurve_create_unique(nullptr, fcurve_descriptor);
  BLI_assert_msg(fcurve, "The same F-Curve is being created twice, this is unexpected.");
  if (fcurve) {
    BKE_fcurve_bezt_resize(*fcurve, sample_count);
  }
  return fcurve;
}

/* Utility: fill in a single fcurve sample at the provided index. */
void set_fcurve_sample(FCurve *fcu, int64_t sample_index, const float frame, const float value)
{
  BLI_assert(sample_index >= 0 && sample_index < fcu->totvert);
  BezTriple &bez = fcu->bezt[sample_index];
  bez.vec[1][0] = frame;
  bez.vec[1][1] = value;
  bez.ipo = BEZT_IPO_LIN;
  bez.f1 = bez.f2 = bez.f3 = BEZT_FLAG_SELECT;
  bez.h1 = bez.h2 = HD_AUTO;
}

FCurveCreationHelper::~FCurveCreationHelper() = default;

void FCurveCreationHelper::ensure_action_data(Main *bmain, const int sample_count)
{
  action_ = animrig::id_action_ensure(bmain, id_);
  channelbag = &animrig::action_channelbag_ensure(*action_, *id_);
  create_fcurves(sample_count);
}

FCurve *FCurveCreationHelper::create_fcurve(const animrig::FCurveDescriptor &fcurve_descriptor,
                                            const int sample_count)
{
  return alembic::create_fcurve(*channelbag, fcurve_descriptor, sample_count);
}

void FCurveCreationHelper::finish()
{
  remove_unnecessary_fcurves();

  for (FCurve *fcu : channelbag->fcurves()) {
    if (fcu) {
      BKE_fcurve_handles_recalc(*fcu);
    }
  }
}

void create_keyframes(Main *bmain,
                      Scene *scene,
                      Span<std::unique_ptr<FCurveCreationHelper>> helpers,
                      const TimeInfo time_info)
{
  if (helpers.is_empty()) {
    return;
  }

  const double fps = scene->frames_per_second();
  const int start_frame = int(round(time_info.min_time * fps));
  const int end_frame = int(round(time_info.max_time * fps));

  const int sample_count = end_frame - start_frame + 1;

  for (const std::unique_ptr<FCurveCreationHelper> &helper : helpers) {
    helper->ensure_action_data(bmain, sample_count);
  }

  int64_t sample_index = 0;
  for (int i = start_frame; i <= end_frame; i++) {
    const double frame_time = (double(i) / fps);
    const ISampleSelector selector = ISampleSelector(frame_time);

    FrameSampleInfo sample_info;
    sample_info.frame = float(i);
    sample_info.sample_index = sample_index++;
    sample_info.selector = selector;

    for (const std::unique_ptr<FCurveCreationHelper> &helper : helpers) {
      helper->set_fcurves_sample(sample_info);
    }
  }

  for (const std::unique_ptr<FCurveCreationHelper> &helper : helpers) {
    helper->finish();
  }
}

}  // namespace io::alembic
}  // namespace blender
