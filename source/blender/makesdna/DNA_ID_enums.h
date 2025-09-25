/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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

/** #IDProperty.type */
typedef enum eIDPropertyType {
  IDP_STRING = 0,
  IDP_INT = 1,
  IDP_FLOAT = 2,
  /** Array containing int, floats, doubles or groups. */
  IDP_ARRAY = 5,
  IDP_GROUP = 6,
  IDP_ID = 7,
  IDP_DOUBLE = 8,
  IDP_IDPARRAY = 9,
  /**
   * True or false value, backed by an `int8_t` underlying type for arrays. Values are expected to
   * be 0 or 1.
   */
  IDP_BOOLEAN = 10,
} eIDPropertyType;
#define IDP_NUMTYPES 11

/** Used by some IDP utils, keep values in sync with type enum above. */
enum {
  IDP_TYPE_FILTER_STRING = 1 << IDP_STRING,
  IDP_TYPE_FILTER_INT = 1 << IDP_INT,
  IDP_TYPE_FILTER_FLOAT = 1 << IDP_FLOAT,
  IDP_TYPE_FILTER_ARRAY = 1 << IDP_ARRAY,
  IDP_TYPE_FILTER_GROUP = 1 << IDP_GROUP,
  IDP_TYPE_FILTER_ID = 1 << IDP_ID,
  IDP_TYPE_FILTER_DOUBLE = 1 << IDP_DOUBLE,
  IDP_TYPE_FILTER_IDPARRAY = 1 << IDP_IDPARRAY,
  IDP_TYPE_FILTER_BOOLEAN = 1 << IDP_BOOLEAN,
};

/** #IDProperty.subtype for #IDP_STRING properties. */
typedef enum eIDPropertySubType {
  IDP_STRING_SUB_UTF8 = 0, /* default */
  IDP_STRING_SUB_BYTE = 1, /* arbitrary byte array, _not_ null terminated */
} eIDPropertySubType;

/** #IDProperty.flag. */
typedef enum eIDPropertyFlag {
  /**
   * This #IDProperty may be library-overridden.
   * Should only be used/be relevant for custom properties.
   */
  IDP_FLAG_OVERRIDABLE_LIBRARY = 1 << 0,
  /**
   * This collection item #IDProperty has been inserted in a local override.
   * This is used by internal code to distinguish between library-originated items and
   * local-inserted ones, as many operations are not allowed on the former.
   */
  IDP_FLAG_OVERRIDELIBRARY_LOCAL = 1 << 1,
  /**
   * This #IDProperty has a static type, i.e. its #eIDPropertyType cannot be changed by assigning a
   * new value to it.
   *
   * Currently, array length is also considered as fixed (i.e. part of the type) when this flag is
   * set. This allows to avoid IDProperty storing vectors e.g. to see their length modified.
   *
   * \note Currently, all overridable IDProp are also statically typed. IDProps used as storage for
   * dynamic RNA properties are also always dynamically typed.
   *
   * \note Internal flag, user have no direct way to define or edit it.
   */
  IDP_FLAG_STATIC_TYPE = 1 << 4,
  /**
   * This means the property is set but RNA will return false when checking
   * #RNA_property_is_set, currently this is a runtime flag.
   */
  IDP_FLAG_GHOST = 1 << 7,
} eIDPropertyFlag;

/**
 * Defines for working with IDs.
 *
 * The tags represent types! This is a dirty way of enabling RTTI. The
 * sig_byte end endian defines aren't really used much.
 */
/* NOTE: this is endianness-sensitive. */
#define MAKE_ID2(c, d) ((d) << 8 | (c))

/**
 * ID from database.
 *
 * Written to #BHead.code (for file IO)
 * and the first 2 bytes of #ID.name (for runtime checks, see #GS macro).
 *
 * These types should also be available on their corresponding DNA struct.
 * It must be a static `constexpr` data member so that it can be used in
 * compile-time expressions and does not take up space in the struct.
 * This is used by e.g. #BKE_id_new_nomain for improved type safety.
 *
 * Update #ID_TYPE_IS_DEPRECATED() when deprecating types.
 */
typedef enum ID_Type {
  ID_SCE = MAKE_ID2('S', 'C'),       /* Scene */
  ID_LI = MAKE_ID2('L', 'I'),        /* Library */
  ID_OB = MAKE_ID2('O', 'B'),        /* Object */
  ID_ME = MAKE_ID2('M', 'E'),        /* Mesh */
  ID_CU_LEGACY = MAKE_ID2('C', 'U'), /* Curve. ID_CV should be used in the future (see #95355). */
  ID_MB = MAKE_ID2('M', 'B'),        /* MetaBall */
  ID_MA = MAKE_ID2('M', 'A'),        /* Material */
  ID_TE = MAKE_ID2('T', 'E'),        /* Tex (Texture) */
  ID_IM = MAKE_ID2('I', 'M'),        /* Image */
  ID_LT = MAKE_ID2('L', 'T'),        /* Lattice */
  ID_LA = MAKE_ID2('L', 'A'),        /* Light */
  ID_CA = MAKE_ID2('C', 'A'),        /* Camera */
  ID_KE = MAKE_ID2('K', 'E'),        /* Key (shape key) */
  ID_WO = MAKE_ID2('W', 'O'),        /* World */
  ID_SCR = MAKE_ID2('S', 'R'),       /* bScreen */
  ID_VF = MAKE_ID2('V', 'F'),        /* VFont (Vector Font) */
  ID_TXT = MAKE_ID2('T', 'X'),       /* Text */
  ID_SPK = MAKE_ID2('S', 'K'),       /* Speaker */
  ID_SO = MAKE_ID2('S', 'O'),        /* Sound */
  ID_GR = MAKE_ID2('G', 'R'),        /* Collection */
  ID_AR = MAKE_ID2('A', 'R'),        /* bArmature */
  ID_AC = MAKE_ID2('A', 'C'),        /* bAction */
  ID_NT = MAKE_ID2('N', 'T'),        /* bNodeTree */
  ID_BR = MAKE_ID2('B', 'R'),        /* Brush */
  ID_PA = MAKE_ID2('P', 'A'),        /* ParticleSettings */
  ID_GD_LEGACY = MAKE_ID2('G', 'D'), /* bGPdata, (legacy Grease Pencil) */
  ID_WM = MAKE_ID2('W', 'M'),        /* wmWindowManager */
  ID_MC = MAKE_ID2('M', 'C'),        /* MovieClip */
  ID_MSK = MAKE_ID2('M', 'S'),       /* Mask */
  ID_LS = MAKE_ID2('L', 'S'),        /* FreestyleLineStyle */
  ID_PAL = MAKE_ID2('P', 'L'),       /* Palette */
  ID_PC = MAKE_ID2('P', 'C'),        /* PaintCurve */
  ID_CF = MAKE_ID2('C', 'F'),        /* CacheFile */
  ID_WS = MAKE_ID2('W', 'S'),        /* WorkSpace */
  ID_LP = MAKE_ID2('L', 'P'),        /* LightProbe */
  ID_CV = MAKE_ID2('C', 'V'),        /* Curves */
  ID_PT = MAKE_ID2('P', 'T'),        /* PointCloud */
  ID_VO = MAKE_ID2('V', 'O'),        /* Volume */
  ID_GP = MAKE_ID2('G', 'P'),        /* Grease Pencil */
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
