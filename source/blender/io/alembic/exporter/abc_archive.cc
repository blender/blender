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
 * The Original Code is Copyright (C) 2020 Blender Foundation.
 * All rights reserved.
 */

#include "abc_archive.h"

#include "BKE_blender_version.h"
#include "BKE_main.h"
#include "BKE_scene.h"

#include "DEG_depsgraph_query.h"

#include "DNA_scene_types.h"

#include <Alembic/AbcCoreOgawa/All.h>
#include <Alembic/AbcGeom/All.h>

#ifdef WIN32
#  include "BLI_path_util.h"
#  include "BLI_string.h"

#  include "utfconv.h"
#endif

namespace blender {
namespace io {
namespace alembic {

using Alembic::Abc::ErrorHandler;
using Alembic::Abc::kWrapExisting;
using Alembic::Abc::MetaData;
using Alembic::Abc::OArchive;
using Alembic::Abc::TimeSampling;
using Alembic::Abc::TimeSamplingPtr;
using Alembic::Abc::TimeSamplingType;

static MetaData create_abc_metadata(const Main *bmain, double scene_fps)
{
  MetaData abc_metadata;

  std::string abc_user_description(bmain->name);
  if (abc_user_description.empty()) {
    abc_user_description = "unknown";
  }

  abc_metadata.set(Alembic::Abc::kApplicationNameKey, "Blender");
  abc_metadata.set(Alembic::Abc::kUserDescriptionKey, abc_user_description);
  abc_metadata.set("blender_version", std::string("v") + BKE_blender_version_string());
  abc_metadata.set("FramesPerTimeUnit", std::to_string(scene_fps));

  time_t raw_time;
  time(&raw_time);
  char buffer[128];

#if defined _WIN32 || defined _WIN64
  ctime_s(buffer, 128, &raw_time);
#else
  ctime_r(&raw_time, buffer);
#endif

  const std::size_t buffer_len = strlen(buffer);
  if (buffer_len > 0 && buffer[buffer_len - 1] == '\n') {
    buffer[buffer_len - 1] = '\0';
  }

  abc_metadata.set(Alembic::Abc::kDateWrittenKey, buffer);
  return abc_metadata;
}

static OArchive *create_archive(std::ofstream *abc_ostream,
                                const std::string &filename,
                                MetaData &abc_metadata)
{
  /* Use stream to support unicode character paths on Windows. */
#ifdef WIN32
  char filename_cstr[FILE_MAX];
  BLI_strncpy(filename_cstr, filename.c_str(), FILE_MAX);

  UTF16_ENCODE(filename_cstr);
  std::wstring wstr(filename_cstr_16);
  abc_ostream->open(wstr.c_str(), std::ios::out | std::ios::binary);
  UTF16_UN_ENCODE(filename_cstr);
#else
  abc_ostream->open(filename, std::ios::out | std::ios::binary);
#endif

  ErrorHandler::Policy policy = ErrorHandler::kThrowPolicy;

  Alembic::AbcCoreOgawa::WriteArchive archive_writer;
  return new OArchive(archive_writer(abc_ostream, abc_metadata), kWrapExisting, policy);
}

/* Construct list of shutter samples.
 *
 * These are taken from the interval [shutter open, shutter close),
 * uniformly sampled with 'nr_of_samples' samples.
 *
 * TODO(Sybren): test that the above interval is indeed half-open.
 *
 * If 'time_relative' is true, samples are returned as time (in seconds) from params.frame_start.
 * If 'time_relative' is false, samples are returned as fractional frames from 0.
 * */
static void get_shutter_samples(double scene_fps,
                                const AlembicExportParams &params,
                                int nr_of_samples,
                                bool time_relative,
                                std::vector<double> &r_samples)
{
  int frame_offset = time_relative ? params.frame_start : 0;
  double time_factor = time_relative ? scene_fps : 1.0;
  double shutter_open = params.shutter_open;
  double shutter_close = params.shutter_close;
  double time_inc = (shutter_close - shutter_open) / nr_of_samples;

  /* sample between shutter open & close */
  for (int sample = 0; sample < nr_of_samples; sample++) {
    double sample_time = shutter_open + time_inc * sample;
    double time = (frame_offset + sample_time) / time_factor;

    r_samples.push_back(time);
  }
}

static TimeSamplingPtr create_time_sampling(double scene_fps,
                                            const AlembicExportParams &params,
                                            int nr_of_samples)
{
  std::vector<double> samples;

  if (params.frame_start == params.frame_end) {
    return TimeSamplingPtr(new TimeSampling());
  }

  get_shutter_samples(scene_fps, params, nr_of_samples, true, samples);

  TimeSamplingType ts(static_cast<uint32_t>(samples.size()), 1.0 / scene_fps);
  return TimeSamplingPtr(new TimeSampling(ts, samples));
}

static void get_frames(double scene_fps,
                       const AlembicExportParams &params,
                       unsigned int nr_of_samples,
                       std::set<double> &r_frames)
{
  /* Get one set of shutter samples, then add those around each frame to export. */
  std::vector<double> shutter_samples;
  get_shutter_samples(scene_fps, params, nr_of_samples, false, shutter_samples);

  for (double frame = params.frame_start; frame <= params.frame_end; frame += 1.0) {
    for (size_t j = 0; j < nr_of_samples; j++) {
      r_frames.insert(frame + shutter_samples[j]);
    }
  }
}

/* ****************************************************************** */

ABCArchive::ABCArchive(const Main *bmain,
                       const Scene *scene,
                       AlembicExportParams params,
                       std::string filename)
    : archive(nullptr)
{
  double scene_fps = FPS;
  MetaData abc_metadata = create_abc_metadata(bmain, scene_fps);

  // Create the Archive.
  archive = create_archive(&abc_ostream_, filename, abc_metadata);

  // Create time samples for transforms and shapes.
  TimeSamplingPtr ts_xform;
  TimeSamplingPtr ts_shapes;

  ts_xform = create_time_sampling(scene_fps, params, params.frame_samples_xform);
  time_sampling_index_transforms_ = archive->addTimeSampling(*ts_xform);

  const bool export_animation = params.frame_start != params.frame_end;
  if (!export_animation || params.frame_samples_shape == params.frame_samples_xform) {
    ts_shapes = ts_xform;
    time_sampling_index_shapes_ = time_sampling_index_transforms_;
  }
  else {
    ts_shapes = create_time_sampling(scene_fps, params, params.frame_samples_shape);
    time_sampling_index_shapes_ = archive->addTimeSampling(*ts_shapes);
  }

  // Construct the frames to export.
  get_frames(scene_fps, params, params.frame_samples_xform, xform_frames_);
  get_frames(scene_fps, params, params.frame_samples_shape, shape_frames_);

  // Merge all frames to get the final set of frames to export.
  export_frames_.insert(xform_frames_.begin(), xform_frames_.end());
  export_frames_.insert(shape_frames_.begin(), shape_frames_.end());

  abc_archive_bbox_ = Alembic::AbcGeom::CreateOArchiveBounds(*archive,
                                                             time_sampling_index_transforms_);
}

ABCArchive::~ABCArchive()
{
  delete archive;
}

uint32_t ABCArchive::time_sampling_index_transforms() const
{
  return time_sampling_index_transforms_;
}

uint32_t ABCArchive::time_sampling_index_shapes() const
{
  return time_sampling_index_shapes_;
}

ABCArchive::Frames::const_iterator ABCArchive::frames_begin() const
{
  return export_frames_.begin();
}
ABCArchive::Frames::const_iterator ABCArchive::frames_end() const
{
  return export_frames_.end();
}
size_t ABCArchive::total_frame_count() const
{
  return export_frames_.size();
}

bool ABCArchive::is_xform_frame(double frame) const
{
  return xform_frames_.find(frame) != xform_frames_.end();
}
bool ABCArchive::is_shape_frame(double frame) const
{
  return shape_frames_.find(frame) != shape_frames_.end();
}
ExportSubset ABCArchive::export_subset_for_frame(double frame) const
{
  ExportSubset subset;
  subset.transforms = is_xform_frame(frame);
  subset.shapes = is_shape_frame(frame);
  return subset;
}

void ABCArchive::update_bounding_box(const Imath::Box3d &bounds)
{
  abc_archive_bbox_.set(bounds);
}

}  // namespace alembic
}  // namespace io
}  // namespace blender
