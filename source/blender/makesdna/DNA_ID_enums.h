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
 */

/** \file
 * \ingroup DNA
 * \brief Enumerations for `DNA_ID.h`.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

enum eIconSizes {
  ICON_SIZE_ICON = 0,
  ICON_SIZE_PREVIEW = 1,

  NUM_ICON_SIZES,
};

/**
 * Defines for working with IDs.
 *
 * The tags represent types! This is a dirty way of enabling RTTI. The
 * sig_byte end endian defines aren't really used much.
 */

#ifdef __BIG_ENDIAN__
/* big endian */
#  define MAKE_ID2(c, d) ((c) << 8 | (d))
#else
/* little endian */
#  define MAKE_ID2(c, d) ((d) << 8 | (c))
#endif

/**
 * ID from database.
 *
 * Written to #BHead.code (for file IO)
 * and the first 2 bytes of #ID.name (for runtime checks, see #GS macro).
 */
typedef enum ID_Type {
  ID_SCE = MAKE_ID2('S', 'C'), /* Scene */
  ID_LI = MAKE_ID2('L', 'I'),  /* Library */
  ID_OB = MAKE_ID2('O', 'B'),  /* Object */
  ID_ME = MAKE_ID2('M', 'E'),  /* Mesh */
  ID_CU = MAKE_ID2('C', 'U'),  /* Curve */
  ID_MB = MAKE_ID2('M', 'B'),  /* MetaBall */
  ID_MA = MAKE_ID2('M', 'A'),  /* Material */
  ID_TE = MAKE_ID2('T', 'E'),  /* Tex (Texture) */
  ID_IM = MAKE_ID2('I', 'M'),  /* Image */
  ID_LT = MAKE_ID2('L', 'T'),  /* Lattice */
  ID_LA = MAKE_ID2('L', 'A'),  /* Light */
  ID_CA = MAKE_ID2('C', 'A'),  /* Camera */
  ID_IP = MAKE_ID2('I', 'P'),  /* Ipo (depreciated, replaced by FCurves) */
  ID_KE = MAKE_ID2('K', 'E'),  /* Key (shape key) */
  ID_WO = MAKE_ID2('W', 'O'),  /* World */
  ID_SCR = MAKE_ID2('S', 'R'), /* Screen */
  ID_VF = MAKE_ID2('V', 'F'),  /* VFont (Vector Font) */
  ID_TXT = MAKE_ID2('T', 'X'), /* Text */
  ID_SPK = MAKE_ID2('S', 'K'), /* Speaker */
  ID_SO = MAKE_ID2('S', 'O'),  /* Sound */
  ID_GR = MAKE_ID2('G', 'R'),  /* Collection */
  ID_AR = MAKE_ID2('A', 'R'),  /* bArmature */
  ID_AC = MAKE_ID2('A', 'C'),  /* bAction */
  ID_NT = MAKE_ID2('N', 'T'),  /* bNodeTree */
  ID_BR = MAKE_ID2('B', 'R'),  /* Brush */
  ID_PA = MAKE_ID2('P', 'A'),  /* ParticleSettings */
  ID_GD = MAKE_ID2('G', 'D'),  /* bGPdata, (Grease Pencil) */
  ID_WM = MAKE_ID2('W', 'M'),  /* WindowManager */
  ID_MC = MAKE_ID2('M', 'C'),  /* MovieClip */
  ID_MSK = MAKE_ID2('M', 'S'), /* Mask */
  ID_LS = MAKE_ID2('L', 'S'),  /* FreestyleLineStyle */
  ID_PAL = MAKE_ID2('P', 'L'), /* Palette */
  ID_PC = MAKE_ID2('P', 'C'),  /* PaintCurve */
  ID_CF = MAKE_ID2('C', 'F'),  /* CacheFile */
  ID_WS = MAKE_ID2('W', 'S'),  /* WorkSpace */
  ID_LP = MAKE_ID2('L', 'P'),  /* LightProbe */
  ID_HA = MAKE_ID2('H', 'A'),  /* Hair */
  ID_PT = MAKE_ID2('P', 'T'),  /* PointCloud */
  ID_VO = MAKE_ID2('V', 'O'),  /* Volume */
  ID_SIM = MAKE_ID2('S', 'I'), /* Simulation (geometry node groups) */
} ID_Type;

/* Only used as 'placeholder' in .blend files for directly linked data-blocks. */
#define ID_LINK_PLACEHOLDER MAKE_ID2('I', 'D') /* (internal use only) */

/* Deprecated. */
#define ID_SCRN MAKE_ID2('S', 'N')

/* NOTE: Fake IDs, needed for `g.sipo->blocktype` or outliner. */
#define ID_SEQ MAKE_ID2('S', 'Q')
/* constraint */
#define ID_CO MAKE_ID2('C', 'O')
/* pose (action channel, used to be ID_AC in code, so we keep code for backwards compatible). */
#define ID_PO MAKE_ID2('A', 'C')
/* used in outliner... */
#define ID_NLA MAKE_ID2('N', 'L')
/* fluidsim Ipo */
#define ID_FLUIDSIM MAKE_ID2('F', 'S')

#ifdef __cplusplus
}
#endif
