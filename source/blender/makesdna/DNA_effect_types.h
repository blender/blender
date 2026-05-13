/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 *
 * Types defined in this file are deprecated, converted into modifiers on load.
 */

#pragma once

#include "BLI_enum_flags.hh"

namespace blender {

/* Don't forget, new effects also in `writefile.cc` for DNA! */

/** #PartEff::flag. */
enum ePartEff_Flag : short {
  // PAF_UNUSED_0 = 1 << 0, /* DEPRECATED, dirty. */
  PAF_BSPLINE = 1 << 1,
  PAF_STATIC = 1 << 2,
  PAF_FACE = 1 << 3,
  PAF_ANIMATED = 1 << 4,
  /** Show particles before they're emitted. */
  PAF_UNBORN = 1 << 5,
  /** Emit only from faces. */
  PAF_OFACE = 1 << 6,
  /** show emitter (don't hide actual mesh). */
  PAF_SHOWE = 1 << 7,
  /** True random emit from faces (not just ordered jitter). */
  PAF_TRAND = 1 << 8,
  /** even distribution in face emission based on face areas. */
  PAF_EDISTR = 1 << 9,
  /** Show particles after they've died. */
  PAF_DIED = 1 << 11,
};
ENUM_OPERATORS(ePartEff_Flag)

/** #PartEff::flag2, for pos/neg #PartEff::flag2neg. */
enum ePartEff_Flag2 : short {
  PAF_TEXTIME = 1, /* Texture timing. */
};
ENUM_OPERATORS(ePartEff_Flag2)

/** #PartEff::type. */
enum eEffect_Type : short {
  EFF_BUILD = 0,
  EFF_PARTICLE = 1,
  EFF_WAVE = 2,
};

/** #PartEff::flag. */
enum eEffect_Flag : short {
  EFF_SELECT = 1,
};
ENUM_OPERATORS(eEffect_Flag)

/** #PartEff::stype. */
enum ePartEff_SType : short {
  PAF_NORMAL = 0,
  PAF_VECT = 1,
};

/** #PartEff::texmap. */
enum ePartEff_TexMap : short {
  PAF_TEXINT = 0,
  PAF_TEXRGB = 1,
  PAF_TEXGRAD = 2,
};

struct Effect {
  struct Effect *next = nullptr, *prev = nullptr;
  eEffect_Type type = EFF_BUILD;
  eEffect_Flag flag = {};
  short buttype = 0;
  char _pad0[2] = {};
};

struct BuildEff {
  /* NOTE: match #Effect. */
  struct BuildEff *next = nullptr, *prev = nullptr;
  eEffect_Type type = EFF_BUILD;
  eEffect_Flag flag = {};
  short buttype = 0;
  /* End header. */

  char _pad0[2] = {};

  float len = 0, sfra = 0;
};

#
#
struct Particle {
  float co[3] = {}, no[3] = {};
  float time = 0, lifetime = 0;
  short mat_nr = 0;
  char _pad0[2] = {};
};

struct Collection;

struct PartEff {
  /* NOTE: match #Effect. */
  struct PartEff *next = nullptr, *prev = nullptr;
  eEffect_Type type = EFF_BUILD;
  ePartEff_Flag flag = {};
  short buttype = 0;
  /* End header. */

  ePartEff_SType stype = PAF_NORMAL;
  short vertgroup = 0, userjit = 0;

  float sta = 0, end = 0, lifetime = 0;
  int totpart = 0, totkey = 0, seed = 0;

  float normfac = 0, obfac = 0, randfac = 0, texfac = 0, randlife = 0;
  float force[3] = {};
  float damp = 0;

  float nabla = 0, vectsize = 0, maxlen = 0, defvec[3] = {};
  char _pad[4] = {};

  float mult[4] = {}, life[4] = {};
  short child[4] = {}, mat[4] = {};
  ePartEff_TexMap texmap = PAF_TEXINT;
  short curmult = 0;
  short staticstep = 0, omat = 0, timetex = 0, speedtex = 0;
  ePartEff_Flag2 flag2 = {}, flag2neg = {};
  short disp = 0, vertgroup_v = 0;

  char vgroupname[/*MAX_VGROUP_NAME*/ 64] = "";
  char vgroupname_v[/*MAX_VGROUP_NAME*/ 64] = "";
  /** Inverse matrix of parent Object. */
  float imat[4][4] = {};

  Particle *keys = nullptr;
  struct Collection *group = nullptr;
};

struct WaveEff {
  /* NOTE: match #Effect. */
  struct WaveEff *next = nullptr, *prev = nullptr;
  eEffect_Type type = EFF_BUILD;
  eEffect_Flag flag = {};
  short buttype = 0, stype = 0;
  /* End header. */

  float startx = 0, starty = 0, height = 0, width = 0;
  float narrow = 0, speed = 0, minfac = 0, damp = 0;

  float timeoffs = 0, lifetime = 0;
};

}  // namespace blender
