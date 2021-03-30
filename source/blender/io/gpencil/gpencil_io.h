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
 * The Original Code is Copyright (C) 2020 Blender Foundation
 * All rights reserved.
 */
#pragma once

/** \file
 * \ingroup bgpencil
 */

#ifdef __cplusplus
extern "C" {
#endif

struct ARegion;
struct bContext;
struct Object;
struct View3D;

typedef struct GpencilIOParams {
  bContext *C;
  ARegion *region;
  View3D *v3d;
  /** Grease pencil object. */
  Object *ob;
  /** Mode (see eGpencilIO_Modes). */
  uint16_t mode;
  int32_t frame_start;
  int32_t frame_end;
  int32_t frame_cur;
  uint32_t flag;
  float scale;
  /** Select mode (see eGpencilExportSelect). */
  uint16_t select_mode;
  /** Frame mode (see eGpencilExportFrame). */
  uint16_t frame_mode;
  /** Stroke sampling factor. */
  float stroke_sample;
  int32_t resolution;
} GpencilIOParams;

/* GpencilIOParams->flag. */
typedef enum eGpencilIOParams_Flag {
  /* Export Filled strokes. */
  GP_EXPORT_FILL = (1 << 0),
  /* Export normalized thickness. */
  GP_EXPORT_NORM_THICKNESS = (1 << 1),
  /* Clip camera area. */
  GP_EXPORT_CLIP_CAMERA = (1 << 2),
} eGpencilIOParams_Flag;

typedef enum eGpencilIO_Modes {
  GP_EXPORT_TO_SVG = 0,
  GP_EXPORT_TO_PDF = 1,

  GP_IMPORT_FROM_SVG = 2,
  /* Add new formats here. */
} eGpencilIO_Modes;

/* Object to be exported. */
typedef enum eGpencilExportSelect {
  GP_EXPORT_ACTIVE = 0,
  GP_EXPORT_SELECTED = 1,
  GP_EXPORT_VISIBLE = 2,
} eGpencilExportSelect;

/** Frame-range to be exported. */
typedef enum eGpencilExportFrame {
  GP_EXPORT_FRAME_ACTIVE = 0,
  GP_EXPORT_FRAME_SELECTED = 1,
} eGpencilExportFrame;

bool gpencil_io_export(const char *filename, struct GpencilIOParams *iparams);
bool gpencil_io_import(const char *filename, struct GpencilIOParams *iparams);

#ifdef __cplusplus
}
#endif
