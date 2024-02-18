/* SPDX-FileCopyrightText: 2021 Tangent Animation. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "usd_reader_volume.hh"

#include "BLI_string.h"

#include "BKE_object.hh"
#include "BKE_volume.hh"

#include "DNA_object_types.h"
#include "DNA_volume_types.h"

#include <pxr/usd/usdVol/openVDBAsset.h>
#include <pxr/usd/usdVol/volume.h>

namespace usdtokens {

static const pxr::TfToken density("density", pxr::TfToken::Immortal);

}

namespace blender::io::usd {

void USDVolumeReader::create_object(Main *bmain, const double /*motionSampleTime*/)
{
  Volume *volume = (Volume *)BKE_volume_add(bmain, name_.c_str());

  object_ = BKE_object_add_only_object(bmain, OB_VOLUME, name_.c_str());
  object_->data = volume;
}

void USDVolumeReader::read_object_data(Main *bmain, const double motionSampleTime)
{
  if (!volume_) {
    return;
  }

  Volume *volume = static_cast<Volume *>(object_->data);

  if (!volume) {
    return;
  }

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
      filepathAttr.Get(&fp, motionSampleTime);

      if (filepathAttr.ValueMightBeTimeVarying()) {
        std::vector<double> filePathTimes;
        filepathAttr.GetTimeSamples(&filePathTimes);

        if (!filePathTimes.empty()) {
          int start = int(filePathTimes.front());
          int end = int(filePathTimes.back());

          volume->is_sequence = char(true);
          volume->frame_start = start;
          volume->frame_duration = (end - start) + 1;
        }
      }

      std::string filepath = fp.GetResolvedPath();

      STRNCPY(volume->filepath, filepath.c_str());
    }
  }

  USDXformReader::read_object_data(bmain, motionSampleTime);
}

}  // namespace blender::io::usd
