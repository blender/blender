/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

/** \file
 * \ingroup DNA
 *
 * Types defined in this file are deprecated, converted into modifiers on load.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Don't forget, new effects also in `writefile.c` for DNA! */

#define PAF_MAXMULT 4

/* paf->flag (keep bit 0 free for compatibility). */
#define PAF_BSPLINE 2
#define PAF_STATIC 4
#define PAF_FACE 8
#define PAF_ANIMATED 16
/* show particles before they're emitted. */
#define PAF_UNBORN 32
/* Emit only from faces. */
#define PAF_OFACE 64
/* show emitter (don't hide actual mesh). */
#define PAF_SHOWE 128
/* true random emit from faces (not just ordered jitter). */
#define PAF_TRAND 256
/* even distribution in face emission based on face areas. */
#define PAF_EDISTR 512
/* Show particles after they've died. */
#define PAF_DIED 2048

/* `paf->flag2` for pos/neg `paf->flag2neg`. */
#define PAF_TEXTIME 1 /* Texture timing. */

/* eff->type */
#define EFF_BUILD 0
#define EFF_PARTICLE 1
#define EFF_WAVE 2

/* eff->flag */
#define EFF_SELECT 1

/* paf->stype */
#define PAF_NORMAL 0
#define PAF_VECT 1

/* paf->texmap */
#define PAF_TEXINT 0
#define PAF_TEXRGB 1
#define PAF_TEXGRAD 2

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
