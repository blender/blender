/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 *
 * Types defined in this file are deprecated, converted into modifiers on load.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Don't forget, new effects also in `writefile.cc` for DNA! */

/** #PartEff::flag. */
enum {
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

/** #PartEff::flag2, for pos/neg #PartEff::flag2neg. */
enum {
  PAF_TEXTIME = 1, /* Texture timing. */
};

/** #PartEff::type. */
enum {
  EFF_BUILD = 0,
  EFF_PARTICLE = 1,
  EFF_WAVE = 2,
};

/** #PartEff::flag. */
enum {
  EFF_SELECT = 1,
};

/** #PartEff::stype. */
enum {
  PAF_NORMAL = 0,
  PAF_VECT = 1,
};

/** #PartEff::texmap. */
enum {
  PAF_TEXINT = 0,
  PAF_TEXRGB = 1,
  PAF_TEXGRAD = 2,
};

typedef struct Effect {
  struct Effect *next, *prev;
  short type, flag, buttype;
  char _pad0[2];
} Effect;

typedef struct BuildEff {
  /* NOTE: match #Effect. */
  struct BuildEff *next, *prev;
  short type, flag, buttype;
  /* End header. */

  char _pad0[2];

  float len, sfra;

} BuildEff;

#
#
typedef struct Particle {
  float co[3], no[3];
  float time, lifetime;
  short mat_nr;
  char _pad0[2];
} Particle;

struct Collection;

typedef struct PartEff {
  /* NOTE: match #Effect. */
  struct PartEff *next, *prev;
  short type, flag, buttype;
  /* End header. */

  short stype, vertgroup, userjit;

  float sta, end, lifetime;
  int totpart, totkey, seed;

  float normfac, obfac, randfac, texfac, randlife;
  float force[3];
  float damp;

  float nabla, vectsize, maxlen, defvec[3];
  char _pad[4];

  float mult[4], life[4];
  short child[4], mat[4];
  short texmap, curmult;
  short staticstep, omat, timetex, speedtex, flag2, flag2neg;
  short disp, vertgroup_v;

  /** MAX_VGROUP_NAME. */
  char vgroupname[64], vgroupname_v[64];
  /** Inverse matrix of parent Object. */
  float imat[4][4];

  Particle *keys;
  struct Collection *group;

} PartEff;

typedef struct WaveEff {
  /* NOTE: match #Effect. */
  struct WaveEff *next, *prev;
  short type, flag, buttype, stype;
  /* End header. */

  float startx, starty, height, width;
  float narrow, speed, minfac, damp;

  float timeoffs, lifetime;

} WaveEff;

#ifdef __cplusplus
}
#endif
