/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 * Implementation of CustomData.
 *
 * BKE_customdata.hh contains the function prototypes for this file.
 */

#include <algorithm>

#include "MEM_guardedalloc.h"

/* Since we have versioning code here (CustomData_verify_versions()). */
#define DNA_DEPRECATED_ALLOW

#include "DNA_ID.h"
#include "DNA_customdata_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_enums.h"
#include "DNA_userdef_types.h"
#include "DNA_vec_types.h"

#include "BLI_bit_vector.hh"
#include "BLI_bitmap.h"
#include "BLI_index_range.hh"
#include "BLI_math_color_blend.h"
#include "BLI_math_quaternion_types.hh"
#include "BLI_math_vector.hh"
#include "BLI_memory_counter.hh"
#include "BLI_mempool.h"
#include "BLI_path_utils.hh"
#include "BLI_resource_scope.hh"
#include "BLI_set.hh"
#include "BLI_span.hh"
#include "BLI_string.h"
#include "BLI_string_ref.hh"
#include "BLI_string_utf8.h"
#include "BLI_string_utils.hh"
#include "BLI_utildefines.h"

#ifndef NDEBUG
#  include "BLI_dynstr.h"
#endif

#include "BLT_translation.hh"

#include "BKE_anonymous_attribute_id.hh"
#include "BKE_attribute_legacy_convert.hh"
#include "BKE_attribute_math.hh"
#include "BKE_attribute_storage.hh"
#include "BKE_ccg.hh"
#include "BKE_customdata.hh"
#include "BKE_customdata_file.h"
#include "BKE_deform.hh"
#include "BKE_library.hh"
#include "BKE_main.hh"
#include "BKE_mesh_remap.hh"
#include "BKE_multires.hh"

#include "BLO_read_write.hh"

#include "bmesh.hh"

#include "CLG_log.h"

/* only for customdata_data_transfer_interp_normal_normals */
#include "data_transfer_intern.hh"

using blender::Array;
using blender::BitVector;
using blender::float2;
using blender::ImplicitSharingInfo;
using blender::IndexRange;
using blender::MutableSpan;
using blender::Set;
using blender::Span;
using blender::StringRef;
using blender::Vector;

/* number of layers to add when growing a CustomData object */
#define CUSTOMDATA_GROW 5

/* ensure typemap size is ok */
BLI_STATIC_ASSERT(BOUNDED_ARRAY_TYPE_SIZE<decltype(CustomData::typemap)>() == CD_NUMTYPES,
                  "size mismatch");

static CLG_LogRef LOG = {"geom.customdata"};

/* -------------------------------------------------------------------- */
/** \name Mesh Mask Utilities
 * \{ */

void CustomData_MeshMasks_update(CustomData_MeshMasks *mask_dst,
                                 const CustomData_MeshMasks *mask_src)
{
  mask_dst->vmask |= mask_src->vmask;
  mask_dst->emask |= mask_src->emask;
  mask_dst->fmask |= mask_src->fmask;
  mask_dst->pmask |= mask_src->pmask;
  mask_dst->lmask |= mask_src->lmask;
}

bool CustomData_MeshMasks_are_matching(const CustomData_MeshMasks *mask_ref,
                                       const CustomData_MeshMasks *mask_required)
{
  return (((mask_required->vmask & mask_ref->vmask) == mask_required->vmask) &&
          ((mask_required->emask & mask_ref->emask) == mask_required->emask) &&
          ((mask_required->fmask & mask_ref->fmask) == mask_required->fmask) &&
          ((mask_required->pmask & mask_ref->pmask) == mask_required->pmask) &&
          ((mask_required->lmask & mask_ref->lmask) == mask_required->lmask));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Layer Type Information Struct
 * \{ */

struct LayerTypeInfo {
  int size; /* the memory size of one element of this layer's data */
  int alignment;

  /** name of the struct used, for file writing */
  const char *structname;
  /** number of structs per element, for file writing */
  int structnum;

  /**
   * default layer name.
   *
   * \note when null this is a way to ensure there is only ever one item
   * see: CustomData_layertype_is_singleton().
   */
  const char *defaultname;

  /**
   * a function to copy count elements of this layer's data
   * (deep copy if appropriate)
   * if null, memcpy is used
   */
  cd_copy copy;

  /**
   * a function to destruct this layer's data.
   *
   * \note implementations should make sure that the data pointer itself is not freed.
   */
  cd_free free;

  /**
   * a function to interpolate between count source elements of this
   * layer's data and store the result in dest
   * if weights == null they should default to 1
   *
   * weights gives the weight for each element in sources
   * count gives the number of elements in sources
   *
   * \note in some cases `dest` pointer is in `sources` so all functions have to take this
   * into account and delay applying changes while reading from sources. See #32395.
   */
  cd_interp interp;

  /** a function to swap the data in corners of the element */
  void (*swap)(void *data, const int *corner_indices);

  /**
   * Set values to the type's default. If undefined, the default is assumed to be zeroes.
   * Memory pointed to by #data is expected to be uninitialized.
   */
  cd_set_default_value set_default_value;
  /**
   * Construct and fill a valid value for the type. Necessary for non-trivial types.
   * Memory pointed to by #data is expected to be uninitialized.
   */
  void (*construct)(void *data, int count);

  /** A function used by mesh validating code, must ensures passed item has valid data. */
  cd_validate validate;

  /** Functions necessary for geometry collapse. */
  bool (*equal)(const void *data1, const void *data2);
  void (*multiply)(void *data, float fac);
  void (*initminmax)(void *min, void *max);
  void (*add)(void *data1, const void *data2);
  void (*dominmax)(const void *data1, void *min, void *max);
  void (*copyvalue)(const void *source, void *dest, int mixmode, const float mixfactor);

  /** A function to read data from a cdf file. */
  bool (*read)(CDataFile *cdf, void *data, int count);

  /** A function to write data to a cdf file. */
  bool (*write)(CDataFile *cdf, const void *data, int count);

  /** A function to determine file size. */
  size_t (*filesize)(CDataFile *cdf, const void *data, int count);

  /**
   * A function to determine max allowed number of layers,
   * should be null or return -1 if no limit.
   */
  int (*layers_max)();
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Callbacks for (#MDeformVert, #CD_MDEFORMVERT)
 * \{ */

static void layerCopy_mdeformvert(const void *source, void *dest, const int count)
{
  int i, size = sizeof(MDeformVert);

  memcpy(dest, source, count * size);

  for (i = 0; i < count; i++) {
    MDeformVert *dvert = static_cast<MDeformVert *>(POINTER_OFFSET(dest, i * size));

    if (dvert->totweight) {
      MDeformWeight *dw = MEM_malloc_arrayN<MDeformWeight>(size_t(dvert->totweight), __func__);

      memcpy(dw, dvert->dw, dvert->totweight * sizeof(*dw));
      dvert->dw = dw;
    }
    else {
      dvert->dw = nullptr;
    }
  }
}

static void layerFree_mdeformvert(void *data, const int count)
{
  for (MDeformVert &dvert : MutableSpan(static_cast<MDeformVert *>(data), count)) {
    if (dvert.dw) {
      MEM_freeN(dvert.dw);
      dvert.dw = nullptr;
      dvert.totweight = 0;
    }
  }
}

static void layerInterp_mdeformvert(const void **sources,
                                    const float *weights,
                                    const int count,
                                    void *dest)
{
  /* A single linked list of #MDeformWeight's.
   * use this to avoid double allocations (which #LinkNode would do). */
  struct MDeformWeight_Link {
    MDeformWeight_Link *next;
    MDeformWeight dw;
  };

  MDeformVert *dvert = static_cast<MDeformVert *>(dest);
  MDeformWeight_Link *dest_dwlink = nullptr;
  MDeformWeight_Link *node;

  /* build a list of unique def_nrs for dest */
  int totweight = 0;
  for (int i = 0; i < count; i++) {
    const MDeformVert *source = static_cast<const MDeformVert *>(sources[i]);
    float interp_weight = weights[i];

    for (int j = 0; j < source->totweight; j++) {
      MDeformWeight *dw = &source->dw[j];
      float weight = dw->weight * interp_weight;

      if (weight == 0.0f) {
        continue;
      }

      for (node = dest_dwlink; node; node = node->next) {
        MDeformWeight *tmp_dw = &node->dw;

        if (tmp_dw->def_nr == dw->def_nr) {
          tmp_dw->weight += weight;
          break;
        }
      }

      /* if this def_nr is not in the list, add it */
      if (!node) {
        MDeformWeight_Link *tmp_dwlink = static_cast<MDeformWeight_Link *>(
            alloca(sizeof(*tmp_dwlink)));
        tmp_dwlink->dw.def_nr = dw->def_nr;
        tmp_dwlink->dw.weight = weight;

        /* Inline linked-list. */
        tmp_dwlink->next = dest_dwlink;
        dest_dwlink = tmp_dwlink;

        totweight++;
      }
    }
  }

  /* Delay writing to the destination in case dest is in sources. */

  /* now we know how many unique deform weights there are, so realloc */
  if (dvert->dw && (dvert->totweight == totweight)) {
    /* pass (fast-path if we don't need to realloc). */
  }
  else {
    if (dvert->dw) {
      MEM_freeN(dvert->dw);
    }

    if (totweight) {
      dvert->dw = MEM_malloc_arrayN<MDeformWeight>(size_t(totweight), __func__);
    }
  }

  if (totweight) {
    dvert->totweight = totweight;
    int i = 0;
    for (node = dest_dwlink; node; node = node->next, i++) {
      node->dw.weight = std::min(node->dw.weight, 1.0f);
      dvert->dw[i] = node->dw;
    }
  }
  else {
    *dvert = MDeformVert{};
  }
}

static void layerConstruct_mdeformvert(void *data, const int count)
{
  std::fill_n(static_cast<MDeformVert *>(data), count, MDeformVert{});
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Callbacks for (#vec3f, #CD_NORMAL)
 * \{ */

static void layerInterp_normal(const void **sources,
                               const float *weights,
                               const int count,
                               void *dest)
{
  /* NOTE: This is linear interpolation, which is not optimal for vectors.
   * Unfortunately, spherical interpolation of more than two values is hairy,
   * so for now it will do... */
  float no[3] = {0.0f};

  for (const int i : IndexRange(count)) {
    madd_v3_v3fl(no, (const float *)sources[i], weights[i]);
  }

  /* Weighted sum of normalized vectors will **not** be normalized, even if weights are. */
  normalize_v3_v3((float *)dest, no);
}

static void layerCopyValue_normal(const void *source,
                                  void *dest,
                                  const int mixmode,
                                  const float mixfactor)
{
  const float *no_src = (const float *)source;
  float *no_dst = (float *)dest;
  float no_tmp[3];

  if (ELEM(mixmode,
           CDT_MIX_NOMIX,
           CDT_MIX_REPLACE_ABOVE_THRESHOLD,
           CDT_MIX_REPLACE_BELOW_THRESHOLD))
  {
    /* Above/below threshold modes are not supported here, fallback to nomix (just in case). */
    copy_v3_v3(no_dst, no_src);
  }
  else { /* Modes that support 'real' mix factor. */
    /* Since we normalize in the end, MIX and ADD are the same op here. */
    if (ELEM(mixmode, CDT_MIX_MIX, CDT_MIX_ADD)) {
      add_v3_v3v3(no_tmp, no_dst, no_src);
      normalize_v3(no_tmp);
    }
    else if (mixmode == CDT_MIX_SUB) {
      sub_v3_v3v3(no_tmp, no_dst, no_src);
      normalize_v3(no_tmp);
    }
    else if (mixmode == CDT_MIX_MUL) {
      mul_v3_v3v3(no_tmp, no_dst, no_src);
      normalize_v3(no_tmp);
    }
    else {
      copy_v3_v3(no_tmp, no_src);
    }
    interp_v3_v3v3_slerp_safe(no_dst, no_dst, no_tmp, mixfactor);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Callbacks for (#MTFace, #CD_MTFACE)
 * \{ */

static void layerCopy_tface(const void *source, void *dest, const int count)
{
  const MTFace *source_tf = (const MTFace *)source;
  MTFace *dest_tf = (MTFace *)dest;
  for (int i = 0; i < count; i++) {
    dest_tf[i] = source_tf[i];
  }
}

static void layerInterp_tface(const void **sources,
                              const float *weights,
                              const int count,
                              void *dest)
{
  MTFace *tf = static_cast<MTFace *>(dest);
  float uv[4][2] = {{0.0f}};

  for (int i = 0; i < count; i++) {
    const float interp_weight = weights[i];
    const MTFace *src = static_cast<const MTFace *>(sources[i]);

    for (int j = 0; j < 4; j++) {
      madd_v2_v2fl(uv[j], src->uv[j], interp_weight);
    }
  }

  /* Delay writing to the destination in case dest is in sources. */
  *tf = *(MTFace *)(*sources);
  memcpy(tf->uv, uv, sizeof(tf->uv));
}

static void layerSwap_tface(void *data, const int *corner_indices)
{
  MTFace *tf = static_cast<MTFace *>(data);
  float uv[4][2];

  for (int j = 0; j < 4; j++) {
    const int source_index = corner_indices[j];
    copy_v2_v2(uv[j], tf->uv[source_index]);
  }

  memcpy(tf->uv, uv, sizeof(tf->uv));
}

static void layerDefault_tface(void *data, const int count)
{
  static MTFace default_tf = {{{0, 0}, {1, 0}, {1, 1}, {0, 1}}};
  MTFace *tf = (MTFace *)data;

  for (int i = 0; i < count; i++) {
    tf[i] = default_tf;
  }
}

static int layerMaxNum_tface()
{
  return MAX_MTFACE;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Callbacks for (#MFloatProperty, #CD_PROP_FLOAT)
 * \{ */

static void layerCopy_propFloat(const void *source, void *dest, const int count)
{
  memcpy(dest, source, sizeof(MFloatProperty) * count);
}

static void layerInterp_propFloat(const void **sources,
                                  const float *weights,
                                  const int count,
                                  void *dest)
{
  float result = 0.0f;
  for (int i = 0; i < count; i++) {
    const float interp_weight = weights[i];
    const float src = *(const float *)sources[i];
    result += src * interp_weight;
  }
  *(float *)dest = result;
}

static bool layerValidate_propFloat(void *data, const uint totitems, const bool do_fixes)
{
  MFloatProperty *fp = static_cast<MFloatProperty *>(data);
  bool has_errors = false;

  for (int i = 0; i < totitems; i++, fp++) {
    if (!isfinite(fp->f)) {
      if (do_fixes) {
        fp->f = 0.0f;
      }
      has_errors = true;
    }
  }

  return has_errors;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Callbacks for (#MIntProperty, #CD_PROP_INT32)
 * \{ */

static void layerInterp_propInt(const void **sources,
                                const float *weights,
                                const int count,
                                void *dest)
{
  float result = 0.0f;
  for (const int i : IndexRange(count)) {
    const float weight = weights[i];
    const float src = *static_cast<const int *>(sources[i]);
    result += src * weight;
  }
  const int rounded_result = int(round(result));
  *static_cast<int *>(dest) = rounded_result;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Callbacks for (#MStringProperty, #CD_PROP_STRING)
 * \{ */

static void layerCopy_propString(const void *source, void *dest, const int count)
{
  memcpy(dest, source, sizeof(MStringProperty) * count);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Callbacks for (#OrigSpaceFace, #CD_ORIGSPACE)
 * \{ */

static void layerCopy_origspace_face(const void *source, void *dest, const int count)
{
  const OrigSpaceFace *source_tf = (const OrigSpaceFace *)source;
  OrigSpaceFace *dest_tf = (OrigSpaceFace *)dest;

  for (int i = 0; i < count; i++) {
    dest_tf[i] = source_tf[i];
  }
}

static void layerInterp_origspace_face(const void **sources,
                                       const float *weights,
                                       const int count,
                                       void *dest)
{
  OrigSpaceFace *osf = static_cast<OrigSpaceFace *>(dest);
  float uv[4][2] = {{0.0f}};

  for (int i = 0; i < count; i++) {
    const float interp_weight = weights[i];
    const OrigSpaceFace *src = static_cast<const OrigSpaceFace *>(sources[i]);

    for (int j = 0; j < 4; j++) {
      madd_v2_v2fl(uv[j], src->uv[j], interp_weight);
    }
  }

  /* Delay writing to the destination in case dest is in sources. */
  memcpy(osf->uv, uv, sizeof(osf->uv));
}

static void layerSwap_origspace_face(void *data, const int *corner_indices)
{
  OrigSpaceFace *osf = static_cast<OrigSpaceFace *>(data);
  float uv[4][2];

  for (int j = 0; j < 4; j++) {
    copy_v2_v2(uv[j], osf->uv[corner_indices[j]]);
  }
  memcpy(osf->uv, uv, sizeof(osf->uv));
}

static void layerDefault_origspace_face(void *data, const int count)
{
  static OrigSpaceFace default_osf = {{{0, 0}, {1, 0}, {1, 1}, {0, 1}}};
  OrigSpaceFace *osf = (OrigSpaceFace *)data;

  for (int i = 0; i < count; i++) {
    osf[i] = default_osf;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Callbacks for (#MDisps, #CD_MDISPS)
 * \{ */

static void layerSwap_mdisps(void *data, const int *ci)
{
  MDisps *s = static_cast<MDisps *>(data);

  if (s->disps) {
    int nverts = (ci[1] == 3) ? 4 : 3; /* silly way to know vertex count of face */
    int corners = multires_mdisp_corners(s);
    int cornersize = s->totdisp / corners;

    if (corners != nverts) {
      /* happens when face changed vertex count in edit mode
       * if it happened, just forgot displacement */

      MEM_freeN(s->disps);
      s->totdisp = (s->totdisp / corners) * nverts;
      s->disps = MEM_calloc_arrayN<float[3]>(s->totdisp, "mdisp swap");
      return;
    }

    float (*d)[3] = MEM_calloc_arrayN<float[3]>(s->totdisp, "mdisps swap");

    for (int S = 0; S < corners; S++) {
      memcpy(d + cornersize * S, s->disps + cornersize * ci[S], sizeof(float[3]) * cornersize);
    }

    MEM_freeN(s->disps);
    s->disps = d;
  }
}

static void layerCopy_mdisps(const void *source, void *dest, const int count)
{
  const MDisps *s = static_cast<const MDisps *>(source);
  MDisps *d = static_cast<MDisps *>(dest);

  for (int i = 0; i < count; i++) {
    if (s[i].disps) {
      d[i].disps = static_cast<float (*)[3]>(MEM_dupallocN(s[i].disps));
      d[i].hidden = static_cast<uint *>(MEM_dupallocN(s[i].hidden));
    }
    else {
      d[i].disps = nullptr;
      d[i].hidden = nullptr;
    }

    /* still copy even if not in memory, displacement can be external */
    d[i].totdisp = s[i].totdisp;
    d[i].level = s[i].level;
  }
}

static void layerFree_mdisps(void *data, const int count)
{
  for (MDisps &d : MutableSpan(static_cast<MDisps *>(data), count)) {
    MEM_SAFE_FREE(d.disps);
    MEM_SAFE_FREE(d.hidden);
    d.totdisp = 0;
    d.level = 0;
  }
}

static void layerConstruct_mdisps(void *data, const int count)
{
  std::fill_n(static_cast<MDisps *>(data), count, MDisps{});
}

static bool layerRead_mdisps(CDataFile *cdf, void *data, const int count)
{
  MDisps *d = static_cast<MDisps *>(data);

  for (int i = 0; i < count; i++) {
    if (!d[i].disps) {
      d[i].disps = MEM_calloc_arrayN<float[3]>(d[i].totdisp, "mdisps read");
    }

    if (!cdf_read_data(cdf, sizeof(float[3]) * d[i].totdisp, d[i].disps)) {
      CLOG_ERROR(&LOG, "failed to read multires displacement %d/%d %d", i, count, d[i].totdisp);
      return false;
    }
  }

  return true;
}

static bool layerWrite_mdisps(CDataFile *cdf, const void *data, const int count)
{
  const MDisps *d = static_cast<const MDisps *>(data);

  for (int i = 0; i < count; i++) {
    if (!cdf_write_data(cdf, sizeof(float[3]) * d[i].totdisp, d[i].disps)) {
      CLOG_ERROR(&LOG, "failed to write multires displacement %d/%d %d", i, count, d[i].totdisp);
      return false;
    }
  }

  return true;
}

static size_t layerFilesize_mdisps(CDataFile * /*cdf*/, const void *data, const int count)
{
  const MDisps *d = static_cast<const MDisps *>(data);
  size_t size = 0;

  for (int i = 0; i < count; i++) {
    size += sizeof(float[3]) * d[i].totdisp;
  }

  return size;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Callbacks for (#CD_BM_ELEM_PYPTR)
 * \{ */

/* copy just zeros in this case */
static void layerCopy_bmesh_elem_py_ptr(const void * /*source*/, void *dest, const int count)
{
  const int size = sizeof(void *);

  for (int i = 0; i < count; i++) {
    void **ptr = (void **)POINTER_OFFSET(dest, i * size);
    *ptr = nullptr;
  }
}

#ifndef WITH_PYTHON
void bpy_bm_generic_invalidate(struct BPy_BMGeneric * /*self*/)
{
  /* dummy */
}
#endif

static void layerFree_bmesh_elem_py_ptr(void *data, const int count)
{
  for (int i = 0; i < count; i++) {
    void **ptr = (void **)POINTER_OFFSET(data, i * sizeof(void *));
    if (*ptr) {
      bpy_bm_generic_invalidate(static_cast<BPy_BMGeneric *>(*ptr));
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Callbacks for (#GridPaintMask, #CD_GRID_PAINT_MASK)
 * \{ */

static void layerCopy_grid_paint_mask(const void *source, void *dest, const int count)
{
  const GridPaintMask *s = static_cast<const GridPaintMask *>(source);
  GridPaintMask *d = static_cast<GridPaintMask *>(dest);

  for (int i = 0; i < count; i++) {
    if (s[i].data) {
      d[i].data = static_cast<float *>(MEM_dupallocN(s[i].data));
      d[i].level = s[i].level;
    }
    else {
      d[i].data = nullptr;
      d[i].level = 0;
    }
  }
}

static void layerFree_grid_paint_mask(void *data, const int count)
{
  for (GridPaintMask &gpm : MutableSpan(static_cast<GridPaintMask *>(data), count)) {
    MEM_SAFE_FREE(gpm.data);
    gpm.level = 0;
  }
}

static void layerConstruct_grid_paint_mask(void *data, const int count)
{
  std::fill_n(static_cast<GridPaintMask *>(data), count, GridPaintMask{});
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Callbacks for (#MLoopCol, #CD_PROP_BYTE_COLOR)
 * \{ */

static void layerCopyValue_mloopcol(const void *source,
                                    void *dest,
                                    const int mixmode,
                                    const float mixfactor)
{
  const MLoopCol *m1 = static_cast<const MLoopCol *>(source);
  MLoopCol *m2 = static_cast<MLoopCol *>(dest);
  uchar tmp_col[4];

  if (ELEM(mixmode,
           CDT_MIX_NOMIX,
           CDT_MIX_REPLACE_ABOVE_THRESHOLD,
           CDT_MIX_REPLACE_BELOW_THRESHOLD))
  {
    /* Modes that do a full copy or nothing. */
    if (ELEM(mixmode, CDT_MIX_REPLACE_ABOVE_THRESHOLD, CDT_MIX_REPLACE_BELOW_THRESHOLD)) {
      /* TODO: Check for a real valid way to get 'factor' value of our dest color? */
      const float f = (float(m2->r) + float(m2->g) + float(m2->b)) / 3.0f;
      if (mixmode == CDT_MIX_REPLACE_ABOVE_THRESHOLD && f < mixfactor) {
        return; /* Do Nothing! */
      }
      if (mixmode == CDT_MIX_REPLACE_BELOW_THRESHOLD && f > mixfactor) {
        return; /* Do Nothing! */
      }
    }
    m2->r = m1->r;
    m2->g = m1->g;
    m2->b = m1->b;
    m2->a = m1->a;
  }
  else { /* Modes that support 'real' mix factor. */
    uchar src[4] = {m1->r, m1->g, m1->b, m1->a};
    uchar dst[4] = {m2->r, m2->g, m2->b, m2->a};

    if (mixmode == CDT_MIX_MIX) {
      blend_color_mix_byte(tmp_col, dst, src);
    }
    else if (mixmode == CDT_MIX_ADD) {
      blend_color_add_byte(tmp_col, dst, src);
    }
    else if (mixmode == CDT_MIX_SUB) {
      blend_color_sub_byte(tmp_col, dst, src);
    }
    else if (mixmode == CDT_MIX_MUL) {
      blend_color_mul_byte(tmp_col, dst, src);
    }
    else {
      memcpy(tmp_col, src, sizeof(tmp_col));
    }

    blend_color_interpolate_byte(dst, dst, tmp_col, mixfactor);

    m2->r = char(dst[0]);
    m2->g = char(dst[1]);
    m2->b = char(dst[2]);
    m2->a = char(dst[3]);
  }
}

static bool layerEqual_mloopcol(const void *data1, const void *data2)
{
  const MLoopCol *m1 = static_cast<const MLoopCol *>(data1);
  const MLoopCol *m2 = static_cast<const MLoopCol *>(data2);
  float r, g, b, a;

  r = m1->r - m2->r;
  g = m1->g - m2->g;
  b = m1->b - m2->b;
  a = m1->a - m2->a;

  return r * r + g * g + b * b + a * a < 0.001f;
}

static void layerMultiply_mloopcol(void *data, const float fac)
{
  MLoopCol *m = static_cast<MLoopCol *>(data);

  m->r = float(m->r) * fac;
  m->g = float(m->g) * fac;
  m->b = float(m->b) * fac;
  m->a = float(m->a) * fac;
}

static void layerAdd_mloopcol(void *data1, const void *data2)
{
  MLoopCol *m = static_cast<MLoopCol *>(data1);
  const MLoopCol *m2 = static_cast<const MLoopCol *>(data2);

  m->r += m2->r;
  m->g += m2->g;
  m->b += m2->b;
  m->a += m2->a;
}

static void layerDoMinMax_mloopcol(const void *data, void *vmin, void *vmax)
{
  const MLoopCol *m = static_cast<const MLoopCol *>(data);
  MLoopCol *min = static_cast<MLoopCol *>(vmin);
  MLoopCol *max = static_cast<MLoopCol *>(vmax);

  min->r = std::min(m->r, min->r);
  min->g = std::min(m->g, min->g);
  min->b = std::min(m->b, min->b);
  min->a = std::min(m->a, min->a);
  max->r = std::max(m->r, max->r);
  max->g = std::max(m->g, max->g);
  max->b = std::max(m->b, max->b);
  max->a = std::max(m->a, max->a);
}

static void layerInitMinMax_mloopcol(void *vmin, void *vmax)
{
  MLoopCol *min = static_cast<MLoopCol *>(vmin);
  MLoopCol *max = static_cast<MLoopCol *>(vmax);

  min->r = 255;
  min->g = 255;
  min->b = 255;
  min->a = 255;

  max->r = 0;
  max->g = 0;
  max->b = 0;
  max->a = 0;
}

static void layerDefault_mloopcol(void *data, const int count)
{
  MLoopCol default_mloopcol = {255, 255, 255, 255};
  MLoopCol *mlcol = (MLoopCol *)data;
  for (int i = 0; i < count; i++) {
    mlcol[i] = default_mloopcol;
  }
}

static void layerInterp_mloopcol(const void **sources, const float *weights, int count, void *dest)
{
  MLoopCol *mc = static_cast<MLoopCol *>(dest);
  struct {
    float a;
    float r;
    float g;
    float b;
  } col = {0};

  for (int i = 0; i < count; i++) {
    const float interp_weight = weights[i];
    const MLoopCol *src = static_cast<const MLoopCol *>(sources[i]);
    col.r += src->r * interp_weight;
    col.g += src->g * interp_weight;
    col.b += src->b * interp_weight;
    col.a += src->a * interp_weight;
  }

  /* Subdivide smooth or fractal can cause problems without clamping
   * although weights should also not cause this situation */

  /* Also delay writing to the destination in case dest is in sources. */
  mc->r = round_fl_to_uchar_clamp(col.r);
  mc->g = round_fl_to_uchar_clamp(col.g);
  mc->b = round_fl_to_uchar_clamp(col.b);
  mc->a = round_fl_to_uchar_clamp(col.a);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Callbacks for #OrigSpaceLoop
 * \{ */

/* origspace is almost exact copy of #MLoopUV, keep in sync. */
static void layerCopyValue_mloop_origspace(const void *source,
                                           void *dest,
                                           const int /*mixmode*/,
                                           const float /*mixfactor*/)
{
  const OrigSpaceLoop *luv1 = static_cast<const OrigSpaceLoop *>(source);
  OrigSpaceLoop *luv2 = static_cast<OrigSpaceLoop *>(dest);

  copy_v2_v2(luv2->uv, luv1->uv);
}

static bool layerEqual_mloop_origspace(const void *data1, const void *data2)
{
  const OrigSpaceLoop *luv1 = static_cast<const OrigSpaceLoop *>(data1);
  const OrigSpaceLoop *luv2 = static_cast<const OrigSpaceLoop *>(data2);

  return len_squared_v2v2(luv1->uv, luv2->uv) < 0.00001f;
}

static void layerMultiply_mloop_origspace(void *data, const float fac)
{
  OrigSpaceLoop *luv = static_cast<OrigSpaceLoop *>(data);

  mul_v2_fl(luv->uv, fac);
}

static void layerInitMinMax_mloop_origspace(void *vmin, void *vmax)
{
  OrigSpaceLoop *min = static_cast<OrigSpaceLoop *>(vmin);
  OrigSpaceLoop *max = static_cast<OrigSpaceLoop *>(vmax);

  INIT_MINMAX2(min->uv, max->uv);
}

static void layerDoMinMax_mloop_origspace(const void *data, void *vmin, void *vmax)
{
  const OrigSpaceLoop *luv = static_cast<const OrigSpaceLoop *>(data);
  OrigSpaceLoop *min = static_cast<OrigSpaceLoop *>(vmin);
  OrigSpaceLoop *max = static_cast<OrigSpaceLoop *>(vmax);

  minmax_v2v2_v2(min->uv, max->uv, luv->uv);
}

static void layerAdd_mloop_origspace(void *data1, const void *data2)
{
  OrigSpaceLoop *l1 = static_cast<OrigSpaceLoop *>(data1);
  const OrigSpaceLoop *l2 = static_cast<const OrigSpaceLoop *>(data2);

  add_v2_v2(l1->uv, l2->uv);
}

static void layerInterp_mloop_origspace(const void **sources,
                                        const float *weights,
                                        int count,
                                        void *dest)
{
  float uv[2];
  zero_v2(uv);

  for (int i = 0; i < count; i++) {
    const float interp_weight = weights[i];
    const OrigSpaceLoop *src = static_cast<const OrigSpaceLoop *>(sources[i]);
    madd_v2_v2fl(uv, src->uv, interp_weight);
  }

  /* Delay writing to the destination in case dest is in sources. */
  copy_v2_v2(((OrigSpaceLoop *)dest)->uv, uv);
}
/* --- end copy */

static void layerInterp_mcol(const void **sources,
                             const float *weights,
                             const int count,
                             void *dest)
{
  MCol *mc = static_cast<MCol *>(dest);
  struct {
    float a;
    float r;
    float g;
    float b;
  } col[4] = {{0.0f}};

  for (int i = 0; i < count; i++) {
    const float interp_weight = weights[i];

    for (int j = 0; j < 4; j++) {
      const MCol *src = static_cast<const MCol *>(sources[i]);
      col[j].a += src[j].a * interp_weight;
      col[j].r += src[j].r * interp_weight;
      col[j].g += src[j].g * interp_weight;
      col[j].b += src[j].b * interp_weight;
    }
  }

  /* Delay writing to the destination in case dest is in sources. */
  for (int j = 0; j < 4; j++) {

    /* Subdivide smooth or fractal can cause problems without clamping
     * although weights should also not cause this situation */
    mc[j].a = round_fl_to_uchar_clamp(col[j].a);
    mc[j].r = round_fl_to_uchar_clamp(col[j].r);
    mc[j].g = round_fl_to_uchar_clamp(col[j].g);
    mc[j].b = round_fl_to_uchar_clamp(col[j].b);
  }
}

static void layerSwap_mcol(void *data, const int *corner_indices)
{
  MCol *mcol = static_cast<MCol *>(data);
  MCol col[4];

  for (int j = 0; j < 4; j++) {
    col[j] = mcol[corner_indices[j]];
  }

  memcpy(mcol, col, sizeof(col));
}

static void layerDefault_mcol(void *data, const int count)
{
  static MCol default_mcol = {255, 255, 255, 255};
  MCol *mcol = (MCol *)data;

  for (int i = 0; i < 4 * count; i++) {
    mcol[i] = default_mcol;
  }
}

static void layerDefault_origindex(void *data, const int count)
{
  copy_vn_i((int *)data, count, ORIGINDEX_NONE);
}

static void layerInterp_shapekey(const void **sources, const float *weights, int count, void *dest)
{
  float **in = (float **)sources;

  if (count <= 0) {
    return;
  }

  float co[3];
  zero_v3(co);

  for (int i = 0; i < count; i++) {
    const float interp_weight = weights[i];
    madd_v3_v3fl(co, in[i], interp_weight);
  }

  /* Delay writing to the destination in case dest is in sources. */
  copy_v3_v3((float *)dest, co);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Callbacks for (#MVertSkin, #CD_MVERT_SKIN)
 * \{ */

static void layerDefault_mvert_skin(void *data, const int count)
{
  MVertSkin *vs = static_cast<MVertSkin *>(data);

  for (int i = 0; i < count; i++) {
    copy_v3_fl(vs[i].radius, 0.25f);
    vs[i].flag = 0;
  }
}

static void layerCopy_mvert_skin(const void *source, void *dest, const int count)
{
  memcpy(dest, source, sizeof(MVertSkin) * count);
}

static void layerInterp_mvert_skin(const void **sources,
                                   const float *weights,
                                   int count,
                                   void *dest)
{
  float radius[3];
  zero_v3(radius);

  for (int i = 0; i < count; i++) {
    const float interp_weight = weights[i];
    const MVertSkin *vs_src = static_cast<const MVertSkin *>(sources[i]);

    madd_v3_v3fl(radius, vs_src->radius, interp_weight);
  }

  /* Delay writing to the destination in case dest is in sources. */
  MVertSkin *vs_dst = static_cast<MVertSkin *>(dest);
  copy_v3_v3(vs_dst->radius, radius);
  vs_dst->flag &= ~MVERT_SKIN_ROOT;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Callbacks for (`short[4][3]`, #CD_TESSLOOPNORMAL)
 * \{ */

static void layerSwap_flnor(void *data, const int *corner_indices)
{
  short (*flnors)[4][3] = static_cast<short (*)[4][3]>(data);
  short nors[4][3];
  int i = 4;

  while (i--) {
    copy_v3_v3_short(nors[i], (*flnors)[corner_indices[i]]);
  }

  memcpy(flnors, nors, sizeof(nors));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Callbacks for (#MPropCol, #CD_PROP_COLOR)
 * \{ */

static void layerCopyValue_propcol(const void *source,
                                   void *dest,
                                   const int mixmode,
                                   const float mixfactor)
{
  const MPropCol *m1 = static_cast<const MPropCol *>(source);
  MPropCol *m2 = static_cast<MPropCol *>(dest);
  float tmp_col[4];

  if (ELEM(mixmode,
           CDT_MIX_NOMIX,
           CDT_MIX_REPLACE_ABOVE_THRESHOLD,
           CDT_MIX_REPLACE_BELOW_THRESHOLD))
  {
    /* Modes that do a full copy or nothing. */
    if (ELEM(mixmode, CDT_MIX_REPLACE_ABOVE_THRESHOLD, CDT_MIX_REPLACE_BELOW_THRESHOLD)) {
      /* TODO: Check for a real valid way to get 'factor' value of our dest color? */
      const float f = (m2->color[0] + m2->color[1] + m2->color[2]) / 3.0f;
      if (mixmode == CDT_MIX_REPLACE_ABOVE_THRESHOLD && f < mixfactor) {
        return; /* Do Nothing! */
      }
      if (mixmode == CDT_MIX_REPLACE_BELOW_THRESHOLD && f > mixfactor) {
        return; /* Do Nothing! */
      }
    }
    copy_v4_v4(m2->color, m1->color);
  }
  else { /* Modes that support 'real' mix factor. */
    if (mixmode == CDT_MIX_MIX) {
      blend_color_mix_float(tmp_col, m2->color, m1->color);
    }
    else if (mixmode == CDT_MIX_ADD) {
      blend_color_add_float(tmp_col, m2->color, m1->color);
    }
    else if (mixmode == CDT_MIX_SUB) {
      blend_color_sub_float(tmp_col, m2->color, m1->color);
    }
    else if (mixmode == CDT_MIX_MUL) {
      blend_color_mul_float(tmp_col, m2->color, m1->color);
    }
    else {
      memcpy(tmp_col, m1->color, sizeof(tmp_col));
    }
    blend_color_interpolate_float(m2->color, m2->color, tmp_col, mixfactor);
  }
}

static bool layerEqual_propcol(const void *data1, const void *data2)
{
  const MPropCol *m1 = static_cast<const MPropCol *>(data1);
  const MPropCol *m2 = static_cast<const MPropCol *>(data2);
  float tot = 0;

  for (int i = 0; i < 4; i++) {
    float c = (m1->color[i] - m2->color[i]);
    tot += c * c;
  }

  return tot < 0.001f;
}

static void layerMultiply_propcol(void *data, const float fac)
{
  MPropCol *m = static_cast<MPropCol *>(data);
  mul_v4_fl(m->color, fac);
}

static void layerAdd_propcol(void *data1, const void *data2)
{
  MPropCol *m = static_cast<MPropCol *>(data1);
  const MPropCol *m2 = static_cast<const MPropCol *>(data2);
  add_v4_v4(m->color, m2->color);
}

static void layerDoMinMax_propcol(const void *data, void *vmin, void *vmax)
{
  const MPropCol *m = static_cast<const MPropCol *>(data);
  MPropCol *min = static_cast<MPropCol *>(vmin);
  MPropCol *max = static_cast<MPropCol *>(vmax);
  minmax_v4v4_v4(min->color, max->color, m->color);
}

static void layerInitMinMax_propcol(void *vmin, void *vmax)
{
  MPropCol *min = static_cast<MPropCol *>(vmin);
  MPropCol *max = static_cast<MPropCol *>(vmax);

  copy_v4_fl(min->color, FLT_MAX);
  copy_v4_fl(max->color, FLT_MIN);
}

static void layerDefault_propcol(void *data, const int count)
{
  /* Default to white, full alpha. */
  MPropCol default_propcol = {{1.0f, 1.0f, 1.0f, 1.0f}};
  MPropCol *pcol = (MPropCol *)data;
  for (int i = 0; i < count; i++) {
    copy_v4_v4(pcol[i].color, default_propcol.color);
  }
}

static void layerInterp_propcol(const void **sources, const float *weights, int count, void *dest)
{
  MPropCol *mc = static_cast<MPropCol *>(dest);
  float col[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  for (int i = 0; i < count; i++) {
    const float interp_weight = weights[i];
    const MPropCol *src = static_cast<const MPropCol *>(sources[i]);
    madd_v4_v4fl(col, src->color, interp_weight);
  }
  copy_v4_v4(mc->color, col);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Callbacks for (#vec3f, #CD_PROP_FLOAT3)
 * \{ */

static void layerInterp_propfloat3(const void **sources,
                                   const float *weights,
                                   int count,
                                   void *dest)
{
  vec3f result = {0.0f, 0.0f, 0.0f};
  for (int i = 0; i < count; i++) {
    const float interp_weight = weights[i];
    const vec3f *src = static_cast<const vec3f *>(sources[i]);
    madd_v3_v3fl(&result.x, &src->x, interp_weight);
  }
  copy_v3_v3((float *)dest, &result.x);
}

static void layerMultiply_propfloat3(void *data, const float fac)
{
  vec3f *vec = static_cast<vec3f *>(data);
  vec->x *= fac;
  vec->y *= fac;
  vec->z *= fac;
}

static void layerAdd_propfloat3(void *data1, const void *data2)
{
  vec3f *vec1 = static_cast<vec3f *>(data1);
  const vec3f *vec2 = static_cast<const vec3f *>(data2);
  vec1->x += vec2->x;
  vec1->y += vec2->y;
  vec1->z += vec2->z;
}

static bool layerValidate_propfloat3(void *data, const uint totitems, const bool do_fixes)
{
  float *values = static_cast<float *>(data);
  bool has_errors = false;
  for (int i = 0; i < totitems * 3; i++) {
    if (!isfinite(values[i])) {
      if (do_fixes) {
        values[i] = 0.0f;
      }
      has_errors = true;
    }
  }
  return has_errors;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Callbacks for (#vec2f, #CD_PROP_FLOAT2)
 * \{ */

static void layerInterp_propfloat2(const void **sources,
                                   const float *weights,
                                   int count,
                                   void *dest)
{
  vec2f result = {0.0f, 0.0f};
  for (int i = 0; i < count; i++) {
    const float interp_weight = weights[i];
    const vec2f *src = static_cast<const vec2f *>(sources[i]);
    madd_v2_v2fl(&result.x, &src->x, interp_weight);
  }
  copy_v2_v2((float *)dest, &result.x);
}

static void layerMultiply_propfloat2(void *data, const float fac)
{
  vec2f *vec = static_cast<vec2f *>(data);
  vec->x *= fac;
  vec->y *= fac;
}

static void layerAdd_propfloat2(void *data1, const void *data2)
{
  vec2f *vec1 = static_cast<vec2f *>(data1);
  const vec2f *vec2 = static_cast<const vec2f *>(data2);
  vec1->x += vec2->x;
  vec1->y += vec2->y;
}

static bool layerValidate_propfloat2(void *data, const uint totitems, const bool do_fixes)
{
  float *values = static_cast<float *>(data);
  bool has_errors = false;
  for (int i = 0; i < totitems * 2; i++) {
    if (!isfinite(values[i])) {
      if (do_fixes) {
        values[i] = 0.0f;
      }
      has_errors = true;
    }
  }
  return has_errors;
}

static bool layerEqual_propfloat2(const void *data1, const void *data2)
{
  const float2 &a = *static_cast<const float2 *>(data1);
  const float2 &b = *static_cast<const float2 *>(data2);
  return blender::math::distance_squared(a, b) < 0.00001f;
}

static void layerInitMinMax_propfloat2(void *vmin, void *vmax)
{
  float2 &min = *static_cast<float2 *>(vmin);
  float2 &max = *static_cast<float2 *>(vmax);
  INIT_MINMAX2(min, max);
}

static void layerDoMinMax_propfloat2(const void *data, void *vmin, void *vmax)
{
  const float2 &value = *static_cast<const float2 *>(data);
  float2 &a = *static_cast<float2 *>(vmin);
  float2 &b = *static_cast<float2 *>(vmax);
  blender::math::min_max(value, a, b);
}

static void layerCopyValue_propfloat2(const void *source,
                                      void *dest,
                                      const int mixmode,
                                      const float mixfactor)
{
  const float2 &a = *static_cast<const float2 *>(source);
  float2 &b = *static_cast<float2 *>(dest);

  /* We only support a limited subset of advanced mixing here-
   * namely the mixfactor interpolation. */
  if (mixmode == CDT_MIX_NOMIX) {
    b = a;
  }
  else {
    b = blender::math::interpolate(b, a, mixfactor);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Callbacks for (`bool`, #CD_PROP_BOOL)
 * \{ */

static void layerInterp_propbool(const void **sources, const float *weights, int count, void *dest)
{
  bool result = false;
  for (int i = 0; i < count; i++) {
    const float interp_weight = weights[i];
    const bool src = *(const bool *)sources[i];
    result |= src && (interp_weight > 0.0f);
  }
  *(bool *)dest = result;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Callbacks for (#math::Quaternion, #CD_PROP_QUATERNION)
 * \{ */

static void layerDefault_propquaternion(void *data, const int count)
{
  using namespace blender;
  MutableSpan(static_cast<math::Quaternion *>(data), count).fill(math::Quaternion::identity());
}

static void layerInterp_propquaternion(const void **sources,
                                       const float *weights,
                                       int count,
                                       void *dest)
{
  using blender::math::Quaternion;
  Quaternion result;
  blender::bke::attribute_math::DefaultMixer<Quaternion> mixer({&result, 1},
                                                               Quaternion::identity());

  for (int i = 0; i < count; i++) {
    const float interp_weight = weights[i];
    const Quaternion *src = static_cast<const Quaternion *>(sources[i]);
    mixer.mix_in(0, *src, interp_weight);
  }
  mixer.finalize();
  *static_cast<Quaternion *>(dest) = result;
}

/* -------------------------------------------------------------------- */
/** \name Callbacks for (#math::Quaternion, #CD_PROP_FLOAT4X4)
 * \{ */

static void layerDefault_propfloat4x4(void *data, const int count)
{
  using namespace blender;
  MutableSpan(static_cast<float4x4 *>(data), count).fill(float4x4::identity());
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Layer Type Information (#LAYERTYPEINFO)
 * \{ */

static const LayerTypeInfo LAYERTYPEINFO[CD_NUMTYPES] = {
    /* 0: CD_MVERT */ /* DEPRECATED */
    {sizeof(MVert),
     alignof(MVert),
     "MVert",
     1,
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     nullptr},
    /* 1: CD_MSTICKY */ /* DEPRECATED */
    {sizeof(float[2]),
     alignof(float2),
     "",
     1,
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     nullptr},
    /* 2: CD_MDEFORMVERT */
    {sizeof(MDeformVert),
     alignof(MDeformVert),
     "MDeformVert",
     1,
     nullptr,
     layerCopy_mdeformvert,
     layerFree_mdeformvert,
     layerInterp_mdeformvert,
     nullptr,
     nullptr,
     layerConstruct_mdeformvert},
    /* 3: CD_MEDGE */ /* DEPRECATED */
    {sizeof(MEdge),
     alignof(MEdge),
     "MEdge",
     1,
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     nullptr},
    /* 4: CD_MFACE */
    {sizeof(MFace),
     alignof(MFace),
     "MFace",
     1,
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     nullptr},
    /* 5: CD_MTFACE */
    {sizeof(MTFace),
     alignof(MTFace),
     "MTFace",
     1,
     N_("UVMap"),
     layerCopy_tface,
     nullptr,
     layerInterp_tface,
     layerSwap_tface,
     nullptr,
     layerDefault_tface,
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     layerMaxNum_tface},
    /* 6: CD_MCOL */
    /* 4 MCol structs per face */
    {sizeof(MCol[4]),
     alignof(MCol[4]),
     "MCol",
     4,
     N_("Col"),
     nullptr,
     nullptr,
     layerInterp_mcol,
     layerSwap_mcol,
     layerDefault_mcol},
    /* 7: CD_ORIGINDEX */
    {sizeof(int),
     alignof(int),
     "",
     0,
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     layerDefault_origindex},
    /* 8: CD_NORMAL */
    /* 3 floats per normal vector */
    {sizeof(float[3]),
     alignof(blender::float3),
     "vec3f",
     1,
     nullptr,
     nullptr,
     nullptr,
     layerInterp_normal,
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     layerCopyValue_normal},
    /* 9: CD_FACEMAP */ /* DEPRECATED */
    {sizeof(int), alignof(int), ""},
    /* 10: CD_PROP_FLOAT */
    {sizeof(MFloatProperty),
     alignof(float),
     "MFloatProperty",
     1,
     N_("Float"),
     layerCopy_propFloat,
     nullptr,
     layerInterp_propFloat,
     nullptr,
     nullptr,
     nullptr,
     layerValidate_propFloat},
    /* 11: CD_PROP_INT32 */
    {sizeof(MIntProperty),
     alignof(int),
     "MIntProperty",
     1,
     N_("Int"),
     nullptr,
     nullptr,
     layerInterp_propInt,
     nullptr},
    /* 12: CD_PROP_STRING */
    {sizeof(MStringProperty),
     alignof(MStringProperty),
     "MStringProperty",
     1,
     N_("String"),
     layerCopy_propString,
     nullptr,
     nullptr,
     nullptr},
    /* 13: CD_ORIGSPACE */
    {sizeof(OrigSpaceFace),
     alignof(OrigSpaceFace),
     "OrigSpaceFace",
     1,
     N_("UVMap"),
     layerCopy_origspace_face,
     nullptr,
     layerInterp_origspace_face,
     layerSwap_origspace_face,
     layerDefault_origspace_face},
    /* 14: CD_ORCO */
    {sizeof(float[3]),
     alignof(blender::float3),
     "",
     0,
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     nullptr},
    /* 15: CD_MTEXPOLY */ /* DEPRECATED */
    /* NOTE: when we expose the UV Map / TexFace split to the user,
     * change this back to face Texture. */
    {sizeof(int), alignof(int), "", 0, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
    /* 16: CD_MLOOPUV */ /* DEPRECATED */
    {sizeof(MLoopUV), alignof(MLoopUV), "MLoopUV", 1, N_("UVMap")},
    /* 17: CD_PROP_BYTE_COLOR */
    {sizeof(MLoopCol),
     alignof(MLoopCol),
     "MLoopCol",
     1,
     N_("Col"),
     nullptr,
     nullptr,
     layerInterp_mloopcol,
     nullptr,
     layerDefault_mloopcol,
     nullptr,
     nullptr,
     layerEqual_mloopcol,
     layerMultiply_mloopcol,
     layerInitMinMax_mloopcol,
     layerAdd_mloopcol,
     layerDoMinMax_mloopcol,
     layerCopyValue_mloopcol,
     nullptr,
     nullptr,
     nullptr,
     nullptr},
    /* 18: CD_TANGENT */ /* DEPRECATED */
    {sizeof(float[4]), alignof(float[4]), "", 0, N_("Tangent")},
    /* 19: CD_MDISPS */
    {sizeof(MDisps),
     alignof(MDisps),
     "MDisps",
     1,
     nullptr,
     layerCopy_mdisps,
     layerFree_mdisps,
     nullptr,
     layerSwap_mdisps,
     nullptr,
     layerConstruct_mdisps,
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     layerRead_mdisps,
     layerWrite_mdisps,
     layerFilesize_mdisps},
    /* 20: CD_PREVIEW_MCOL */
    {sizeof(blender::float4x4),
     alignof(blender::float4x4),
     "mat4x4f",
     1,
     N_("4 by 4 Float Matrix"),
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     layerDefault_propfloat4x4},
    /* 21: CD_ID_MCOL */ /* DEPRECATED */
    {sizeof(MCol[4]),
     alignof(MCol[4]),
     "",
     0,
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     nullptr},
    /* 22: CD_PROP_INT16_2D */
    {sizeof(blender::short2),
     alignof(blender::short2),
     "vec2s",
     1,
     N_("2D 16-Bit Integer"),
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     nullptr},
    /* 23: CD_CLOTH_ORCO */
    {sizeof(float[3]),
     alignof(float[3]),
     "",
     0,
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     nullptr},
    /* 24: CD_RECAST */
    {sizeof(MRecast),
     alignof(MRecast),
     "MRecast",
     1,
     N_("Recast"),
     nullptr,
     nullptr,
     nullptr,
     nullptr},
    /* 25: CD_MPOLY */ /* DEPRECATED */
    {sizeof(MPoly),
     alignof(MPoly),
     "MPoly",
     1,
     N_("NGon Face"),
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     nullptr},
    /* 26: CD_MLOOP */ /* DEPRECATED */
    {sizeof(MLoop),
     alignof(MLoop),
     "MLoop",
     1,
     N_("NGon Face-Vertex"),
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     nullptr},
    /* 27: CD_SHAPE_KEYINDEX */
    {sizeof(int), alignof(int), "", 0, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
    /* 28: CD_SHAPEKEY */
    {sizeof(float[3]),
     alignof(float[3]),
     "",
     0,
     N_("ShapeKey"),
     nullptr,
     nullptr,
     layerInterp_shapekey},
    /* 29: CD_BWEIGHT */ /* DEPRECATED */
    {sizeof(MFloatProperty), alignof(MFloatProperty), "MFloatProperty", 1},
    /* 30: CD_CREASE */ /* DEPRECATED */
    {sizeof(float), alignof(float), ""},
    /* 31: CD_ORIGSPACE_MLOOP */
    {sizeof(OrigSpaceLoop),
     alignof(OrigSpaceLoop),
     "OrigSpaceLoop",
     1,
     N_("OS Loop"),
     nullptr,
     nullptr,
     layerInterp_mloop_origspace,
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     layerEqual_mloop_origspace,
     layerMultiply_mloop_origspace,
     layerInitMinMax_mloop_origspace,
     layerAdd_mloop_origspace,
     layerDoMinMax_mloop_origspace,
     layerCopyValue_mloop_origspace},
    /* 32: CD_PREVIEW_MLOOPCOL */ /* DEPRECATED */ /* UNUSED */
    {},
    /* 33: CD_BM_ELEM_PYPTR */
    {sizeof(void *),
     alignof(void *),
     "",
     1,
     nullptr,
     layerCopy_bmesh_elem_py_ptr,
     layerFree_bmesh_elem_py_ptr,
     nullptr,
     nullptr,
     nullptr},
    /* 34: CD_PAINT_MASK */ /* DEPRECATED */
    {sizeof(float), alignof(float), "", 0, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
    /* 35: CD_GRID_PAINT_MASK */
    {sizeof(GridPaintMask),
     alignof(GridPaintMask),
     "GridPaintMask",
     1,
     nullptr,
     layerCopy_grid_paint_mask,
     layerFree_grid_paint_mask,
     nullptr,
     nullptr,
     nullptr,
     layerConstruct_grid_paint_mask},
    /* 36: CD_MVERT_SKIN */
    {sizeof(MVertSkin),
     alignof(MVertSkin),
     "MVertSkin",
     1,
     nullptr,
     layerCopy_mvert_skin,
     nullptr,
     layerInterp_mvert_skin,
     nullptr,
     layerDefault_mvert_skin},
    /* 37: CD_FREESTYLE_EDGE */ /* DEPRECATED */
    {sizeof(FreestyleEdge), alignof(FreestyleEdge), "FreestyleEdge", 1},
    /* 38: CD_FREESTYLE_FACE */ /* DEPRECATED */
    {sizeof(FreestyleFace), alignof(FreestyleFace), "FreestyleFace", 1},
    /* 39: CD_MLOOPTANGENT */
    {sizeof(float[4]),
     alignof(float[4]),
     "",
     0,
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     nullptr},
    /* 40: CD_TESSLOOPNORMAL */
    {sizeof(short[4][3]),
     alignof(short[4][3]),
     "",
     0,
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     layerSwap_flnor,
     nullptr},
    /* 41: CD_CUSTOMLOOPNORMAL */ /* DEPRECATED */
    {sizeof(short[2]), alignof(short[2]), "vec2s", 1},
    /* 42: CD_SCULPT_FACE_SETS */ /* DEPRECATED */
    {sizeof(int), alignof(int), ""},
    /* 43: CD_LOCATION */
    {sizeof(float[3]),
     alignof(float[3]),
     "vec3f",
     1,
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     nullptr},
    /* 44: CD_RADIUS */
    {sizeof(float),
     alignof(float),
     "MFloatProperty",
     1,
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     nullptr},
    /* 45: CD_PROP_INT8 */
    {sizeof(int8_t),
     alignof(int8_t),
     "MInt8Property",
     1,
     N_("Int8"),
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     nullptr},
    /* 46: CD_PROP_INT32_2D */
    {sizeof(blender::int2),
     alignof(blender::int2),
     "vec2i",
     1,
     N_("Int 2D"),
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     nullptr},
    /* 47: CD_PROP_COLOR */
    {sizeof(MPropCol),
     alignof(MPropCol),
     "MPropCol",
     1,
     N_("Color"),
     nullptr,
     nullptr,
     layerInterp_propcol,
     nullptr,
     layerDefault_propcol,
     nullptr,
     nullptr,
     layerEqual_propcol,
     layerMultiply_propcol,
     layerInitMinMax_propcol,
     layerAdd_propcol,
     layerDoMinMax_propcol,
     layerCopyValue_propcol,
     nullptr,
     nullptr,
     nullptr,
     nullptr},
    /* 48: CD_PROP_FLOAT3 */
    {sizeof(float[3]),
     alignof(blender::float3),
     "vec3f",
     1,
     N_("Float3"),
     nullptr,
     nullptr,
     layerInterp_propfloat3,
     nullptr,
     nullptr,
     nullptr,
     layerValidate_propfloat3,
     nullptr,
     layerMultiply_propfloat3,
     nullptr,
     layerAdd_propfloat3},
    /* 49: CD_PROP_FLOAT2 */
    {sizeof(float[2]),
     alignof(float2),
     "vec2f",
     1,
     N_("Float2"),
     nullptr,
     nullptr,
     layerInterp_propfloat2,
     nullptr,
     nullptr,
     nullptr,
     layerValidate_propfloat2,
     layerEqual_propfloat2,
     layerMultiply_propfloat2,
     layerInitMinMax_propfloat2,
     layerAdd_propfloat2,
     layerDoMinMax_propfloat2,
     layerCopyValue_propfloat2},
    /* 50: CD_PROP_BOOL */
    {sizeof(bool),
     alignof(bool),
     "bool",
     1,
     N_("Boolean"),
     nullptr,
     nullptr,
     layerInterp_propbool,
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     nullptr},
    /* 51: CD_HAIRLENGTH */ /* DEPRECATED */ /* UNUSED */
    {sizeof(float),
     alignof(float),
     "float",
     1,
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     nullptr,
     nullptr},
    /* 52: CD_PROP_QUATERNION */
    {sizeof(float[4]),
     alignof(blender::float4),
     "vec4f",
     1,
     N_("Quaternion"),
     nullptr,
     nullptr,
     layerInterp_propquaternion,
     nullptr,
     layerDefault_propquaternion},
};

static_assert(sizeof(mat4x4f) == sizeof(blender::float4x4));

static const char *LAYERTYPENAMES[CD_NUMTYPES] = {
    /*   0-4 */ "CDMVert",
    "CDMSticky",
    "CDMDeformVert",
    "CDMEdge",
    "CDMFace",
    /*   5-9 */ "CDMTFace",
    "CDMCol",
    "CDOrigIndex",
    "CDNormal",
    "CDFaceMap",
    /* 10-14 */ "CDMFloatProperty",
    "CDMIntProperty",
    "CDMStringProperty",
    "CDOrigSpace",
    "CDOrco",
    /* 15-19 */ "CDMTexPoly",
    "CDMLoopUV",
    "CDMloopCol",
    "CDTangent",
    "CDMDisps",
    /* 20-24 */ "CDPreviewMCol",
    "CDIDMCol",
    "CDTextureMCol",
    "CDClothOrco",
    "CDMRecast",

    /* BMESH ONLY */
    /* 25-29 */ "CDMPoly",
    "CDMLoop",
    "CDShapeKeyIndex",
    "CDShapeKey",
    "CDBevelWeight",
    /* 30-34 */ "CDSubSurfCrease",
    "CDOrigSpaceLoop",
    "CDPreviewLoopCol",
    "CDBMElemPyPtr",
    "CDPaintMask",
    /* 35-36 */ "CDGridPaintMask",
    "CDMVertSkin",
    /* 37-38 */ "CDFreestyleEdge",
    "CDFreestyleFace",
    /* 39-42 */ "CDMLoopTangent",
    "CDTessLoopNormal",
    "CDCustomLoopNormal",
    "CDSculptFaceGroups",
    /* 43-46 */ "CDHairPoint",
    "CDPropInt8",
    "CDHairMapping",
    "CDPoint",
    "CDPropCol",
    "CDPropFloat3",
    "CDPropFloat2",
    "CDPropBoolean",
    "CDHairLength",
    "CDPropQuaternion",
};

const CustomData_MeshMasks CD_MASK_BAREMESH = {
    /*vmask*/ CD_MASK_PROP_FLOAT3,
    /*emask*/ CD_MASK_PROP_INT32_2D,
    /*fmask*/ 0,
    /*pmask*/ 0,
    /*lmask*/ CD_MASK_PROP_INT32,
};
const CustomData_MeshMasks CD_MASK_BAREMESH_ORIGINDEX = {
    /*vmask*/ CD_MASK_PROP_FLOAT3 | CD_MASK_ORIGINDEX,
    /*emask*/ CD_MASK_PROP_INT32_2D | CD_MASK_ORIGINDEX,
    /*fmask*/ 0,
    /*pmask*/ CD_MASK_ORIGINDEX,
    /*lmask*/ CD_MASK_PROP_INT32,
};
const CustomData_MeshMasks CD_MASK_MESH = {
    /*vmask*/ (CD_MASK_PROP_FLOAT3 | CD_MASK_MDEFORMVERT | CD_MASK_MVERT_SKIN | CD_MASK_PROP_ALL),
    /*emask*/
    CD_MASK_PROP_ALL,
    /*fmask*/ 0,
    /*pmask*/
    CD_MASK_PROP_ALL,
    /*lmask*/
    (CD_MASK_MDISPS | CD_MASK_GRID_PAINT_MASK | CD_MASK_PROP_ALL),
};
const CustomData_MeshMasks CD_MASK_DERIVEDMESH = {
    /*vmask*/ (CD_MASK_ORIGINDEX | CD_MASK_MDEFORMVERT | CD_MASK_SHAPEKEY | CD_MASK_MVERT_SKIN |
               CD_MASK_ORCO | CD_MASK_CLOTH_ORCO | CD_MASK_PROP_ALL),
    /*emask*/
    (CD_MASK_ORIGINDEX | CD_MASK_PROP_ALL),
    /*fmask*/ (CD_MASK_ORIGINDEX | CD_MASK_ORIGSPACE),
    /*pmask*/
    (CD_MASK_ORIGINDEX | CD_MASK_PROP_ALL),
    /*lmask*/
    (CD_MASK_ORIGSPACE_MLOOP | CD_MASK_PROP_ALL), /* XXX: MISSING #CD_MASK_MLOOPTANGENT ? */
};
const CustomData_MeshMasks CD_MASK_BMESH = {
    /*vmask*/ (CD_MASK_MDEFORMVERT | CD_MASK_MVERT_SKIN | CD_MASK_SHAPEKEY |
               CD_MASK_SHAPE_KEYINDEX | CD_MASK_PROP_ALL),
    /*emask*/ CD_MASK_PROP_ALL,
    /*fmask*/ 0,
    /*pmask*/
    CD_MASK_PROP_ALL,
    /*lmask*/
    (CD_MASK_MDISPS | CD_MASK_GRID_PAINT_MASK | CD_MASK_PROP_ALL),
};
const CustomData_MeshMasks CD_MASK_EVERYTHING = {
    /*vmask*/ (CD_MASK_BM_ELEM_PYPTR | CD_MASK_ORIGINDEX | CD_MASK_MDEFORMVERT |
               CD_MASK_MVERT_SKIN | CD_MASK_ORCO | CD_MASK_CLOTH_ORCO | CD_MASK_SHAPEKEY |
               CD_MASK_SHAPE_KEYINDEX | CD_MASK_PROP_ALL),
    /*emask*/
    (CD_MASK_BM_ELEM_PYPTR | CD_MASK_ORIGINDEX | CD_MASK_PROP_ALL),
    /*fmask*/
    (CD_MASK_MFACE | CD_MASK_ORIGINDEX | CD_MASK_NORMAL | CD_MASK_MTFACE | CD_MASK_MCOL |
     CD_MASK_ORIGSPACE | CD_MASK_TESSLOOPNORMAL | CD_MASK_PROP_ALL),
    /*pmask*/
    (CD_MASK_BM_ELEM_PYPTR | CD_MASK_ORIGINDEX | CD_MASK_PROP_ALL),
    /*lmask*/
    (CD_MASK_BM_ELEM_PYPTR | CD_MASK_MDISPS | CD_MASK_NORMAL | CD_MASK_MLOOPTANGENT |
     CD_MASK_ORIGSPACE_MLOOP | CD_MASK_GRID_PAINT_MASK | CD_MASK_PROP_ALL),
};

static const LayerTypeInfo *layerType_getInfo(const eCustomDataType type)
{
  if (type < 0 || type >= CD_NUMTYPES) {
    return nullptr;
  }

  return &LAYERTYPEINFO[type];
}

static const char *layerType_getName(const eCustomDataType type)
{
  if (type < 0 || type >= CD_NUMTYPES) {
    return nullptr;
  }

  return LAYERTYPENAMES[type];
}

void customData_mask_layers__print(const CustomData_MeshMasks *mask)
{
  printf("verts mask=0x%" PRIx64 ":\n", mask->vmask);
  for (int i = 0; i < CD_NUMTYPES; i++) {
    if (mask->vmask & CD_TYPE_AS_MASK(eCustomDataType(i))) {
      printf("  %s\n", layerType_getName(eCustomDataType(i)));
    }
  }

  printf("edges mask=0x%" PRIx64 ":\n", mask->emask);
  for (int i = 0; i < CD_NUMTYPES; i++) {
    if (mask->emask & CD_TYPE_AS_MASK(eCustomDataType(i))) {
      printf("  %s\n", layerType_getName(eCustomDataType(i)));
    }
  }

  printf("faces mask=0x%" PRIx64 ":\n", mask->fmask);
  for (int i = 0; i < CD_NUMTYPES; i++) {
    if (mask->fmask & CD_TYPE_AS_MASK(eCustomDataType(i))) {
      printf("  %s\n", layerType_getName(eCustomDataType(i)));
    }
  }

  printf("loops mask=0x%" PRIx64 ":\n", mask->lmask);
  for (int i = 0; i < CD_NUMTYPES; i++) {
    if (mask->lmask & CD_TYPE_AS_MASK(eCustomDataType(i))) {
      printf("  %s\n", layerType_getName(eCustomDataType(i)));
    }
  }

  printf("polys mask=0x%" PRIx64 ":\n", mask->pmask);
  for (int i = 0; i < CD_NUMTYPES; i++) {
    if (mask->pmask & CD_TYPE_AS_MASK(eCustomDataType(i))) {
      printf("  %s\n", layerType_getName(eCustomDataType(i)));
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name CustomData Functions
 * \{ */

static void customData_update_offsets(CustomData *data);

static CustomDataLayer *customData_add_layer__internal(
    CustomData *data,
    eCustomDataType type,
    std::optional<eCDAllocType> alloctype,
    void *layer_data_to_assign,
    const ImplicitSharingInfo *sharing_info_to_assign,
    int totelem,
    const StringRef name);

void CustomData_update_typemap(CustomData *data)
{
  int lasttype = -1;

  for (int i = 0; i < CD_NUMTYPES; i++) {
    data->typemap[i] = -1;
  }

  for (int i = 0; i < data->totlayer; i++) {
    const eCustomDataType type = eCustomDataType(data->layers[i].type);
    if (type != lasttype) {
      data->typemap[type] = i;
      lasttype = type;
    }
  }
}

/* currently only used in BLI_assert */
#ifndef NDEBUG
static bool customdata_typemap_is_valid(const CustomData *data)
{
  CustomData data_copy = *data;
  CustomData_update_typemap(&data_copy);
  return (memcmp(data->typemap, data_copy.typemap, sizeof(data->typemap)) == 0);
}
#endif

static void *copy_layer_data(const eCustomDataType type, const void *data, const int totelem)
{
  const LayerTypeInfo &type_info = *layerType_getInfo(type);
  const int64_t size_in_bytes = int64_t(totelem) * type_info.size;
  void *new_data = MEM_mallocN_aligned(size_in_bytes, type_info.alignment, __func__);
  if (type_info.copy) {
    type_info.copy(data, new_data, totelem);
  }
  else {
    memcpy(new_data, data, size_in_bytes);
  }
  return new_data;
}

static void free_layer_data(const eCustomDataType type, const void *data, const int totelem)
{
  const LayerTypeInfo &type_info = *layerType_getInfo(type);
  if (type_info.free) {
    type_info.free(const_cast<void *>(data), totelem);
  }
  MEM_freeN(const_cast<void *>(data));
}

static bool customdata_merge_internal(const CustomData *source,
                                      CustomData *dest,
                                      const eCustomDataMask mask,
                                      const std::optional<eCDAllocType> alloctype,
                                      const int totelem)
{
  bool changed = false;

  int last_type = -1;
  int last_active = 0;
  int last_render = 0;
  int last_clone = 0;
  int last_mask = 0;
  int current_type_layer_count = 0;
  int max_current_type_layer_count = -1;

  for (int i = 0; i < source->totlayer; i++) {
    const CustomDataLayer &src_layer = source->layers[i];
    const eCustomDataType type = eCustomDataType(src_layer.type);
    const int src_layer_flag = src_layer.flag;

    if (type != last_type) {
      /* Don't exceed layer count on destination. */
      const int layernum_dst = CustomData_number_of_layers(dest, type);
      current_type_layer_count = layernum_dst;
      max_current_type_layer_count = CustomData_layertype_layers_max(type);
      last_active = src_layer.active;
      last_render = src_layer.active_rnd;
      last_clone = src_layer.active_clone;
      last_mask = src_layer.active_mask;
      last_type = type;
    }
    else {
      current_type_layer_count++;
    }

    if (src_layer_flag & CD_FLAG_NOCOPY) {
      /* Don't merge this layer because it's not supposed to leave the source data. */
      continue;
    }
    if (!(mask & CD_TYPE_AS_MASK(type))) {
      /* Don't merge this layer because it does not match the type mask. */
      continue;
    }
    if ((max_current_type_layer_count != -1) &&
        (current_type_layer_count >= max_current_type_layer_count))
    {
      /* Don't merge this layer because the maximum amount of layers of this type is reached. */
      continue;
    }
    if (CustomData_get_named_layer_index(dest, type, src_layer.name) != -1) {
      /* Don't merge this layer because it exists in the destination already. */
      continue;
    }

    void *layer_data_to_assign = nullptr;
    const ImplicitSharingInfo *sharing_info_to_assign = nullptr;
    if (!alloctype.has_value()) {
      if (src_layer.data != nullptr) {
        if (src_layer.sharing_info == nullptr) {
          /* Can't share the layer, duplicate it instead. */
          layer_data_to_assign = copy_layer_data(type, src_layer.data, totelem);
        }
        else {
          /* Share the layer. */
          layer_data_to_assign = src_layer.data;
          sharing_info_to_assign = src_layer.sharing_info;
        }
      }
    }

    CustomDataLayer *new_layer = customData_add_layer__internal(dest,
                                                                type,
                                                                alloctype,
                                                                layer_data_to_assign,
                                                                sharing_info_to_assign,
                                                                totelem,
                                                                src_layer.name);

    new_layer->uid = src_layer.uid;
    new_layer->flag |= src_layer_flag & (CD_FLAG_EXTERNAL | CD_FLAG_IN_MEMORY);
    new_layer->active = last_active;
    new_layer->active_rnd = last_render;
    new_layer->active_clone = last_clone;
    new_layer->active_mask = last_mask;
    changed = true;
  }

  CustomData_update_typemap(dest);
  return changed;
}

bool CustomData_merge(const CustomData *source,
                      CustomData *dest,
                      eCustomDataMask mask,
                      int totelem)
{
  return customdata_merge_internal(source, dest, mask, std::nullopt, totelem);
}

bool CustomData_merge_layout(const CustomData *source,
                             CustomData *dest,
                             const eCustomDataMask mask,
                             const eCDAllocType alloctype,
                             const int totelem)
{
  return customdata_merge_internal(source, dest, mask, alloctype, totelem);
}

CustomData CustomData_shallow_copy_remove_non_bmesh_attributes(const CustomData *src,
                                                               const eCustomDataMask mask)
{
  Vector<CustomDataLayer> dst_layers;
  for (const CustomDataLayer &layer : Span<CustomDataLayer>{src->layers, src->totlayer}) {
    if (BM_attribute_stored_in_bmesh_builtin(layer.name)) {
      continue;
    }
    if (!(mask & CD_TYPE_AS_MASK(eCustomDataType(layer.type)))) {
      continue;
    }
    dst_layers.append(layer);
  }

  CustomData dst = *src;
  dst.layers = MEM_calloc_arrayN<CustomDataLayer>(dst_layers.size(), __func__);
  dst.maxlayer = dst.totlayer = dst_layers.size();
  memcpy(dst.layers, dst_layers.data(), dst_layers.as_span().size_in_bytes());

  CustomData_update_typemap(&dst);

  return dst;
}

/**
 * An #ImplicitSharingInfo that knows how to free the entire referenced custom data layer
 * (including potentially separately allocated chunks like for vertex groups).
 */
class CustomDataLayerImplicitSharing : public ImplicitSharingInfo {
 private:
  const void *data_;
  int totelem_;
  const eCustomDataType type_;

 public:
  CustomDataLayerImplicitSharing(const void *data, const int totelem, const eCustomDataType type)
      : ImplicitSharingInfo(), data_(data), totelem_(totelem), type_(type)
  {
  }

 private:
  void delete_self_with_data() override
  {
    if (data_ != nullptr) {
      free_layer_data(type_, data_, totelem_);
    }
    MEM_delete(this);
  }

  void delete_data_only() override
  {
    free_layer_data(type_, data_, totelem_);
    data_ = nullptr;
    totelem_ = 0;
  }
};

/** Create a #ImplicitSharingInfo that takes ownership of the data. */
static const ImplicitSharingInfo *make_implicit_sharing_info_for_layer(const eCustomDataType type,
                                                                       const void *data,
                                                                       const int totelem)
{
  return MEM_new<CustomDataLayerImplicitSharing>(__func__, data, totelem, type);
}

/**
 * If the layer data is currently shared (hence it is immutable), create a copy that can be edited.
 */
static void ensure_layer_data_is_mutable(CustomDataLayer &layer, const int totelem)
{
  if (layer.data == nullptr) {
    return;
  }
  BLI_assert(layer.sharing_info != nullptr);
  if (layer.sharing_info->is_mutable()) {
    layer.sharing_info->tag_ensured_mutable();
  }
  else {
    const eCustomDataType type = eCustomDataType(layer.type);
    const void *old_data = layer.data;
    /* Copy the layer before removing the user because otherwise the data might be freed while
     * we're still copying from it here. */
    layer.data = copy_layer_data(type, old_data, totelem);
    layer.sharing_info->remove_user_and_delete_if_last();
    layer.sharing_info = make_implicit_sharing_info_for_layer(type, layer.data, totelem);
  }
}

[[maybe_unused]] static bool layer_is_mutable(CustomDataLayer &layer)
{
  if (!layer.data) {
    return true;
  }
  return layer.sharing_info->is_mutable();
}

void CustomData_ensure_data_is_mutable(CustomDataLayer *layer, const int totelem)
{
  ensure_layer_data_is_mutable(*layer, totelem);
}

void CustomData_ensure_layers_are_mutable(CustomData *data, int totelem)
{
  for (const int i : IndexRange(data->totlayer)) {
    ensure_layer_data_is_mutable(data->layers[i], totelem);
  }
}

void CustomData_realloc(CustomData *data,
                        const int old_size,
                        const int new_size,
                        const eCDAllocType alloctype)
{
  BLI_assert(new_size >= 0);
  for (int i = 0; i < data->totlayer; i++) {
    CustomDataLayer *layer = &data->layers[i];
    const LayerTypeInfo *typeInfo = layerType_getInfo(eCustomDataType(layer->type));
    const int64_t old_size_in_bytes = int64_t(old_size) * typeInfo->size;
    const int64_t new_size_in_bytes = int64_t(new_size) * typeInfo->size;

    void *new_layer_data = (new_size > 0) ? MEM_mallocN_aligned(
                                                new_size_in_bytes, typeInfo->alignment, __func__) :
                                            nullptr;
    if (old_size_in_bytes > 0) {
      if (new_layer_data != nullptr) {
        /* Copy data to new array. */
        if (typeInfo->copy) {
          typeInfo->copy(layer->data, new_layer_data, std::min(old_size, new_size));
        }
        else {
          BLI_assert(layer->data != nullptr);
          memcpy(new_layer_data, layer->data, std::min(old_size_in_bytes, new_size_in_bytes));
        }
      }
      BLI_assert(layer->sharing_info != nullptr);
      layer->sharing_info->remove_user_and_delete_if_last();
      layer->sharing_info = nullptr;
    }
    /* Take ownership of new array. */
    layer->data = new_layer_data;
    if (layer->data) {
      layer->sharing_info = make_implicit_sharing_info_for_layer(
          eCustomDataType(layer->type), layer->data, new_size);
    }

    if (new_size > old_size) {
      const int new_elements_num = new_size - old_size;
      void *new_elements_begin = POINTER_OFFSET(layer->data, old_size_in_bytes);
      switch (alloctype) {
        case CD_CONSTRUCT: {
          /* Initialize new values for non-trivial types. */
          if (typeInfo->construct) {
            typeInfo->construct(new_elements_begin, new_elements_num);
          }
          break;
        }
        case CD_SET_DEFAULT: {
          if (typeInfo->set_default_value) {
            typeInfo->set_default_value(new_elements_begin, new_elements_num);
          }
          else {
            memset(new_elements_begin, 0, typeInfo->size * new_elements_num);
          }
          break;
        }
      }
    }
  }
}

void CustomData_init_from(const CustomData *source,
                          CustomData *dest,
                          eCustomDataMask mask,
                          int totelem)
{
  CustomData_reset(dest);

  if (source->external) {
    dest->external = static_cast<CustomDataExternal *>(MEM_dupallocN(source->external));
  }

  CustomData_merge(source, dest, mask, totelem);
}

void CustomData_init_layout_from(const CustomData *source,
                                 CustomData *dest,
                                 eCustomDataMask mask,
                                 eCDAllocType alloctype,
                                 int totelem)
{
  CustomData_reset(dest);

  if (source->external) {
    dest->external = static_cast<CustomDataExternal *>(MEM_dupallocN(source->external));
  }

  CustomData_merge_layout(source, dest, mask, alloctype, totelem);
}

static void customData_free_layer__internal(CustomDataLayer *layer)
{
  if (!layer->sharing_info) {
    BLI_assert(!layer->data);
    return;
  }
  layer->sharing_info->remove_user_and_delete_if_last();
  layer->sharing_info = nullptr;
}

static void CustomData_external_free(CustomData *data)
{
  if (data->external) {
    MEM_freeN(data->external);
    data->external = nullptr;
  }
}

void CustomData_reset(CustomData *data)
{
  *data = CustomData{};
  copy_vn_i(data->typemap, CD_NUMTYPES, -1);
}

void CustomData_free(CustomData *data)
{
  for (int i = 0; i < data->totlayer; i++) {
    customData_free_layer__internal(&data->layers[i]);
  }

  if (data->layers) {
    MEM_freeN(data->layers);
  }

  CustomData_external_free(data);
  CustomData_reset(data);
}

static void customData_update_offsets(CustomData *data)
{
  const LayerTypeInfo *typeInfo;
  int offset = 0;

  for (int i = 0; i < data->totlayer; i++) {
    typeInfo = layerType_getInfo(eCustomDataType(data->layers[i].type));

    data->layers[i].offset = offset;
    offset += typeInfo->size;
  }

  data->totsize = offset;
  CustomData_update_typemap(data);
}

/* to use when we're in the middle of modifying layers */
static int CustomData_get_layer_index__notypemap(const CustomData *data,
                                                 const eCustomDataType type)
{
  for (int i = 0; i < data->totlayer; i++) {
    if (data->layers[i].type == type) {
      return i;
    }
  }

  return -1;
}

/* -------------------------------------------------------------------- */
/* index values to access the layers (offset from the layer start) */

int CustomData_get_layer_index(const CustomData *data, const eCustomDataType type)
{
  BLI_assert(customdata_typemap_is_valid(data));
  return data->typemap[type];
}

int CustomData_get_layer_index_n(const CustomData *data, const eCustomDataType type, const int n)
{
  BLI_assert(n >= 0);
  int i = CustomData_get_layer_index(data, type);

  if (i != -1) {
    /* If the value of n goes past the block of layers of the correct type, return -1. */
    i = (i + n < data->totlayer && data->layers[i + n].type == type) ? (i + n) : (-1);
  }

  return i;
}

int CustomData_get_named_layer_index(const CustomData *data,
                                     const eCustomDataType type,
                                     const StringRef name)
{
  for (int i = 0; i < data->totlayer; i++) {
    if (data->layers[i].type == type) {
      if (data->layers[i].name == name) {
        return i;
      }
    }
  }

  return -1;
}

int CustomData_get_named_layer_index_notype(const CustomData *data, const StringRef name)
{
  for (int i = 0; i < data->totlayer; i++) {
    if (data->layers[i].name == name) {
      return i;
    }
  }

  return -1;
}

int CustomData_get_active_layer_index(const CustomData *data, const eCustomDataType type)
{
  const int layer_index = data->typemap[type];
  BLI_assert(customdata_typemap_is_valid(data));
  return (layer_index != -1) ? layer_index + data->layers[layer_index].active : -1;
}

int CustomData_get_render_layer_index(const CustomData *data, const eCustomDataType type)
{
  const int layer_index = data->typemap[type];
  BLI_assert(customdata_typemap_is_valid(data));
  return (layer_index != -1) ? layer_index + data->layers[layer_index].active_rnd : -1;
}

int CustomData_get_clone_layer_index(const CustomData *data, const eCustomDataType type)
{
  const int layer_index = data->typemap[type];
  BLI_assert(customdata_typemap_is_valid(data));
  return (layer_index != -1) ? layer_index + data->layers[layer_index].active_clone : -1;
}

int CustomData_get_stencil_layer_index(const CustomData *data, const eCustomDataType type)
{
  const int layer_index = data->typemap[type];
  BLI_assert(customdata_typemap_is_valid(data));
  return (layer_index != -1) ? layer_index + data->layers[layer_index].active_mask : -1;
}

/* -------------------------------------------------------------------- */
/* index values per layer type */

int CustomData_get_named_layer(const CustomData *data,
                               const eCustomDataType type,
                               const StringRef name)
{
  const int named_index = CustomData_get_named_layer_index(data, type, name);
  const int layer_index = data->typemap[type];
  BLI_assert(customdata_typemap_is_valid(data));
  return (named_index != -1) ? named_index - layer_index : -1;
}

int CustomData_get_active_layer(const CustomData *data, const eCustomDataType type)
{
  const int layer_index = data->typemap[type];
  BLI_assert(customdata_typemap_is_valid(data));
  return (layer_index != -1) ? data->layers[layer_index].active : -1;
}

int CustomData_get_render_layer(const CustomData *data, const eCustomDataType type)
{
  const int layer_index = data->typemap[type];
  BLI_assert(customdata_typemap_is_valid(data));
  return (layer_index != -1) ? data->layers[layer_index].active_rnd : -1;
}

int CustomData_get_clone_layer(const CustomData *data, const eCustomDataType type)
{
  const int layer_index = data->typemap[type];
  BLI_assert(customdata_typemap_is_valid(data));
  return (layer_index != -1) ? data->layers[layer_index].active_clone : -1;
}

int CustomData_get_stencil_layer(const CustomData *data, const eCustomDataType type)
{
  const int layer_index = data->typemap[type];
  BLI_assert(customdata_typemap_is_valid(data));
  return (layer_index != -1) ? data->layers[layer_index].active_mask : -1;
}

const char *CustomData_get_active_layer_name(const CustomData *data, const eCustomDataType type)
{
  /* Get the layer index of the active layer of this type. */
  const int layer_index = CustomData_get_active_layer_index(data, type);
  return layer_index < 0 ? nullptr : data->layers[layer_index].name;
}

const char *CustomData_get_render_layer_name(const CustomData *data, const eCustomDataType type)
{
  const int layer_index = CustomData_get_render_layer_index(data, type);
  return layer_index < 0 ? nullptr : data->layers[layer_index].name;
}

void CustomData_set_layer_active(CustomData *data, const eCustomDataType type, const int n)
{
#ifndef NDEBUG
  const int layer_num = CustomData_number_of_layers(data, type);
#endif
  for (int i = 0; i < data->totlayer; i++) {
    if (data->layers[i].type == type) {
      BLI_assert(uint(n) < uint(layer_num));
      data->layers[i].active = n;
    }
  }
}

void CustomData_set_layer_render(CustomData *data, const eCustomDataType type, const int n)
{
#ifndef NDEBUG
  const int layer_num = CustomData_number_of_layers(data, type);
#endif
  for (int i = 0; i < data->totlayer; i++) {
    if (data->layers[i].type == type) {
      BLI_assert(uint(n) < uint(layer_num));
      data->layers[i].active_rnd = n;
    }
  }
}

void CustomData_set_layer_clone(CustomData *data, const eCustomDataType type, const int n)
{
#ifndef NDEBUG
  const int layer_num = CustomData_number_of_layers(data, type);
#endif
  for (int i = 0; i < data->totlayer; i++) {
    if (data->layers[i].type == type) {
      BLI_assert(uint(n) < uint(layer_num));
      data->layers[i].active_clone = n;
    }
  }
}

void CustomData_set_layer_stencil(CustomData *data, const eCustomDataType type, const int n)
{
#ifndef NDEBUG
  const int layer_num = CustomData_number_of_layers(data, type);
#endif
  for (int i = 0; i < data->totlayer; i++) {
    if (data->layers[i].type == type) {
      BLI_assert(uint(n) < uint(layer_num));
      data->layers[i].active_mask = n;
    }
  }
}

void CustomData_set_layer_active_index(CustomData *data, const eCustomDataType type, const int n)
{
#ifndef NDEBUG
  const int layer_num = CustomData_number_of_layers(data, type);
#endif
  const int layer_index = n - data->typemap[type];
  BLI_assert(customdata_typemap_is_valid(data));

  for (int i = 0; i < data->totlayer; i++) {
    if (data->layers[i].type == type) {
      BLI_assert(uint(layer_index) < uint(layer_num));
      data->layers[i].active = layer_index;
    }
  }
}

void CustomData_set_layer_render_index(CustomData *data, const eCustomDataType type, const int n)
{
#ifndef NDEBUG
  const int layer_num = CustomData_number_of_layers(data, type);
#endif
  const int layer_index = n - data->typemap[type];
  BLI_assert(customdata_typemap_is_valid(data));

  for (int i = 0; i < data->totlayer; i++) {
    if (data->layers[i].type == type) {
      BLI_assert(uint(layer_index) < uint(layer_num));
      data->layers[i].active_rnd = layer_index;
    }
  }
}

void CustomData_set_layer_clone_index(CustomData *data, const eCustomDataType type, const int n)
{
#ifndef NDEBUG
  const int layer_num = CustomData_number_of_layers(data, type);
#endif
  const int layer_index = n - data->typemap[type];
  BLI_assert(customdata_typemap_is_valid(data));

  for (int i = 0; i < data->totlayer; i++) {
    if (data->layers[i].type == type) {
      BLI_assert(uint(layer_index) < uint(layer_num));
      data->layers[i].active_clone = layer_index;
    }
  }
}

void CustomData_set_layer_flag(CustomData *data, const eCustomDataType type, const int flag)
{
  for (int i = 0; i < data->totlayer; i++) {
    if (data->layers[i].type == type) {
      data->layers[i].flag |= flag;
    }
  }
}

bool CustomData_layer_is_anonymous(const CustomData *data, eCustomDataType type, int n)
{
  const int layer_index = CustomData_get_layer_index_n(data, type, n);

  BLI_assert(layer_index >= 0);

  return blender::bke::attribute_name_is_anonymous(data->layers[layer_index].name);
}

static void customData_resize(CustomData *data, const int grow_amount)
{
  data->layers = static_cast<CustomDataLayer *>(
      MEM_reallocN(data->layers, (data->maxlayer + grow_amount) * sizeof(CustomDataLayer)));
  data->maxlayer += grow_amount;
}

static CustomDataLayer *customData_add_layer__internal(
    CustomData *data,
    const eCustomDataType type,
    const std::optional<eCDAllocType> alloctype,
    void *layer_data_to_assign,
    const ImplicitSharingInfo *sharing_info_to_assign,
    const int totelem,
    StringRef name)
{
  const LayerTypeInfo &type_info = *layerType_getInfo(type);
  int flag = 0;

  /* Some layer types only support a single layer. */
  if (!type_info.defaultname && CustomData_has_layer(data, type)) {
    /* This function doesn't support dealing with existing layer data for these layer types when
     * the layer already exists. */
    BLI_assert(layer_data_to_assign == nullptr);
    return &data->layers[CustomData_get_layer_index(data, type)];
  }

  int index = data->totlayer;
  if (index >= data->maxlayer) {
    customData_resize(data, CUSTOMDATA_GROW);
  }

  data->totlayer++;

  /* Keep layers ordered by type. */
  for (; index > 0 && data->layers[index - 1].type > type; index--) {
    data->layers[index] = data->layers[index - 1];
  }

  CustomDataLayer &new_layer = data->layers[index];

  /* Clear remaining data on the layer. The original data on the layer has been moved to another
   * index. Without this, it can happen that information from the previous layer at that index
   * leaks into the new layer. */
  new_layer = CustomDataLayer{};

  const int64_t size_in_bytes = int64_t(totelem) * type_info.size;
  const char *alloc_name = layerType_getName(type);

  if (alloctype.has_value()) {
    switch (*alloctype) {
      case CD_SET_DEFAULT: {
        if (totelem > 0) {
          new_layer.data = MEM_mallocN_aligned(size_in_bytes, type_info.alignment, alloc_name);
          if (type_info.set_default_value) {
            type_info.set_default_value(new_layer.data, totelem);
          }
          else {
            /* Alternatively, #MEM_calloc_arrayN is faster, but has no aligned version. */
            memset(new_layer.data, 0, size_in_bytes);
          }
        }
        break;
      }
      case CD_CONSTRUCT: {
        if (totelem > 0) {
          new_layer.data = MEM_mallocN_aligned(size_in_bytes, type_info.alignment, alloc_name);
          if (type_info.construct) {
            type_info.construct(new_layer.data, totelem);
          }
        }
        break;
      }
    }
  }
  else {
    if (totelem == 0 && sharing_info_to_assign == nullptr) {
      MEM_SAFE_FREE(layer_data_to_assign);
    }
    else {
      new_layer.data = layer_data_to_assign;
      new_layer.sharing_info = sharing_info_to_assign;
      if (new_layer.sharing_info) {
        new_layer.sharing_info->add_user();
      }
    }
  }

  if (new_layer.data != nullptr && new_layer.sharing_info == nullptr) {
    /* Make layer data shareable. */
    new_layer.sharing_info = make_implicit_sharing_info_for_layer(type, new_layer.data, totelem);
  }

  new_layer.type = type;
  new_layer.flag = flag;

  /* Set default name if none exists. Note we only call DATA_()  once
   * we know there is a default name, to avoid overhead of locale lookups
   * in the depsgraph. */
  if (name.is_empty() && type_info.defaultname) {
    name = DATA_(type_info.defaultname);
  }

  if (!name.is_empty()) {
    name.copy_utf8_truncated(new_layer.name);
    CustomData_set_layer_unique_name(data, index);
  }
  else {
    new_layer.name[0] = '\0';
  }

  if (index > 0 && data->layers[index - 1].type == type) {
    new_layer.active = data->layers[index - 1].active;
    new_layer.active_rnd = data->layers[index - 1].active_rnd;
    new_layer.active_clone = data->layers[index - 1].active_clone;
    new_layer.active_mask = data->layers[index - 1].active_mask;
  }
  else {
    new_layer.active = 0;
    new_layer.active_rnd = 0;
    new_layer.active_clone = 0;
    new_layer.active_mask = 0;
  }

  customData_update_offsets(data);

  return &data->layers[index];
}

void *CustomData_add_layer(CustomData *data,
                           const eCustomDataType type,
                           eCDAllocType alloctype,
                           const int totelem)
{
  const LayerTypeInfo *typeInfo = layerType_getInfo(type);

  CustomDataLayer *layer = customData_add_layer__internal(
      data, type, alloctype, nullptr, nullptr, totelem, typeInfo->defaultname);
  CustomData_update_typemap(data);

  if (layer) {
    return layer->data;
  }

  return nullptr;
}

const void *CustomData_add_layer_with_data(CustomData *data,
                                           const eCustomDataType type,
                                           void *layer_data,
                                           const int totelem,
                                           const ImplicitSharingInfo *sharing_info)
{
  const LayerTypeInfo *typeInfo = layerType_getInfo(type);

  CustomDataLayer *layer = customData_add_layer__internal(
      data, type, std::nullopt, layer_data, sharing_info, totelem, typeInfo->defaultname);
  CustomData_update_typemap(data);

  if (layer) {
    return layer->data;
  }

  return nullptr;
}

void *CustomData_add_layer_named(CustomData *data,
                                 const eCustomDataType type,
                                 const eCDAllocType alloctype,
                                 const int totelem,
                                 const StringRef name)
{
  CustomDataLayer *layer = customData_add_layer__internal(
      data, type, alloctype, nullptr, nullptr, totelem, name);
  CustomData_update_typemap(data);

  if (layer) {
    return layer->data;
  }
  return nullptr;
}

const void *CustomData_add_layer_named_with_data(CustomData *data,
                                                 eCustomDataType type,
                                                 void *layer_data,
                                                 int totelem,
                                                 const StringRef name,
                                                 const ImplicitSharingInfo *sharing_info)
{
  CustomDataLayer *layer = customData_add_layer__internal(
      data, type, std::nullopt, layer_data, sharing_info, totelem, name);
  CustomData_update_typemap(data);

  if (layer) {
    return layer->data;
  }
  return nullptr;
}

bool CustomData_free_layer(CustomData *data, const eCustomDataType type, const int index)
{
  const int index_first = CustomData_get_layer_index(data, type);
  const int n = index - index_first;

  BLI_assert(index >= index_first);
  if ((index_first == -1) || (n < 0)) {
    return false;
  }
  BLI_assert(data->layers[index].type == type);

  customData_free_layer__internal(&data->layers[index]);

  for (int i = index + 1; i < data->totlayer; i++) {
    data->layers[i - 1] = data->layers[i];
  }

  data->totlayer--;

  /* if layer was last of type in array, set new active layer */
  int i = CustomData_get_layer_index__notypemap(data, type);

  if (i != -1) {
    /* don't decrement zero index */
    const int index_nonzero = n ? n : 1;
    CustomDataLayer *layer;

    for (layer = &data->layers[i]; i < data->totlayer && layer->type == type; i++, layer++) {
      if (layer->active >= index_nonzero) {
        layer->active--;
      }
      if (layer->active_rnd >= index_nonzero) {
        layer->active_rnd--;
      }
      if (layer->active_clone >= index_nonzero) {
        layer->active_clone--;
      }
      if (layer->active_mask >= index_nonzero) {
        layer->active_mask--;
      }
    }
  }

  if (data->totlayer <= data->maxlayer - CUSTOMDATA_GROW) {
    customData_resize(data, -CUSTOMDATA_GROW);
  }

  customData_update_offsets(data);

  return true;
}

bool CustomData_free_layer_named(CustomData *data, const StringRef name)
{
  for (const int i : IndexRange(data->totlayer)) {
    const CustomDataLayer &layer = data->layers[i];
    if (StringRef(layer.name) == name) {
      CustomData_free_layer(data, eCustomDataType(layer.type), i);
      return true;
    }
  }
  return false;
}

bool CustomData_free_layer_active(CustomData *data, const eCustomDataType type)
{
  const int index = CustomData_get_active_layer_index(data, type);
  if (index == -1) {
    return false;
  }
  return CustomData_free_layer(data, type, index);
}

void CustomData_free_layers(CustomData *data, const eCustomDataType type)
{
  const int index = CustomData_get_layer_index(data, type);
  while (CustomData_free_layer(data, type, index)) {
    /* pass */
  }
}

bool CustomData_has_layer_named(const CustomData *data,
                                const eCustomDataType type,
                                const StringRef name)
{
  return CustomData_get_named_layer_index(data, type, name) != -1;
}

bool CustomData_has_layer(const CustomData *data, const eCustomDataType type)
{
  return (CustomData_get_layer_index(data, type) != -1);
}

int CustomData_number_of_layers(const CustomData *data, const eCustomDataType type)
{
  int number = 0;

  for (int i = 0; i < data->totlayer; i++) {
    if (data->layers[i].type == type) {
      number++;
    }
  }

  return number;
}

int CustomData_number_of_anonymous_layers(const CustomData *data, const eCustomDataType type)
{
  int number = 0;

  for (int i = 0; i < data->totlayer; i++) {
    if (data->layers[i].type == type &&
        blender::bke::attribute_name_is_anonymous(data->layers[i].name))
    {
      number++;
    }
  }

  return number;
}

int CustomData_number_of_layers_typemask(const CustomData *data, const eCustomDataMask mask)
{
  int number = 0;

  for (int i = 0; i < data->totlayer; i++) {
    if (mask & CD_TYPE_AS_MASK(eCustomDataType(data->layers[i].type))) {
      number++;
    }
  }

  return number;
}

void CustomData_set_only_copy(const CustomData *data, const eCustomDataMask mask)
{
  for (int i = 0; i < data->totlayer; i++) {
    if (!(mask & CD_TYPE_AS_MASK(eCustomDataType(data->layers[i].type)))) {
      data->layers[i].flag |= CD_FLAG_NOCOPY;
    }
  }
}

void CustomData_copy_elements(const eCustomDataType type,
                              const void *src_data,
                              void *dst_data,
                              const int count)
{
  const LayerTypeInfo *typeInfo = layerType_getInfo(type);

  if (typeInfo->copy) {
    typeInfo->copy(src_data, dst_data, count);
  }
  else {
    memcpy(dst_data, src_data, size_t(count) * typeInfo->size);
  }
}

void CustomData_copy_data_layer(const CustomData *source,
                                CustomData *dest,
                                const int src_layer_index,
                                const int dst_layer_index,
                                const int src_index,
                                const int dst_index,
                                const int count)
{
  const LayerTypeInfo *typeInfo;

  BLI_assert(layer_is_mutable(dest->layers[dst_layer_index]));

  const void *src_data = source->layers[src_layer_index].data;
  void *dst_data = dest->layers[dst_layer_index].data;

  typeInfo = layerType_getInfo(eCustomDataType(source->layers[src_layer_index].type));

  const size_t src_offset = size_t(src_index) * typeInfo->size;
  const size_t dst_offset = size_t(dst_index) * typeInfo->size;

  if (!count || !src_data || !dst_data) {
    if (count && !(src_data == nullptr && dst_data == nullptr)) {
      CLOG_WARN(&LOG,
                "null data for %s type (%p --> %p), skipping",
                layerType_getName(eCustomDataType(source->layers[src_layer_index].type)),
                (void *)src_data,
                (void *)dst_data);
    }
    return;
  }

  if (typeInfo->copy) {
    typeInfo->copy(
        POINTER_OFFSET(src_data, src_offset), POINTER_OFFSET(dst_data, dst_offset), count);
  }
  else {
    memcpy(POINTER_OFFSET(dst_data, dst_offset),
           POINTER_OFFSET(src_data, src_offset),
           size_t(count) * typeInfo->size);
  }
}

void CustomData_copy_data(const CustomData *source,
                          CustomData *dest,
                          const int source_index,
                          const int dest_index,
                          const int count)
{
  /* copies a layer at a time */
  int dest_i = 0;
  for (int src_i = 0; src_i < source->totlayer; src_i++) {

    /* find the first dest layer with type >= the source type
     * (this should work because layers are ordered by type)
     */
    while (dest_i < dest->totlayer && dest->layers[dest_i].type < source->layers[src_i].type) {
      dest_i++;
    }

    /* if there are no more dest layers, we're done */
    if (dest_i >= dest->totlayer) {
      return;
    }

    /* if we found a matching layer, copy the data */
    if (dest->layers[dest_i].type == source->layers[src_i].type) {
      CustomData_copy_data_layer(source, dest, src_i, dest_i, source_index, dest_index, count);

      /* if there are multiple source & dest layers of the same type,
       * we don't want to copy all source layers to the same dest, so
       * increment dest_i
       */
      dest_i++;
    }
  }
}

void CustomData_copy_layer_type_data(const CustomData *source,
                                     CustomData *destination,
                                     const eCustomDataType type,
                                     int source_index,
                                     int destination_index,
                                     int count)
{
  const int source_layer_index = CustomData_get_layer_index(source, type);
  if (source_layer_index == -1) {
    return;
  }
  const int destinaiton_layer_index = CustomData_get_layer_index(destination, type);
  if (destinaiton_layer_index == -1) {
    return;
  }
  CustomData_copy_data_layer(source,
                             destination,
                             source_layer_index,
                             destinaiton_layer_index,
                             source_index,
                             destination_index,
                             count);
}

void CustomData_free_elem(CustomData *data, const int index, const int count)
{
  for (int i = 0; i < data->totlayer; i++) {
    const LayerTypeInfo *typeInfo = layerType_getInfo(eCustomDataType(data->layers[i].type));

    if (typeInfo->free) {
      size_t offset = size_t(index) * typeInfo->size;
      BLI_assert(layer_is_mutable(data->layers[i]));

      typeInfo->free(POINTER_OFFSET(data->layers[i].data, offset), count);
    }
  }
}

#define SOURCE_BUF_SIZE 100

void CustomData_interp(const CustomData *source,
                       CustomData *dest,
                       const int *src_indices,
                       const float *weights,
                       int count,
                       int dest_index)
{
  if (count <= 0) {
    return;
  }

  const void *source_buf[SOURCE_BUF_SIZE];
  const void **sources = source_buf;

  /* Slow fallback in case we're interpolating a ridiculous number of elements. */
  if (count > SOURCE_BUF_SIZE) {
    sources = MEM_malloc_arrayN<const void *>(size_t(count), __func__);
  }

  /* If no weights are given, generate default ones to produce an average result. */
  float default_weights_buf[SOURCE_BUF_SIZE];
  float *default_weights = nullptr;
  if (weights == nullptr) {
    default_weights = (count > SOURCE_BUF_SIZE) ?
                          MEM_malloc_arrayN<float>(size_t(count), __func__) :
                          default_weights_buf;
    copy_vn_fl(default_weights, count, 1.0f / count);
    weights = default_weights;
  }

  /* interpolates a layer at a time */
  int dest_i = 0;
  for (int src_i = 0; src_i < source->totlayer; src_i++) {
    const LayerTypeInfo *typeInfo = layerType_getInfo(eCustomDataType(source->layers[src_i].type));
    if (!typeInfo->interp) {
      continue;
    }

    /* find the first dest layer with type >= the source type
     * (this should work because layers are ordered by type)
     */
    while (dest_i < dest->totlayer && dest->layers[dest_i].type < source->layers[src_i].type) {
      dest_i++;
    }

    /* if there are no more dest layers, we're done */
    if (dest_i >= dest->totlayer) {
      break;
    }

    /* if we found a matching layer, copy the data */
    if (dest->layers[dest_i].type == source->layers[src_i].type) {
      void *src_data = source->layers[src_i].data;

      for (int j = 0; j < count; j++) {
        sources[j] = POINTER_OFFSET(src_data, size_t(src_indices[j]) * typeInfo->size);
      }

      typeInfo->interp(
          sources,
          weights,
          count,
          POINTER_OFFSET(dest->layers[dest_i].data, size_t(dest_index) * typeInfo->size));

      /* if there are multiple source & dest layers of the same type,
       * we don't want to copy all source layers to the same dest, so
       * increment dest_i
       */
      dest_i++;
    }
  }

  if (count > SOURCE_BUF_SIZE) {
    MEM_freeN(sources);
  }
  if (!ELEM(default_weights, nullptr, default_weights_buf)) {
    MEM_freeN(default_weights);
  }
}

void CustomData_swap_corners(CustomData *data, const int index, const int *corner_indices)
{
  for (int i = 0; i < data->totlayer; i++) {
    const LayerTypeInfo *typeInfo = layerType_getInfo(eCustomDataType(data->layers[i].type));

    if (typeInfo->swap) {
      const size_t offset = size_t(index) * typeInfo->size;

      typeInfo->swap(POINTER_OFFSET(data->layers[i].data, offset), corner_indices);
    }
  }
}

void *CustomData_get_for_write(CustomData *data,
                               const int index,
                               const eCustomDataType type,
                               int totelem)
{
  BLI_assert(index >= 0);
  void *layer_data = CustomData_get_layer_for_write(data, type, totelem);
  if (!layer_data) {
    return nullptr;
  }
  return POINTER_OFFSET(layer_data, size_t(index) * layerType_getInfo(type)->size);
}

void *CustomData_get_n_for_write(
    CustomData *data, const eCustomDataType type, const int index, const int n, int totelem)
{
  BLI_assert(index >= 0);
  void *layer_data = CustomData_get_layer_n_for_write(data, type, n, totelem);
  if (!layer_data) {
    return nullptr;
  }

  return POINTER_OFFSET(layer_data, size_t(index) * layerType_getInfo(type)->size);
}

const void *CustomData_get_layer(const CustomData *data, const eCustomDataType type)
{
  int layer_index = CustomData_get_active_layer_index(data, type);
  if (layer_index == -1) {
    return nullptr;
  }

  return data->layers[layer_index].data;
}

void *CustomData_get_layer_for_write(CustomData *data,
                                     const eCustomDataType type,
                                     const int totelem)
{
  const int layer_index = CustomData_get_active_layer_index(data, type);
  if (layer_index == -1) {
    return nullptr;
  }
  CustomDataLayer &layer = data->layers[layer_index];
  ensure_layer_data_is_mutable(layer, totelem);
  return layer.data;
}

const void *CustomData_get_layer_n(const CustomData *data, const eCustomDataType type, const int n)
{
  int layer_index = CustomData_get_layer_index_n(data, type, n);
  if (layer_index == -1) {
    return nullptr;
  }
  return data->layers[layer_index].data;
}

void *CustomData_get_layer_n_for_write(CustomData *data,
                                       const eCustomDataType type,
                                       const int n,
                                       const int totelem)
{
  const int layer_index = CustomData_get_layer_index_n(data, type, n);
  if (layer_index == -1) {
    return nullptr;
  }
  CustomDataLayer &layer = data->layers[layer_index];
  ensure_layer_data_is_mutable(layer, totelem);
  return layer.data;
}

const void *CustomData_get_layer_named(const CustomData *data,
                                       const eCustomDataType type,
                                       const StringRef name)
{
  int layer_index = CustomData_get_named_layer_index(data, type, name);
  if (layer_index == -1) {
    return nullptr;
  }
  return data->layers[layer_index].data;
}

void *CustomData_get_layer_named_for_write(CustomData *data,
                                           const eCustomDataType type,
                                           const StringRef name,
                                           const int totelem)
{
  const int layer_index = CustomData_get_named_layer_index(data, type, name);
  if (layer_index == -1) {
    return nullptr;
  }
  CustomDataLayer &layer = data->layers[layer_index];
  ensure_layer_data_is_mutable(layer, totelem);
  return layer.data;
}

int CustomData_get_offset(const CustomData *data, const eCustomDataType type)
{
  int layer_index = CustomData_get_active_layer_index(data, type);
  if (layer_index == -1) {
    return -1;
  }
  return data->layers[layer_index].offset;
}

int CustomData_get_n_offset(const CustomData *data, const eCustomDataType type, const int n)
{
  int layer_index = CustomData_get_layer_index_n(data, type, n);
  if (layer_index == -1) {
    return -1;
  }

  return data->layers[layer_index].offset;
}

int CustomData_get_offset_named(const CustomData *data,
                                const eCustomDataType type,
                                const StringRef name)
{
  int layer_index = CustomData_get_named_layer_index(data, type, name);
  if (layer_index == -1) {
    return -1;
  }

  return data->layers[layer_index].offset;
}

bool CustomData_set_layer_name(CustomData *data,
                               const eCustomDataType type,
                               const int n,
                               const StringRef name)
{
  const int layer_index = CustomData_get_layer_index_n(data, type, n);
  if (layer_index == -1) {
    return false;
  }

  name.copy_utf8_truncated(data->layers[layer_index].name);

  return true;
}

const char *CustomData_get_layer_name(const CustomData *data,
                                      const eCustomDataType type,
                                      const int n)
{
  const int layer_index = CustomData_get_layer_index_n(data, type, n);

  return (layer_index == -1) ? nullptr : data->layers[layer_index].name;
}

/* BMesh functions */

void CustomData_bmesh_init_pool(CustomData *data, const int totelem, const char htype)
{
  int chunksize;

  /* Dispose old pools before calling here to avoid leaks */
  BLI_assert(data->pool == nullptr);

  switch (htype) {
    case BM_VERT:
      chunksize = bm_mesh_chunksize_default.totvert;
      break;
    case BM_EDGE:
      chunksize = bm_mesh_chunksize_default.totedge;
      break;
    case BM_LOOP:
      chunksize = bm_mesh_chunksize_default.totloop;
      break;
    case BM_FACE:
      chunksize = bm_mesh_chunksize_default.totface;
      break;
    default:
      BLI_assert_unreachable();
      chunksize = 512;
      break;
  }

  /* If there are no layers, no pool is needed just yet */
  if (data->totlayer) {
    data->pool = BLI_mempool_create(data->totsize, totelem, chunksize, BLI_MEMPOOL_NOP);
  }
}

bool CustomData_bmesh_merge_layout(const CustomData *source,
                                   CustomData *dest,
                                   eCustomDataMask mask,
                                   eCDAllocType alloctype,
                                   BMesh *bm,
                                   const char htype)
{

  if (CustomData_number_of_layers_typemask(source, mask) == 0) {
    return false;
  }

  /* copy old layer description so that old data can be copied into
   * the new allocation */
  CustomData destold = *dest;
  if (destold.layers) {
    destold.layers = static_cast<CustomDataLayer *>(MEM_dupallocN(destold.layers));
  }

  if (CustomData_merge_layout(source, dest, mask, alloctype, 0) == false) {
    if (destold.layers) {
      MEM_freeN(destold.layers);
    }
    return false;
  }

  const BMCustomDataCopyMap map = CustomData_bmesh_copy_map_calc(destold, *dest);

  int iter_type;
  int totelem;
  switch (htype) {
    case BM_VERT:
      iter_type = BM_VERTS_OF_MESH;
      totelem = bm->totvert;
      break;
    case BM_EDGE:
      iter_type = BM_EDGES_OF_MESH;
      totelem = bm->totedge;
      break;
    case BM_LOOP:
      iter_type = BM_LOOPS_OF_FACE;
      totelem = bm->totloop;
      break;
    case BM_FACE:
      iter_type = BM_FACES_OF_MESH;
      totelem = bm->totface;
      break;
    default: /* should never happen */
      BLI_assert_msg(0, "invalid type given");
      iter_type = BM_VERTS_OF_MESH;
      totelem = bm->totvert;
      break;
  }

  dest->pool = nullptr;
  CustomData_bmesh_init_pool(dest, totelem, htype);

  if (iter_type != BM_LOOPS_OF_FACE) {
    BMHeader *h;
    BMIter iter;
    /* Ensure all current elements follow new customdata layout. */
    BM_ITER_MESH (h, &iter, bm, iter_type) {
      void *tmp = nullptr;
      CustomData_bmesh_copy_block(*dest, map, h->data, &tmp);
      CustomData_bmesh_free_block(&destold, &h->data);
      h->data = tmp;
    }
  }
  else {
    BMFace *f;
    BMLoop *l;
    BMIter iter;
    BMIter liter;

    /* Ensure all current elements follow new customdata layout. */
    BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
      BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
        void *tmp = nullptr;
        CustomData_bmesh_copy_block(*dest, map, l->head.data, &tmp);
        CustomData_bmesh_free_block(&destold, &l->head.data);
        l->head.data = tmp;
      }
    }
  }

  if (destold.pool) {
    BLI_mempool_destroy(destold.pool);
  }
  if (destold.layers) {
    MEM_freeN(destold.layers);
  }
  return true;
}

void CustomData_bmesh_free_block(CustomData *data, void **block)
{
  if (*block == nullptr) {
    return;
  }

  for (int i = 0; i < data->totlayer; i++) {
    const LayerTypeInfo *typeInfo = layerType_getInfo(eCustomDataType(data->layers[i].type));

    if (typeInfo->free) {
      int offset = data->layers[i].offset;
      typeInfo->free(POINTER_OFFSET(*block, offset), 1);
    }
  }

  if (data->totsize) {
    BLI_mempool_free(data->pool, *block);
  }

  *block = nullptr;
}

void CustomData_bmesh_free_block_data(CustomData *data, void *block)
{
  if (block == nullptr) {
    return;
  }
  for (int i = 0; i < data->totlayer; i++) {
    const LayerTypeInfo *typeInfo = layerType_getInfo(eCustomDataType(data->layers[i].type));
    if (typeInfo->free) {
      const size_t offset = data->layers[i].offset;
      typeInfo->free(POINTER_OFFSET(block, offset), 1);
    }
  }
  if (data->totsize) {
    memset(block, 0, data->totsize);
  }
}

void CustomData_bmesh_alloc_block(CustomData *data, void **block)
{
  if (*block) {
    CustomData_bmesh_free_block(data, block);
  }

  if (data->totsize > 0) {
    *block = BLI_mempool_alloc(data->pool);
  }
  else {
    *block = nullptr;
  }
}

void CustomData_data_set_default_value(const eCustomDataType type, void *elem)
{
  const LayerTypeInfo *typeInfo = layerType_getInfo(type);
  if (typeInfo->set_default_value) {
    typeInfo->set_default_value(elem, 1);
  }
  else {
    memset(elem, 0, typeInfo->size);
  }
}

static void CustomData_bmesh_set_default_n(CustomData *data, void **block, const int n)
{
  const int offset = data->layers[n].offset;
  CustomData_data_set_default_value(eCustomDataType(data->layers[n].type),
                                    POINTER_OFFSET(*block, offset));
}

void CustomData_bmesh_set_default(CustomData *data, void **block)
{
  if (*block == nullptr) {
    CustomData_bmesh_alloc_block(data, block);
  }

  for (int i = 0; i < data->totlayer; i++) {
    CustomData_bmesh_set_default_n(data, block, i);
  }
}

BMCustomDataCopyMap CustomData_bmesh_copy_map_calc(const CustomData &src,
                                                   const CustomData &dst,
                                                   const eCustomDataMask mask_exclude)
{
  BMCustomDataCopyMap map;
  for (const CustomDataLayer &layer_dst : Span(dst.layers, dst.totlayer)) {
    const int dst_offset = layer_dst.offset;
    const eCustomDataType dst_type = eCustomDataType(layer_dst.type);
    const LayerTypeInfo &type_info = *layerType_getInfo(dst_type);

    const int src_offset = CustomData_get_offset_named(&src, dst_type, layer_dst.name);
    if (src_offset == -1 || CD_TYPE_AS_MASK(dst_type) & mask_exclude) {
      if (type_info.set_default_value) {
        map.defaults.append({type_info.set_default_value, dst_offset});
      }
      else {
        map.trivial_defaults.append({type_info.size, dst_offset});
      }
    }
    else {
      if (type_info.copy) {
        map.copies.append({type_info.copy, src_offset, dst_offset});
      }
      else {
        /* NOTE: A way to improve performance of copies (by reducing the number of `memcpy`
         * calls) would be combining contiguous chunks in the source and result format. */
        map.trivial_copies.append({type_info.size, src_offset, dst_offset});
      }
    }

    if (type_info.free) {
      map.free.append({type_info.free, dst_offset});
    }
  }
  return map;
}

void CustomData_bmesh_copy_block(CustomData &dst_data,
                                 const BMCustomDataCopyMap &copy_map,
                                 const void *src_block,
                                 void **dst_block)
{
  if (*dst_block) {
    for (const BMCustomDataCopyMap::Free &info : copy_map.free) {
      info.fn(POINTER_OFFSET(*dst_block, info.dst_offset), 1);
    }
  }
  else {
    if (dst_data.totsize == 0) {
      return;
    }
    *dst_block = BLI_mempool_alloc(dst_data.pool);
  }

  for (const BMCustomDataCopyMap::TrivialCopy &info : copy_map.trivial_copies) {
    memcpy(POINTER_OFFSET(*dst_block, info.dst_offset),
           POINTER_OFFSET(src_block, info.src_offset),
           info.size);
  }
  for (const BMCustomDataCopyMap::Copy &info : copy_map.copies) {
    info.fn(POINTER_OFFSET(src_block, info.src_offset),
            POINTER_OFFSET(*dst_block, info.dst_offset),
            1);
  }
  for (const BMCustomDataCopyMap::TrivialDefault &info : copy_map.trivial_defaults) {
    memset(POINTER_OFFSET(*dst_block, info.dst_offset), 0, info.size);
  }
  for (const BMCustomDataCopyMap::Default &info : copy_map.defaults) {
    info.fn(POINTER_OFFSET(*dst_block, info.dst_offset), 1);
  }
}

void CustomData_bmesh_copy_block(CustomData &data, void *src_block, void **dst_block)
{
  if (*dst_block) {
    for (const CustomDataLayer &layer : Span(data.layers, data.totlayer)) {
      const LayerTypeInfo &info = *layerType_getInfo(eCustomDataType(layer.type));
      if (info.free) {
        info.free(POINTER_OFFSET(*dst_block, layer.offset), 1);
      }
    }
  }
  else {
    if (data.totsize == 0) {
      return;
    }
    *dst_block = BLI_mempool_alloc(data.pool);
  }

  for (const CustomDataLayer &layer : Span(data.layers, data.totlayer)) {
    const int offset = layer.offset;
    const LayerTypeInfo &info = *layerType_getInfo(eCustomDataType(layer.type));
    if (info.copy) {
      info.copy(POINTER_OFFSET(src_block, offset), POINTER_OFFSET(*dst_block, offset), 1);
    }
    else {
      memcpy(POINTER_OFFSET(*dst_block, offset), POINTER_OFFSET(src_block, offset), info.size);
    }
  }
}

void *CustomData_bmesh_get(const CustomData *data, void *block, const eCustomDataType type)
{
  int layer_index = CustomData_get_active_layer_index(data, type);
  if (layer_index == -1) {
    return nullptr;
  }

  return POINTER_OFFSET(block, data->layers[layer_index].offset);
}

void *CustomData_bmesh_get_n(const CustomData *data,
                             void *block,
                             const eCustomDataType type,
                             const int n)
{
  int layer_index = CustomData_get_layer_index(data, type);
  if (layer_index == -1) {
    return nullptr;
  }

  return POINTER_OFFSET(block, data->layers[layer_index + n].offset);
}

void *CustomData_bmesh_get_layer_n(const CustomData *data, void *block, const int n)
{
  if (n < 0 || n >= data->totlayer) {
    return nullptr;
  }

  return POINTER_OFFSET(block, data->layers[n].offset);
}

bool CustomData_layer_has_math(const CustomData *data, const int layer_n)
{
  const LayerTypeInfo *typeInfo = layerType_getInfo(eCustomDataType(data->layers[layer_n].type));

  if (typeInfo->equal && typeInfo->add && typeInfo->multiply && typeInfo->initminmax &&
      typeInfo->dominmax)
  {
    return true;
  }

  return false;
}

bool CustomData_layer_has_interp(const CustomData *data, const int layer_n)
{
  const LayerTypeInfo *typeInfo = layerType_getInfo(eCustomDataType(data->layers[layer_n].type));

  if (typeInfo->interp) {
    return true;
  }

  return false;
}

bool CustomData_has_math(const CustomData *data)
{
  /* interpolates a layer at a time */
  for (int i = 0; i < data->totlayer; i++) {
    if (CustomData_layer_has_math(data, i)) {
      return true;
    }
  }

  return false;
}

bool CustomData_bmesh_has_free(const CustomData *data)
{
  for (int i = 0; i < data->totlayer; i++) {
    const LayerTypeInfo *typeInfo = layerType_getInfo(eCustomDataType(data->layers[i].type));
    if (typeInfo->free) {
      return true;
    }
  }
  return false;
}

bool CustomData_has_interp(const CustomData *data)
{
  /* interpolates a layer at a time */
  for (int i = 0; i < data->totlayer; i++) {
    if (CustomData_layer_has_interp(data, i)) {
      return true;
    }
  }

  return false;
}

void CustomData_data_copy_value(const eCustomDataType type, const void *source, void *dest)
{
  const LayerTypeInfo *typeInfo = layerType_getInfo(type);

  if (!dest) {
    return;
  }

  if (typeInfo->copy) {
    typeInfo->copy(source, dest, 1);
  }
  else {
    memcpy(dest, source, typeInfo->size);
  }
}

void CustomData_data_mix_value(const eCustomDataType type,
                               const void *source,
                               void *dest,
                               const int mixmode,
                               const float mixfactor)
{
  const LayerTypeInfo *typeInfo = layerType_getInfo(type);

  if (!dest) {
    return;
  }

  if (typeInfo->copyvalue) {
    typeInfo->copyvalue(source, dest, mixmode, mixfactor);
  }
  else {
    /* Mere copy if no advanced interpolation is supported. */
    memcpy(dest, source, typeInfo->size);
  }
}

bool CustomData_data_equals(const eCustomDataType type, const void *data1, const void *data2)
{
  const LayerTypeInfo *typeInfo = layerType_getInfo(type);

  if (typeInfo->equal) {
    return typeInfo->equal(data1, data2);
  }

  return !memcmp(data1, data2, typeInfo->size);
}

void CustomData_data_initminmax(const eCustomDataType type, void *min, void *max)
{
  const LayerTypeInfo *typeInfo = layerType_getInfo(type);

  if (typeInfo->initminmax) {
    typeInfo->initminmax(min, max);
  }
}

void CustomData_data_dominmax(const eCustomDataType type, const void *data, void *min, void *max)
{
  const LayerTypeInfo *typeInfo = layerType_getInfo(type);

  if (typeInfo->dominmax) {
    typeInfo->dominmax(data, min, max);
  }
}

void CustomData_data_multiply(const eCustomDataType type, void *data, const float fac)
{
  const LayerTypeInfo *typeInfo = layerType_getInfo(type);

  if (typeInfo->multiply) {
    typeInfo->multiply(data, fac);
  }
}

void CustomData_data_add(const eCustomDataType type, void *data1, const void *data2)
{
  const LayerTypeInfo *typeInfo = layerType_getInfo(type);

  if (typeInfo->add) {
    typeInfo->add(data1, data2);
  }
}

void CustomData_bmesh_set_n(
    CustomData *data, void *block, const eCustomDataType type, const int n, const void *source)
{
  void *dest = CustomData_bmesh_get_n(data, block, type, n);
  const LayerTypeInfo *typeInfo = layerType_getInfo(type);

  if (!dest) {
    return;
  }

  if (typeInfo->copy) {
    typeInfo->copy(source, dest, 1);
  }
  else {
    memcpy(dest, source, typeInfo->size);
  }
}

void CustomData_bmesh_interp_n(CustomData *data,
                               const void **src_blocks_ofs,
                               const float *weights,
                               int count,
                               void *dst_block_ofs,
                               int n)
{
  BLI_assert(weights != nullptr);
  BLI_assert(count > 0);

  CustomDataLayer *layer = &data->layers[n];
  const LayerTypeInfo *typeInfo = layerType_getInfo(eCustomDataType(layer->type));

  typeInfo->interp(src_blocks_ofs, weights, count, dst_block_ofs);
}

void CustomData_bmesh_interp(
    CustomData *data, const void **src_blocks, const float *weights, int count, void *dst_block)
{
  if (count <= 0) {
    return;
  }

  void *source_buf[SOURCE_BUF_SIZE];
  const void **sources = (const void **)source_buf;

  /* Slow fallback in case we're interpolating a ridiculous number of elements. */
  if (count > SOURCE_BUF_SIZE) {
    sources = MEM_malloc_arrayN<const void *>(size_t(count), __func__);
  }

  /* If no weights are given, generate default ones to produce an average result. */
  float default_weights_buf[SOURCE_BUF_SIZE];
  float *default_weights = nullptr;
  if (weights == nullptr) {
    default_weights = (count > SOURCE_BUF_SIZE) ?
                          MEM_malloc_arrayN<float>(size_t(count), __func__) :
                          default_weights_buf;
    copy_vn_fl(default_weights, count, 1.0f / count);
    weights = default_weights;
  }

  /* interpolates a layer at a time */
  for (int i = 0; i < data->totlayer; i++) {
    CustomDataLayer *layer = &data->layers[i];
    const LayerTypeInfo *typeInfo = layerType_getInfo(eCustomDataType(layer->type));
    if (typeInfo->interp) {
      for (int j = 0; j < count; j++) {
        sources[j] = POINTER_OFFSET(src_blocks[j], layer->offset);
      }
      CustomData_bmesh_interp_n(
          data, sources, weights, count, POINTER_OFFSET(dst_block, layer->offset), i);
    }
  }

  if (count > SOURCE_BUF_SIZE) {
    MEM_freeN(sources);
  }
  if (!ELEM(default_weights, nullptr, default_weights_buf)) {
    MEM_freeN(default_weights);
  }
}

int CustomData_sizeof(const eCustomDataType type)
{
  const LayerTypeInfo *typeInfo = layerType_getInfo(type);

  return typeInfo->size;
}

const char *CustomData_layertype_name(const eCustomDataType type)
{
  return layerType_getName(type);
}

bool CustomData_layertype_is_singleton(const eCustomDataType type)
{
  const LayerTypeInfo *typeInfo = layerType_getInfo(type);
  return typeInfo->defaultname == nullptr;
}

bool CustomData_layertype_is_dynamic(const eCustomDataType type)
{
  const LayerTypeInfo *typeInfo = layerType_getInfo(type);

  return (typeInfo->free != nullptr);
}

int CustomData_layertype_layers_max(const eCustomDataType type)
{
  const LayerTypeInfo *typeInfo = layerType_getInfo(type);

  /* Same test as for singleton above. */
  if (typeInfo->defaultname == nullptr) {
    return 1;
  }
  if (typeInfo->layers_max == nullptr) {
    return -1;
  }

  return typeInfo->layers_max();
}

static bool cd_layer_find_dupe(CustomData *data,
                               const StringRef name,
                               const eCustomDataType type,
                               const int index)
{
  /* see if there is a duplicate */
  for (int i = 0; i < data->totlayer; i++) {
    if (i != index) {
      CustomDataLayer *layer = &data->layers[i];

      if (CD_TYPE_AS_MASK(type) & CD_MASK_PROP_ALL) {
        if ((CD_TYPE_AS_MASK(eCustomDataType(layer->type)) & CD_MASK_PROP_ALL) &&
            layer->name == name)
        {
          return true;
        }
      }
      else {
        if (i != index && layer->type == type && layer->name == name) {
          return true;
        }
      }
    }
  }

  return false;
}

int CustomData_name_maxncpy_calc(const blender::StringRef name)
{
  if (name.startswith(".")) {
    return MAX_CUSTOMDATA_LAYER_NAME_NO_PREFIX;
  }
  for (const blender::StringRef prefix : {UV_PINNED_NAME "."}) {
    if (name.startswith(prefix)) {
      return MAX_CUSTOMDATA_LAYER_NAME;
    }
  }
  return MAX_CUSTOMDATA_LAYER_NAME_NO_PREFIX;
}

void CustomData_set_layer_unique_name(CustomData *data, const int index)
{
  CustomDataLayer *nlayer = &data->layers[index];
  const LayerTypeInfo *typeInfo = layerType_getInfo(eCustomDataType(nlayer->type));

  if (!typeInfo->defaultname) {
    return;
  }

  const int name_maxncpy = CustomData_name_maxncpy_calc(nlayer->name);

  /* Set default name if none specified. Note we only call DATA_() when
   * needed to avoid overhead of locale lookups in the depsgraph. */
  if (nlayer->name[0] == '\0') {
    STRNCPY_UTF8(nlayer->name, DATA_(typeInfo->defaultname));
  }

  const char *defname = ""; /* Dummy argument, never used as `name` is never zero length. */
  BLI_uniquename_cb(
      [&](const StringRef name) {
        return cd_layer_find_dupe(data, name, eCustomDataType(nlayer->type), index);
      },
      defname,
      '.',
      nlayer->name,
      name_maxncpy);
}

bool CustomData_verify_versions(CustomData *data, const int index)
{
  const LayerTypeInfo *typeInfo;
  CustomDataLayer *layer = &data->layers[index];
  bool keeplayer = true;

  if (layer->type >= CD_NUMTYPES) {
    keeplayer = false; /* unknown layer type from future version */
  }
  else {
    typeInfo = layerType_getInfo(eCustomDataType(layer->type));

    if (!typeInfo->defaultname && (index > 0) && data->layers[index - 1].type == layer->type) {
      keeplayer = false; /* multiple layers of which we only support one */
    }
    /* This is a preemptive fix for cases that should not happen
     * (layers that should not be written in .blend files),
     * but can happen due to bugs (see e.g. #62318).
     * Also for forward compatibility, in future,
     * we may put into `.blend` file some currently un-written data types,
     * this should cover that case as well.
     * Better to be safe here, and fix issue on the fly rather than crash... */
    /* 0 structnum is used in writing code to tag layer types that should not be written. */
    else if (typeInfo->structnum == 0 &&
             /* XXX Not sure why those three are exception, maybe that should be fixed? */
             !ELEM(layer->type,
                   CD_PAINT_MASK,
                   CD_FACEMAP,
                   CD_MTEXPOLY,
                   CD_SCULPT_FACE_SETS,
                   CD_CREASE))
    {
      keeplayer = false;
      CLOG_WARN(&LOG, ".blend file read: removing a data layer that should not have been written");
    }
  }

  if (!keeplayer) {
    for (int i = index + 1; i < data->totlayer; i++) {
      data->layers[i - 1] = data->layers[i];
    }
    data->totlayer--;
  }

  return keeplayer;
}

static bool CustomData_layer_ensure_data_exists(CustomDataLayer *layer, size_t count)
{
  BLI_assert(layer);
  const LayerTypeInfo *typeInfo = layerType_getInfo(eCustomDataType(layer->type));
  BLI_assert(typeInfo);

  if (layer->data || count == 0) {
    return false;
  }

  switch (layer->type) {
    /* When more instances of corrupt files are found, add them here. */
    case CD_PROP_BOOL:   /* See #84935. */
    case CD_MLOOPUV:     /* See #90620. */
    case CD_PROP_FLOAT2: /* See #90620. */
      layer->data = MEM_calloc_arrayN(
          count, typeInfo->size, layerType_getName(eCustomDataType(layer->type)));
      BLI_assert(layer->data);
      if (typeInfo->set_default_value) {
        typeInfo->set_default_value(layer->data, count);
      }
      return true;
      break;

    case CD_MTEXPOLY:
      /* TODO: Investigate multiple test failures on cycles, e.g. cycles_shadow_catcher_cpu. */
      break;

    default:
      /* Log an error so we can collect instances of bad files. */
      CLOG_WARN(&LOG, "CustomDataLayer->data is null for type %d.", layer->type);
      break;
  }
  return false;
}

bool CustomData_layer_validate(CustomDataLayer *layer, const uint totitems, const bool do_fixes)
{
  BLI_assert(layer);
  const LayerTypeInfo *typeInfo = layerType_getInfo(eCustomDataType(layer->type));
  BLI_assert(typeInfo);

  if (do_fixes) {
    CustomData_layer_ensure_data_exists(layer, totitems);
  }

  BLI_assert((totitems == 0) || layer->data);

  if (typeInfo->validate != nullptr) {
    return typeInfo->validate(layer->data, totitems, do_fixes);
  }

  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name External Files
 * \{ */

static void customdata_external_filename(char filepath[FILE_MAX],
                                         ID *id,
                                         CustomDataExternal *external)
{
  BLI_strncpy(filepath, external->filepath, FILE_MAX);
  BLI_path_abs(filepath, ID_BLEND_PATH_FROM_GLOBAL(id));
}

void CustomData_external_reload(CustomData *data, ID * /*id*/, eCustomDataMask mask, int totelem)
{
  for (int i = 0; i < data->totlayer; i++) {
    CustomDataLayer *layer = &data->layers[i];
    const LayerTypeInfo *typeInfo = layerType_getInfo(eCustomDataType(layer->type));

    if (!(mask & CD_TYPE_AS_MASK(eCustomDataType(layer->type)))) {
      /* pass */
    }
    else if ((layer->flag & CD_FLAG_EXTERNAL) && (layer->flag & CD_FLAG_IN_MEMORY)) {
      if (typeInfo->free) {
        typeInfo->free(layer->data, totelem);
      }
      layer->flag &= ~CD_FLAG_IN_MEMORY;
    }
  }
}

void CustomData_external_read(CustomData *data, ID *id, eCustomDataMask mask, const int totelem)
{
  CustomDataExternal *external = data->external;
  CustomDataLayer *layer;
  char filepath[FILE_MAX];
  int update = 0;

  if (!external) {
    return;
  }

  for (int i = 0; i < data->totlayer; i++) {
    layer = &data->layers[i];
    const LayerTypeInfo *typeInfo = layerType_getInfo(eCustomDataType(layer->type));

    if (!(mask & CD_TYPE_AS_MASK(eCustomDataType(layer->type)))) {
      /* pass */
    }
    else if (layer->flag & CD_FLAG_IN_MEMORY) {
      /* pass */
    }
    else if ((layer->flag & CD_FLAG_EXTERNAL) && typeInfo->read) {
      update = 1;
    }
  }

  if (!update) {
    return;
  }

  customdata_external_filename(filepath, id, external);

  CDataFile *cdf = cdf_create(CDF_TYPE_MESH);
  if (!cdf_read_open(cdf, filepath)) {
    cdf_free(cdf);
    CLOG_ERROR(&LOG,
               "Failed to read %s layer from %s.",
               layerType_getName(eCustomDataType(layer->type)),
               filepath);
    return;
  }

  for (int i = 0; i < data->totlayer; i++) {
    layer = &data->layers[i];
    const LayerTypeInfo *typeInfo = layerType_getInfo(eCustomDataType(layer->type));

    if (!(mask & CD_TYPE_AS_MASK(eCustomDataType(layer->type)))) {
      /* pass */
    }
    else if (layer->flag & CD_FLAG_IN_MEMORY) {
      /* pass */
    }
    else if ((layer->flag & CD_FLAG_EXTERNAL) && typeInfo->read) {
      const CDataFileLayer *blay = cdf_layer_find(cdf, layer->type, layer->name);

      if (blay) {
        if (cdf_read_layer(cdf, blay)) {
          if (typeInfo->read(cdf, layer->data, totelem)) {
            /* pass */
          }
          else {
            break;
          }
          layer->flag |= CD_FLAG_IN_MEMORY;
        }
        else {
          break;
        }
      }
    }
  }

  cdf_read_close(cdf);
  cdf_free(cdf);
}

void CustomData_external_write(
    CustomData *data, ID *id, eCustomDataMask mask, const int totelem, const int free)
{
  CustomDataExternal *external = data->external;
  int update = 0;
  char filepath[FILE_MAX];

  if (!external) {
    return;
  }

  /* test if there is anything to write */
  for (int i = 0; i < data->totlayer; i++) {
    CustomDataLayer *layer = &data->layers[i];
    const LayerTypeInfo *typeInfo = layerType_getInfo(eCustomDataType(layer->type));

    if (!(mask & CD_TYPE_AS_MASK(eCustomDataType(layer->type)))) {
      /* pass */
    }
    else if ((layer->flag & CD_FLAG_EXTERNAL) && typeInfo->write) {
      update = 1;
    }
  }

  if (!update) {
    return;
  }

  /* make sure data is read before we try to write */
  CustomData_external_read(data, id, mask, totelem);
  customdata_external_filename(filepath, id, external);

  CDataFile *cdf = cdf_create(CDF_TYPE_MESH);

  for (int i = 0; i < data->totlayer; i++) {
    CustomDataLayer *layer = &data->layers[i];
    const LayerTypeInfo *typeInfo = layerType_getInfo(eCustomDataType(layer->type));

    if ((layer->flag & CD_FLAG_EXTERNAL) && typeInfo->filesize) {
      if (layer->flag & CD_FLAG_IN_MEMORY) {
        cdf_layer_add(
            cdf, layer->type, layer->name, typeInfo->filesize(cdf, layer->data, totelem));
      }
      else {
        cdf_free(cdf);
        return; /* read failed for a layer! */
      }
    }
  }

  if (!cdf_write_open(cdf, filepath)) {
    CLOG_ERROR(&LOG, "Failed to open %s for writing.", filepath);
    cdf_free(cdf);
    return;
  }

  int i;
  for (i = 0; i < data->totlayer; i++) {
    CustomDataLayer *layer = &data->layers[i];
    const LayerTypeInfo *typeInfo = layerType_getInfo(eCustomDataType(layer->type));

    if ((layer->flag & CD_FLAG_EXTERNAL) && typeInfo->write) {
      CDataFileLayer *blay = cdf_layer_find(cdf, layer->type, layer->name);

      if (cdf_write_layer(cdf, blay)) {
        if (typeInfo->write(cdf, layer->data, totelem)) {
          /* pass */
        }
        else {
          break;
        }
      }
      else {
        break;
      }
    }
  }

  if (i != data->totlayer) {
    CLOG_ERROR(&LOG, "Failed to write data to %s.", filepath);
    cdf_write_close(cdf);
    cdf_free(cdf);
    return;
  }

  for (i = 0; i < data->totlayer; i++) {
    CustomDataLayer *layer = &data->layers[i];
    const LayerTypeInfo *typeInfo = layerType_getInfo(eCustomDataType(layer->type));

    if ((layer->flag & CD_FLAG_EXTERNAL) && typeInfo->write) {
      if (free) {
        if (typeInfo->free) {
          typeInfo->free(layer->data, totelem);
        }
        layer->flag &= ~CD_FLAG_IN_MEMORY;
      }
    }
  }

  cdf_write_close(cdf);
  cdf_free(cdf);
}

void CustomData_external_add(CustomData *data,
                             ID * /*id*/,
                             const eCustomDataType type,
                             const int /*totelem*/,
                             const char *filepath)
{
  CustomDataExternal *external = data->external;

  int layer_index = CustomData_get_active_layer_index(data, type);
  if (layer_index == -1) {
    return;
  }

  CustomDataLayer *layer = &data->layers[layer_index];

  if (layer->flag & CD_FLAG_EXTERNAL) {
    return;
  }

  if (!external) {
    external = MEM_callocN<CustomDataExternal>(__func__);
    data->external = external;
  }
  STRNCPY(external->filepath, filepath);

  layer->flag |= CD_FLAG_EXTERNAL | CD_FLAG_IN_MEMORY;
}

void CustomData_external_remove(CustomData *data,
                                ID *id,
                                const eCustomDataType type,
                                const int totelem)
{
  CustomDataExternal *external = data->external;

  int layer_index = CustomData_get_active_layer_index(data, type);
  if (layer_index == -1) {
    return;
  }

  CustomDataLayer *layer = &data->layers[layer_index];

  if (!external) {
    return;
  }

  if (layer->flag & CD_FLAG_EXTERNAL) {
    if (!(layer->flag & CD_FLAG_IN_MEMORY)) {
      CustomData_external_read(data, id, CD_TYPE_AS_MASK(eCustomDataType(layer->type)), totelem);
    }

    layer->flag &= ~CD_FLAG_EXTERNAL;
  }
}

bool CustomData_external_test(CustomData *data, const eCustomDataType type)
{
  int layer_index = CustomData_get_active_layer_index(data, type);
  if (layer_index == -1) {
    return false;
  }

  CustomDataLayer *layer = &data->layers[layer_index];
  return (layer->flag & CD_FLAG_EXTERNAL) != 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mesh-to-Mesh Data Transfer
 * \{ */

static void customdata_data_transfer_interp_generic(const CustomDataTransferLayerMap *laymap,
                                                    void *data_dst,
                                                    const void **sources,
                                                    const float *weights,
                                                    const int count,
                                                    const float mix_factor)
{
  BLI_assert(weights != nullptr);
  BLI_assert(count > 0);

  /* Fake interpolation, we actually copy highest weighted source to dest.
   * Note we also handle bitflags here,
   * in which case we rather choose to transfer value of elements totaling
   * more than 0.5 of weight. */
  int best_src_idx = 0;

  const int data_type = laymap->data_type;
  const int mix_mode = laymap->mix_mode;

  size_t data_size;

  cd_interp interp_cd = nullptr;
  cd_copy copy_cd = nullptr;

  if (!sources) {
    /* Not supported here, abort. */
    return;
  }

  if (int(data_type) & CD_FAKE) {
    data_size = laymap->data_size;
  }
  else {
    const LayerTypeInfo *type_info = layerType_getInfo(eCustomDataType(data_type));

    data_size = size_t(type_info->size);
    interp_cd = type_info->interp;
    copy_cd = type_info->copy;
  }

  void *tmp_dst = MEM_mallocN(data_size, __func__);

  if (count > 1 && !interp_cd) {
    /* We just choose highest weighted source. */
    float max_weight = 0.0f;

    for (int i = 0; i < count; i++) {
      if (weights[i] > max_weight) {
        max_weight = weights[i];
        best_src_idx = i;
      }
    }
  }

  BLI_assert(best_src_idx >= 0);

  if (interp_cd) {
    interp_cd(sources, weights, count, tmp_dst);
  }
  /* No interpolation, just copy highest weight source element's data. */
  else if (copy_cd) {
    copy_cd(sources[best_src_idx], tmp_dst, 1);
  }
  else {
    memcpy(tmp_dst, sources[best_src_idx], data_size);
  }

  if (!(int(data_type) & CD_FAKE)) {
    CustomData_data_mix_value(eCustomDataType(data_type), tmp_dst, data_dst, mix_mode, mix_factor);
  }
  /* Else we can do nothing by default, needs custom interp func!
   * Note this is here only for sake of consistency, not expected to be used much actually? */
  else {
    if (mix_factor >= 0.5f) {
      memcpy(data_dst, tmp_dst, data_size);
    }
  }

  MEM_freeN(tmp_dst);
}

void customdata_data_transfer_interp_normal_normals(const CustomDataTransferLayerMap *laymap,
                                                    void *data_dst,
                                                    const void **sources,
                                                    const float *weights,
                                                    const int count,
                                                    const float mix_factor)
{
  BLI_assert(weights != nullptr);
  BLI_assert(count > 0);

  const eCustomDataType data_type = eCustomDataType(laymap->data_type);
  BLI_assert(data_type == CD_NORMAL);
  const int mix_mode = laymap->mix_mode;

  SpaceTransform *space_transform = static_cast<SpaceTransform *>(laymap->interp_data);

  const LayerTypeInfo *type_info = layerType_getInfo(data_type);
  cd_interp interp_cd = type_info->interp;

  float tmp_dst[3];

  if (!sources) {
    /* Not supported here, abort. */
    return;
  }

  interp_cd(sources, weights, count, tmp_dst);
  if (space_transform) {
    /* tmp_dst is in source space so far, bring it back in destination space. */
    BLI_space_transform_invert_normal(space_transform, tmp_dst);
  }

  CustomData_data_mix_value(data_type, tmp_dst, data_dst, mix_mode, mix_factor);
}

void CustomData_data_transfer(const MeshPairRemap *me_remap, CustomDataTransferLayerMap *laymap)
{
  using namespace blender;
  MeshPairRemapItem *mapit = me_remap->items;
  const int totelem = me_remap->items_num;

  const int data_type = laymap->data_type;
  GVArraySpan data_src_span;
  const void *data_src = [&]() -> const void * {
    if (std::holds_alternative<const void *>(laymap->data_src)) {
      return std::get<const void *>(laymap->data_src);
    }
    data_src_span = std::get<GVArray>(laymap->data_src);
    return data_src_span.data();
  }();
  void *data_dst = [&]() -> void * {
    if (std::holds_alternative<void *>(laymap->data_dst)) {
      return std::get<void *>(laymap->data_dst);
    }
    return std::get<GMutableVArraySpan>(laymap->data_dst).data();
  }();

  size_t data_step;
  size_t data_size;
  size_t data_offset;

  cd_datatransfer_interp interp = nullptr;

  size_t tmp_buff_size = 32;
  const void **tmp_data_src = nullptr;

  /* NOTE: null data_src may happen and be valid (see vgroups...). */
  if (!data_dst) {
    return;
  }

  if (data_src) {
    tmp_data_src = MEM_malloc_arrayN<const void *>(tmp_buff_size, __func__);
  }

  if (int(data_type) & CD_FAKE) {
    data_step = laymap->elem_size;
    data_size = laymap->data_size;
    data_offset = laymap->data_offset;
  }
  else {
    const LayerTypeInfo *type_info = layerType_getInfo(eCustomDataType(data_type));

    /* NOTE: we can use 'fake' CDLayers for crease :/. */
    data_size = size_t(type_info->size);
    data_step = laymap->elem_size ? laymap->elem_size : data_size;
    data_offset = laymap->data_offset;
  }

  interp = laymap->interp ? laymap->interp : customdata_data_transfer_interp_generic;

  for (int i = 0; i < totelem; i++, data_dst = POINTER_OFFSET(data_dst, data_step), mapit++) {
    const int sources_num = mapit->sources_num;
    const float mix_factor = laymap->mix_factor *
                             (laymap->mix_weights ? laymap->mix_weights[i] : 1.0f);

    if (!sources_num) {
      /* No sources for this element, skip it. */
      continue;
    }

    if (tmp_data_src) {
      if (UNLIKELY(sources_num > tmp_buff_size)) {
        tmp_buff_size = size_t(sources_num);
        tmp_data_src = (const void **)MEM_reallocN((void *)tmp_data_src,
                                                   sizeof(*tmp_data_src) * tmp_buff_size);
      }

      for (int j = 0; j < sources_num; j++) {
        const size_t src_idx = size_t(mapit->indices_src[j]);
        tmp_data_src[j] = POINTER_OFFSET(data_src, (data_step * src_idx) + data_offset);
      }
    }

    interp(laymap,
           POINTER_OFFSET(data_dst, data_offset),
           tmp_data_src,
           mapit->weights_src,
           sources_num,
           mix_factor);
  }

  if (laymap->tag_modified_fn) {
    laymap->tag_modified_fn();
  }
  if (std::holds_alternative<GMutableVArraySpan>(laymap->data_dst)) {
    std::get<GMutableVArraySpan>(laymap->data_dst).save();
  }

  MEM_SAFE_FREE(tmp_data_src);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Custom Data IO
 * \{ */

static void get_type_file_write_info(const eCustomDataType type,
                                     const char **r_struct_name,
                                     int *r_struct_num)
{
  const LayerTypeInfo *typeInfo = layerType_getInfo(type);

  *r_struct_name = typeInfo->structname;
  *r_struct_num = typeInfo->structnum;
}

void CustomData_blend_write_prepare(CustomData &data,
                                    const blender::bke::AttrDomain domain,
                                    const int domain_size,
                                    Vector<CustomDataLayer, 16> &layers_to_write,
                                    blender::bke::AttributeStorage::BlendWriteData &write_data)
{
  using namespace blender::bke;
  for (const CustomDataLayer &layer : Span(data.layers, data.totlayer)) {
    if (layer.flag & CD_FLAG_NOCOPY) {
      continue;
    }
    const StringRef name = layer.name;
    if (attribute_name_is_anonymous(name)) {
      continue;
    }

    /* We always write the data in the new #AttributeStorage format, even though it's not yet used
     * at runtime. This block should be removed when the new format is used at runtime. */
    const eCustomDataType data_type = eCustomDataType(layer.type);
    if (const std::optional<AttrType> type = custom_data_type_to_attr_type(data_type)) {
      ::Attribute attribute_dna{};
      attribute_dna.name = layer.name;
      attribute_dna.data_type = int16_t(*type);
      attribute_dna.domain = int8_t(domain);
      attribute_dna.storage_type = int8_t(AttrStorageType::Array);

      /* Do not increase the user count; #::AttributeArray does not act as an owner of the
       * attribute data, since it's only used temporarily for writing files. Changing the user
       * count would be okay too, but it's unnecessary because none of this data should be
       * modified while it's being written anyway. */
      auto &array_dna = write_data.scope.construct<::AttributeArray>();
      array_dna.data = layer.data;
      array_dna.sharing_info = layer.sharing_info;
      array_dna.size = domain_size;
      attribute_dna.data = &array_dna;

      write_data.attributes.append(attribute_dna);
      continue;
    }

    layers_to_write.append(layer);
  }
  data.totlayer = layers_to_write.size();
  data.maxlayer = data.totlayer;
  std::fill_n(data.typemap, CD_NUMTYPES, 0);
  data.totsize = 0;
  if (layers_to_write.is_empty()) {
    data.layers = nullptr;
  }

  /* NOTE: `data->layers` may be null, this happens when adding
   * a legacy #MPoly struct to a mesh with no other face attributes.
   * This leaves us with no unique ID for DNA to identify the old
   * data with when loading the file. */
  if (!data.layers && layers_to_write.size() > 0) {
    /* We just need an address that's unique. */
    data.layers = reinterpret_cast<CustomDataLayer *>(&data.layers);
  }
}

static void write_mdisps(BlendWriter *writer,
                         const int count,
                         const MDisps *mdlist,
                         const int external)
{
  if (mdlist) {
    BLO_write_struct_array(writer, MDisps, count, mdlist);
    for (int i = 0; i < count; i++) {
      const MDisps *md = &mdlist[i];
      if (md->disps) {
        if (!external) {
          BLO_write_float3_array(writer, md->totdisp, &md->disps[0][0]);
        }
      }

      if (md->hidden) {
        BLO_write_int8_array(writer,
                             BLI_BITMAP_SIZE(md->totdisp) * sizeof(BLI_bitmap),
                             reinterpret_cast<const int8_t *>(md->hidden));
      }
    }
  }
}

static void write_grid_paint_mask(BlendWriter *writer,
                                  int count,
                                  const GridPaintMask *grid_paint_mask)
{
  if (grid_paint_mask) {
    BLO_write_struct_array(writer, GridPaintMask, count, grid_paint_mask);
    for (int i = 0; i < count; i++) {
      const GridPaintMask *gpm = &grid_paint_mask[i];
      if (gpm->data) {
        const uint32_t gridsize = uint32_t(CCG_grid_size(gpm->level));
        BLO_write_float_array(writer, gridsize * gridsize, gpm->data);
      }
    }
  }
}

static void blend_write_layer_data(BlendWriter *writer,
                                   const CustomDataLayer &layer,
                                   const int count)
{
  switch (layer.type) {
    case CD_MDEFORMVERT:
      BKE_defvert_blend_write(writer, count, static_cast<const MDeformVert *>(layer.data));
      break;
    case CD_MDISPS:
      write_mdisps(
          writer, count, static_cast<const MDisps *>(layer.data), layer.flag & CD_FLAG_EXTERNAL);
      break;
    case CD_PAINT_MASK:
      BLO_write_float_array(writer, count, static_cast<const float *>(layer.data));
      break;
    case CD_GRID_PAINT_MASK:
      write_grid_paint_mask(writer, count, static_cast<const GridPaintMask *>(layer.data));
      break;
    case CD_PROP_BOOL:
      BLI_STATIC_ASSERT(sizeof(bool) == sizeof(uint8_t),
                        "bool type is expected to have the same size as uint8_t")
      BLO_write_uint8_array(writer, count, static_cast<const uint8_t *>(layer.data));
      break;
    default: {
      const char *structname;
      int structnum;
      get_type_file_write_info(eCustomDataType(layer.type), &structname, &structnum);
      if (structnum > 0) {
        int datasize = structnum * count;
        BLO_write_struct_array_by_name(writer, structname, datasize, layer.data);
      }
      else if (!BLO_write_is_undo(writer)) { /* Do not warn on undo. */
        printf("%s error: layer '%s':%d - can't be written to file\n",
               __func__,
               structname,
               layer.type);
      }
    }
  }
}

void CustomData_blend_write(BlendWriter *writer,
                            CustomData *data,
                            Span<CustomDataLayer> layers_to_write,
                            int count,
                            eCustomDataMask cddata_mask,
                            ID *id)
{
  /* write external customdata (not for undo) */
  if (data->external && !BLO_write_is_undo(writer)) {
    CustomData_external_write(data, id, cddata_mask, count, 0);
  }

  for (const CustomDataLayer &layer : layers_to_write) {
    const size_t size_in_bytes = CustomData_sizeof(eCustomDataType(layer.type)) * count;
    BLO_write_shared(writer, layer.data, size_in_bytes, layer.sharing_info, [&]() {
      blend_write_layer_data(writer, layer, count);
    });
  }

  BLO_write_struct_array_at_address(
      writer, CustomDataLayer, data->totlayer, data->layers, layers_to_write.data());

  if (data->external) {
    BLO_write_struct(writer, CustomDataExternal, data->external);
  }
}

static void blend_read_mdisps(BlendDataReader *reader,
                              const int count,
                              MDisps *mdisps,
                              const int external)
{
  if (mdisps) {
    for (int i = 0; i < count; i++) {
      MDisps &md = mdisps[i];

      BLO_read_float3_array(reader, md.totdisp, reinterpret_cast<float **>(&md.disps));
      BLO_read_int8_array(reader,
                          BLI_BITMAP_SIZE(md.totdisp) * sizeof(BLI_bitmap),
                          reinterpret_cast<int8_t **>(&md.hidden));

      if (md.totdisp && !md.level) {
        /* this calculation is only correct for loop mdisps;
         * if loading pre-BMesh face mdisps this will be
         * overwritten with the correct value in
         * #bm_corners_to_loops() */
        float gridsize = sqrtf(md.totdisp);
        md.level = int(logf(gridsize - 1.0f) / float(M_LN2)) + 1;
      }

      if (!external && !md.disps) {
        md.totdisp = 0;
      }
    }
  }
}

static void blend_read_paint_mask(BlendDataReader *reader,
                                  int count,
                                  GridPaintMask *grid_paint_mask)
{
  if (grid_paint_mask) {
    for (int i = 0; i < count; i++) {
      GridPaintMask *gpm = &grid_paint_mask[i];
      if (gpm->data) {
        const int gridsize = CCG_grid_size(gpm->level);
        BLO_read_float_array(reader, gridsize * gridsize, &gpm->data);
      }
    }
  }
}

static void blend_read_layer_data(BlendDataReader *reader, CustomDataLayer &layer, const int count)
{
  switch (layer.type) {
    case CD_MDEFORMVERT:
      BLO_read_struct_array(reader, MDeformVert, count, &layer.data);
      BKE_defvert_blend_read(reader, count, static_cast<MDeformVert *>(layer.data));
      break;
    case CD_MDISPS:
      BLO_read_struct_array(reader, MDisps, count, &layer.data);
      blend_read_mdisps(
          reader, count, static_cast<MDisps *>(layer.data), layer.flag & CD_FLAG_EXTERNAL);
      break;
    case CD_PAINT_MASK:
      BLO_read_float_array(reader, count, reinterpret_cast<float **>(&layer.data));
      break;
    case CD_GRID_PAINT_MASK:
      BLO_read_struct_array(reader, GridPaintMask, count, &layer.data);
      blend_read_paint_mask(reader, count, static_cast<GridPaintMask *>(layer.data));
      break;
    case CD_PROP_BOOL:
      BLI_STATIC_ASSERT(sizeof(bool) == sizeof(uint8_t),
                        "bool type is expected to have the same size as uint8_t")
      BLO_read_uint8_array(reader, count, reinterpret_cast<uint8_t **>(&layer.data));
      break;
    default: {
      const char *structname;
      int structnum;
      get_type_file_write_info(eCustomDataType(layer.type), &structname, &structnum);
      if (structnum > 0) {
        const int data_num = structnum * count;
        layer.data = BLO_read_struct_by_name_array(reader, structname, data_num, layer.data);
      }
      else {
        /* Can happen with deprecated types of customdata. */
        const size_t elem_size = CustomData_sizeof(eCustomDataType(layer.type));
        BLO_read_struct_array(reader, char, elem_size *count, &layer.data);
      }
    }
  }

  if (CustomData_layer_ensure_data_exists(&layer, count)) {
    /* Under normal operations, this shouldn't happen, but...
     * For a CD_PROP_BOOL example, see #84935.
     * For a CD_MLOOPUV example, see #90620. */
    CLOG_WARN(&LOG,
              "Allocated custom data layer that was not saved correctly for layer.type = %d.",
              layer.type);
  }
}

void CustomData_blend_read(BlendDataReader *reader, CustomData *data, const int count)
{
  BLO_read_struct_array(reader, CustomDataLayer, data->totlayer, &data->layers);

  /* Annoying workaround for bug #31079 loading legacy files with
   * no polygons _but_ have stale custom-data. */
  if (UNLIKELY(count == 0 && data->layers == nullptr && data->totlayer != 0)) {
    CustomData_reset(data);
    return;
  }
  /* There was a short time (Blender 500 sub 33) where the custom data struct was saved in an
   * invalid state (see @11d2f48882). This check is unfortunate, but avoids crashing when trying to
   * load the invalid data (see e.g. #143720). */
  if (UNLIKELY(data->layers == nullptr && data->totlayer != 0)) {
    CustomData_reset(data);
    return;
  }

  BLO_read_struct(reader, CustomDataExternal, &data->external);

  int i = 0;
  while (i < data->totlayer) {
    CustomDataLayer *layer = &data->layers[i];

    if (layer->flag & CD_FLAG_EXTERNAL) {
      layer->flag &= ~CD_FLAG_IN_MEMORY;
    }
    layer->sharing_info = nullptr;

    if (CustomData_verify_versions(data, i)) {
      layer->sharing_info = BLO_read_shared(
          reader, &layer->data, [&]() -> const ImplicitSharingInfo * {
            blend_read_layer_data(reader, *layer, count);
            if (layer->data == nullptr) {
              return nullptr;
            }
            return make_implicit_sharing_info_for_layer(
                eCustomDataType(layer->type), layer->data, count);
          });
      i++;
    }
  }

  /* Ensure allocated size is set to the size of the read array. While this should always be the
   * case (see #CustomData_blend_write_prepare), there can be some corruption in rare cases (e.g.
   * files saved between ff3d535bc2a63092 and 945f32e66d6ada2a). */
  data->maxlayer = data->totlayer;

  CustomData_update_typemap(data);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Custom Data Debugging
 * \{ */

#ifndef NDEBUG

void CustomData_debug_info_from_layers(const CustomData *data, const char *indent, DynStr *dynstr)
{
  for (eCustomDataType type = eCustomDataType(0); type < CD_NUMTYPES;
       type = eCustomDataType(type + 1))
  {
    if (CustomData_has_layer(data, type)) {
      /* NOTE: doesn't account for multiple layers. */
      const char *name = CustomData_layertype_name(type);
      const int size = CustomData_sizeof(type);
      const void *pt = CustomData_get_layer(data, type);
      const int pt_size = pt ? int(MEM_allocN_len(pt) / size) : 0;
      const char *structname;
      int structnum;
      get_type_file_write_info(type, &structname, &structnum);
      BLI_dynstr_appendf(
          dynstr,
          "%sdict(name='%s', struct='%s', type=%d, ptr='%p', elem=%d, length=%d),\n",
          indent,
          name,
          structname,
          type,
          pt,
          size,
          pt_size);
    }
  }
}

#endif /* !NDEBUG */

/** \} */

namespace blender::bke {

/* -------------------------------------------------------------------- */
/** \name Custom Data C++ API
 * \{ */

std::optional<VolumeGridType> custom_data_type_to_volume_grid_type(const eCustomDataType type)
{
  switch (type) {
    case CD_PROP_FLOAT:
      return VOLUME_GRID_FLOAT;
    case CD_PROP_FLOAT3:
      return VOLUME_GRID_VECTOR_FLOAT;
    case CD_PROP_INT32:
      return VOLUME_GRID_INT;
    case CD_PROP_BOOL:
      return VOLUME_GRID_BOOLEAN;
    default:
      return std::nullopt;
  }
}

std::optional<eCustomDataType> volume_grid_type_to_custom_data_type(const VolumeGridType type)
{
  switch (type) {
    case VOLUME_GRID_FLOAT:
      return CD_PROP_FLOAT;
    case VOLUME_GRID_VECTOR_FLOAT:
      return CD_PROP_FLOAT3;
    case VOLUME_GRID_INT:
      return CD_PROP_INT32;
    case VOLUME_GRID_BOOLEAN:
      return CD_PROP_BOOL;
    default:
      return std::nullopt;
  }
}

/** \} */

}  // namespace blender::bke

size_t CustomData_get_elem_size(const CustomDataLayer *layer)
{
  return LAYERTYPEINFO[layer->type].size;
}

void CustomData_count_memory(const CustomData &data,
                             const int totelem,
                             blender::MemoryCounter &memory)
{
  for (const CustomDataLayer &layer : Span{data.layers, data.totlayer}) {
    memory.add_shared(layer.sharing_info, [&](blender::MemoryCounter &shared_memory) {
      /* Not quite correct for all types, but this is only a rough approximation anyway. */
      const int64_t elem_size = CustomData_get_elem_size(&layer);
      shared_memory.add(totelem * elem_size);
    });
  }
}
