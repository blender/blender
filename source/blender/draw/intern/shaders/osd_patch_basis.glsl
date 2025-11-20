/* SPDX-FileCopyrightText: 2021-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * File that contains the output of `openSubdiv_getGLSLPatchBasisSource`.
 * The structures here are copy of the latest version only to satisfy building without OSL enabled.
 */

#pragma once

/* This file must replaced at runtime. The following content is only a possible implementation. */
#pragma runtime_generated

struct OsdPatchParam {
  int field0;
  int field1;
  float sharpness;
};

struct OsdPatchArray {
  int regDesc;
  int desc;
  int numPatches;
  int indexBase;
  int stride;
  int primitiveIdBase;
};

struct OsdPatchCoord {
  int arrayIndex;
  int patchIndex;
  int vertIndex;
  float s;
  float t;
};
