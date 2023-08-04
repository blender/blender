/* SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2011-2022 Blender Foundation */

#pragma once

#include <pxr/base/tf/token.h>
#include <pxr/base/vt/value.h>
#include <pxr/usd/sdf/path.h>

#include "DNA_ID.h"

#include "BLI_hash.hh"

template<> struct blender::DefaultHash<pxr::SdfPath> {
  uint64_t operator()(const pxr::SdfPath &value) const
  {
    return (uint64_t)value.GetHash();
  }
};

template<> struct blender::DefaultHash<pxr::TfToken> {
  uint64_t operator()(const pxr::TfToken &value) const
  {
    return (uint64_t)value.Hash();
  }
};

namespace blender::io::hydra {

class HydraSceneDelegate;

class IdData {
 public:
  const ID *id;
  pxr::SdfPath prim_id;

 protected:
  HydraSceneDelegate *scene_delegate_;

 public:
  IdData(HydraSceneDelegate *scene_delegate, const ID *id, pxr::SdfPath const &prim_id);
  virtual ~IdData() = default;

  virtual void init() = 0;
  virtual void insert() = 0;
  virtual void remove() = 0;
  virtual void update() = 0;

  virtual pxr::VtValue get_data(pxr::TfToken const &key) const = 0;
};

#define ID_LOG(level, msg, ...) \
  CLOG_INFO(LOG_HYDRA_SCENE, level, "%s: " msg, prim_id.GetText(), ##__VA_ARGS__);

#define ID_LOGN(level, msg, ...) \
  CLOG_INFO(LOG_HYDRA_SCENE, \
            level, \
            "%s (%s): " msg, \
            prim_id.GetText(), \
            id ? id->name : "", \
            ##__VA_ARGS__);

}  // namespace blender::io::hydra
