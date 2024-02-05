/* SPDX-FileCopyrightText: 2021 NVIDIA Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "usd_reader_skeleton.h"
#include "usd_skel_convert.h"

#include "BKE_armature.hh"
#include "BKE_idprop.h"
#include "BKE_object.hh"

#include "DNA_armature_types.h"
#include "DNA_object_types.h"

#include "MEM_guardedalloc.h"

#include "WM_api.hh"

namespace blender::io::usd {

bool USDSkeletonReader::valid() const
{
  return skel_ && USDXformReader::valid();
}

void USDSkeletonReader::create_object(Main *bmain, const double /*motionSampleTime*/)
{
  object_ = BKE_object_add_only_object(bmain, OB_ARMATURE, name_.c_str());

  bArmature *arm = BKE_armature_add(bmain, name_.c_str());
  object_->data = arm;
}

void USDSkeletonReader::read_object_data(Main *bmain, const double motionSampleTime)
{
  if (!object_ || !object_->data || !skel_) {
    return;
  }

  import_skeleton(bmain, object_, skel_, reports());

  USDXformReader::read_object_data(bmain, motionSampleTime);
}

}  // namespace blender::io::usd
