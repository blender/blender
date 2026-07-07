/* SPDX-FileCopyrightText: 2004-2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "DNA_defs.h"

namespace blender {

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

struct FluidVertexVelocity {
  float vel[3] = {};
};

struct FluidsimSettings {
  /** DEPRECATED. For fast RNA access. */
  struct FluidsimModifierData *fmd = nullptr;
  /* threadcont the calculation is done with */
  int threads = 0;
  char _pad1[4] = {};
  /* domain, fluid or obstacle */
  short type = 0;
  /* Display advanced options in fluid sim tab (on=1, off=0). */
  short show_advancedoptions = 0;

  /* domain object settings */
  /* resolutions */
  short resolutionxyz = 0;
  short previewresxyz = 0;
  /* size of the domain in real units (meters along largest resolution x, y, z extent) */
  float realsize = 0;
  /* show original meshes, preview or final sim */
  short guiDisplayMode = 0;
  short renderDisplayMode = 0;

  /* fluid properties */
  float viscosityValue = 0;
  DNA_DEPRECATED short viscosityMode = 0;
  short viscosityExponent = 0;
  /* gravity strength */
  float grav[3] = {};
  /* anim start end time (in seconds) */
  float animStart = 0, animEnd = 0;
  /* bake start end time (in blender frames) */
  int bakeStart = 0, bakeEnd = 0;
  /* offset for baked frames */
  int frameOffset = 0;
  char _pad2[4] = {};
  /* g star param (LBM compressibility) */
  float gstar = 0;
  /* activate refinement? */
  int maxRefine = 0;

  /* fluid object type settings */
  /* gravity strength */
  float iniVelx = 0, iniVely = 0, iniVelz = 0;

  /**
   * Store output path, and file prefix for baked fluid surface.
   */
  char surfdataPath[/*FILE_MAX*/ 1024] = "";

  /* store start coords of axis aligned bounding box together with size */
  /* values are initialized during derived mesh display. */
  float bbStart[3] = {}, bbSize[3] = {};

  /* additional flags depending on the type, lower short contains flags
   * to check validity, higher short additional flags */
  short typeFlags = 0;
  /**
   * Switch off velocity generation,
   * volume init type for fluid/obstacles (volume=1, shell=2, both=3).
   */
  char domainNovecgen = 0, volumeInitType = 0;

  /* boundary "stickiness" for part slip values */
  float partSlipValue = 0;

  /* number of tracers to generate */
  int generateTracers = 0;
  /* particle generation - on if >0, then determines amount (experimental...) */
  float generateParticles = 0;
  /* smooth fluid surface? */
  float surfaceSmoothing = 0;
  /** Number of surface subdivisions. */
  int surfaceSubdivs = 0;
  /** GUI flags. */
  int flag = 0;

  /** Particle display - size scaling, and alpha influence. */
  float particleInfSize = 0, particleInfAlpha = 0;
  /* testing vars */
  float farFieldSize = 0;

  /** Vertex velocities of simulated fluid mesh. */
  struct FluidVertexVelocity *meshVelocities = nullptr;
  /** Number of vertices in simulated fluid mesh. */
  int totvert = 0;

  /* Fluid control settings */
  float cpsTimeStart = 0;
  float cpsTimeEnd = 0;
  float cpsQuality = 0;

  float attractforceStrength = 0;
  float attractforceRadius = 0;
  float velocityforceStrength = 0;
  float velocityforceRadius = 0;

  int lastgoodframe = 0;

  /** Simulation/flow rate control. */
  float animRate = 0;
};

}  // namespace blender
