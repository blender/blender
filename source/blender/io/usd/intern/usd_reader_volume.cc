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
 * The Original Code is Copyright (C) 2021 Tangent Animation.
 * All rights reserved.
 */

#include "usd_reader_volume.h"

#include "BKE_object.h"
#include "BKE_volume.h"

#include "DNA_object_types.h"
#include "DNA_volume_types.h"

#include <pxr/usd/usdVol/openVDBAsset.h>
#include <pxr/usd/usdVol/volume.h>

#include <iostream>

namespace usdtokens {

static const pxr::TfToken density("density", pxr::TfToken::Immortal);

}

namespace blender::io::usd {

void USDVolumeReader::create_object(Main *bmain, const double /* motionSampleTime */)
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

    pxr::UsdAttribute fieldNameAttr = fieldBase.GetFieldNameAttr();

    if (fieldNameAttr.IsAuthored()) {
      pxr::TfToken fieldName;
      fieldNameAttr.Get(&fieldName, motionSampleTime);

      /* A Blender volume creates density by default. */
      if (fieldName != usdtokens::density) {
        BKE_volume_grid_add(volume, fieldName.GetString().c_str(), VOLUME_GRID_FLOAT);
      }
    }

    pxr::UsdAttribute filepathAttr = fieldBase.GetFilePathAttr();

    if (filepathAttr.IsAuthored()) {
      pxr::SdfAssetPath fp;
      filepathAttr.Get(&fp, motionSampleTime);

      if (filepathAttr.ValueMightBeTimeVarying()) {
        std::vector<double> filePathTimes;
        filepathAttr.GetTimeSamples(&filePathTimes);

        if (!filePathTimes.empty()) {
          int start = static_cast<int>(filePathTimes.front());
          int end = static_cast<int>(filePathTimes.back());

          volume->is_sequence = static_cast<char>(true);
          volume->frame_start = start;
          volume->frame_duration = (end - start) + 1;
        }
      }

      std::string filepath = fp.GetResolvedPath();

      strcpy(volume->filepath, filepath.c_str());
    }
  }

  USDXformReader::read_object_data(bmain, motionSampleTime);
}

}  // namespace blender::io::usd
