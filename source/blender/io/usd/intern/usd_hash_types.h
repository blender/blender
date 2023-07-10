#ifndef BLENDER_USD_HASH_TYPES_H
#define BLENDER_USD_HASH_TYPES_H

#include <pxr/base/tf/token.h>
#include <pxr/usd/sdf/valueTypeName.h>

namespace blender {
template<> struct DefaultHash<pxr::SdfValueTypeName> {
  uint64_t operator()(const pxr::SdfValueTypeName &value) const
  {
    return value.GetHash();
  }
};

template<> struct DefaultHash<pxr::TfToken> {
  uint64_t operator()(const pxr::TfToken &value) const
  {
    return value.Hash();
  }
};
}  // namespace blender

#endif  // BLENDER_USD_HASH_TYPES_H

