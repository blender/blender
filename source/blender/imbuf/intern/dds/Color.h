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

/** 32 bit color stored as BGRA. */
class Color32 {
 public:
  Color32() {}
  Color32(const Color32 &) = default;

  Color32(unsigned char R, unsigned char G, unsigned char B)
  {
    setRGBA(R, G, B, 0xFF);
  }
  Color32(unsigned char R, unsigned char G, unsigned char B, unsigned char A)
  {
    setRGBA(R, G, B, A);
  }
#if 0
  Color32(unsigned char c[4])
  {
    setRGBA(c[0], c[1], c[2], c[3]);
  }
  Color32(float R, float G, float B)
  {
    setRGBA(uint(R * 255), uint(G * 255), uint(B * 255), 0xFF);
  }
  Color32(float R, float G, float B, float A)
  {
    setRGBA(uint(R * 255), uint(G * 255), uint(B * 255), uint(A * 255));
  }
#endif
  Color32(unsigned int U) : u(U) {}

  void setRGBA(unsigned char R, unsigned char G, unsigned char B, unsigned char A)
  {
    r = R;
    g = G;
    b = B;
    a = A;
  }

  void setBGRA(unsigned char B, unsigned char G, unsigned char R, unsigned char A = 0xFF)
  {
    r = R;
    g = G;
    b = B;
    a = A;
  }

  operator unsigned int() const
  {
    return u;
  }

  union {
    struct {
      unsigned char b, g, r, a;
    };
    unsigned int u;
  };
};

/** 16 bit 565 BGR color. */
class Color16 {
 public:
  Color16() {}
  Color16(const Color16 &c) : u(c.u) {}
  explicit Color16(unsigned short U) : u(U) {}

  union {
    struct {
      unsigned short b : 5;
      unsigned short g : 6;
      unsigned short r : 5;
    };
    unsigned short u;
  };
};
