#include "BLI_generic_virtual_array.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_span.hh"
#include "BLI_utildefines.h"
#include "BLI_virtual_array.hh"

#include "bmesh.h"
#include <memory>
#include <type_traits>

namespace blender::bmesh {
template<typename BMType> static constexpr int get_htype_from_type()
{
  if constexpr (std::is_same_v<BMType, BMVert>) {
    return BM_VERT;
  }
  if constexpr (std::is_same_v<BMType, BMEdge>) {
    return BM_EDGE;
  }
  if constexpr (std::is_same_v<BMType, BMLoop>) {
    return BM_LOOP;
  }
  if constexpr (std::is_same_v<BMType, BMFace>) {
    return BM_FACE;
  }
  else {
    return -1;
    // static_assert(false, "BMType is not one of (BMVert, BMEdge, BMLoop, BMFace).");
  }
}

template<typename T, typename BMType, int htype = get_htype_from_type<BMType>()>
class BMeshAttrArray : public blender::VArrayImpl<T> {
  // static const int htype = get_htype_from_type<BMType>();

 private:
  BMesh *bm_;
  int cd_offset_;

  int64_t get_size(BMesh *bm)
  {
    if constexpr (htype == BM_VERT) {
      return bm->totvert;
    }
    else if constexpr (htype == BM_EDGE) {
      return bm->totedge;
    }
    else if constexpr (htype == BM_LOOP) {
      return bm->totloop;
    }
    else if constexpr (htype == BM_FACE) {
      return bm->totface;
    }
    else {
      // static_assert(false, "Invalid element type.");
      BLI_assert_unreachable();
      return 0;
    }
  }

 public:
  using base_type = T;

  // BMeshAttrArray(const int64_t size) = 0;
  BMeshAttrArray(BMesh *bm, int cd_offset)
      : VArrayImpl<T>(get_size(bm)), bm_(bm), cd_offset_(cd_offset)
  {
    //
  }

  BMeshAttrArray(const BMeshAttrArray &b)
      : VArrayImpl<T>(get_size(b.bm_)), bm_(b.bm_), cd_offset_(b.cd_offset_)
  {
  }

  virtual CommonVArrayInfo common_info() const override
  {
    CommonVArrayInfo info = {};

    info.type = CommonVArrayInfo::Type::Any;
    info.may_have_ownership = false;

    return info;
  }

  virtual T get(int64_t index) const override
  {
    BMType **table = nullptr;

    if constexpr (htype == BM_VERT) {
      table = bm_->vtable;
    }
    else if constexpr (htype == BM_EDGE) {
      table = bm_->etable;
    }
    else if constexpr (htype == BM_LOOP) {
      // static_assert(false, "Not supported for loops");
    }
    else if constexpr (htype == BM_FACE) {
      table = bm_->ftable;
    }

    return *static_cast<T *>(POINTER_OFFSET(table[index]->head.data, cd_offset_));
  }
};

namespace detail {
template<typename BMType>
static GVArray bmesh_attr_gvarray_intern(BMesh *bm, CustomDataLayer *layer)
{
  auto make_array = [&](auto *impl) {
    using T = typename std::remove_reference_t<decltype(*impl)>::base_type;

    return VArray<T>(static_cast<VArrayImpl<T> *>(impl));
    //
  };

  switch (layer->type) {
    case CD_PROP_FLOAT:
      return make_array(new BMeshAttrArray<float, BMType>(bm, layer->offset));
    case CD_PROP_FLOAT2:
      return make_array(new BMeshAttrArray<float2, BMType>(bm, layer->offset));
    case CD_PROP_FLOAT3:
      return make_array(new BMeshAttrArray<float3, BMType>(bm, layer->offset));
    case CD_PROP_COLOR:
      return make_array(new BMeshAttrArray<float4, BMType>(bm, layer->offset));
    case CD_PROP_BYTE_COLOR:
      return make_array(new BMeshAttrArray<uchar4, BMType>(bm, layer->offset));
    case CD_PROP_BOOL:
      return make_array(new BMeshAttrArray<bool, BMType>(bm, layer->offset));
    case CD_PROP_INT8:
      return make_array(new BMeshAttrArray<int8_t, BMType>(bm, layer->offset));
    case CD_PROP_INT32:
      return make_array(new BMeshAttrArray<int32_t, BMType>(bm, layer->offset));
    case CD_ORIGINDEX:
      return make_array(new BMeshAttrArray<int32_t, BMType>(bm, layer->offset));
  }

  return GVArray(VArray<int>::ForSpan({nullptr, 0}));
}
};  // namespace detail

static GVArray bmesh_attr_gvarray(BMesh *bm, int htype, const char *name)
{
  CustomData *cdata;

  switch (htype) {
    case BM_VERT:
      cdata = &bm->vdata;
      break;
    case BM_EDGE:
      cdata = &bm->edata;
      break;
    case BM_LOOP:
      printf("%s: loops are not supported\n", __func__);
      return GVArray(VArray<int>::ForSpan({nullptr, 0}));
    case BM_FACE:
      cdata = &bm->pdata;
      break;
    default:
      printf("Invalid element type %d\n", htype);
      BLI_assert_unreachable();
      return GVArray(VArray<int>::ForSpan({nullptr, 0}));
  }

  CustomDataLayer *layer = nullptr;

  for (int i = 0; i < cdata->totlayer; i++) {
    if (STREQ(cdata->layers[i].name, name)) {
      layer = &cdata->layers[i];
      break;
    }
  }

  if (!layer) {
    printf("Unknown attribute %s\n", name);
    return GVArray(VArray<int>::ForSpan({nullptr, 0}));
  }

  switch (htype) {
    case BM_VERT:
      return detail::bmesh_attr_gvarray_intern<BMVert>(bm, layer);
    case BM_EDGE:
      return detail::bmesh_attr_gvarray_intern<BMEdge>(bm, layer);
    /*case BM_LOOP:
      break;*/
    case BM_FACE:
      return detail::bmesh_attr_gvarray_intern<BMFace>(bm, layer);
  }

  return GVArray(VArray<int>::ForSpan({nullptr, 0}));
}

}  // namespace blender::bmesh
