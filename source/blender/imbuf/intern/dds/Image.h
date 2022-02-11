/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbdds
 */

/*
 * This file is based on a similar file from the NVIDIA texture tools
 * (http://nvidia-texture-tools.googlecode.com/)
 *
 * Original license from NVIDIA follows.
 */

/* This code is in the public domain -- <castanyo@yahoo.es> */

#pragma once

#include "Color.h"
#include "Common.h"

/** 32 bit RGBA image. */
class Image {
 public:
  enum Format {
    Format_RGB,
    Format_ARGB,
  };

  Image();
  ~Image();

  void allocate(uint w, uint h);
#if 0
  bool load(const char *name);

  void wrap(void *data, uint w, uint h);
  void unwrap();
#endif

  uint width() const;
  uint height() const;

  const Color32 *scanline(uint h) const;
  Color32 *scanline(uint h);

  const Color32 *pixels() const;
  Color32 *pixels();

  const Color32 &pixel(uint idx) const;
  Color32 &pixel(uint idx);

  const Color32 &pixel(uint x, uint y) const;
  Color32 &pixel(uint x, uint y);

  Format format() const;
  void setFormat(Format f);

 private:
  void free();

 private:
  uint m_width;
  uint m_height;
  Format m_format;
  Color32 *m_data;
};

inline const Color32 &Image::pixel(uint x, uint y) const
{
  return pixel(y * width() + x);
}

inline Color32 &Image::pixel(uint x, uint y)
{
  return pixel(y * width() + x);
}
