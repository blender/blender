/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>

#include "BLI_utildefines.h"

#include "BLI_fileops.h"

#ifdef WIN32
#  include "BLI_winstuff.h"
#endif

#include "BLT_translation.hh"

#include "DNA_modifier_types.h"

#include "MOD_meshcache_util.hh" /* own include */

struct PC2Head {
  char header[12];  /* 'POINTCACHE2\0' */
  int file_version; /* unused - should be 1 */
  int verts_tot;
  float start;
  float sampling;
  int frame_tot;
}; /* frames, verts */

static bool meshcache_read_pc2_head(FILE *fp,
                                    const int verts_tot,
                                    PC2Head *pc2_head,
                                    const char **r_err_str)
{
  if (!fread(pc2_head, sizeof(*pc2_head), 1, fp)) {
    *r_err_str = RPT_("Missing header");
    return false;
  }

  if (!STREQ(pc2_head->header, "POINTCACHE2")) {
    *r_err_str = RPT_("Invalid header");
    return false;
  }

  /* NOTE: this is endianness-sensitive. */
  /* The pc2_head->file_version and following values would need to be switched on big-endian
   * systems. */

  if (pc2_head->verts_tot != verts_tot) {
    *r_err_str = RPT_("Vertex count mismatch");
    return false;
  }

  if (pc2_head->frame_tot <= 0) {
    *r_err_str = RPT_("Invalid frame total");
    return false;
  }
  /* Intentionally don't seek back. */

  return true;
}

/**
 * Gets the index range and factor
 *
 * currently same as for MDD
 */
static bool meshcache_read_pc2_range(FILE *fp,
                                     const int verts_tot,
                                     const float frame,
                                     const char interp,
                                     int r_index_range[2],
                                     float *r_factor,
                                     const char **r_err_str)
{
  PC2Head pc2_head;

  /* first check interpolation and get the vert locations */

  if (meshcache_read_pc2_head(fp, verts_tot, &pc2_head, r_err_str) == false) {
    return false;
  }

  MOD_meshcache_calc_range(frame, interp, pc2_head.frame_tot, r_index_range, r_factor);

  return true;
}

static bool meshcache_read_pc2_range_from_time(FILE *fp,
                                               const int verts_tot,
                                               const float time,
                                               const float fps,
                                               float *r_frame,
                                               const char **r_err_str)
{
  PC2Head pc2_head;
  float frame;

  if (meshcache_read_pc2_head(fp, verts_tot, &pc2_head, r_err_str) == false) {
    return false;
  }

  frame = ((time / fps) - pc2_head.start) / pc2_head.sampling;

  if (frame >= pc2_head.frame_tot) {
    frame = float(pc2_head.frame_tot - 1);
  }
  else if (frame < 0.0f) {
    frame = 0.0f;
  }

  *r_frame = frame;
  return true;
}

bool MOD_meshcache_read_pc2_index(FILE *fp,
                                  float (*vertexCos)[3],
                                  const int verts_tot,
                                  const int index,
                                  const float factor,
                                  const char **r_err_str)
{
  PC2Head pc2_head;

  if (meshcache_read_pc2_head(fp, verts_tot, &pc2_head, r_err_str) == false) {
    return false;
  }

  if (BLI_fseek(fp, sizeof(float[3]) * index * pc2_head.verts_tot, SEEK_CUR) != 0) {
    *r_err_str = RPT_("Failed to seek frame");
    return false;
  }

  size_t verts_read_num = 0;
  errno = 0;
  if (factor >= 1.0f) {
    float *vco = *vertexCos;
    uint i;
    for (i = pc2_head.verts_tot; i != 0; i--, vco += 3) {
      verts_read_num += fread(vco, sizeof(float[3]), 1, fp);

      /* NOTE: this is endianness-sensitive. */
      /* The `vco` values would need to be switched on big-endian systems. */
    }
  }
  else {
    const float ifactor = 1.0f - factor;
    float *vco = *vertexCos;
    uint i;
    for (i = pc2_head.verts_tot; i != 0; i--, vco += 3) {
      float tvec[3];
      verts_read_num += fread(tvec, sizeof(float[3]), 1, fp);

      /* NOTE: this is endianness-sensitive. */
      /* The `tvec` values would need to be switched on big-endian systems. */

      vco[0] = (vco[0] * ifactor) + (tvec[0] * factor);
      vco[1] = (vco[1] * ifactor) + (tvec[1] * factor);
      vco[2] = (vco[2] * ifactor) + (tvec[2] * factor);
    }
  }

  if (verts_read_num != pc2_head.verts_tot) {
    *r_err_str = errno ? strerror(errno) : RPT_("Vertex coordinate read failed");
    return false;
  }

  return true;
}

bool MOD_meshcache_read_pc2_frame(FILE *fp,
                                  float (*vertexCos)[3],
                                  const int verts_tot,
                                  const char interp,
                                  const float frame,
                                  const char **r_err_str)
{
  int index_range[2];
  float factor;

  if (meshcache_read_pc2_range(fp,
                               verts_tot,
                               frame,
                               interp,
                               index_range,
                               &factor, /* read into these values */
                               r_err_str) == false)
  {
    return false;
  }

  if (index_range[0] == index_range[1]) {
    /* read single */
    if ((BLI_fseek(fp, 0, SEEK_SET) == 0) &&
        MOD_meshcache_read_pc2_index(fp, vertexCos, verts_tot, index_range[0], 1.0f, r_err_str))
    {
      return true;
    }

    return false;
  }

  /* read both and interpolate */
  if ((BLI_fseek(fp, 0, SEEK_SET) == 0) &&
      MOD_meshcache_read_pc2_index(fp, vertexCos, verts_tot, index_range[0], 1.0f, r_err_str) &&
      (BLI_fseek(fp, 0, SEEK_SET) == 0) &&
      MOD_meshcache_read_pc2_index(fp, vertexCos, verts_tot, index_range[1], factor, r_err_str))
  {
    return true;
  }

  return false;
}

bool MOD_meshcache_read_pc2_times(const char *filepath,
                                  float (*vertexCos)[3],
                                  const int verts_tot,
                                  const char interp,
                                  const float time,
                                  const float fps,
                                  const char time_mode,
                                  const char **r_err_str)
{
  float frame;

  FILE *fp = BLI_fopen(filepath, "rb");
  bool ok;

  if (fp == nullptr) {
    *r_err_str = errno ? strerror(errno) : RPT_("Unknown error opening file");
    return false;
  }

  switch (time_mode) {
    case MOD_MESHCACHE_TIME_FRAME: {
      frame = time;
      break;
    }
    case MOD_MESHCACHE_TIME_SECONDS: {
      /* we need to find the closest time */
      if (meshcache_read_pc2_range_from_time(fp, verts_tot, time, fps, &frame, r_err_str) == false)
      {
        fclose(fp);
        return false;
      }
      rewind(fp);
      break;
    }
    case MOD_MESHCACHE_TIME_FACTOR:
    default: {
      PC2Head pc2_head;
      if (meshcache_read_pc2_head(fp, verts_tot, &pc2_head, r_err_str) == false) {
        fclose(fp);
        return false;
      }

      frame = std::clamp(time, 0.0f, 1.0f) * float(pc2_head.frame_tot);
      rewind(fp);
      break;
    }
  }

  ok = MOD_meshcache_read_pc2_frame(fp, vertexCos, verts_tot, interp, frame, r_err_str);

  fclose(fp);
  return ok;
}
