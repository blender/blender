/* SPDX-FileCopyrightText: 2002-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef __MODELREADER_H__
#define __MODELREADER_H__

#include "GeoCommon.h"

#ifdef WITH_CXX_GUARDEDALLOC
#  include "MEM_guardedalloc.h"
#endif

/*
 * Virtual class for input file readers
 *
 * @author Tao Ju
 */
class ModelReader {
 public:
  /// Constructor
  ModelReader(){};

  /// Get next triangle
  virtual Triangle *getNextTriangle() = 0;
  virtual int getNextTriangle(int t[3]) = 0;

  /// Get bounding box
  virtual float getBoundingBox(float origin[3]) = 0;

  /// Get number of triangles
  virtual int getNumTriangles() = 0;

  /// Get storage size
  virtual int getMemory() = 0;

  /// Reset file reading location
  virtual void reset() = 0;

  /// For explicit vertex models
  virtual int getNumVertices() = 0;

  virtual void getNextVertex(float v[3]) = 0;

  virtual void printInfo() = 0;

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("DUALCON:ModelReader")
#endif
};

#endif /* __MODELREADER_H__ */
