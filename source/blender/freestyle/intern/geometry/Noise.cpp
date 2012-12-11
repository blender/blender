
//
//  Copyright (C) : Please refer to the COPYRIGHT file distributed 
//   with this source distribution. 
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
///////////////////////////////////////////////////////////////////////////////

#include "Noise.h"
# include <stdlib.h>
# include <stdio.h>
# include <math.h>
#include <time.h>

#define MINX		-1000000
#define MINY		MINX
#define MINZ		MINX
#define SCURVE(a) ((a)*(a)*(3.0-2.0*(a)))
#define REALSCALE ( 2.0 / 65536.0 )
#define NREALSCALE ( 2.0 / 4096.0 )
#define HASH3D(a,b,c) hashTable[hashTable[hashTable[(a) & 0xfff] ^ ((b) & 0xfff)] ^ ((c) & 0xfff)]
#define HASH(a,b,c) (xtab[(xtab[(xtab[(a) & 0xff] ^ (b)) & 0xff] ^ (c)) & 0xff] & 0xff)
#define INCRSUM(m,s,x,y,z)	((s)*(RTable[m]*0.5		\
					+ RTable[m+1]*(x)	\
					+ RTable[m+2]*(y)	\
					+ RTable[m+3]*(z)))
#define MAXSIZE		500
#define nrand()		((float)rand()/(float)RAND_MAX)
#define seednrand(x)	srand(x*RAND_MAX)

#define BM 0xff

#define N 0x1000
#define NP 12   /* 2^N */
#define NM 0xfff

#define s_curve(t) ( t * t * (3. - 2. * t) )

#define lerp(t, a, b) ( a + t * (b - a) )

#define setup(i,b0,b1,r0,r1)\
	t = i + N;\
	b0 = ((int)t) & BM;\
	b1 = (b0+1) & BM;\
	r0 = t - (int)t;\
	r1 = r0 - 1.;

static void normalize2(float v[2])
{
  float s;

  s = sqrt(v[0] * v[0] + v[1] * v[1]);
  v[0] = v[0] / s;
  v[1] = v[1] / s;
}

static void normalize3(float v[3])
{
  float s;

  s = sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
  v[0] = v[0] / s;
  v[1] = v[1] / s;
  v[2] = v[2] / s;
}

float Noise::turbulence1(float arg, float freq, float amp, unsigned oct)
{
  float t;
  float vec;
  
  for (t = 0; oct > 0 && freq > 0; freq *= 2, amp /= 2, --oct) 
    {
      vec = freq * arg;
      t += smoothNoise1(vec) * amp;
    }
  return t;
}

float Noise::turbulence2(Vec2f& v, float freq, float amp, unsigned oct)
{
  float t;
  Vec2f vec;
  
  for (t = 0; oct > 0 && freq > 0; freq *= 2, amp /= 2, --oct) 
    {
      vec.x() = freq * v.x();
      vec.y() = freq * v.y();
      t += smoothNoise2(vec) * amp;
    }
  return t;
}

float Noise::turbulence3(Vec3f& v, float freq, float amp, unsigned oct)
{
  float t;
  Vec3f vec;
  
  for (t = 0; oct > 0 && freq > 0; freq *= 2, amp /= 2, --oct) 
    {
      vec.x() = freq * v.x();
      vec.y() = freq * v.y();
      vec.z() = freq * v.z();
      t += smoothNoise3(vec) * amp;
    }
  return t;
}

// Noise functions over 1, 2, and 3 dimensions

float Noise::smoothNoise1(float arg)
{
  int bx0, bx1;
  float rx0, rx1, sx, t, u, v, vec;

  vec = arg;
  setup(vec, bx0,bx1, rx0,rx1);

  sx = s_curve(rx0);

  u = rx0 * g1[ p[ bx0 ] ];
  v = rx1 * g1[ p[ bx1 ] ];

  return lerp(sx, u, v);
}

float Noise::smoothNoise2(Vec2f& vec)
{
  int bx0, bx1, by0, by1, b00, b10, b01, b11;
  float rx0, rx1, ry0, ry1, *q, sx, sy, a, b, t, u, v;
  register int i, j;

  setup(vec.x(), bx0,bx1, rx0,rx1);
  setup(vec.y(), by0,by1, ry0,ry1);

  i = p[ bx0 ];
  j = p[ bx1 ];

  b00 = p[ i + by0 ];
  b10 = p[ j + by0 ];
  b01 = p[ i + by1 ];
  b11 = p[ j + by1 ];

  sx = s_curve(rx0);
  sy = s_curve(ry0);

#define at2(rx,ry) ( rx * q[0] + ry * q[1] )

  q = g2[ b00 ] ; u = at2(rx0,ry0);
  q = g2[ b10 ] ; v = at2(rx1,ry0);
  a = lerp(sx, u, v);

  q = g2[ b01 ] ; u = at2(rx0,ry1);
  q = g2[ b11 ] ; v = at2(rx1,ry1);
  b = lerp(sx, u, v);

  return lerp(sy, a, b);
}

float Noise::smoothNoise3(Vec3f& vec)
{
  int bx0, bx1, by0, by1, bz0, bz1, b00, b10, b01, b11;
  float rx0, rx1, ry0, ry1, rz0, rz1, *q, sy, sz, a, b, c, d, t, u, v;
  register int i, j;

  setup(vec.x(), bx0,bx1, rx0,rx1);
  setup(vec.y(), by0,by1, ry0,ry1);
  setup(vec.z(), bz0,bz1, rz0,rz1);

  i = p[ bx0 ];
  j = p[ bx1 ];

  b00 = p[ i + by0 ];
  b10 = p[ j + by0 ];
  b01 = p[ i + by1 ];
  b11 = p[ j + by1 ];

  t  = s_curve(rx0);
  sy = s_curve(ry0);
  sz = s_curve(rz0);

#define at3(rx,ry,rz) ( rx * q[0] + ry * q[1] + rz * q[2] )

  q = g3[ b00 + bz0 ] ;
  u = at3(rx0,ry0,rz0);
  q = g3[ b10 + bz0 ] ;
  v = at3(rx1,ry0,rz0);
  a = lerp(t, u, v);

  q = g3[ b01 + bz0 ] ;
  u = at3(rx0,ry1,rz0);
  q = g3[ b11 + bz0 ] ;
  v = at3(rx1,ry1,rz0);
  b = lerp(t, u, v);

  c = lerp(sy, a, b);

  q = g3[ b00 + bz1 ] ;
  u = at3(rx0,ry0,rz1);
  q = g3[ b10 + bz1 ] ;
  v = at3(rx1,ry0,rz1);
  a = lerp(t, u, v);

  q = g3[ b01 + bz1 ] ;
  u = at3(rx0,ry1,rz1);
  q = g3[ b11 + bz1 ] ;
  v = at3(rx1,ry1,rz1);
  b = lerp(t, u, v);

  d = lerp(sy, a, b);

  return lerp(sz, c, d);
}

Noise::Noise(long seed)
{
  int i, j, k;
  
  seednrand((seed < 0) ? time(NULL) : seed);
  for (i = 0 ; i < _Noise_B_ ; i++) 
    {
      p[i] = i;

      g1[i] = (float)((rand() % (_Noise_B_ + _Noise_B_)) - _Noise_B_) / _Noise_B_;

      for (j = 0 ; j < 2 ; j++)
	g2[i][j] = (float)((rand() % (_Noise_B_ + _Noise_B_)) - _Noise_B_) / _Noise_B_;
      normalize2(g2[i]);

      for (j = 0 ; j < 3 ; j++)
	g3[i][j] = (float)((rand() % (_Noise_B_ + _Noise_B_)) - _Noise_B_) / _Noise_B_;
      normalize3(g3[i]);
    }

  while (--i) 
    {
      k = p[i];
      p[i] = p[j = rand() % _Noise_B_];
      p[j] = k;
    }

  for (i = 0 ; i < _Noise_B_ + 2 ; i++) 
    {
      p[_Noise_B_ + i] = p[i];
      g1[_Noise_B_ + i] = g1[i];
      for (j = 0 ; j < 2 ; j++)
	g2[_Noise_B_ + i][j] = g2[i][j];
      for (j = 0 ; j < 3 ; j++)
	g3[_Noise_B_ + i][j] = g3[i][j];
    }
}
