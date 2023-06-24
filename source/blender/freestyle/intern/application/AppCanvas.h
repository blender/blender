/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup freestyle
 */

#include "../stroke/Canvas.h"
#include "AppView.h"

namespace Freestyle {

class AppCanvas : public Canvas {
 public:
  AppCanvas();
  AppCanvas(AppView *iViewer);
  AppCanvas(const AppCanvas &iBrother);
  virtual ~AppCanvas();

  /** operations that need to be done before a draw */
  virtual void preDraw();

  /** operations that need to be done after a draw */
  virtual void postDraw();

  /** Erases the layers and clears the canvas */
  virtual void Erase();

  /* init the canvas */
  virtual void init();

  /** Reads a pixel area from the canvas */
  virtual void readColorPixels(int x, int y, int w, int h, RGBImage &oImage) const;
  /** Reads a depth pixel area from the canvas */
  virtual void readDepthPixels(int x, int y, int w, int h, GrayImage &oImage) const;

  virtual BBox<Vec3r> scene3DBBox() const;

  /* abstract */
  virtual void RenderStroke(Stroke *);
  virtual void update();

  /** accessors */
  virtual int width() const;
  virtual int height() const;
  virtual BBox<Vec2i> border() const;
  virtual float thickness() const;

  AppView *_pViewer;
  inline const AppView *viewer() const
  {
    return _pViewer;
  }

  /** modifiers */
  void setViewer(AppView *iViewer);

  /* soc */
  void setPassDiffuse(float *buf, int width, int height)
  {
    _pass_diffuse.buf = buf;
    _pass_diffuse.width = width;
    _pass_diffuse.height = height;
  }
  void setPassZ(float *buf, int width, int height)
  {
    _pass_z.buf = buf;
    _pass_z.width = width;
    _pass_z.height = height;
  }

 private:
  struct {
    float *buf;
    int width, height;
  } _pass_diffuse, _pass_z;
};

} /* namespace Freestyle */
