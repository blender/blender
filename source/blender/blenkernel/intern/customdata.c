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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2006 Blender Foundation.
 * All rights reserved.
 * Implementation of CustomData.
 *
 * BKE_customdata.h contains the function prototypes for this file.
 */

/** \file
 * \ingroup bke
 */

#include "MEM_guardedalloc.h"

#include "DNA_customdata_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_ID.h"

#include "BLI_utildefines.h"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_string_utils.h"
#include "BLI_math.h"
#include "BLI_math_color_blend.h"
#include "BLI_mempool.h"

#include "BLT_translation.h"

#include "BKE_customdata.h"
#include "BKE_customdata_file.h"
#include "BKE_main.h"
#include "BKE_mesh_mapping.h"
#include "BKE_mesh_remap.h"
#include "BKE_multires.h"

#include "bmesh.h"

#include "CLG_log.h"

/* only for customdata_data_transfer_interp_normal_normals */
#include "data_transfer_intern.h"

/* number of layers to add when growing a CustomData object */
#define CUSTOMDATA_GROW 5

/* ensure typemap size is ok */
BLI_STATIC_ASSERT(ARRAY_SIZE(((CustomData *)NULL)->typemap) == CD_NUMTYPES, "size mismatch");

static CLG_LogRef LOG = {"bke.customdata"};

/** Update mask_dst with layers defined in mask_src (equivalent to a bitwise OR). */
void CustomData_MeshMasks_update(CustomData_MeshMasks *mask_dst,
                                 const CustomData_MeshMasks *mask_src)
{
  mask_dst->vmask |= mask_src->vmask;
  mask_dst->emask |= mask_src->emask;
  mask_dst->fmask |= mask_src->fmask;
  mask_dst->pmask |= mask_src->pmask;
  mask_dst->lmask |= mask_src->lmask;
}

/** Return True if all layers set in \a mask_required are also set in \a mask_ref */
bool CustomData_MeshMasks_are_matching(const CustomData_MeshMasks *mask_ref,
                                       const CustomData_MeshMasks *mask_required)
{
  return (((mask_required->vmask & mask_ref->vmask) == mask_required->vmask) &&
          ((mask_required->emask & mask_ref->emask) == mask_required->emask) &&
          ((mask_required->fmask & mask_ref->fmask) == mask_required->fmask) &&
          ((mask_required->pmask & mask_ref->pmask) == mask_required->pmask) &&
          ((mask_required->lmask & mask_ref->lmask) == mask_required->lmask));
}

/********************* Layer type information **********************/
typedef struct LayerTypeInfo {
  int size; /* the memory size of one element of this layer's data */

  /** name of the struct used, for file writing */
  const char *structname;
  /** number of structs per element, for file writing */
  int structnum;

  /**
   * default layer name.
   * note! when NULL this is a way to ensure there is only ever one item
   * see: CustomData_layertype_is_singleton() */
  const char *defaultname;

  /**
   * a function to copy count elements of this layer's data
   * (deep copy if appropriate)
   * if NULL, memcpy is used
   */
  cd_copy copy;

  /**
   * a function to free any dynamically allocated components of this
   * layer's data (note the data pointer itself should not be freed)
   * size should be the size of one element of this layer's data (e.g.
   * LayerTypeInfo.size)
   */
  void (*free)(void *data, int count, int size);

  /**
   * a function to interpolate between count source elements of this
   * layer's data and store the result in dest
   * if weights == NULL or sub_weights == NULL, they should default to 1
   *
   * weights gives the weight for each element in sources
   * sub_weights gives the sub-element weights for each element in sources
   *    (there should be (sub element count)^2 weights per element)
   * count gives the number of elements in sources
   *
   * \note in some cases \a dest pointer is in \a sources
   *       so all functions have to take this into account and delay
   *       applying changes while reading from sources.
   *       See bug [#32395] - Campbell.
   */
  cd_interp interp;

  /** a function to swap the data in corners of the element */
  void (*swap)(void *data, const int *corner_indices);

  /**
   * a function to set a layer's data to default values. if NULL, the
   * default is assumed to be all zeros */
  void (*set_default)(void *data, int count);

  /** A function used by mesh validating code, must ensures passed item has valid data. */
  cd_validate validate;

  /** functions necessary for geometry collapse */
  bool (*equal)(const void *data1, const void *data2);
  void (*multiply)(void *data, float fac);
  void (*initminmax)(void *min, void *max);
  void (*add)(void *data1, const void *data2);
  void (*dominmax)(const void *data1, void *min, void *max);
  void (*copyvalue)(const void *source, void *dest, const int mixmode, const float mixfactor);

  /** a function to read data from a cdf file */
  int (*read)(CDataFile *cdf, void *data, int count);

  /** a function to write data to a cdf file */
  int (*write)(CDataFile *cdf, const void *data, int count);

  /** a function to determine file size */
  size_t (*filesize)(CDataFile *cdf, const void *data, int count);

  /** a function to determine max allowed number of layers,
   * should be NULL or return -1 if no limit */
  int (*layers_max)(void);
} LayerTypeInfo;

static void layerCopy_mdeformvert(const void *source, void *dest, int count)
{
  int i, size = sizeof(MDeformVert);

  memcpy(dest, source, count * size);

  for (i = 0; i < count; ++i) {
    MDeformVert *dvert = POINTER_OFFSET(dest, i * size);

    if (dvert->totweight) {
      MDeformWeight *dw = MEM_malloc_arrayN(
          dvert->totweight, sizeof(*dw), "layerCopy_mdeformvert dw");

      memcpy(dw, dvert->dw, dvert->totweight * sizeof(*dw));
      dvert->dw = dw;
    }
    else {
      dvert->dw = NULL;
    }
  }
}

static void layerFree_mdeformvert(void *data, int count, int size)
{
  int i;

  for (i = 0; i < count; ++i) {
    MDeformVert *dvert = POINTER_OFFSET(data, i * size);

    if (dvert->dw) {
      MEM_freeN(dvert->dw);
      dvert->dw = NULL;
      dvert->totweight = 0;
    }
  }
}

/* copy just zeros in this case */
static void layerCopy_bmesh_elem_py_ptr(const void *UNUSED(source), void *dest, int count)
{
  int i, size = sizeof(void *);

  for (i = 0; i < count; ++i) {
    void **ptr = POINTER_OFFSET(dest, i * size);
    *ptr = NULL;
  }
}

#ifndef WITH_PYTHON
void bpy_bm_generic_invalidate(struct BPy_BMGeneric *UNUSED(self))
{
  /* dummy */
}
#endif

static void layerFree_bmesh_elem_py_ptr(void *data, int count, int size)
{
  int i;

  for (i = 0; i < count; ++i) {
    void **ptr = POINTER_OFFSET(data, i * size);
    if (*ptr) {
      bpy_bm_generic_invalidate(*ptr);
    }
  }
}

static void layerInterp_mdeformvert(const void **sources,
                                    const float *weights,
                                    const float *UNUSED(sub_weights),
                                    int count,
                                    void *dest)
{
  /* a single linked list of MDeformWeight's
   * use this to avoid double allocs (which LinkNode would do) */
  struct MDeformWeight_Link {
    struct MDeformWeight_Link *next;
    MDeformWeight dw;
  };

  MDeformVert *dvert = dest;
  struct MDeformWeight_Link *dest_dwlink = NULL;
  struct MDeformWeight_Link *node;
  int i, j, totweight;

  if (count <= 0) {
    return;
  }

  /* build a list of unique def_nrs for dest */
  totweight = 0;
  for (i = 0; i < count; ++i) {
    const MDeformVert *source = sources[i];
    float interp_weight = weights ? weights[i] : 1.0f;

    for (j = 0; j < source->totweight; ++j) {
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
        struct MDeformWeight_Link *tmp_dwlink = alloca(sizeof(*tmp_dwlink));
        tmp_dwlink->dw.def_nr = dw->def_nr;
        tmp_dwlink->dw.weight = weight;

        /* inline linklist */
        tmp_dwlink->next = dest_dwlink;
        dest_dwlink = tmp_dwlink;

        totweight++;
      }
    }
  }

  /* delay writing to the destination incase dest is in sources */

  /* now we know how many unique deform weights there are, so realloc */
  if (dvert->dw && (dvert->totweight == totweight)) {
    /* pass (fastpath if we don't need to realloc) */
  }
  else {
    if (dvert->dw) {
      MEM_freeN(dvert->dw);
    }

    if (totweight) {
      dvert->dw = MEM_malloc_arrayN(totweight, sizeof(*dvert->dw), __func__);
    }
  }

  if (totweight) {
    dvert->totweight = totweight;
    for (i = 0, node = dest_dwlink; node; node = node->next, i++) {
      dvert->dw[i] = node->dw;
    }
  }
  else {
    memset(dvert, 0, sizeof(*dvert));
  }
}

static void layerInterp_normal(const void **sources,
                               const float *weights,
                               const float *UNUSED(sub_weights),
                               int count,
                               void *dest)
{
  /* Note: This is linear interpolation, which is not optimal for vectors.
   * Unfortunately, spherical interpolation of more than two values is hairy,
   * so for now it will do... */
  float no[3] = {0.0f};

  while (count--) {
    madd_v3_v3fl(no, (const float *)sources[count], weights[count]);
  }

  /* Weighted sum of normalized vectors will **not** be normalized, even if weights are. */
  normalize_v3_v3((float *)dest, no);
}

static bool layerValidate_normal(void *data, const uint totitems, const bool do_fixes)
{
  static const float no_default[3] = {0.0f, 0.0f, 1.0f}; /* Z-up default normal... */
  float(*no)[3] = data;
  bool has_errors = false;

  for (int i = 0; i < totitems; i++, no++) {
    if (!is_finite_v3((float *)no)) {
      has_errors = true;
      if (do_fixes) {
        copy_v3_v3((float *)no, no_default);
      }
    }
    else if (!compare_ff(len_squared_v3((float *)no), 1.0f, 1e-6f)) {
      has_errors = true;
      if (do_fixes) {
        normalize_v3((float *)no);
      }
    }
  }

  return has_errors;
}

static void layerCopyValue_normal(const void *source,
                                  void *dest,
                                  const int mixmode,
                                  const float mixfactor)
{
  const float *no_src = source;
  float *no_dst = dest;
  float no_tmp[3];

  if (ELEM(mixmode,
           CDT_MIX_NOMIX,
           CDT_MIX_REPLACE_ABOVE_THRESHOLD,
           CDT_MIX_REPLACE_BELOW_THRESHOLD)) {
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

static void layerCopy_tface(const void *source, void *dest, int count)
{
  const MTFace *source_tf = (const MTFace *)source;
  MTFace *dest_tf = (MTFace *)dest;
  int i;

  for (i = 0; i < count; ++i) {
    dest_tf[i] = source_tf[i];
  }
}

static void layerInterp_tface(
    const void **sources, const float *weights, const float *sub_weights, int count, void *dest)
{
  MTFace *tf = dest;
  int i, j, k;
  float uv[4][2] = {{0.0f}};
  const float *sub_weight;

  if (count <= 0) {
    return;
  }

  sub_weight = sub_weights;
  for (i = 0; i < count; ++i) {
    float weight = weights ? weights[i] : 1;
    const MTFace *src = sources[i];

    for (j = 0; j < 4; ++j) {
      if (sub_weights) {
        for (k = 0; k < 4; ++k, ++sub_weight) {
          madd_v2_v2fl(uv[j], src->uv[k], (*sub_weight) * weight);
        }
      }
      else {
        madd_v2_v2fl(uv[j], src->uv[j], weight);
      }
    }
  }

  /* delay writing to the destination incase dest is in sources */
  *tf = *(MTFace *)(*sources);
  memcpy(tf->uv, uv, sizeof(tf->uv));
}

static void layerSwap_tface(void *data, const int *corner_indices)
{
  MTFace *tf = data;
  float uv[4][2];
  int j;

  for (j = 0; j < 4; ++j) {
    const int source_index = corner_indices[j];
    copy_v2_v2(uv[j], tf->uv[source_index]);
  }

  memcpy(tf->uv, uv, sizeof(tf->uv));
}

static void layerDefault_tface(void *data, int count)
{
  static MTFace default_tf = {{{0, 0}, {1, 0}, {1, 1}, {0, 1}}};
  MTFace *tf = (MTFace *)data;
  int i;

  for (i = 0; i < count; i++) {
    tf[i] = default_tf;
  }
}

static int layerMaxNum_tface(void)
{
  return MAX_MTFACE;
}

static void layerCopy_propFloat(const void *source, void *dest, int count)
{
  memcpy(dest, source, sizeof(MFloatProperty) * count);
}

static bool layerValidate_propFloat(void *data, const uint totitems, const bool do_fixes)
{
  MFloatProperty *fp = data;
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

static void layerCopy_propInt(const void *source, void *dest, int count)
{
  memcpy(dest, source, sizeof(MIntProperty) * count);
}

static void layerCopy_propString(const void *source, void *dest, int count)
{
  memcpy(dest, source, sizeof(MStringProperty) * count);
}

static void layerCopy_origspace_face(const void *source, void *dest, int count)
{
  const OrigSpaceFace *source_tf = (const OrigSpaceFace *)source;
  OrigSpaceFace *dest_tf = (OrigSpaceFace *)dest;
  int i;

  for (i = 0; i < count; ++i) {
    dest_tf[i] = source_tf[i];
  }
}

static void layerInterp_origspace_face(
    const void **sources, const float *weights, const float *sub_weights, int count, void *dest)
{
  OrigSpaceFace *osf = dest;
  int i, j, k;
  float uv[4][2] = {{0.0f}};
  const float *sub_weight;

  if (count <= 0) {
    return;
  }

  sub_weight = sub_weights;
  for (i = 0; i < count; ++i) {
    float weight = weights ? weights[i] : 1;
    const OrigSpaceFace *src = sources[i];

    for (j = 0; j < 4; ++j) {
      if (sub_weights) {
        for (k = 0; k < 4; ++k, ++sub_weight) {
          madd_v2_v2fl(uv[j], src->uv[k], (*sub_weight) * weight);
        }
      }
      else {
        madd_v2_v2fl(uv[j], src->uv[j], weight);
      }
    }
  }

  /* delay writing to the destination in case dest is in sources */
  memcpy(osf->uv, uv, sizeof(osf->uv));
}

static void layerSwap_origspace_face(void *data, const int *corner_indices)
{
  OrigSpaceFace *osf = data;
  float uv[4][2];
  int j;

  for (j = 0; j < 4; ++j) {
    copy_v2_v2(uv[j], osf->uv[corner_indices[j]]);
  }
  memcpy(osf->uv, uv, sizeof(osf->uv));
}

static void layerDefault_origspace_face(void *data, int count)
{
  static OrigSpaceFace default_osf = {{{0, 0}, {1, 0}, {1, 1}, {0, 1}}};
  OrigSpaceFace *osf = (OrigSpaceFace *)data;
  int i;

  for (i = 0; i < count; i++) {
    osf[i] = default_osf;
  }
}

static void layerSwap_mdisps(void *data, const int *ci)
{
  MDisps *s = data;
  float(*d)[3] = NULL;
  int corners, cornersize, S;

  if (s->disps) {
    int nverts = (ci[1] == 3) ? 4 : 3; /* silly way to know vertex count of face */
    corners = multires_mdisp_corners(s);
    cornersize = s->totdisp / corners;

    if (corners != nverts) {
      /* happens when face changed vertex count in edit mode
       * if it happened, just forgot displacement */

      MEM_freeN(s->disps);
      s->totdisp = (s->totdisp / corners) * nverts;
      s->disps = MEM_calloc_arrayN(s->totdisp, sizeof(float) * 3, "mdisp swap");
      return;
    }

    d = MEM_calloc_arrayN(s->totdisp, 3 * sizeof(float), "mdisps swap");

    for (S = 0; S < corners; S++) {
      memcpy(d + cornersize * S, s->disps + cornersize * ci[S], cornersize * 3 * sizeof(float));
    }

    MEM_freeN(s->disps);
    s->disps = d;
  }
}

static void layerCopy_mdisps(const void *source, void *dest, int count)
{
  int i;
  const MDisps *s = source;
  MDisps *d = dest;

  for (i = 0; i < count; ++i) {
    if (s[i].disps) {
      d[i].disps = MEM_dupallocN(s[i].disps);
      d[i].hidden = MEM_dupallocN(s[i].hidden);
    }
    else {
      d[i].disps = NULL;
      d[i].hidden = NULL;
    }

    /* still copy even if not in memory, displacement can be external */
    d[i].totdisp = s[i].totdisp;
    d[i].level = s[i].level;
  }
}

static void layerFree_mdisps(void *data, int count, int UNUSED(size))
{
  int i;
  MDisps *d = data;

  for (i = 0; i < count; ++i) {
    if (d[i].disps) {
      MEM_freeN(d[i].disps);
    }
    if (d[i].hidden) {
      MEM_freeN(d[i].hidden);
    }
    d[i].disps = NULL;
    d[i].hidden = NULL;
    d[i].totdisp = 0;
    d[i].level = 0;
  }
}

static int layerRead_mdisps(CDataFile *cdf, void *data, int count)
{
  MDisps *d = data;
  int i;

  for (i = 0; i < count; ++i) {
    if (!d[i].disps) {
      d[i].disps = MEM_calloc_arrayN(d[i].totdisp, 3 * sizeof(float), "mdisps read");
    }

    if (!cdf_read_data(cdf, d[i].totdisp * 3 * sizeof(float), d[i].disps)) {
      CLOG_ERROR(&LOG, "failed to read multires displacement %d/%d %d", i, count, d[i].totdisp);
      return 0;
    }
  }

  return 1;
}

static int layerWrite_mdisps(CDataFile *cdf, const void *data, int count)
{
  const MDisps *d = data;
  int i;

  for (i = 0; i < count; ++i) {
    if (!cdf_write_data(cdf, d[i].totdisp * 3 * sizeof(float), d[i].disps)) {
      CLOG_ERROR(&LOG, "failed to write multires displacement %d/%d %d", i, count, d[i].totdisp);
      return 0;
    }
  }

  return 1;
}

static size_t layerFilesize_mdisps(CDataFile *UNUSED(cdf), const void *data, int count)
{
  const MDisps *d = data;
  size_t size = 0;
  int i;

  for (i = 0; i < count; ++i) {
    size += d[i].totdisp * 3 * sizeof(float);
  }

  return size;
}

static void layerCopy_grid_paint_mask(const void *source, void *dest, int count)
{
  int i;
  const GridPaintMask *s = source;
  GridPaintMask *d = dest;

  for (i = 0; i < count; ++i) {
    if (s[i].data) {
      d[i].data = MEM_dupallocN(s[i].data);
      d[i].level = s[i].level;
    }
    else {
      d[i].data = NULL;
      d[i].level = 0;
    }
  }
}

static void layerFree_grid_paint_mask(void *data, int count, int UNUSED(size))
{
  int i;
  GridPaintMask *gpm = data;

  for (i = 0; i < count; ++i) {
    if (gpm[i].data) {
      MEM_freeN(gpm[i].data);
    }
    gpm[i].data = NULL;
    gpm[i].level = 0;
  }
}

/* --------- */
static void layerCopyValue_mloopcol(const void *source,
                                    void *dest,
                                    const int mixmode,
                                    const float mixfactor)
{
  const MLoopCol *m1 = source;
  MLoopCol *m2 = dest;
  unsigned char tmp_col[4];

  if (ELEM(mixmode,
           CDT_MIX_NOMIX,
           CDT_MIX_REPLACE_ABOVE_THRESHOLD,
           CDT_MIX_REPLACE_BELOW_THRESHOLD)) {
    /* Modes that do a full copy or nothing. */
    if (ELEM(mixmode, CDT_MIX_REPLACE_ABOVE_THRESHOLD, CDT_MIX_REPLACE_BELOW_THRESHOLD)) {
      /* TODO: Check for a real valid way to get 'factor' value of our dest color? */
      const float f = ((float)m2->r + (float)m2->g + (float)m2->b) / 3.0f;
      if (mixmode == CDT_MIX_REPLACE_ABOVE_THRESHOLD && f < mixfactor) {
        return; /* Do Nothing! */
      }
      else if (mixmode == CDT_MIX_REPLACE_BELOW_THRESHOLD && f > mixfactor) {
        return; /* Do Nothing! */
      }
    }
    m2->r = m1->r;
    m2->g = m1->g;
    m2->b = m1->b;
  }
  else { /* Modes that support 'real' mix factor. */
    unsigned char src[4] = {m1->r, m1->g, m1->b, m1->a};
    unsigned char dst[4] = {m2->r, m2->g, m2->b, m2->a};

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

    m2->r = (char)dst[0];
    m2->g = (char)dst[1];
    m2->b = (char)dst[2];
  }
  m2->a = m1->a;
}

static bool layerEqual_mloopcol(const void *data1, const void *data2)
{
  const MLoopCol *m1 = data1, *m2 = data2;
  float r, g, b, a;

  r = m1->r - m2->r;
  g = m1->g - m2->g;
  b = m1->b - m2->b;
  a = m1->a - m2->a;

  return r * r + g * g + b * b + a * a < 0.001f;
}

static void layerMultiply_mloopcol(void *data, float fac)
{
  MLoopCol *m = data;

  m->r = (float)m->r * fac;
  m->g = (float)m->g * fac;
  m->b = (float)m->b * fac;
  m->a = (float)m->a * fac;
}

static void layerAdd_mloopcol(void *data1, const void *data2)
{
  MLoopCol *m = data1;
  const MLoopCol *m2 = data2;

  m->r += m2->r;
  m->g += m2->g;
  m->b += m2->b;
  m->a += m2->a;
}

static void layerDoMinMax_mloopcol(const void *data, void *vmin, void *vmax)
{
  const MLoopCol *m = data;
  MLoopCol *min = vmin, *max = vmax;

  if (m->r < min->r) {
    min->r = m->r;
  }
  if (m->g < min->g) {
    min->g = m->g;
  }
  if (m->b < min->b) {
    min->b = m->b;
  }
  if (m->a < min->a) {
    min->a = m->a;
  }

  if (m->r > max->r) {
    max->r = m->r;
  }
  if (m->g > max->g) {
    max->g = m->g;
  }
  if (m->b > max->b) {
    max->b = m->b;
  }
  if (m->a > max->a) {
    max->a = m->a;
  }
}

static void layerInitMinMax_mloopcol(void *vmin, void *vmax)
{
  MLoopCol *min = vmin, *max = vmax;

  min->r = 255;
  min->g = 255;
  min->b = 255;
  min->a = 255;

  max->r = 0;
  max->g = 0;
  max->b = 0;
  max->a = 0;
}

static void layerDefault_mloopcol(void *data, int count)
{
  MLoopCol default_mloopcol = {255, 255, 255, 255};
  MLoopCol *mlcol = (MLoopCol *)data;
  int i;
  for (i = 0; i < count; i++) {
    mlcol[i] = default_mloopcol;
  }
}

static void layerInterp_mloopcol(
    const void **sources, const float *weights, const float *sub_weights, int count, void *dest)
{
  MLoopCol *mc = dest;
  struct {
    float a;
    float r;
    float g;
    float b;
  } col = {0};

  const float *sub_weight = sub_weights;
  for (int i = 0; i < count; ++i) {
    float weight = weights ? weights[i] : 1;
    const MLoopCol *src = sources[i];
    if (sub_weights) {
      col.r += src->r * (*sub_weight) * weight;
      col.g += src->g * (*sub_weight) * weight;
      col.b += src->b * (*sub_weight) * weight;
      col.a += src->a * (*sub_weight) * weight;
      sub_weight++;
    }
    else {
      col.r += src->r * weight;
      col.g += src->g * weight;
      col.b += src->b * weight;
      col.a += src->a * weight;
    }
  }

  /* Subdivide smooth or fractal can cause problems without clamping
   * although weights should also not cause this situation */

  /* also delay writing to the destination incase dest is in sources */
  mc->r = round_fl_to_uchar_clamp(col.r);
  mc->g = round_fl_to_uchar_clamp(col.g);
  mc->b = round_fl_to_uchar_clamp(col.b);
  mc->a = round_fl_to_uchar_clamp(col.a);
}

static int layerMaxNum_mloopcol(void)
{
  return MAX_MCOL;
}

static void layerCopyValue_mloopuv(const void *source,
                                   void *dest,
                                   const int mixmode,
                                   const float mixfactor)
{
  const MLoopUV *luv1 = source;
  MLoopUV *luv2 = dest;

  /* We only support a limited subset of advanced mixing here -
   * namely the mixfactor interpolation. */

  if (mixmode == CDT_MIX_NOMIX) {
    copy_v2_v2(luv2->uv, luv1->uv);
  }
  else {
    interp_v2_v2v2(luv2->uv, luv2->uv, luv1->uv, mixfactor);
  }
}

static bool layerEqual_mloopuv(const void *data1, const void *data2)
{
  const MLoopUV *luv1 = data1, *luv2 = data2;

  return len_squared_v2v2(luv1->uv, luv2->uv) < 0.00001f;
}

static void layerMultiply_mloopuv(void *data, float fac)
{
  MLoopUV *luv = data;

  mul_v2_fl(luv->uv, fac);
}

static void layerInitMinMax_mloopuv(void *vmin, void *vmax)
{
  MLoopUV *min = vmin, *max = vmax;

  INIT_MINMAX2(min->uv, max->uv);
}

static void layerDoMinMax_mloopuv(const void *data, void *vmin, void *vmax)
{
  const MLoopUV *luv = data;
  MLoopUV *min = vmin, *max = vmax;

  minmax_v2v2_v2(min->uv, max->uv, luv->uv);
}

static void layerAdd_mloopuv(void *data1, const void *data2)
{
  MLoopUV *l1 = data1;
  const MLoopUV *l2 = data2;

  add_v2_v2(l1->uv, l2->uv);
}

static void layerInterp_mloopuv(
    const void **sources, const float *weights, const float *sub_weights, int count, void *dest)
{
  float uv[2];
  int flag = 0;
  int i;

  zero_v2(uv);

  if (sub_weights) {
    const float *sub_weight = sub_weights;
    for (i = 0; i < count; i++) {
      float weight = (weights ? weights[i] : 1.0f) * (*sub_weight);
      const MLoopUV *src = sources[i];
      madd_v2_v2fl(uv, src->uv, weight);
      if (weight > 0.0f) {
        flag |= src->flag;
      }
      sub_weight++;
    }
  }
  else {
    for (i = 0; i < count; i++) {
      float weight = weights ? weights[i] : 1;
      const MLoopUV *src = sources[i];
      madd_v2_v2fl(uv, src->uv, weight);
      if (weight > 0.0f) {
        flag |= src->flag;
      }
    }
  }

  /* delay writing to the destination incase dest is in sources */
  copy_v2_v2(((MLoopUV *)dest)->uv, uv);
  ((MLoopUV *)dest)->flag = flag;
}

static bool layerValidate_mloopuv(void *data, const uint totitems, const bool do_fixes)
{
  MLoopUV *uv = data;
  bool has_errors = false;

  for (int i = 0; i < totitems; i++, uv++) {
    if (!is_finite_v2(uv->uv)) {
      if (do_fixes) {
        zero_v2(uv->uv);
      }
      has_errors = true;
    }
  }

  return has_errors;
}

/* origspace is almost exact copy of mloopuv's, keep in sync */
static void layerCopyValue_mloop_origspace(const void *source,
                                           void *dest,
                                           const int UNUSED(mixmode),
                                           const float UNUSED(mixfactor))
{
  const OrigSpaceLoop *luv1 = source;
  OrigSpaceLoop *luv2 = dest;

  copy_v2_v2(luv2->uv, luv1->uv);
}

static bool layerEqual_mloop_origspace(const void *data1, const void *data2)
{
  const OrigSpaceLoop *luv1 = data1, *luv2 = data2;

  return len_squared_v2v2(luv1->uv, luv2->uv) < 0.00001f;
}

static void layerMultiply_mloop_origspace(void *data, float fac)
{
  OrigSpaceLoop *luv = data;

  mul_v2_fl(luv->uv, fac);
}

static void layerInitMinMax_mloop_origspace(void *vmin, void *vmax)
{
  OrigSpaceLoop *min = vmin, *max = vmax;

  INIT_MINMAX2(min->uv, max->uv);
}

static void layerDoMinMax_mloop_origspace(const void *data, void *vmin, void *vmax)
{
  const OrigSpaceLoop *luv = data;
  OrigSpaceLoop *min = vmin, *max = vmax;

  minmax_v2v2_v2(min->uv, max->uv, luv->uv);
}

static void layerAdd_mloop_origspace(void *data1, const void *data2)
{
  OrigSpaceLoop *l1 = data1;
  const OrigSpaceLoop *l2 = data2;

  add_v2_v2(l1->uv, l2->uv);
}

static void layerInterp_mloop_origspace(
    const void **sources, const float *weights, const float *sub_weights, int count, void *dest)
{
  float uv[2];
  int i;

  zero_v2(uv);

  if (sub_weights) {
    const float *sub_weight = sub_weights;
    for (i = 0; i < count; i++) {
      float weight = weights ? weights[i] : 1.0f;
      const OrigSpaceLoop *src = sources[i];
      madd_v2_v2fl(uv, src->uv, (*sub_weight) * weight);
      sub_weight++;
    }
  }
  else {
    for (i = 0; i < count; i++) {
      float weight = weights ? weights[i] : 1.0f;
      const OrigSpaceLoop *src = sources[i];
      madd_v2_v2fl(uv, src->uv, weight);
    }
  }

  /* delay writing to the destination incase dest is in sources */
  copy_v2_v2(((OrigSpaceLoop *)dest)->uv, uv);
}
/* --- end copy */

static void layerInterp_mcol(
    const void **sources, const float *weights, const float *sub_weights, int count, void *dest)
{
  MCol *mc = dest;
  int i, j, k;
  struct {
    float a;
    float r;
    float g;
    float b;
  } col[4] = {{0.0f}};

  const float *sub_weight;

  if (count <= 0) {
    return;
  }

  sub_weight = sub_weights;
  for (i = 0; i < count; ++i) {
    float weight = weights ? weights[i] : 1;

    for (j = 0; j < 4; ++j) {
      if (sub_weights) {
        const MCol *src = sources[i];
        for (k = 0; k < 4; ++k, ++sub_weight, ++src) {
          const float w = (*sub_weight) * weight;
          col[j].a += src->a * w;
          col[j].r += src->r * w;
          col[j].g += src->g * w;
          col[j].b += src->b * w;
        }
      }
      else {
        const MCol *src = sources[i];
        col[j].a += src[j].a * weight;
        col[j].r += src[j].r * weight;
        col[j].g += src[j].g * weight;
        col[j].b += src[j].b * weight;
      }
    }
  }

  /* delay writing to the destination incase dest is in sources */
  for (j = 0; j < 4; ++j) {

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
  MCol *mcol = data;
  MCol col[4];
  int j;

  for (j = 0; j < 4; ++j) {
    col[j] = mcol[corner_indices[j]];
  }

  memcpy(mcol, col, sizeof(col));
}

static void layerDefault_mcol(void *data, int count)
{
  static MCol default_mcol = {255, 255, 255, 255};
  MCol *mcol = (MCol *)data;
  int i;

  for (i = 0; i < 4 * count; i++) {
    mcol[i] = default_mcol;
  }
}

static void layerDefault_origindex(void *data, int count)
{
  copy_vn_i((int *)data, count, ORIGINDEX_NONE);
}

static void layerInterp_bweight(const void **sources,
                                const float *weights,
                                const float *UNUSED(sub_weights),
                                int count,
                                void *dest)
{
  float f;
  float **in = (float **)sources;
  int i;

  if (count <= 0) {
    return;
  }

  f = 0.0f;

  if (weights) {
    for (i = 0; i < count; ++i) {
      f += *in[i] * weights[i];
    }
  }
  else {
    for (i = 0; i < count; ++i) {
      f += *in[i];
    }
  }

  /* delay writing to the destination incase dest is in sources */
  *((float *)dest) = f;
}

static void layerInterp_shapekey(const void **sources,
                                 const float *weights,
                                 const float *UNUSED(sub_weights),
                                 int count,
                                 void *dest)
{
  float co[3];
  float **in = (float **)sources;
  int i;

  if (count <= 0) {
    return;
  }

  zero_v3(co);

  if (weights) {
    for (i = 0; i < count; ++i) {
      madd_v3_v3fl(co, in[i], weights[i]);
    }
  }
  else {
    for (i = 0; i < count; ++i) {
      add_v3_v3(co, in[i]);
    }
  }

  /* delay writing to the destination incase dest is in sources */
  copy_v3_v3((float *)dest, co);
}

static void layerDefault_mvert_skin(void *data, int count)
{
  MVertSkin *vs = data;
  int i;

  for (i = 0; i < count; i++) {
    copy_v3_fl(vs[i].radius, 0.25f);
    vs[i].flag = 0;
  }
}

static void layerCopy_mvert_skin(const void *source, void *dest, int count)
{
  memcpy(dest, source, sizeof(MVertSkin) * count);
}

static void layerInterp_mvert_skin(const void **sources,
                                   const float *weights,
                                   const float *UNUSED(sub_weights),
                                   int count,
                                   void *dest)
{
  MVertSkin *vs_dst = dest;
  float radius[3], w;
  int i;

  zero_v3(radius);
  for (i = 0; i < count; i++) {
    const MVertSkin *vs_src = sources[i];
    w = weights ? weights[i] : 1.0f;

    madd_v3_v3fl(radius, vs_src->radius, w);
  }

  /* delay writing to the destination incase dest is in sources */
  vs_dst = dest;
  copy_v3_v3(vs_dst->radius, radius);
  vs_dst->flag &= ~MVERT_SKIN_ROOT;
}

static void layerSwap_flnor(void *data, const int *corner_indices)
{
  short(*flnors)[4][3] = data;
  short nors[4][3];
  int i = 4;

  while (i--) {
    copy_v3_v3_short(nors[i], (*flnors)[corner_indices[i]]);
  }

  memcpy(flnors, nors, sizeof(nors));
}

static void layerDefault_fmap(void *data, int count)
{
  int *fmap_num = (int *)data;
  for (int i = 0; i < count; i++) {
    fmap_num[i] = -1;
  }
}

static const LayerTypeInfo LAYERTYPEINFO[CD_NUMTYPES] = {
    /* 0: CD_MVERT */
    {sizeof(MVert), "MVert", 1, NULL, NULL, NULL, NULL, NULL, NULL},
    /* 1: CD_MSTICKY */ /* DEPRECATED */
    {sizeof(float) * 2, "", 1, NULL, NULL, NULL, NULL, NULL, NULL},
    /* 2: CD_MDEFORMVERT */
    {sizeof(MDeformVert),
     "MDeformVert",
     1,
     NULL,
     layerCopy_mdeformvert,
     layerFree_mdeformvert,
     layerInterp_mdeformvert,
     NULL,
     NULL},
    /* 3: CD_MEDGE */
    {sizeof(MEdge), "MEdge", 1, NULL, NULL, NULL, NULL, NULL, NULL},
    /* 4: CD_MFACE */
    {sizeof(MFace), "MFace", 1, NULL, NULL, NULL, NULL, NULL, NULL},
    /* 5: CD_MTFACE */
    {sizeof(MTFace),
     "MTFace",
     1,
     N_("UVMap"),
     layerCopy_tface,
     NULL,
     layerInterp_tface,
     layerSwap_tface,
     layerDefault_tface,
     NULL,
     NULL,
     NULL,
     NULL,
     NULL,
     NULL,
     NULL,
     NULL,
     NULL,
     NULL,
     layerMaxNum_tface},
    /* 6: CD_MCOL */
    /* 4 MCol structs per face */
    {sizeof(MCol) * 4,
     "MCol",
     4,
     N_("Col"),
     NULL,
     NULL,
     layerInterp_mcol,
     layerSwap_mcol,
     layerDefault_mcol,
     NULL,
     NULL,
     NULL,
     NULL,
     NULL,
     NULL,
     NULL,
     NULL,
     NULL,
     NULL,
     layerMaxNum_mloopcol},
    /* 7: CD_ORIGINDEX */
    {sizeof(int), "", 0, NULL, NULL, NULL, NULL, NULL, layerDefault_origindex},
    /* 8: CD_NORMAL */
    /* 3 floats per normal vector */
    {sizeof(float) * 3,
     "vec3f",
     1,
     NULL,
     NULL,
     NULL,
     layerInterp_normal,
     NULL,
     NULL,
     layerValidate_normal,
     NULL,
     NULL,
     NULL,
     NULL,
     NULL,
     layerCopyValue_normal},
    /* 9: CD_FACEMAP */
    {sizeof(int), "", 0, NULL, NULL, NULL, NULL, NULL, layerDefault_fmap, NULL},
    /* 10: CD_PROP_FLT */
    {sizeof(MFloatProperty),
     "MFloatProperty",
     1,
     N_("Float"),
     layerCopy_propFloat,
     NULL,
     NULL,
     NULL,
     NULL,
     layerValidate_propFloat},
    /* 11: CD_PROP_INT */
    {sizeof(MIntProperty), "MIntProperty", 1, N_("Int"), layerCopy_propInt, NULL, NULL, NULL},
    /* 12: CD_PROP_STR */
    {sizeof(MStringProperty),
     "MStringProperty",
     1,
     N_("String"),
     layerCopy_propString,
     NULL,
     NULL,
     NULL},
    /* 13: CD_ORIGSPACE */
    {sizeof(OrigSpaceFace),
     "OrigSpaceFace",
     1,
     N_("UVMap"),
     layerCopy_origspace_face,
     NULL,
     layerInterp_origspace_face,
     layerSwap_origspace_face,
     layerDefault_origspace_face},
    /* 14: CD_ORCO */
    {sizeof(float) * 3, "", 0, NULL, NULL, NULL, NULL, NULL, NULL},
    /* 15: CD_MTEXPOLY */ /* DEPRECATED */
    /* note, when we expose the UV Map / TexFace split to the user,
     * change this back to face Texture. */
    {sizeof(int), "", 0, NULL, NULL, NULL, NULL, NULL, NULL},
    /* 16: CD_MLOOPUV */
    {sizeof(MLoopUV),
     "MLoopUV",
     1,
     N_("UVMap"),
     NULL,
     NULL,
     layerInterp_mloopuv,
     NULL,
     NULL,
     layerValidate_mloopuv,
     layerEqual_mloopuv,
     layerMultiply_mloopuv,
     layerInitMinMax_mloopuv,
     layerAdd_mloopuv,
     layerDoMinMax_mloopuv,
     layerCopyValue_mloopuv,
     NULL,
     NULL,
     NULL,
     layerMaxNum_tface},
    /* 17: CD_MLOOPCOL */
    {sizeof(MLoopCol),
     "MLoopCol",
     1,
     N_("Col"),
     NULL,
     NULL,
     layerInterp_mloopcol,
     NULL,
     layerDefault_mloopcol,
     NULL,
     layerEqual_mloopcol,
     layerMultiply_mloopcol,
     layerInitMinMax_mloopcol,
     layerAdd_mloopcol,
     layerDoMinMax_mloopcol,
     layerCopyValue_mloopcol,
     NULL,
     NULL,
     NULL,
     layerMaxNum_mloopcol},
    /* 18: CD_TANGENT */
    {sizeof(float) * 4 * 4, "", 0, N_("Tangent"), NULL, NULL, NULL, NULL, NULL},
    /* 19: CD_MDISPS */
    {sizeof(MDisps),
     "MDisps",
     1,
     NULL,
     layerCopy_mdisps,
     layerFree_mdisps,
     NULL,
     layerSwap_mdisps,
     NULL,
     NULL,
     NULL,
     NULL,
     NULL,
     NULL,
     NULL,
     NULL,
     layerRead_mdisps,
     layerWrite_mdisps,
     layerFilesize_mdisps},
    /* 20: CD_PREVIEW_MCOL */
    {sizeof(MCol) * 4,
     "MCol",
     4,
     N_("PreviewCol"),
     NULL,
     NULL,
     layerInterp_mcol,
     layerSwap_mcol,
     layerDefault_mcol},
    /* 21: CD_ID_MCOL */ /* DEPRECATED */
    {sizeof(MCol) * 4, "", 0, NULL, NULL, NULL, NULL, NULL, NULL},
    /* 22: CD_TEXTURE_MCOL */
    {sizeof(MCol) * 4,
     "MCol",
     4,
     N_("TexturedCol"),
     NULL,
     NULL,
     layerInterp_mcol,
     layerSwap_mcol,
     layerDefault_mcol},
    /* 23: CD_CLOTH_ORCO */
    {sizeof(float) * 3, "", 0, NULL, NULL, NULL, NULL, NULL, NULL},
    /* 24: CD_RECAST */
    {sizeof(MRecast), "MRecast", 1, N_("Recast"), NULL, NULL, NULL, NULL},

    /* BMESH ONLY */
    /* 25: CD_MPOLY */
    {sizeof(MPoly), "MPoly", 1, N_("NGon Face"), NULL, NULL, NULL, NULL, NULL},
    /* 26: CD_MLOOP */
    {sizeof(MLoop), "MLoop", 1, N_("NGon Face-Vertex"), NULL, NULL, NULL, NULL, NULL},
    /* 27: CD_SHAPE_KEYINDEX */
    {sizeof(int), "", 0, NULL, NULL, NULL, NULL, NULL, NULL},
    /* 28: CD_SHAPEKEY */
    {sizeof(float) * 3, "", 0, N_("ShapeKey"), NULL, NULL, layerInterp_shapekey},
    /* 29: CD_BWEIGHT */
    {sizeof(float), "", 0, N_("BevelWeight"), NULL, NULL, layerInterp_bweight},
    /* 30: CD_CREASE */
    {sizeof(float), "", 0, N_("SubSurfCrease"), NULL, NULL, layerInterp_bweight},
    /* 31: CD_ORIGSPACE_MLOOP */
    {sizeof(OrigSpaceLoop),
     "OrigSpaceLoop",
     1,
     N_("OS Loop"),
     NULL,
     NULL,
     layerInterp_mloop_origspace,
     NULL,
     NULL,
     NULL,
     layerEqual_mloop_origspace,
     layerMultiply_mloop_origspace,
     layerInitMinMax_mloop_origspace,
     layerAdd_mloop_origspace,
     layerDoMinMax_mloop_origspace,
     layerCopyValue_mloop_origspace},
    /* 32: CD_PREVIEW_MLOOPCOL */
    {sizeof(MLoopCol),
     "MLoopCol",
     1,
     N_("PreviewLoopCol"),
     NULL,
     NULL,
     layerInterp_mloopcol,
     NULL,
     layerDefault_mloopcol,
     NULL,
     layerEqual_mloopcol,
     layerMultiply_mloopcol,
     layerInitMinMax_mloopcol,
     layerAdd_mloopcol,
     layerDoMinMax_mloopcol,
     layerCopyValue_mloopcol},
    /* 33: CD_BM_ELEM_PYPTR */
    {sizeof(void *),
     "",
     1,
     NULL,
     layerCopy_bmesh_elem_py_ptr,
     layerFree_bmesh_elem_py_ptr,
     NULL,
     NULL,
     NULL},

    /* END BMESH ONLY */

    /* 34: CD_PAINT_MASK */
    {sizeof(float), "", 0, NULL, NULL, NULL, NULL, NULL, NULL},
    /* 35: CD_GRID_PAINT_MASK */
    {sizeof(GridPaintMask),
     "GridPaintMask",
     1,
     NULL,
     layerCopy_grid_paint_mask,
     layerFree_grid_paint_mask,
     NULL,
     NULL,
     NULL},
    /* 36: CD_MVERT_SKIN */
    {sizeof(MVertSkin),
     "MVertSkin",
     1,
     NULL,
     layerCopy_mvert_skin,
     NULL,
     layerInterp_mvert_skin,
     NULL,
     layerDefault_mvert_skin},
    /* 37: CD_FREESTYLE_EDGE */
    {sizeof(FreestyleEdge), "FreestyleEdge", 1, NULL, NULL, NULL, NULL, NULL, NULL},
    /* 38: CD_FREESTYLE_FACE */
    {sizeof(FreestyleFace), "FreestyleFace", 1, NULL, NULL, NULL, NULL, NULL, NULL},
    /* 39: CD_MLOOPTANGENT */
    {sizeof(float[4]), "", 0, NULL, NULL, NULL, NULL, NULL, NULL},
    /* 40: CD_TESSLOOPNORMAL */
    {sizeof(short[4][3]), "", 0, NULL, NULL, NULL, NULL, layerSwap_flnor, NULL},
    /* 41: CD_CUSTOMLOOPNORMAL */
    {sizeof(short[2]), "vec2s", 1, NULL, NULL, NULL, NULL, NULL, NULL},
};

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
    /* 39-41 */ "CDMLoopTangent",
    "CDTessLoopNormal",
    "CDCustomLoopNormal",
};

const CustomData_MeshMasks CD_MASK_BAREMESH = {
    .vmask = CD_MASK_MVERT | CD_MASK_BWEIGHT,
    .emask = CD_MASK_MEDGE | CD_MASK_BWEIGHT,
    .fmask = 0,
    .lmask = CD_MASK_MLOOP,
    .pmask = CD_MASK_MPOLY | CD_MASK_FACEMAP,
};
const CustomData_MeshMasks CD_MASK_BAREMESH_ORIGINDEX = {
    .vmask = CD_MASK_MVERT | CD_MASK_BWEIGHT | CD_MASK_ORIGINDEX,
    .emask = CD_MASK_MEDGE | CD_MASK_BWEIGHT | CD_MASK_ORIGINDEX,
    .fmask = 0,
    .lmask = CD_MASK_MLOOP,
    .pmask = CD_MASK_MPOLY | CD_MASK_FACEMAP | CD_MASK_ORIGINDEX,
};
const CustomData_MeshMasks CD_MASK_MESH = {
    .vmask = (CD_MASK_MVERT | CD_MASK_MDEFORMVERT | CD_MASK_MVERT_SKIN | CD_MASK_PAINT_MASK |
              CD_MASK_GENERIC_DATA),
    .emask = (CD_MASK_MEDGE | CD_MASK_FREESTYLE_EDGE | CD_MASK_GENERIC_DATA),
    .fmask = 0,
    .lmask = (CD_MASK_MLOOP | CD_MASK_MDISPS | CD_MASK_MLOOPUV | CD_MASK_MLOOPCOL |
              CD_MASK_CUSTOMLOOPNORMAL | CD_MASK_GRID_PAINT_MASK | CD_MASK_GENERIC_DATA),
    .pmask = (CD_MASK_MPOLY | CD_MASK_RECAST | CD_MASK_FACEMAP | CD_MASK_FREESTYLE_FACE |
              CD_MASK_GENERIC_DATA),
};
const CustomData_MeshMasks CD_MASK_EDITMESH = {
    .vmask = (CD_MASK_MDEFORMVERT | CD_MASK_PAINT_MASK | CD_MASK_MVERT_SKIN | CD_MASK_SHAPEKEY |
              CD_MASK_SHAPE_KEYINDEX | CD_MASK_GENERIC_DATA),
    .emask = (CD_MASK_GENERIC_DATA),
    .fmask = 0,
    .lmask = (CD_MASK_MDISPS | CD_MASK_MLOOPUV | CD_MASK_MLOOPCOL | CD_MASK_CUSTOMLOOPNORMAL |
              CD_MASK_GRID_PAINT_MASK | CD_MASK_GENERIC_DATA),
    .pmask = (CD_MASK_RECAST | CD_MASK_FACEMAP | CD_MASK_GENERIC_DATA),
};
const CustomData_MeshMasks CD_MASK_DERIVEDMESH = {
    .vmask = (CD_MASK_ORIGINDEX | CD_MASK_MDEFORMVERT | CD_MASK_SHAPEKEY | CD_MASK_MVERT_SKIN |
              CD_MASK_ORCO | CD_MASK_CLOTH_ORCO | CD_MASK_GENERIC_DATA),
    .emask = (CD_MASK_ORIGINDEX | CD_MASK_FREESTYLE_EDGE | CD_MASK_GENERIC_DATA),
    .fmask = (CD_MASK_ORIGINDEX | CD_MASK_ORIGSPACE | CD_MASK_PREVIEW_MCOL | CD_MASK_TANGENT),
    .lmask = (CD_MASK_MLOOPUV | CD_MASK_MLOOPCOL | CD_MASK_CUSTOMLOOPNORMAL |
              CD_MASK_PREVIEW_MLOOPCOL | CD_MASK_ORIGSPACE_MLOOP |
              CD_MASK_GENERIC_DATA), /* XXX MISSING CD_MASK_MLOOPTANGENT ? */
    .pmask = (CD_MASK_ORIGINDEX | CD_MASK_RECAST | CD_MASK_FREESTYLE_FACE | CD_MASK_FACEMAP |
              CD_MASK_GENERIC_DATA),
};
const CustomData_MeshMasks CD_MASK_BMESH = {
    .vmask = (CD_MASK_MDEFORMVERT | CD_MASK_BWEIGHT | CD_MASK_MVERT_SKIN | CD_MASK_SHAPEKEY |
              CD_MASK_SHAPE_KEYINDEX | CD_MASK_PAINT_MASK | CD_MASK_GENERIC_DATA),
    .emask = (CD_MASK_BWEIGHT | CD_MASK_CREASE | CD_MASK_FREESTYLE_EDGE | CD_MASK_GENERIC_DATA),
    .fmask = 0,
    .lmask = (CD_MASK_MDISPS | CD_MASK_MLOOPUV | CD_MASK_MLOOPCOL | CD_MASK_CUSTOMLOOPNORMAL |
              CD_MASK_GRID_PAINT_MASK | CD_MASK_GENERIC_DATA),
    .pmask = (CD_MASK_RECAST | CD_MASK_FREESTYLE_FACE | CD_MASK_FACEMAP | CD_MASK_GENERIC_DATA),
};
/**
 * cover values copied by #BKE_mesh_loops_to_tessdata
 */
const CustomData_MeshMasks CD_MASK_FACECORNERS = {
    .vmask = 0,
    .emask = 0,
    .fmask = (CD_MASK_MTFACE | CD_MASK_MCOL | CD_MASK_PREVIEW_MCOL | CD_MASK_ORIGSPACE |
              CD_MASK_TESSLOOPNORMAL | CD_MASK_TANGENT),
    .lmask = (CD_MASK_MLOOPUV | CD_MASK_MLOOPCOL | CD_MASK_PREVIEW_MLOOPCOL |
              CD_MASK_ORIGSPACE_MLOOP | CD_MASK_NORMAL | CD_MASK_MLOOPTANGENT),
    .pmask = 0,
};
const CustomData_MeshMasks CD_MASK_EVERYTHING = {
    .vmask = (CD_MASK_MVERT | CD_MASK_BM_ELEM_PYPTR | CD_MASK_ORIGINDEX | CD_MASK_NORMAL |
              CD_MASK_MDEFORMVERT | CD_MASK_BWEIGHT | CD_MASK_MVERT_SKIN | CD_MASK_ORCO |
              CD_MASK_CLOTH_ORCO | CD_MASK_SHAPEKEY | CD_MASK_SHAPE_KEYINDEX | CD_MASK_PAINT_MASK |
              CD_MASK_GENERIC_DATA),
    .emask = (CD_MASK_MEDGE | CD_MASK_BM_ELEM_PYPTR | CD_MASK_ORIGINDEX | CD_MASK_BWEIGHT |
              CD_MASK_CREASE | CD_MASK_FREESTYLE_EDGE | CD_MASK_GENERIC_DATA),
    .fmask = (CD_MASK_MFACE | CD_MASK_ORIGINDEX | CD_MASK_NORMAL | CD_MASK_MTFACE | CD_MASK_MCOL |
              CD_MASK_ORIGSPACE | CD_MASK_TANGENT | CD_MASK_TESSLOOPNORMAL | CD_MASK_PREVIEW_MCOL |
              CD_MASK_GENERIC_DATA),
    .lmask = (CD_MASK_MLOOP | CD_MASK_BM_ELEM_PYPTR | CD_MASK_MDISPS | CD_MASK_NORMAL |
              CD_MASK_MLOOPUV | CD_MASK_MLOOPCOL | CD_MASK_CUSTOMLOOPNORMAL |
              CD_MASK_MLOOPTANGENT | CD_MASK_PREVIEW_MLOOPCOL | CD_MASK_ORIGSPACE_MLOOP |
              CD_MASK_GRID_PAINT_MASK | CD_MASK_GENERIC_DATA),
    .pmask = (CD_MASK_MPOLY | CD_MASK_BM_ELEM_PYPTR | CD_MASK_ORIGINDEX | CD_MASK_NORMAL |
              CD_MASK_RECAST | CD_MASK_FACEMAP | CD_MASK_FREESTYLE_FACE | CD_MASK_GENERIC_DATA),
};

static const LayerTypeInfo *layerType_getInfo(int type)
{
  if (type < 0 || type >= CD_NUMTYPES) {
    return NULL;
  }

  return &LAYERTYPEINFO[type];
}

static const char *layerType_getName(int type)
{
  if (type < 0 || type >= CD_NUMTYPES) {
    return NULL;
  }

  return LAYERTYPENAMES[type];
}

void customData_mask_layers__print(const CustomData_MeshMasks *mask)
{
  int i;

  printf("verts mask=0x%lx:\n", (long unsigned int)mask->vmask);
  for (i = 0; i < CD_NUMTYPES; i++) {
    if (mask->vmask & CD_TYPE_AS_MASK(i)) {
      printf("  %s\n", layerType_getName(i));
    }
  }

  printf("edges mask=0x%lx:\n", (long unsigned int)mask->emask);
  for (i = 0; i < CD_NUMTYPES; i++) {
    if (mask->emask & CD_TYPE_AS_MASK(i)) {
      printf("  %s\n", layerType_getName(i));
    }
  }

  printf("faces mask=0x%lx:\n", (long unsigned int)mask->fmask);
  for (i = 0; i < CD_NUMTYPES; i++) {
    if (mask->fmask & CD_TYPE_AS_MASK(i)) {
      printf("  %s\n", layerType_getName(i));
    }
  }

  printf("loops mask=0x%lx:\n", (long unsigned int)mask->lmask);
  for (i = 0; i < CD_NUMTYPES; i++) {
    if (mask->lmask & CD_TYPE_AS_MASK(i)) {
      printf("  %s\n", layerType_getName(i));
    }
  }

  printf("polys mask=0x%lx:\n", (long unsigned int)mask->pmask);
  for (i = 0; i < CD_NUMTYPES; i++) {
    if (mask->pmask & CD_TYPE_AS_MASK(i)) {
      printf("  %s\n", layerType_getName(i));
    }
  }
}

/********************* CustomData functions *********************/
static void customData_update_offsets(CustomData *data);

static CustomDataLayer *customData_add_layer__internal(CustomData *data,
                                                       int type,
                                                       eCDAllocType alloctype,
                                                       void *layerdata,
                                                       int totelem,
                                                       const char *name);

void CustomData_update_typemap(CustomData *data)
{
  int i, lasttype = -1;

  for (i = 0; i < CD_NUMTYPES; i++) {
    data->typemap[i] = -1;
  }

  for (i = 0; i < data->totlayer; i++) {
    const int type = data->layers[i].type;
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

bool CustomData_merge(const struct CustomData *source,
                      struct CustomData *dest,
                      CustomDataMask mask,
                      eCDAllocType alloctype,
                      int totelem)
{
  /*const LayerTypeInfo *typeInfo;*/
  CustomDataLayer *layer, *newlayer;
  void *data;
  int i, type, lasttype = -1, lastactive = 0, lastrender = 0, lastclone = 0, lastmask = 0,
               flag = 0;
  int number = 0, maxnumber = -1;
  bool changed = false;

  for (i = 0; i < source->totlayer; ++i) {
    layer = &source->layers[i];
    /*typeInfo = layerType_getInfo(layer->type);*/ /*UNUSED*/

    type = layer->type;
    flag = layer->flag;

    if (type != lasttype) {
      number = 0;
      maxnumber = CustomData_layertype_layers_max(type);
      lastactive = layer->active;
      lastrender = layer->active_rnd;
      lastclone = layer->active_clone;
      lastmask = layer->active_mask;
      lasttype = type;
    }
    else {
      number++;
    }

    if (flag & CD_FLAG_NOCOPY) {
      continue;
    }
    else if (!(mask & CD_TYPE_AS_MASK(type))) {
      continue;
    }
    else if ((maxnumber != -1) && (number >= maxnumber)) {
      continue;
    }
    else if (CustomData_get_layer_named(dest, type, layer->name)) {
      continue;
    }

    switch (alloctype) {
      case CD_ASSIGN:
      case CD_REFERENCE:
      case CD_DUPLICATE:
        data = layer->data;
        break;
      default:
        data = NULL;
        break;
    }

    if ((alloctype == CD_ASSIGN) && (flag & CD_FLAG_NOFREE)) {
      newlayer = customData_add_layer__internal(
          dest, type, CD_REFERENCE, data, totelem, layer->name);
    }
    else {
      newlayer = customData_add_layer__internal(dest, type, alloctype, data, totelem, layer->name);
    }

    if (newlayer) {
      newlayer->uid = layer->uid;

      newlayer->active = lastactive;
      newlayer->active_rnd = lastrender;
      newlayer->active_clone = lastclone;
      newlayer->active_mask = lastmask;
      newlayer->flag |= flag & (CD_FLAG_EXTERNAL | CD_FLAG_IN_MEMORY);
      changed = true;
    }
  }

  CustomData_update_typemap(dest);
  return changed;
}

/* NOTE: Take care of referenced layers by yourself! */
void CustomData_realloc(CustomData *data, int totelem)
{
  int i;
  for (i = 0; i < data->totlayer; ++i) {
    CustomDataLayer *layer = &data->layers[i];
    const LayerTypeInfo *typeInfo;
    if (layer->flag & CD_FLAG_NOFREE) {
      continue;
    }
    typeInfo = layerType_getInfo(layer->type);
    layer->data = MEM_reallocN(layer->data, (size_t)totelem * typeInfo->size);
  }
}

void CustomData_copy(const struct CustomData *source,
                     struct CustomData *dest,
                     CustomDataMask mask,
                     eCDAllocType alloctype,
                     int totelem)
{
  CustomData_reset(dest);

  if (source->external) {
    dest->external = MEM_dupallocN(source->external);
  }

  CustomData_merge(source, dest, mask, alloctype, totelem);
}

static void customData_free_layer__internal(CustomDataLayer *layer, int totelem)
{
  const LayerTypeInfo *typeInfo;

  if (!(layer->flag & CD_FLAG_NOFREE) && layer->data) {
    typeInfo = layerType_getInfo(layer->type);

    if (typeInfo->free) {
      typeInfo->free(layer->data, totelem, typeInfo->size);
    }

    if (layer->data) {
      MEM_freeN(layer->data);
    }
  }
}

static void CustomData_external_free(CustomData *data)
{
  if (data->external) {
    MEM_freeN(data->external);
    data->external = NULL;
  }
}

void CustomData_reset(CustomData *data)
{
  memset(data, 0, sizeof(*data));
  copy_vn_i(data->typemap, CD_NUMTYPES, -1);
}

void CustomData_free(CustomData *data, int totelem)
{
  int i;

  for (i = 0; i < data->totlayer; ++i) {
    customData_free_layer__internal(&data->layers[i], totelem);
  }

  if (data->layers) {
    MEM_freeN(data->layers);
  }

  CustomData_external_free(data);
  CustomData_reset(data);
}

void CustomData_free_typemask(struct CustomData *data, int totelem, CustomDataMask mask)
{
  int i;

  for (i = 0; i < data->totlayer; ++i) {
    CustomDataLayer *layer = &data->layers[i];
    if (!(mask & CD_TYPE_AS_MASK(layer->type))) {
      continue;
    }
    customData_free_layer__internal(layer, totelem);
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
  int i, offset = 0;

  for (i = 0; i < data->totlayer; ++i) {
    typeInfo = layerType_getInfo(data->layers[i].type);

    data->layers[i].offset = offset;
    offset += typeInfo->size;
  }

  data->totsize = offset;
  CustomData_update_typemap(data);
}

/* to use when we're in the middle of modifying layers */
static int CustomData_get_layer_index__notypemap(const CustomData *data, int type)
{
  int i;

  for (i = 0; i < data->totlayer; ++i) {
    if (data->layers[i].type == type) {
      return i;
    }
  }

  return -1;
}

/* -------------------------------------------------------------------- */
/* index values to access the layers (offset from the layer start) */

int CustomData_get_layer_index(const CustomData *data, int type)
{
  BLI_assert(customdata_typemap_is_valid(data));
  return data->typemap[type];
}

int CustomData_get_layer_index_n(const struct CustomData *data, int type, int n)
{
  int i = CustomData_get_layer_index(data, type);

  if (i != -1) {
    BLI_assert(i + n < data->totlayer);
    i = (data->layers[i + n].type == type) ? (i + n) : (-1);
  }

  return i;
}

int CustomData_get_named_layer_index(const CustomData *data, int type, const char *name)
{
  int i;

  for (i = 0; i < data->totlayer; ++i) {
    if (data->layers[i].type == type) {
      if (STREQ(data->layers[i].name, name)) {
        return i;
      }
    }
  }

  return -1;
}

int CustomData_get_active_layer_index(const CustomData *data, int type)
{
  const int layer_index = data->typemap[type];
  BLI_assert(customdata_typemap_is_valid(data));
  return (layer_index != -1) ? layer_index + data->layers[layer_index].active : -1;
}

int CustomData_get_render_layer_index(const CustomData *data, int type)
{
  const int layer_index = data->typemap[type];
  BLI_assert(customdata_typemap_is_valid(data));
  return (layer_index != -1) ? layer_index + data->layers[layer_index].active_rnd : -1;
}

int CustomData_get_clone_layer_index(const CustomData *data, int type)
{
  const int layer_index = data->typemap[type];
  BLI_assert(customdata_typemap_is_valid(data));
  return (layer_index != -1) ? layer_index + data->layers[layer_index].active_clone : -1;
}

int CustomData_get_stencil_layer_index(const CustomData *data, int type)
{
  const int layer_index = data->typemap[type];
  BLI_assert(customdata_typemap_is_valid(data));
  return (layer_index != -1) ? layer_index + data->layers[layer_index].active_mask : -1;
}

/* -------------------------------------------------------------------- */
/* index values per layer type */

int CustomData_get_named_layer(const struct CustomData *data, int type, const char *name)
{
  const int named_index = CustomData_get_named_layer_index(data, type, name);
  const int layer_index = data->typemap[type];
  BLI_assert(customdata_typemap_is_valid(data));
  return (named_index != -1) ? named_index - layer_index : -1;
}

int CustomData_get_active_layer(const CustomData *data, int type)
{
  const int layer_index = data->typemap[type];
  BLI_assert(customdata_typemap_is_valid(data));
  return (layer_index != -1) ? data->layers[layer_index].active : -1;
}

int CustomData_get_render_layer(const CustomData *data, int type)
{
  const int layer_index = data->typemap[type];
  BLI_assert(customdata_typemap_is_valid(data));
  return (layer_index != -1) ? data->layers[layer_index].active_rnd : -1;
}

int CustomData_get_clone_layer(const CustomData *data, int type)
{
  const int layer_index = data->typemap[type];
  BLI_assert(customdata_typemap_is_valid(data));
  return (layer_index != -1) ? data->layers[layer_index].active_clone : -1;
}

int CustomData_get_stencil_layer(const CustomData *data, int type)
{
  const int layer_index = data->typemap[type];
  BLI_assert(customdata_typemap_is_valid(data));
  return (layer_index != -1) ? data->layers[layer_index].active_mask : -1;
}

void CustomData_set_layer_active(CustomData *data, int type, int n)
{
  int i;

  for (i = 0; i < data->totlayer; ++i) {
    if (data->layers[i].type == type) {
      data->layers[i].active = n;
    }
  }
}

void CustomData_set_layer_render(CustomData *data, int type, int n)
{
  int i;

  for (i = 0; i < data->totlayer; ++i) {
    if (data->layers[i].type == type) {
      data->layers[i].active_rnd = n;
    }
  }
}

void CustomData_set_layer_clone(CustomData *data, int type, int n)
{
  int i;

  for (i = 0; i < data->totlayer; ++i) {
    if (data->layers[i].type == type) {
      data->layers[i].active_clone = n;
    }
  }
}

void CustomData_set_layer_stencil(CustomData *data, int type, int n)
{
  int i;

  for (i = 0; i < data->totlayer; ++i) {
    if (data->layers[i].type == type) {
      data->layers[i].active_mask = n;
    }
  }
}

/* For using with an index from CustomData_get_active_layer_index and
 * CustomData_get_render_layer_index. */
void CustomData_set_layer_active_index(CustomData *data, int type, int n)
{
  int i;

  for (i = 0; i < data->totlayer; ++i) {
    if (data->layers[i].type == type) {
      data->layers[i].active = n - i;
    }
  }
}

void CustomData_set_layer_render_index(CustomData *data, int type, int n)
{
  int i;

  for (i = 0; i < data->totlayer; ++i) {
    if (data->layers[i].type == type) {
      data->layers[i].active_rnd = n - i;
    }
  }
}

void CustomData_set_layer_clone_index(CustomData *data, int type, int n)
{
  int i;

  for (i = 0; i < data->totlayer; ++i) {
    if (data->layers[i].type == type) {
      data->layers[i].active_clone = n - i;
    }
  }
}

void CustomData_set_layer_stencil_index(CustomData *data, int type, int n)
{
  int i;

  for (i = 0; i < data->totlayer; ++i) {
    if (data->layers[i].type == type) {
      data->layers[i].active_mask = n - i;
    }
  }
}

void CustomData_set_layer_flag(struct CustomData *data, int type, int flag)
{
  int i;

  for (i = 0; i < data->totlayer; ++i) {
    if (data->layers[i].type == type) {
      data->layers[i].flag |= flag;
    }
  }
}

void CustomData_clear_layer_flag(struct CustomData *data, int type, int flag)
{
  const int nflag = ~flag;

  for (int i = 0; i < data->totlayer; ++i) {
    if (data->layers[i].type == type) {
      data->layers[i].flag &= nflag;
    }
  }
}

static int customData_resize(CustomData *data, int amount)
{
  CustomDataLayer *tmp = MEM_calloc_arrayN(
      (data->maxlayer + amount), sizeof(*tmp), "CustomData->layers");
  if (!tmp) {
    return 0;
  }

  data->maxlayer += amount;
  if (data->layers) {
    memcpy(tmp, data->layers, sizeof(*tmp) * data->totlayer);
    MEM_freeN(data->layers);
  }
  data->layers = tmp;

  return 1;
}

static CustomDataLayer *customData_add_layer__internal(CustomData *data,
                                                       int type,
                                                       eCDAllocType alloctype,
                                                       void *layerdata,
                                                       int totelem,
                                                       const char *name)
{
  const LayerTypeInfo *typeInfo = layerType_getInfo(type);
  int flag = 0, index = data->totlayer;
  void *newlayerdata = NULL;

  /* Passing a layerdata to copy from with an alloctype that won't copy is
   * most likely a bug */
  BLI_assert(!layerdata || (alloctype == CD_ASSIGN) || (alloctype == CD_DUPLICATE) ||
             (alloctype == CD_REFERENCE));

  if (!typeInfo->defaultname && CustomData_has_layer(data, type)) {
    return &data->layers[CustomData_get_layer_index(data, type)];
  }

  if ((alloctype == CD_ASSIGN) || (alloctype == CD_REFERENCE)) {
    newlayerdata = layerdata;
  }
  else if (totelem > 0 && typeInfo->size > 0) {
    if (alloctype == CD_DUPLICATE && layerdata) {
      newlayerdata = MEM_malloc_arrayN((size_t)totelem, typeInfo->size, layerType_getName(type));
    }
    else {
      newlayerdata = MEM_calloc_arrayN((size_t)totelem, typeInfo->size, layerType_getName(type));
    }

    if (!newlayerdata) {
      return NULL;
    }
  }

  if (alloctype == CD_DUPLICATE && layerdata) {
    if (typeInfo->copy) {
      typeInfo->copy(layerdata, newlayerdata, totelem);
    }
    else {
      memcpy(newlayerdata, layerdata, (size_t)totelem * typeInfo->size);
    }
  }
  else if (alloctype == CD_DEFAULT) {
    if (typeInfo->set_default) {
      typeInfo->set_default(newlayerdata, totelem);
    }
  }
  else if (alloctype == CD_REFERENCE) {
    flag |= CD_FLAG_NOFREE;
  }

  if (index >= data->maxlayer) {
    if (!customData_resize(data, CUSTOMDATA_GROW)) {
      if (newlayerdata != layerdata) {
        MEM_freeN(newlayerdata);
      }
      return NULL;
    }
  }

  data->totlayer++;

  /* keep layers ordered by type */
  for (; index > 0 && data->layers[index - 1].type > type; --index) {
    data->layers[index] = data->layers[index - 1];
  }

  data->layers[index].type = type;
  data->layers[index].flag = flag;
  data->layers[index].data = newlayerdata;

  /* Set default name if none exists. Note we only call DATA_()  once
   * we know there is a default name, to avoid overhead of locale lookups
   * in the depsgraph. */
  if (!name && typeInfo->defaultname) {
    name = DATA_(typeInfo->defaultname);
  }

  if (name) {
    BLI_strncpy(data->layers[index].name, name, sizeof(data->layers[index].name));
    CustomData_set_layer_unique_name(data, index);
  }
  else {
    data->layers[index].name[0] = '\0';
  }

  if (index > 0 && data->layers[index - 1].type == type) {
    data->layers[index].active = data->layers[index - 1].active;
    data->layers[index].active_rnd = data->layers[index - 1].active_rnd;
    data->layers[index].active_clone = data->layers[index - 1].active_clone;
    data->layers[index].active_mask = data->layers[index - 1].active_mask;
  }
  else {
    data->layers[index].active = 0;
    data->layers[index].active_rnd = 0;
    data->layers[index].active_clone = 0;
    data->layers[index].active_mask = 0;
  }

  customData_update_offsets(data);

  return &data->layers[index];
}

void *CustomData_add_layer(
    CustomData *data, int type, eCDAllocType alloctype, void *layerdata, int totelem)
{
  CustomDataLayer *layer;
  const LayerTypeInfo *typeInfo = layerType_getInfo(type);

  layer = customData_add_layer__internal(
      data, type, alloctype, layerdata, totelem, typeInfo->defaultname);
  CustomData_update_typemap(data);

  if (layer) {
    return layer->data;
  }

  return NULL;
}

/*same as above but accepts a name*/
void *CustomData_add_layer_named(CustomData *data,
                                 int type,
                                 eCDAllocType alloctype,
                                 void *layerdata,
                                 int totelem,
                                 const char *name)
{
  CustomDataLayer *layer;

  layer = customData_add_layer__internal(data, type, alloctype, layerdata, totelem, name);
  CustomData_update_typemap(data);

  if (layer) {
    return layer->data;
  }

  return NULL;
}

bool CustomData_free_layer(CustomData *data, int type, int totelem, int index)
{
  const int index_first = CustomData_get_layer_index(data, type);
  const int n = index - index_first;
  int i;

  BLI_assert(index >= index_first);
  if ((index_first == -1) || (n < 0)) {
    return false;
  }
  BLI_assert(data->layers[index].type == type);

  customData_free_layer__internal(&data->layers[index], totelem);

  for (i = index + 1; i < data->totlayer; ++i) {
    data->layers[i - 1] = data->layers[i];
  }

  data->totlayer--;

  /* if layer was last of type in array, set new active layer */
  i = CustomData_get_layer_index__notypemap(data, type);

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

bool CustomData_free_layer_active(CustomData *data, int type, int totelem)
{
  int index = 0;
  index = CustomData_get_active_layer_index(data, type);
  if (index == -1) {
    return false;
  }
  return CustomData_free_layer(data, type, totelem, index);
}

void CustomData_free_layers(CustomData *data, int type, int totelem)
{
  const int index = CustomData_get_layer_index(data, type);
  while (CustomData_free_layer(data, type, totelem, index)) {
    /* pass */
  }
}

bool CustomData_has_layer(const CustomData *data, int type)
{
  return (CustomData_get_layer_index(data, type) != -1);
}

int CustomData_number_of_layers(const CustomData *data, int type)
{
  int i, number = 0;

  for (i = 0; i < data->totlayer; i++) {
    if (data->layers[i].type == type) {
      number++;
    }
  }

  return number;
}

int CustomData_number_of_layers_typemask(const CustomData *data, CustomDataMask mask)
{
  int i, number = 0;

  for (i = 0; i < data->totlayer; i++) {
    if (mask & CD_TYPE_AS_MASK(data->layers[i].type)) {
      number++;
    }
  }

  return number;
}

static void *customData_duplicate_referenced_layer_index(CustomData *data,
                                                         const int layer_index,
                                                         const int totelem)
{
  CustomDataLayer *layer;

  if (layer_index == -1) {
    return NULL;
  }

  layer = &data->layers[layer_index];

  if (layer->flag & CD_FLAG_NOFREE) {
    /* MEM_dupallocN won't work in case of complex layers, like e.g.
     * CD_MDEFORMVERT, which has pointers to allocated data...
     * So in case a custom copy function is defined, use it!
     */
    const LayerTypeInfo *typeInfo = layerType_getInfo(layer->type);

    if (typeInfo->copy) {
      void *dst_data = MEM_malloc_arrayN(
          (size_t)totelem, typeInfo->size, "CD duplicate ref layer");
      typeInfo->copy(layer->data, dst_data, totelem);
      layer->data = dst_data;
    }
    else {
      layer->data = MEM_dupallocN(layer->data);
    }

    layer->flag &= ~CD_FLAG_NOFREE;
  }

  return layer->data;
}

void *CustomData_duplicate_referenced_layer(CustomData *data, const int type, const int totelem)
{
  int layer_index;

  /* get the layer index of the first layer of type */
  layer_index = CustomData_get_active_layer_index(data, type);

  return customData_duplicate_referenced_layer_index(data, layer_index, totelem);
}

void *CustomData_duplicate_referenced_layer_n(CustomData *data,
                                              const int type,
                                              const int n,
                                              const int totelem)
{
  int layer_index;

  /* get the layer index of the desired layer */
  layer_index = CustomData_get_layer_index_n(data, type, n);

  return customData_duplicate_referenced_layer_index(data, layer_index, totelem);
}

void *CustomData_duplicate_referenced_layer_named(CustomData *data,
                                                  const int type,
                                                  const char *name,
                                                  const int totelem)
{
  int layer_index;

  /* get the layer index of the desired layer */
  layer_index = CustomData_get_named_layer_index(data, type, name);

  return customData_duplicate_referenced_layer_index(data, layer_index, totelem);
}

bool CustomData_is_referenced_layer(struct CustomData *data, int type)
{
  CustomDataLayer *layer;
  int layer_index;

  /* get the layer index of the first layer of type */
  layer_index = CustomData_get_active_layer_index(data, type);
  if (layer_index == -1) {
    return false;
  }

  layer = &data->layers[layer_index];

  return (layer->flag & CD_FLAG_NOFREE) != 0;
}

void CustomData_free_temporary(CustomData *data, int totelem)
{
  CustomDataLayer *layer;
  int i, j;
  bool changed = false;

  for (i = 0, j = 0; i < data->totlayer; ++i) {
    layer = &data->layers[i];

    if (i != j) {
      data->layers[j] = data->layers[i];
    }

    if ((layer->flag & CD_FLAG_TEMPORARY) == CD_FLAG_TEMPORARY) {
      customData_free_layer__internal(layer, totelem);
      changed = true;
    }
    else {
      j++;
    }
  }

  data->totlayer = j;

  if (data->totlayer <= data->maxlayer - CUSTOMDATA_GROW) {
    customData_resize(data, -CUSTOMDATA_GROW);
    changed = true;
  }

  if (changed) {
    customData_update_offsets(data);
  }
}

void CustomData_set_only_copy(const struct CustomData *data, CustomDataMask mask)
{
  int i;

  for (i = 0; i < data->totlayer; ++i) {
    if (!(mask & CD_TYPE_AS_MASK(data->layers[i].type))) {
      data->layers[i].flag |= CD_FLAG_NOCOPY;
    }
  }
}

void CustomData_copy_elements(int type, void *src_data_ofs, void *dst_data_ofs, int count)
{
  const LayerTypeInfo *typeInfo = layerType_getInfo(type);

  if (typeInfo->copy) {
    typeInfo->copy(src_data_ofs, dst_data_ofs, count);
  }
  else {
    memcpy(dst_data_ofs, src_data_ofs, (size_t)count * typeInfo->size);
  }
}

static void CustomData_copy_data_layer(const CustomData *source,
                                       CustomData *dest,
                                       int src_i,
                                       int dst_i,
                                       int src_index,
                                       int dst_index,
                                       int count)
{
  const LayerTypeInfo *typeInfo;

  const void *src_data = source->layers[src_i].data;
  void *dst_data = dest->layers[dst_i].data;

  typeInfo = layerType_getInfo(source->layers[src_i].type);

  const size_t src_offset = (size_t)src_index * typeInfo->size;
  const size_t dst_offset = (size_t)dst_index * typeInfo->size;

  if (!count || !src_data || !dst_data) {
    if (count && !(src_data == NULL && dst_data == NULL)) {
      CLOG_WARN(&LOG,
                "null data for %s type (%p --> %p), skipping",
                layerType_getName(source->layers[src_i].type),
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
           (size_t)count * typeInfo->size);
  }
}

void CustomData_copy_data_named(
    const CustomData *source, CustomData *dest, int source_index, int dest_index, int count)
{
  int src_i, dest_i;

  /* copies a layer at a time */
  for (src_i = 0; src_i < source->totlayer; ++src_i) {

    dest_i = CustomData_get_named_layer_index(
        dest, source->layers[src_i].type, source->layers[src_i].name);

    /* if we found a matching layer, copy the data */
    if (dest_i != -1) {
      CustomData_copy_data_layer(source, dest, src_i, dest_i, source_index, dest_index, count);
    }
  }
}

void CustomData_copy_data(
    const CustomData *source, CustomData *dest, int source_index, int dest_index, int count)
{
  int src_i, dest_i;

  /* copies a layer at a time */
  dest_i = 0;
  for (src_i = 0; src_i < source->totlayer; ++src_i) {

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
                                     int type,
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

void CustomData_free_elem(CustomData *data, int index, int count)
{
  int i;
  const LayerTypeInfo *typeInfo;

  for (i = 0; i < data->totlayer; ++i) {
    if (!(data->layers[i].flag & CD_FLAG_NOFREE)) {
      typeInfo = layerType_getInfo(data->layers[i].type);

      if (typeInfo->free) {
        size_t offset = (size_t)index * typeInfo->size;

        typeInfo->free(POINTER_OFFSET(data->layers[i].data, offset), count, typeInfo->size);
      }
    }
  }
}

#define SOURCE_BUF_SIZE 100

void CustomData_interp(const CustomData *source,
                       CustomData *dest,
                       const int *src_indices,
                       const float *weights,
                       const float *sub_weights,
                       int count,
                       int dest_index)
{
  int src_i, dest_i;
  int j;
  const void *source_buf[SOURCE_BUF_SIZE];
  const void **sources = source_buf;

  /* slow fallback in case we're interpolating a ridiculous number of
   * elements
   */
  if (count > SOURCE_BUF_SIZE) {
    sources = MEM_malloc_arrayN(count, sizeof(*sources), __func__);
  }

  /* interpolates a layer at a time */
  dest_i = 0;
  for (src_i = 0; src_i < source->totlayer; ++src_i) {
    const LayerTypeInfo *typeInfo = layerType_getInfo(source->layers[src_i].type);
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

      for (j = 0; j < count; ++j) {
        sources[j] = POINTER_OFFSET(src_data, (size_t)src_indices[j] * typeInfo->size);
      }

      typeInfo->interp(
          sources,
          weights,
          sub_weights,
          count,
          POINTER_OFFSET(dest->layers[dest_i].data, (size_t)dest_index * typeInfo->size));

      /* if there are multiple source & dest layers of the same type,
       * we don't want to copy all source layers to the same dest, so
       * increment dest_i
       */
      dest_i++;
    }
  }

  if (count > SOURCE_BUF_SIZE) {
    MEM_freeN((void *)sources);
  }
}

/**
 * Swap data inside each item, for all layers.
 * This only applies to item types that may store several sub-item data
 * (e.g. corner data [UVs, VCol, ...] of tessellated faces).
 *
 * \param corner_indices: A mapping 'new_index -> old_index' of sub-item data.
 */
void CustomData_swap_corners(struct CustomData *data, int index, const int *corner_indices)
{
  const LayerTypeInfo *typeInfo;
  int i;

  for (i = 0; i < data->totlayer; ++i) {
    typeInfo = layerType_getInfo(data->layers[i].type);

    if (typeInfo->swap) {
      const size_t offset = (size_t)index * typeInfo->size;

      typeInfo->swap(POINTER_OFFSET(data->layers[i].data, offset), corner_indices);
    }
  }
}

/**
 * Swap two items of given custom data, in all available layers.
 */
void CustomData_swap(struct CustomData *data, const int index_a, const int index_b)
{
  int i;
  char buff_static[256];

  if (index_a == index_b) {
    return;
  }

  for (i = 0; i < data->totlayer; ++i) {
    const LayerTypeInfo *typeInfo = layerType_getInfo(data->layers[i].type);
    const size_t size = typeInfo->size;
    const size_t offset_a = size * index_a;
    const size_t offset_b = size * index_b;

    void *buff = size <= sizeof(buff_static) ? buff_static : MEM_mallocN(size, __func__);
    memcpy(buff, POINTER_OFFSET(data->layers[i].data, offset_a), size);
    memcpy(POINTER_OFFSET(data->layers[i].data, offset_a),
           POINTER_OFFSET(data->layers[i].data, offset_b),
           size);
    memcpy(POINTER_OFFSET(data->layers[i].data, offset_b), buff, size);

    if (buff != buff_static) {
      MEM_freeN(buff);
    }
  }
}

void *CustomData_get(const CustomData *data, int index, int type)
{
  int layer_index;

  BLI_assert(index >= 0);

  /* get the layer index of the active layer of type */
  layer_index = CustomData_get_active_layer_index(data, type);
  if (layer_index == -1) {
    return NULL;
  }

  /* get the offset of the desired element */
  const size_t offset = (size_t)index * layerType_getInfo(type)->size;

  return POINTER_OFFSET(data->layers[layer_index].data, offset);
}

void *CustomData_get_n(const CustomData *data, int type, int index, int n)
{
  int layer_index;

  BLI_assert(index >= 0 && n >= 0);

  /* get the layer index of the first layer of type */
  layer_index = data->typemap[type];
  if (layer_index == -1) {
    return NULL;
  }

  const size_t offset = (size_t)index * layerType_getInfo(type)->size;
  return POINTER_OFFSET(data->layers[layer_index + n].data, offset);
}

void *CustomData_get_layer(const CustomData *data, int type)
{
  /* get the layer index of the active layer of type */
  int layer_index = CustomData_get_active_layer_index(data, type);
  if (layer_index == -1) {
    return NULL;
  }

  return data->layers[layer_index].data;
}

void *CustomData_get_layer_n(const CustomData *data, int type, int n)
{
  /* get the layer index of the active layer of type */
  int layer_index = CustomData_get_layer_index_n(data, type, n);
  if (layer_index == -1) {
    return NULL;
  }

  return data->layers[layer_index].data;
}

void *CustomData_get_layer_named(const struct CustomData *data, int type, const char *name)
{
  int layer_index = CustomData_get_named_layer_index(data, type, name);
  if (layer_index == -1) {
    return NULL;
  }

  return data->layers[layer_index].data;
}

int CustomData_get_offset(const CustomData *data, int type)
{
  /* get the layer index of the active layer of type */
  int layer_index = CustomData_get_active_layer_index(data, type);
  if (layer_index == -1) {
    return -1;
  }

  return data->layers[layer_index].offset;
}

int CustomData_get_n_offset(const CustomData *data, int type, int n)
{
  /* get the layer index of the active layer of type */
  int layer_index = CustomData_get_layer_index_n(data, type, n);
  if (layer_index == -1) {
    return -1;
  }

  return data->layers[layer_index].offset;
}

bool CustomData_set_layer_name(const CustomData *data, int type, int n, const char *name)
{
  /* get the layer index of the first layer of type */
  const int layer_index = CustomData_get_layer_index_n(data, type, n);

  if ((layer_index == -1) || !name) {
    return false;
  }

  BLI_strncpy(data->layers[layer_index].name, name, sizeof(data->layers[layer_index].name));

  return true;
}

const char *CustomData_get_layer_name(const CustomData *data, int type, int n)
{
  const int layer_index = CustomData_get_layer_index_n(data, type, n);

  return (layer_index == -1) ? NULL : data->layers[layer_index].name;
}

void *CustomData_set_layer(const CustomData *data, int type, void *ptr)
{
  /* get the layer index of the first layer of type */
  int layer_index = CustomData_get_active_layer_index(data, type);

  if (layer_index == -1) {
    return NULL;
  }

  data->layers[layer_index].data = ptr;

  return ptr;
}

void *CustomData_set_layer_n(const struct CustomData *data, int type, int n, void *ptr)
{
  /* get the layer index of the first layer of type */
  int layer_index = CustomData_get_layer_index_n(data, type, n);
  if (layer_index == -1) {
    return NULL;
  }

  data->layers[layer_index].data = ptr;

  return ptr;
}

void CustomData_set(const CustomData *data, int index, int type, const void *source)
{
  void *dest = CustomData_get(data, index, type);
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

/* BMesh functions */
/* needed to convert to/from different face reps */
void CustomData_to_bmeshpoly(CustomData *fdata, CustomData *ldata, int totloop)
{
  for (int i = 0; i < fdata->totlayer; i++) {
    if (fdata->layers[i].type == CD_MTFACE) {
      CustomData_add_layer_named(
          ldata, CD_MLOOPUV, CD_CALLOC, NULL, totloop, fdata->layers[i].name);
    }
    else if (fdata->layers[i].type == CD_MCOL) {
      CustomData_add_layer_named(
          ldata, CD_MLOOPCOL, CD_CALLOC, NULL, totloop, fdata->layers[i].name);
    }
    else if (fdata->layers[i].type == CD_MDISPS) {
      CustomData_add_layer_named(
          ldata, CD_MDISPS, CD_CALLOC, NULL, totloop, fdata->layers[i].name);
    }
    else if (fdata->layers[i].type == CD_TESSLOOPNORMAL) {
      CustomData_add_layer_named(
          ldata, CD_NORMAL, CD_CALLOC, NULL, totloop, fdata->layers[i].name);
    }
  }
}

void CustomData_from_bmeshpoly(CustomData *fdata, CustomData *ldata, int total)
{
  int i;

  /* avoid accumulating extra layers */
  BLI_assert(!CustomData_from_bmeshpoly_test(fdata, ldata, false));

  for (i = 0; i < ldata->totlayer; i++) {
    if (ldata->layers[i].type == CD_MLOOPUV) {
      CustomData_add_layer_named(fdata, CD_MTFACE, CD_CALLOC, NULL, total, ldata->layers[i].name);
    }
    if (ldata->layers[i].type == CD_MLOOPCOL) {
      CustomData_add_layer_named(fdata, CD_MCOL, CD_CALLOC, NULL, total, ldata->layers[i].name);
    }
    else if (ldata->layers[i].type == CD_PREVIEW_MLOOPCOL) {
      CustomData_add_layer_named(
          fdata, CD_PREVIEW_MCOL, CD_CALLOC, NULL, total, ldata->layers[i].name);
    }
    else if (ldata->layers[i].type == CD_ORIGSPACE_MLOOP) {
      CustomData_add_layer_named(
          fdata, CD_ORIGSPACE, CD_CALLOC, NULL, total, ldata->layers[i].name);
    }
    else if (ldata->layers[i].type == CD_NORMAL) {
      CustomData_add_layer_named(
          fdata, CD_TESSLOOPNORMAL, CD_CALLOC, NULL, total, ldata->layers[i].name);
    }
    else if (ldata->layers[i].type == CD_TANGENT) {
      CustomData_add_layer_named(fdata, CD_TANGENT, CD_CALLOC, NULL, total, ldata->layers[i].name);
    }
  }

  CustomData_bmesh_update_active_layers(fdata, ldata);
}

#ifndef NDEBUG
/**
 * Debug check, used to assert when we expect layers to be in/out of sync.
 *
 * \param fallback: Use when there are no layers to handle,
 * since callers may expect success or failure.
 */
bool CustomData_from_bmeshpoly_test(CustomData *fdata, CustomData *ldata, bool fallback)
{
  int a_num = 0, b_num = 0;
#  define LAYER_CMP(l_a, t_a, l_b, t_b) \
    ((a_num += CustomData_number_of_layers(l_a, t_a)) == \
     (b_num += CustomData_number_of_layers(l_b, t_b)))

  if (!LAYER_CMP(ldata, CD_MLOOPUV, fdata, CD_MTFACE)) {
    return false;
  }
  if (!LAYER_CMP(ldata, CD_MLOOPCOL, fdata, CD_MCOL)) {
    return false;
  }
  if (!LAYER_CMP(ldata, CD_PREVIEW_MLOOPCOL, fdata, CD_PREVIEW_MCOL)) {
    return false;
  }
  if (!LAYER_CMP(ldata, CD_ORIGSPACE_MLOOP, fdata, CD_ORIGSPACE)) {
    return false;
  }
  if (!LAYER_CMP(ldata, CD_NORMAL, fdata, CD_TESSLOOPNORMAL)) {
    return false;
  }
  if (!LAYER_CMP(ldata, CD_TANGENT, fdata, CD_TANGENT)) {
    return false;
  }

#  undef LAYER_CMP

  /* if no layers are on either CustomData's,
   * then there was nothing to do... */
  return a_num ? true : fallback;
}
#endif

void CustomData_bmesh_update_active_layers(CustomData *fdata, CustomData *ldata)
{
  int act;

  if (CustomData_has_layer(ldata, CD_MLOOPUV)) {
    act = CustomData_get_active_layer(ldata, CD_MLOOPUV);
    CustomData_set_layer_active(fdata, CD_MTFACE, act);

    act = CustomData_get_render_layer(ldata, CD_MLOOPUV);
    CustomData_set_layer_render(fdata, CD_MTFACE, act);

    act = CustomData_get_clone_layer(ldata, CD_MLOOPUV);
    CustomData_set_layer_clone(fdata, CD_MTFACE, act);

    act = CustomData_get_stencil_layer(ldata, CD_MLOOPUV);
    CustomData_set_layer_stencil(fdata, CD_MTFACE, act);
  }

  if (CustomData_has_layer(ldata, CD_MLOOPCOL)) {
    act = CustomData_get_active_layer(ldata, CD_MLOOPCOL);
    CustomData_set_layer_active(fdata, CD_MCOL, act);

    act = CustomData_get_render_layer(ldata, CD_MLOOPCOL);
    CustomData_set_layer_render(fdata, CD_MCOL, act);

    act = CustomData_get_clone_layer(ldata, CD_MLOOPCOL);
    CustomData_set_layer_clone(fdata, CD_MCOL, act);

    act = CustomData_get_stencil_layer(ldata, CD_MLOOPCOL);
    CustomData_set_layer_stencil(fdata, CD_MCOL, act);
  }
}

/* update active indices for active/render/clone/stencil custom data layers
 * based on indices from fdata layers
 * used by do_versions in readfile.c when creating pdata and ldata for pre-bmesh
 * meshes and needed to preserve active/render/clone/stencil flags set in pre-bmesh files
 */
void CustomData_bmesh_do_versions_update_active_layers(CustomData *fdata, CustomData *ldata)
{
  int act;

  if (CustomData_has_layer(fdata, CD_MTFACE)) {
    act = CustomData_get_active_layer(fdata, CD_MTFACE);
    CustomData_set_layer_active(ldata, CD_MLOOPUV, act);

    act = CustomData_get_render_layer(fdata, CD_MTFACE);
    CustomData_set_layer_render(ldata, CD_MLOOPUV, act);

    act = CustomData_get_clone_layer(fdata, CD_MTFACE);
    CustomData_set_layer_clone(ldata, CD_MLOOPUV, act);

    act = CustomData_get_stencil_layer(fdata, CD_MTFACE);
    CustomData_set_layer_stencil(ldata, CD_MLOOPUV, act);
  }

  if (CustomData_has_layer(fdata, CD_MCOL)) {
    act = CustomData_get_active_layer(fdata, CD_MCOL);
    CustomData_set_layer_active(ldata, CD_MLOOPCOL, act);

    act = CustomData_get_render_layer(fdata, CD_MCOL);
    CustomData_set_layer_render(ldata, CD_MLOOPCOL, act);

    act = CustomData_get_clone_layer(fdata, CD_MCOL);
    CustomData_set_layer_clone(ldata, CD_MLOOPCOL, act);

    act = CustomData_get_stencil_layer(fdata, CD_MCOL);
    CustomData_set_layer_stencil(ldata, CD_MLOOPCOL, act);
  }
}

void CustomData_bmesh_init_pool(CustomData *data, int totelem, const char htype)
{
  int chunksize;

  /* Dispose old pools before calling here to avoid leaks */
  BLI_assert(data->pool == NULL);

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
      BLI_assert(0);
      chunksize = 512;
      break;
  }

  /* If there are no layers, no pool is needed just yet */
  if (data->totlayer) {
    data->pool = BLI_mempool_create(data->totsize, totelem, chunksize, BLI_MEMPOOL_NOP);
  }
}

bool CustomData_bmesh_merge(const CustomData *source,
                            CustomData *dest,
                            CustomDataMask mask,
                            eCDAllocType alloctype,
                            BMesh *bm,
                            const char htype)
{
  BMHeader *h;
  BMIter iter;
  CustomData destold;
  void *tmp;
  int iter_type;
  int totelem;

  if (CustomData_number_of_layers_typemask(source, mask) == 0) {
    return false;
  }

  /* copy old layer description so that old data can be copied into
   * the new allocation */
  destold = *dest;
  if (destold.layers) {
    destold.layers = MEM_dupallocN(destold.layers);
  }

  if (CustomData_merge(source, dest, mask, alloctype, 0) == false) {
    if (destold.layers) {
      MEM_freeN(destold.layers);
    }
    return false;
  }

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
      BLI_assert(!"invalid type given");
      iter_type = BM_VERTS_OF_MESH;
      totelem = bm->totvert;
      break;
  }

  dest->pool = NULL;
  CustomData_bmesh_init_pool(dest, totelem, htype);

  if (iter_type != BM_LOOPS_OF_FACE) {
    /*ensure all current elements follow new customdata layout*/
    BM_ITER_MESH (h, &iter, bm, iter_type) {
      tmp = NULL;
      CustomData_bmesh_copy_data(&destold, dest, h->data, &tmp);
      CustomData_bmesh_free_block(&destold, &h->data);
      h->data = tmp;
    }
  }
  else {
    BMFace *f;
    BMLoop *l;
    BMIter liter;

    /*ensure all current elements follow new customdata layout*/
    BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
      BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
        tmp = NULL;
        CustomData_bmesh_copy_data(&destold, dest, l->head.data, &tmp);
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
  const LayerTypeInfo *typeInfo;
  int i;

  if (*block == NULL) {
    return;
  }

  for (i = 0; i < data->totlayer; ++i) {
    if (!(data->layers[i].flag & CD_FLAG_NOFREE)) {
      typeInfo = layerType_getInfo(data->layers[i].type);

      if (typeInfo->free) {
        int offset = data->layers[i].offset;
        typeInfo->free(POINTER_OFFSET(*block, offset), 1, typeInfo->size);
      }
    }
  }

  if (data->totsize) {
    BLI_mempool_free(data->pool, *block);
  }

  *block = NULL;
}

/**
 * Same as #CustomData_bmesh_free_block but zero the memory rather then freeing.
 */
void CustomData_bmesh_free_block_data(CustomData *data, void *block)
{
  const LayerTypeInfo *typeInfo;
  int i;

  if (block == NULL) {
    return;
  }

  for (i = 0; i < data->totlayer; ++i) {
    if (!(data->layers[i].flag & CD_FLAG_NOFREE)) {
      typeInfo = layerType_getInfo(data->layers[i].type);

      if (typeInfo->free) {
        const size_t offset = data->layers[i].offset;
        typeInfo->free(POINTER_OFFSET(block, offset), 1, typeInfo->size);
      }
    }
  }

  if (data->totsize) {
    memset(block, 0, data->totsize);
  }
}

static void CustomData_bmesh_alloc_block(CustomData *data, void **block)
{

  if (*block) {
    CustomData_bmesh_free_block(data, block);
  }

  if (data->totsize > 0) {
    *block = BLI_mempool_alloc(data->pool);
  }
  else {
    *block = NULL;
  }
}

void CustomData_bmesh_copy_data(const CustomData *source,
                                CustomData *dest,
                                void *src_block,
                                void **dest_block)
{
  const LayerTypeInfo *typeInfo;
  int dest_i, src_i;

  if (*dest_block == NULL) {
    CustomData_bmesh_alloc_block(dest, dest_block);
    if (*dest_block) {
      memset(*dest_block, 0, dest->totsize);
    }
  }

  /* copies a layer at a time */
  dest_i = 0;
  for (src_i = 0; src_i < source->totlayer; ++src_i) {

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
    if (dest->layers[dest_i].type == source->layers[src_i].type &&
        STREQ(dest->layers[dest_i].name, source->layers[src_i].name)) {
      const void *src_data = POINTER_OFFSET(src_block, source->layers[src_i].offset);
      void *dest_data = POINTER_OFFSET(*dest_block, dest->layers[dest_i].offset);

      typeInfo = layerType_getInfo(source->layers[src_i].type);

      if (typeInfo->copy) {
        typeInfo->copy(src_data, dest_data, 1);
      }
      else {
        memcpy(dest_data, src_data, typeInfo->size);
      }

      /* if there are multiple source & dest layers of the same type,
       * we don't want to copy all source layers to the same dest, so
       * increment dest_i
       */
      dest_i++;
    }
  }
}

/* BMesh Custom Data Functions.
 * Should replace edit-mesh ones with these as well, due to more efficient memory alloc.
 */
void *CustomData_bmesh_get(const CustomData *data, void *block, int type)
{
  int layer_index;

  /* get the layer index of the first layer of type */
  layer_index = CustomData_get_active_layer_index(data, type);
  if (layer_index == -1) {
    return NULL;
  }

  return POINTER_OFFSET(block, data->layers[layer_index].offset);
}

void *CustomData_bmesh_get_n(const CustomData *data, void *block, int type, int n)
{
  int layer_index;

  /* get the layer index of the first layer of type */
  layer_index = CustomData_get_layer_index(data, type);
  if (layer_index == -1) {
    return NULL;
  }

  return POINTER_OFFSET(block, data->layers[layer_index + n].offset);
}

/*gets from the layer at physical index n, note: doesn't check type.*/
void *CustomData_bmesh_get_layer_n(const CustomData *data, void *block, int n)
{
  if (n < 0 || n >= data->totlayer) {
    return NULL;
  }

  return POINTER_OFFSET(block, data->layers[n].offset);
}

bool CustomData_layer_has_math(const struct CustomData *data, int layer_n)
{
  const LayerTypeInfo *typeInfo = layerType_getInfo(data->layers[layer_n].type);

  if (typeInfo->equal && typeInfo->add && typeInfo->multiply && typeInfo->initminmax &&
      typeInfo->dominmax) {
    return true;
  }

  return false;
}

bool CustomData_layer_has_interp(const struct CustomData *data, int layer_n)
{
  const LayerTypeInfo *typeInfo = layerType_getInfo(data->layers[layer_n].type);

  if (typeInfo->interp) {
    return true;
  }

  return false;
}

bool CustomData_has_math(const struct CustomData *data)
{
  int i;

  /* interpolates a layer at a time */
  for (i = 0; i < data->totlayer; ++i) {
    if (CustomData_layer_has_math(data, i)) {
      return true;
    }
  }

  return false;
}

/* a non bmesh version would have to check layer->data */
bool CustomData_bmesh_has_free(const struct CustomData *data)
{
  const LayerTypeInfo *typeInfo;
  int i;

  for (i = 0; i < data->totlayer; ++i) {
    if (!(data->layers[i].flag & CD_FLAG_NOFREE)) {
      typeInfo = layerType_getInfo(data->layers[i].type);
      if (typeInfo->free) {
        return true;
      }
    }
  }
  return false;
}

bool CustomData_has_interp(const struct CustomData *data)
{
  int i;

  /* interpolates a layer at a time */
  for (i = 0; i < data->totlayer; ++i) {
    if (CustomData_layer_has_interp(data, i)) {
      return true;
    }
  }

  return false;
}

bool CustomData_has_referenced(const struct CustomData *data)
{
  int i;
  for (i = 0; i < data->totlayer; ++i) {
    if (data->layers[i].flag & CD_FLAG_NOFREE) {
      return true;
    }
  }
  return false;
}

/* copies the "value" (e.g. mloopuv uv or mloopcol colors) from one block to
 * another, while not overwriting anything else (e.g. flags)*/
void CustomData_data_copy_value(int type, const void *source, void *dest)
{
  const LayerTypeInfo *typeInfo = layerType_getInfo(type);

  if (!dest) {
    return;
  }

  if (typeInfo->copyvalue) {
    typeInfo->copyvalue(source, dest, CDT_MIX_NOMIX, 0.0f);
  }
  else {
    memcpy(dest, source, typeInfo->size);
  }
}

/* Mixes the "value" (e.g. mloopuv uv or mloopcol colors) from one block into
 * another, while not overwriting anything else (e.g. flags)*/
void CustomData_data_mix_value(
    int type, const void *source, void *dest, const int mixmode, const float mixfactor)
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

bool CustomData_data_equals(int type, const void *data1, const void *data2)
{
  const LayerTypeInfo *typeInfo = layerType_getInfo(type);

  if (typeInfo->equal) {
    return typeInfo->equal(data1, data2);
  }
  else {
    return !memcmp(data1, data2, typeInfo->size);
  }
}

void CustomData_data_initminmax(int type, void *min, void *max)
{
  const LayerTypeInfo *typeInfo = layerType_getInfo(type);

  if (typeInfo->initminmax) {
    typeInfo->initminmax(min, max);
  }
}

void CustomData_data_dominmax(int type, const void *data, void *min, void *max)
{
  const LayerTypeInfo *typeInfo = layerType_getInfo(type);

  if (typeInfo->dominmax) {
    typeInfo->dominmax(data, min, max);
  }
}

void CustomData_data_multiply(int type, void *data, float fac)
{
  const LayerTypeInfo *typeInfo = layerType_getInfo(type);

  if (typeInfo->multiply) {
    typeInfo->multiply(data, fac);
  }
}

void CustomData_data_add(int type, void *data1, const void *data2)
{
  const LayerTypeInfo *typeInfo = layerType_getInfo(type);

  if (typeInfo->add) {
    typeInfo->add(data1, data2);
  }
}

void CustomData_bmesh_set(const CustomData *data, void *block, int type, const void *source)
{
  void *dest = CustomData_bmesh_get(data, block, type);
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

void CustomData_bmesh_set_n(CustomData *data, void *block, int type, int n, const void *source)
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

void CustomData_bmesh_set_layer_n(CustomData *data, void *block, int n, const void *source)
{
  void *dest = CustomData_bmesh_get_layer_n(data, block, n);
  const LayerTypeInfo *typeInfo = layerType_getInfo(data->layers[n].type);

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

/**
 * \note src_blocks_ofs & dst_block_ofs
 * must be pointers to the data, offset by layer->offset already.
 */
void CustomData_bmesh_interp_n(CustomData *data,
                               const void **src_blocks_ofs,
                               const float *weights,
                               const float *sub_weights,
                               int count,
                               void *dst_block_ofs,
                               int n)
{
  CustomDataLayer *layer = &data->layers[n];
  const LayerTypeInfo *typeInfo = layerType_getInfo(layer->type);

  typeInfo->interp(src_blocks_ofs, weights, sub_weights, count, dst_block_ofs);
}

void CustomData_bmesh_interp(CustomData *data,
                             const void **src_blocks,
                             const float *weights,
                             const float *sub_weights,
                             int count,
                             void *dst_block)
{
  int i, j;
  void *source_buf[SOURCE_BUF_SIZE];
  const void **sources = (const void **)source_buf;

  /* slow fallback in case we're interpolating a ridiculous number of
   * elements
   */
  if (count > SOURCE_BUF_SIZE) {
    sources = MEM_malloc_arrayN(count, sizeof(*sources), __func__);
  }

  /* interpolates a layer at a time */
  for (i = 0; i < data->totlayer; ++i) {
    CustomDataLayer *layer = &data->layers[i];
    const LayerTypeInfo *typeInfo = layerType_getInfo(layer->type);
    if (typeInfo->interp) {
      for (j = 0; j < count; ++j) {
        sources[j] = POINTER_OFFSET(src_blocks[j], layer->offset);
      }
      CustomData_bmesh_interp_n(
          data, sources, weights, sub_weights, count, POINTER_OFFSET(dst_block, layer->offset), i);
    }
  }

  if (count > SOURCE_BUF_SIZE) {
    MEM_freeN((void *)sources);
  }
}

static void CustomData_bmesh_set_default_n(CustomData *data, void **block, int n)
{
  const LayerTypeInfo *typeInfo;
  int offset = data->layers[n].offset;

  typeInfo = layerType_getInfo(data->layers[n].type);

  if (typeInfo->set_default) {
    typeInfo->set_default(POINTER_OFFSET(*block, offset), 1);
  }
  else {
    memset(POINTER_OFFSET(*block, offset), 0, typeInfo->size);
  }
}

void CustomData_bmesh_set_default(CustomData *data, void **block)
{
  int i;

  if (*block == NULL) {
    CustomData_bmesh_alloc_block(data, block);
  }

  for (i = 0; i < data->totlayer; ++i) {
    CustomData_bmesh_set_default_n(data, block, i);
  }
}

/**
 * \param use_default_init: initializes data which can't be copied,
 * typically you'll want to use this if the BM_xxx create function
 * is called with BM_CREATE_SKIP_CD flag
 */
void CustomData_to_bmesh_block(const CustomData *source,
                               CustomData *dest,
                               int src_index,
                               void **dest_block,
                               bool use_default_init)
{
  const LayerTypeInfo *typeInfo;
  int dest_i, src_i;

  if (*dest_block == NULL) {
    CustomData_bmesh_alloc_block(dest, dest_block);
  }

  /* copies a layer at a time */
  dest_i = 0;
  for (src_i = 0; src_i < source->totlayer; ++src_i) {

    /* find the first dest layer with type >= the source type
     * (this should work because layers are ordered by type)
     */
    while (dest_i < dest->totlayer && dest->layers[dest_i].type < source->layers[src_i].type) {
      if (use_default_init) {
        CustomData_bmesh_set_default_n(dest, dest_block, dest_i);
      }
      dest_i++;
    }

    /* if there are no more dest layers, we're done */
    if (dest_i >= dest->totlayer) {
      break;
    }

    /* if we found a matching layer, copy the data */
    if (dest->layers[dest_i].type == source->layers[src_i].type) {
      int offset = dest->layers[dest_i].offset;
      const void *src_data = source->layers[src_i].data;
      void *dest_data = POINTER_OFFSET(*dest_block, offset);

      typeInfo = layerType_getInfo(dest->layers[dest_i].type);
      const size_t src_offset = (size_t)src_index * typeInfo->size;

      if (typeInfo->copy) {
        typeInfo->copy(POINTER_OFFSET(src_data, src_offset), dest_data, 1);
      }
      else {
        memcpy(dest_data, POINTER_OFFSET(src_data, src_offset), typeInfo->size);
      }

      /* if there are multiple source & dest layers of the same type,
       * we don't want to copy all source layers to the same dest, so
       * increment dest_i
       */
      dest_i++;
    }
  }

  if (use_default_init) {
    while (dest_i < dest->totlayer) {
      CustomData_bmesh_set_default_n(dest, dest_block, dest_i);
      dest_i++;
    }
  }
}

void CustomData_from_bmesh_block(const CustomData *source,
                                 CustomData *dest,
                                 void *src_block,
                                 int dst_index)
{
  int dest_i, src_i;

  /* copies a layer at a time */
  dest_i = 0;
  for (src_i = 0; src_i < source->totlayer; ++src_i) {

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
      const LayerTypeInfo *typeInfo = layerType_getInfo(dest->layers[dest_i].type);
      int offset = source->layers[src_i].offset;
      const void *src_data = POINTER_OFFSET(src_block, offset);
      void *dst_data = POINTER_OFFSET(dest->layers[dest_i].data,
                                      (size_t)dst_index * typeInfo->size);

      if (typeInfo->copy) {
        typeInfo->copy(src_data, dst_data, 1);
      }
      else {
        memcpy(dst_data, src_data, typeInfo->size);
      }

      /* if there are multiple source & dest layers of the same type,
       * we don't want to copy all source layers to the same dest, so
       * increment dest_i
       */
      dest_i++;
    }
  }
}

void CustomData_file_write_info(int type, const char **r_struct_name, int *r_struct_num)
{
  const LayerTypeInfo *typeInfo = layerType_getInfo(type);

  *r_struct_name = typeInfo->structname;
  *r_struct_num = typeInfo->structnum;
}

/**
 * Prepare given custom data for file writing.
 *
 * \param data: the customdata to tweak for .blend file writing (modified in place).
 * \param r_write_layers: contains a reduced set of layers to be written to file,
 * use it with writestruct_at_address()
 * (caller must free it if != \a write_layers_buff).
 *
 * \param write_layers_buff: an optional buffer for r_write_layers (to avoid allocating it).
 * \param write_layers_size: the size of pre-allocated \a write_layer_buff.
 *
 * \warning After this func has ran, given custom data is no more valid from Blender PoV
 * (its totlayer is invalid). This func shall always be called with localized data
 * (as it is in write_meshes()).
 *
 * \note data->typemap is not updated here, since it is always rebuilt on file read anyway.
 * This means written typemap does not match written layers (as returned by \a r_write_layers).
 * Trivial to fix is ever needed.
 */
void CustomData_file_write_prepare(CustomData *data,
                                   CustomDataLayer **r_write_layers,
                                   CustomDataLayer *write_layers_buff,
                                   size_t write_layers_size)
{
  CustomDataLayer *write_layers = write_layers_buff;
  const size_t chunk_size = (write_layers_size > 0) ? write_layers_size : CD_TEMP_CHUNK_SIZE;

  const int totlayer = data->totlayer;
  int i, j;

  for (i = 0, j = 0; i < totlayer; i++) {
    CustomDataLayer *layer = &data->layers[i];
    if (layer->flag & CD_FLAG_NOCOPY) { /* Layers with this flag set are not written to file. */
      data->totlayer--;
      /* CLOG_WARN(&LOG, "skipping layer %p (%s)", layer, layer->name); */
    }
    else {
      if (UNLIKELY((size_t)j >= write_layers_size)) {
        if (write_layers == write_layers_buff) {
          write_layers = MEM_malloc_arrayN(
              (write_layers_size + chunk_size), sizeof(*write_layers), __func__);
          if (write_layers_buff) {
            memcpy(write_layers, write_layers_buff, sizeof(*write_layers) * write_layers_size);
          }
        }
        else {
          write_layers = MEM_reallocN(write_layers,
                                      sizeof(*write_layers) * (write_layers_size + chunk_size));
        }
        write_layers_size += chunk_size;
      }
      write_layers[j++] = *layer;
    }
  }
  BLI_assert(j == data->totlayer);
  data->maxlayer = data->totlayer; /* We only write that much of data! */
  *r_write_layers = write_layers;
}

int CustomData_sizeof(int type)
{
  const LayerTypeInfo *typeInfo = layerType_getInfo(type);

  return typeInfo->size;
}

const char *CustomData_layertype_name(int type)
{
  return layerType_getName(type);
}

/**
 * Can only ever be one of these.
 */
bool CustomData_layertype_is_singleton(int type)
{
  const LayerTypeInfo *typeInfo = layerType_getInfo(type);
  return typeInfo->defaultname == NULL;
}

/**
 * \return Maximum number of layers of given \a type, -1 means 'no limit'.
 */
int CustomData_layertype_layers_max(const int type)
{
  const LayerTypeInfo *typeInfo = layerType_getInfo(type);

  /* Same test as for singleton above. */
  if (typeInfo->defaultname == NULL) {
    return 1;
  }
  else if (typeInfo->layers_max == NULL) {
    return -1;
  }

  return typeInfo->layers_max();
}

static bool CustomData_is_property_layer(int type)
{
  if ((type == CD_PROP_FLT) || (type == CD_PROP_INT) || (type == CD_PROP_STR)) {
    return true;
  }
  return false;
}

static bool cd_layer_find_dupe(CustomData *data, const char *name, int type, int index)
{
  int i;
  /* see if there is a duplicate */
  for (i = 0; i < data->totlayer; i++) {
    if (i != index) {
      CustomDataLayer *layer = &data->layers[i];

      if (CustomData_is_property_layer(type)) {
        if (CustomData_is_property_layer(layer->type) && STREQ(layer->name, name)) {
          return true;
        }
      }
      else {
        if (i != index && layer->type == type && STREQ(layer->name, name)) {
          return true;
        }
      }
    }
  }

  return false;
}

static bool customdata_unique_check(void *arg, const char *name)
{
  struct {
    CustomData *data;
    int type;
    int index;
  } *data_arg = arg;
  return cd_layer_find_dupe(data_arg->data, name, data_arg->type, data_arg->index);
}

void CustomData_set_layer_unique_name(CustomData *data, int index)
{
  CustomDataLayer *nlayer = &data->layers[index];
  const LayerTypeInfo *typeInfo = layerType_getInfo(nlayer->type);

  struct {
    CustomData *data;
    int type;
    int index;
  } data_arg;
  data_arg.data = data;
  data_arg.type = nlayer->type;
  data_arg.index = index;

  if (!typeInfo->defaultname) {
    return;
  }

  /* Set default name if none specified. Note we only call DATA_() when
   * needed to avoid overhead of locale lookups in the depsgraph. */
  if (nlayer->name[0] == '\0') {
    STRNCPY(nlayer->name, DATA_(typeInfo->defaultname));
  }

  BLI_uniquename_cb(
      customdata_unique_check, &data_arg, NULL, '.', nlayer->name, sizeof(nlayer->name));
}

void CustomData_validate_layer_name(const CustomData *data,
                                    int type,
                                    const char *name,
                                    char *outname)
{
  int index = -1;

  /* if a layer name was given, try to find that layer */
  if (name[0]) {
    index = CustomData_get_named_layer_index(data, type, name);
  }

  if (index == -1) {
    /* either no layer was specified, or the layer we want has been
     * deleted, so assign the active layer to name
     */
    index = CustomData_get_active_layer_index(data, type);
    BLI_strncpy(outname, data->layers[index].name, MAX_CUSTOMDATA_LAYER_NAME);
  }
  else {
    BLI_strncpy(outname, name, MAX_CUSTOMDATA_LAYER_NAME);
  }
}

bool CustomData_verify_versions(struct CustomData *data, int index)
{
  const LayerTypeInfo *typeInfo;
  CustomDataLayer *layer = &data->layers[index];
  bool keeplayer = true;
  int i;

  if (layer->type >= CD_NUMTYPES) {
    keeplayer = false; /* unknown layer type from future version */
  }
  else {
    typeInfo = layerType_getInfo(layer->type);

    if (!typeInfo->defaultname && (index > 0) && data->layers[index - 1].type == layer->type) {
      keeplayer = false; /* multiple layers of which we only support one */
    }
    /* This is a pre-emptive fix for cases that should not happen
     * (layers that should not be written in .blend files),
     * but can happen due to bugs (see e.g. T62318).
     * Also for forward compatibility, in future,
     * we may put into `.blend` file some currently un-written data types,
     * this should cover that case as well.
     * Better to be safe here, and fix issue on the fly rather than crash... */
    /* 0 structnum is used in writing code to tag layer types that should not be written. */
    else if (typeInfo->structnum == 0 &&
             /* XXX Not sure why those two are exception, maybe that should be fixed? */
             !ELEM(layer->type, CD_PAINT_MASK, CD_FACEMAP)) {
      keeplayer = false;
      CLOG_WARN(&LOG, ".blend file read: removing a data layer that should not have been written");
    }
  }

  if (!keeplayer) {
    for (i = index + 1; i < data->totlayer; ++i) {
      data->layers[i - 1] = data->layers[i];
    }
    data->totlayer--;
  }

  return keeplayer;
}

/**
 * Validate and fix data of \a layer,
 * if possible (needs relevant callback in layer's type to be defined).
 *
 * \return True if some errors were found.
 */
bool CustomData_layer_validate(CustomDataLayer *layer, const uint totitems, const bool do_fixes)
{
  const LayerTypeInfo *typeInfo = layerType_getInfo(layer->type);

  if (typeInfo->validate != NULL) {
    return typeInfo->validate(layer->data, totitems, do_fixes);
  }

  return false;
}

/****************************** External Files *******************************/

static void customdata_external_filename(char filename[FILE_MAX],
                                         ID *id,
                                         CustomDataExternal *external)
{
  BLI_strncpy(filename, external->filename, FILE_MAX);
  BLI_path_abs(filename, ID_BLEND_PATH_FROM_GLOBAL(id));
}

void CustomData_external_reload(CustomData *data, ID *UNUSED(id), CustomDataMask mask, int totelem)
{
  CustomDataLayer *layer;
  const LayerTypeInfo *typeInfo;
  int i;

  for (i = 0; i < data->totlayer; i++) {
    layer = &data->layers[i];
    typeInfo = layerType_getInfo(layer->type);

    if (!(mask & CD_TYPE_AS_MASK(layer->type))) {
      /* pass */
    }
    else if ((layer->flag & CD_FLAG_EXTERNAL) && (layer->flag & CD_FLAG_IN_MEMORY)) {
      if (typeInfo->free) {
        typeInfo->free(layer->data, totelem, typeInfo->size);
      }
      layer->flag &= ~CD_FLAG_IN_MEMORY;
    }
  }
}

void CustomData_external_read(CustomData *data, ID *id, CustomDataMask mask, int totelem)
{
  CustomDataExternal *external = data->external;
  CustomDataLayer *layer;
  CDataFile *cdf;
  CDataFileLayer *blay;
  char filename[FILE_MAX];
  const LayerTypeInfo *typeInfo;
  int i, update = 0;

  if (!external) {
    return;
  }

  for (i = 0; i < data->totlayer; i++) {
    layer = &data->layers[i];
    typeInfo = layerType_getInfo(layer->type);

    if (!(mask & CD_TYPE_AS_MASK(layer->type))) {
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

  customdata_external_filename(filename, id, external);

  cdf = cdf_create(CDF_TYPE_MESH);
  if (!cdf_read_open(cdf, filename)) {
    cdf_free(cdf);
    CLOG_ERROR(&LOG, "Failed to read %s layer from %s.", layerType_getName(layer->type), filename);
    return;
  }

  for (i = 0; i < data->totlayer; i++) {
    layer = &data->layers[i];
    typeInfo = layerType_getInfo(layer->type);

    if (!(mask & CD_TYPE_AS_MASK(layer->type))) {
      /* pass */
    }
    else if (layer->flag & CD_FLAG_IN_MEMORY) {
      /* pass */
    }
    else if ((layer->flag & CD_FLAG_EXTERNAL) && typeInfo->read) {
      blay = cdf_layer_find(cdf, layer->type, layer->name);

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
    CustomData *data, ID *id, CustomDataMask mask, int totelem, int free)
{
  CustomDataExternal *external = data->external;
  CustomDataLayer *layer;
  CDataFile *cdf;
  CDataFileLayer *blay;
  const LayerTypeInfo *typeInfo;
  int i, update = 0;
  char filename[FILE_MAX];

  if (!external) {
    return;
  }

  /* test if there is anything to write */
  for (i = 0; i < data->totlayer; i++) {
    layer = &data->layers[i];
    typeInfo = layerType_getInfo(layer->type);

    if (!(mask & CD_TYPE_AS_MASK(layer->type))) {
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
  customdata_external_filename(filename, id, external);

  cdf = cdf_create(CDF_TYPE_MESH);

  for (i = 0; i < data->totlayer; i++) {
    layer = &data->layers[i];
    typeInfo = layerType_getInfo(layer->type);

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

  if (!cdf_write_open(cdf, filename)) {
    CLOG_ERROR(&LOG, "Failed to open %s for writing.", filename);
    cdf_free(cdf);
    return;
  }

  for (i = 0; i < data->totlayer; i++) {
    layer = &data->layers[i];
    typeInfo = layerType_getInfo(layer->type);

    if ((layer->flag & CD_FLAG_EXTERNAL) && typeInfo->write) {
      blay = cdf_layer_find(cdf, layer->type, layer->name);

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
    CLOG_ERROR(&LOG, "Failed to write data to %s.", filename);
    cdf_write_close(cdf);
    cdf_free(cdf);
    return;
  }

  for (i = 0; i < data->totlayer; i++) {
    layer = &data->layers[i];
    typeInfo = layerType_getInfo(layer->type);

    if ((layer->flag & CD_FLAG_EXTERNAL) && typeInfo->write) {
      if (free) {
        if (typeInfo->free) {
          typeInfo->free(layer->data, totelem, typeInfo->size);
        }
        layer->flag &= ~CD_FLAG_IN_MEMORY;
      }
    }
  }

  cdf_write_close(cdf);
  cdf_free(cdf);
}

void CustomData_external_add(
    CustomData *data, ID *UNUSED(id), int type, int UNUSED(totelem), const char *filename)
{
  CustomDataExternal *external = data->external;
  CustomDataLayer *layer;
  int layer_index;

  layer_index = CustomData_get_active_layer_index(data, type);
  if (layer_index == -1) {
    return;
  }

  layer = &data->layers[layer_index];

  if (layer->flag & CD_FLAG_EXTERNAL) {
    return;
  }

  if (!external) {
    external = MEM_callocN(sizeof(CustomDataExternal), "CustomDataExternal");
    data->external = external;
  }
  BLI_strncpy(external->filename, filename, sizeof(external->filename));

  layer->flag |= CD_FLAG_EXTERNAL | CD_FLAG_IN_MEMORY;
}

void CustomData_external_remove(CustomData *data, ID *id, int type, int totelem)
{
  CustomDataExternal *external = data->external;
  CustomDataLayer *layer;
  // char filename[FILE_MAX];
  int layer_index;  // i, remove_file;

  layer_index = CustomData_get_active_layer_index(data, type);
  if (layer_index == -1) {
    return;
  }

  layer = &data->layers[layer_index];

  if (!external) {
    return;
  }

  if (layer->flag & CD_FLAG_EXTERNAL) {
    if (!(layer->flag & CD_FLAG_IN_MEMORY)) {
      CustomData_external_read(data, id, CD_TYPE_AS_MASK(layer->type), totelem);
    }

    layer->flag &= ~CD_FLAG_EXTERNAL;
  }
}

bool CustomData_external_test(CustomData *data, int type)
{
  CustomDataLayer *layer;
  int layer_index;

  layer_index = CustomData_get_active_layer_index(data, type);
  if (layer_index == -1) {
    return false;
  }

  layer = &data->layers[layer_index];
  return (layer->flag & CD_FLAG_EXTERNAL) != 0;
}

/* ********** Mesh-to-mesh data transfer ********** */
static void copy_bit_flag(void *dst, const void *src, const size_t data_size, const uint64_t flag)
{
#define COPY_BIT_FLAG(_type, _dst, _src, _f) \
  { \
    const _type _val = *((_type *)(_src)) & ((_type)(_f)); \
    *((_type *)(_dst)) &= ~((_type)(_f)); \
    *((_type *)(_dst)) |= _val; \
  } \
  (void)0

  switch (data_size) {
    case 1:
      COPY_BIT_FLAG(uint8_t, dst, src, flag);
      break;
    case 2:
      COPY_BIT_FLAG(uint16_t, dst, src, flag);
      break;
    case 4:
      COPY_BIT_FLAG(uint32_t, dst, src, flag);
      break;
    case 8:
      COPY_BIT_FLAG(uint64_t, dst, src, flag);
      break;
    default:
      // CLOG_ERROR(&LOG, "Unknown flags-container size (%zu)", datasize);
      break;
  }

#undef COPY_BIT_FLAG
}

static bool check_bit_flag(const void *data, const size_t data_size, const uint64_t flag)
{
  switch (data_size) {
    case 1:
      return ((*((uint8_t *)data) & ((uint8_t)flag)) != 0);
    case 2:
      return ((*((uint16_t *)data) & ((uint16_t)flag)) != 0);
    case 4:
      return ((*((uint32_t *)data) & ((uint32_t)flag)) != 0);
    case 8:
      return ((*((uint64_t *)data) & ((uint64_t)flag)) != 0);
    default:
      // CLOG_ERROR(&LOG, "Unknown flags-container size (%zu)", datasize);
      return false;
  }
}

static void customdata_data_transfer_interp_generic(const CustomDataTransferLayerMap *laymap,
                                                    void *data_dst,
                                                    const void **sources,
                                                    const float *weights,
                                                    const int count,
                                                    const float mix_factor)
{
  /* Fake interpolation, we actually copy highest weighted source to dest.
   * Note we also handle bitflags here,
   * in which case we rather choose to transfer value of elements totaling
   * more than 0.5 of weight. */
  int best_src_idx = 0;

  const int data_type = laymap->data_type;
  const int mix_mode = laymap->mix_mode;

  size_t data_size;
  const uint64_t data_flag = laymap->data_flag;

  cd_interp interp_cd = NULL;
  cd_copy copy_cd = NULL;

  void *tmp_dst;

  if (!sources) {
    /* Not supported here, abort. */
    return;
  }

  if (data_type & CD_FAKE) {
    data_size = laymap->data_size;
  }
  else {
    const LayerTypeInfo *type_info = layerType_getInfo(data_type);

    data_size = (size_t)type_info->size;
    interp_cd = type_info->interp;
    copy_cd = type_info->copy;
  }

  tmp_dst = MEM_mallocN(data_size, __func__);

  if (count > 1 && !interp_cd) {
    int i;

    if (data_flag) {
      /* Boolean case, we can 'interpolate' in two groups,
       * and choose value from highest weighted group. */
      float tot_weight_true = 0.0f;
      int item_true_idx = -1, item_false_idx = -1;

      for (i = 0; i < count; i++) {
        if (check_bit_flag(sources[i], data_size, data_flag)) {
          tot_weight_true += weights[i];
          item_true_idx = i;
        }
        else {
          item_false_idx = i;
        }
      }
      best_src_idx = (tot_weight_true >= 0.5f) ? item_true_idx : item_false_idx;
    }
    else {
      /* We just choose highest weighted source. */
      float max_weight = 0.0f;

      for (i = 0; i < count; i++) {
        if (weights[i] > max_weight) {
          max_weight = weights[i];
          best_src_idx = i;
        }
      }
    }
  }

  BLI_assert(best_src_idx >= 0);

  if (interp_cd) {
    interp_cd(sources, weights, NULL, count, tmp_dst);
  }
  else if (data_flag) {
    copy_bit_flag(tmp_dst, sources[best_src_idx], data_size, data_flag);
  }
  /* No interpolation, just copy highest weight source element's data. */
  else if (copy_cd) {
    copy_cd(sources[best_src_idx], tmp_dst, 1);
  }
  else {
    memcpy(tmp_dst, sources[best_src_idx], data_size);
  }

  if (data_flag) {
    /* Bool flags, only copy if dest data is set (resp. unset) -
     * only 'advanced' modes we can support here! */
    if (mix_factor >= 0.5f && ((mix_mode == CDT_MIX_TRANSFER) ||
                               (mix_mode == CDT_MIX_REPLACE_ABOVE_THRESHOLD &&
                                check_bit_flag(data_dst, data_size, data_flag)) ||
                               (mix_mode == CDT_MIX_REPLACE_BELOW_THRESHOLD &&
                                !check_bit_flag(data_dst, data_size, data_flag)))) {
      copy_bit_flag(data_dst, tmp_dst, data_size, data_flag);
    }
  }
  else if (!(data_type & CD_FAKE)) {
    CustomData_data_mix_value(data_type, tmp_dst, data_dst, mix_mode, mix_factor);
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

/* Normals are special, we need to take care of source & destination spaces... */
void customdata_data_transfer_interp_normal_normals(const CustomDataTransferLayerMap *laymap,
                                                    void *data_dst,
                                                    const void **sources,
                                                    const float *weights,
                                                    const int count,
                                                    const float mix_factor)
{
  const int data_type = laymap->data_type;
  const int mix_mode = laymap->mix_mode;

  SpaceTransform *space_transform = laymap->interp_data;

  const LayerTypeInfo *type_info = layerType_getInfo(data_type);
  cd_interp interp_cd = type_info->interp;

  float tmp_dst[3];

  BLI_assert(data_type == CD_NORMAL);

  if (!sources) {
    /* Not supported here, abort. */
    return;
  }

  interp_cd(sources, weights, NULL, count, tmp_dst);
  if (space_transform) {
    /* tmp_dst is in source space so far, bring it back in destination space. */
    BLI_space_transform_invert_normal(space_transform, tmp_dst);
  }

  CustomData_data_mix_value(data_type, tmp_dst, data_dst, mix_mode, mix_factor);
}

void CustomData_data_transfer(const MeshPairRemap *me_remap,
                              const CustomDataTransferLayerMap *laymap)
{
  MeshPairRemapItem *mapit = me_remap->items;
  const int totelem = me_remap->items_num;
  int i;

  const int data_type = laymap->data_type;
  const void *data_src = laymap->data_src;
  void *data_dst = laymap->data_dst;

  size_t data_step;
  size_t data_size;
  size_t data_offset;

  cd_datatransfer_interp interp = NULL;

  size_t tmp_buff_size = 32;
  const void **tmp_data_src = NULL;

  /* Note: NULL data_src may happen and be valid (see vgroups...). */
  if (!data_dst) {
    return;
  }

  if (data_src) {
    tmp_data_src = MEM_malloc_arrayN(tmp_buff_size, sizeof(*tmp_data_src), __func__);
  }

  if (data_type & CD_FAKE) {
    data_step = laymap->elem_size;
    data_size = laymap->data_size;
    data_offset = laymap->data_offset;
  }
  else {
    const LayerTypeInfo *type_info = layerType_getInfo(data_type);

    /* Note: we can use 'fake' CDLayers, like e.g. for crease, bweight, etc. :/ */
    data_size = (size_t)type_info->size;
    data_step = laymap->elem_size ? laymap->elem_size : data_size;
    data_offset = laymap->data_offset;
  }

  interp = laymap->interp ? laymap->interp : customdata_data_transfer_interp_generic;

  for (i = 0; i < totelem; i++, data_dst = POINTER_OFFSET(data_dst, data_step), mapit++) {
    const int sources_num = mapit->sources_num;
    const float mix_factor = laymap->mix_weights ? laymap->mix_weights[i] : laymap->mix_factor;
    int j;

    if (!sources_num) {
      /* No sources for this element, skip it. */
      continue;
    }

    if (tmp_data_src) {
      if (UNLIKELY(sources_num > tmp_buff_size)) {
        tmp_buff_size = (size_t)sources_num;
        tmp_data_src = MEM_reallocN((void *)tmp_data_src, sizeof(*tmp_data_src) * tmp_buff_size);
      }

      for (j = 0; j < sources_num; j++) {
        const size_t src_idx = (size_t)mapit->indices_src[j];
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

  MEM_SAFE_FREE(tmp_data_src);
}
