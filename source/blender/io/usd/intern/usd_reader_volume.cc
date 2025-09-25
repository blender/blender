/* SPDX-FileCopyrightText: 2021 Tangent Animation. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "usd_reader_volume.hh"

#include "BLI_path_utils.hh"
#include "BLI_string.h"

#include "BKE_main.hh"
#include "BKE_object.hh"
#include "BKE_volume.hh"

#include "DNA_object_types.h"
#include "DNA_volume_types.h"

#include <pxr/usd/usdVol/openVDBAsset.h>
#include <pxr/usd/usdVol/volume.h>

namespace blender::io::usd {

void USDVolumeReader::create_object(Main *bmain)
{
  Volume *volume = BKE_volume_add(bmain, name_.c_str());

  object_ = BKE_object_add_only_object(bmain, OB_VOLUME, name_.c_str());
  object_->data = volume;
}

void USDVolumeReader::read_object_data(Main *bmain, const pxr::UsdTimeCode time)
{
  Volume *volume = static_cast<Volume *>(object_->data);

  pxr::UsdVolVolume::FieldMap fields = volume_.GetFieldPaths();

  for (pxr::UsdVolVolume::FieldMap::const_iterator it = fields.begin(); it != fields.end(); ++it) {

    pxr::UsdPrim fieldPrim = prim_.GetStage()->GetPrimAtPath(it->second);

    if (!fieldPrim.IsA<pxr::UsdVolOpenVDBAsset>()) {
      continue;
    }

    pxr::UsdVolOpenVDBAsset fieldBase(fieldPrim);

    pxr::UsdAttribute filepathAttr = fieldBase.GetFilePathAttr();

    if (filepathAttr.IsAuthored()) {
      pxr::SdfAssetPath fp;
      filepathAttr.Get(&fp, time);

      const std::string filepath = fp.GetResolvedPath();
      STRNCPY(volume->filepath, filepath.c_str());

      if (import_params_.relative_path && !BLI_path_is_rel(volume->filepath)) {
        BLI_path_rel(volume->filepath, BKE_main_blendfile_path_from_global());
      }

      if (filepathAttr.ValueMightBeTimeVarying()) {
        std::vector<double> filePathTimes;
        filepathAttr.GetTimeSamples(&filePathTimes);

        if (!filePathTimes.empty()) {
          const int start = int(filePathTimes.front());
          const int end = int(filePathTimes.back());
          const int offset = BLI_path_sequence_decode(
              volume->filepath, nullptr, 0, nullptr, 0, nullptr);

          volume->is_sequence = char(true);
          volume->frame_start = start;
          volume->frame_duration = (end - start) + 1;
          volume->frame_offset = offset - 1;
        }
      }
    }
  }

  USDXformReader::read_object_data(bmain, time);
}

}  // namespace blender::io::usd
