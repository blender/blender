/* SPDX-FileCopyrightText: 2004-2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "DNA_ID.h"
#include "DNA_defs.h"

typedef struct FluidVertexVelocity {
  float vel[3];
} FluidVertexVelocity;

typedef struct FluidsimSettings {
  /** DEPRECATED. For fast RNA access. */
  struct FluidsimModifierData *fmd;
  /* threadcont the calculation is done with */
  int threads;
  char _pad1[4];
  /* domain, fluid or obstacle */
  short type;
  /* Display advanced options in fluid sim tab (on=1, off=0). */
  short show_advancedoptions;

  /* domain object settings */
  /* resolutions */
  short resolutionxyz;
  short previewresxyz;
  /* size of the domain in real units (meters along largest resolution x, y, z extent) */
  float realsize;
  /* show original meshes, preview or final sim */
  short guiDisplayMode;
  short renderDisplayMode;

  /* fluid properties */
  float viscosityValue;
  short viscosityMode DNA_DEPRECATED;
  short viscosityExponent;
  /* gravity strength */
  float grav[3];
  /* anim start end time (in seconds) */
  float animStart, animEnd;
  /* bake start end time (in blender frames) */
  int bakeStart, bakeEnd;
  /* offset for baked frames */
  int frameOffset;
  char _pad2[4];
  /* g star param (LBM compressibility) */
  float gstar;
  /* activate refinement? */
  int maxRefine;

  /* fluid object type settings */
  /* gravity strength */
  float iniVelx, iniVely, iniVelz;

  /**
   * Store output path, and file prefix for baked fluid surface.
   */
  char surfdataPath[/*FILE_MAX*/ 1024];

  /* store start coords of axis aligned bounding box together with size */
  /* values are initialized during derived mesh display. */
  float bbStart[3], bbSize[3];

  /* additional flags depending on the type, lower short contains flags
   * to check validity, higher short additional flags */
  short typeFlags;
  /**
   * Switch off velocity generation,
   * volume init type for fluid/obstacles (volume=1, shell=2, both=3).
   */
  char domainNovecgen, volumeInitType;

  /* boundary "stickiness" for part slip values */
  float partSlipValue;

  /* number of tracers to generate */
  int generateTracers;
  /* particle generation - on if >0, then determines amount (experimental...) */
  float generateParticles;
  /* smooth fluid surface? */
  float surfaceSmoothing;
  /** Number of surface subdivisions. */
  int surfaceSubdivs;
  /** GUI flags. */
  int flag;

  /** Particle display - size scaling, and alpha influence. */
  float particleInfSize, particleInfAlpha;
  /* testing vars */
  float farFieldSize;

  /** Vertex velocities of simulated fluid mesh. */
  struct FluidVertexVelocity *meshVelocities;
  /** Number of vertices in simulated fluid mesh. */
  int totvert;

  /* Fluid control settings */
  float cpsTimeStart;
  float cpsTimeEnd;
  float cpsQuality;

  float attractforceStrength;
  float attractforceRadius;
  float velocityforceStrength;
  float velocityforceRadius;

  int lastgoodframe;

  /** Simulation/flow rate control. */
  float animRate;
} FluidsimSettings;

/** #Object::fluidsimSettings */
enum {
  OB_FLUIDSIM_ENABLE = 1,
  OB_FLUIDSIM_DOMAIN = 1 << 1,
  OB_FLUIDSIM_FLUID = 1 << 2,
  OB_FLUIDSIM_OBSTACLE = 1 << 3,
  OB_FLUIDSIM_INFLOW = 1 << 4,
  OB_FLUIDSIM_OUTFLOW = 1 << 5,
  OB_FLUIDSIM_PARTICLE = 1 << 6,
  OB_FLUIDSIM_CONTROL = 1 << 7,
};

/** #FluidsimSettings::flags. */
enum {
  OB_FLUIDSIM_REVERSE = 1 << 0,
  OB_FLUIDSIM_ACTIVE = 1 << 1,
  OB_FLUIDSIM_OVERRIDE_TIME = 1 << 2,
};
