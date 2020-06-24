/******************************************************************************
 *
 * MantaFlow fluid solver framework
 * Copyright 2011 Tobias Pfaff, Nils Thuerey
 *
 * This program is free software, distributed under the terms of the
 * Apache License, Version 2.0
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Random numbers
 *
 * Based on an example by Makoto Matsumoto, Takuji Nishimura, Shawn Cokus, and Richard J. Wagner
 *
 ******************************************************************************/

#ifndef _RANDOMSTREAM_H
#define _RANDOMSTREAM_H

#include <iostream>
#include <stdio.h>
#include <time.h>
#include "vectorbase.h"

namespace Manta {

class MTRand {
  // Data
 public:
  typedef unsigned long uint32;  // unsigned integer type, at least 32 bits

  enum { N = 624 };       // length of state vector
  enum { SAVE = N + 1 };  // length of array for save()

 protected:
  enum { M = 397 };  // period parameter

  uint32 state[N];  // internal state
  uint32 *pNext;    // next value to get from state
  int left;         // number of values left before reload needed

  // Methods
 public:
  MTRand(const uint32 &oneSeed);                               // initialize with a simple uint32
  MTRand(uint32 *const bigSeed, uint32 const seedLength = N);  // or an array
  MTRand();  // auto-initialize with /dev/urandom or time() and clock()

  // Do NOT use for CRYPTOGRAPHY without securely hashing several returned
  // values together, otherwise the generator state can be learned after
  // reading 624 consecutive values.

  // Access to 32-bit random numbers
  double rand();                       // real number in [0,1]
  double rand(const double &n);        // real number in [0,n]
  double randExc();                    // real number in [0,1)
  double randExc(const double &n);     // real number in [0,n)
  double randDblExc();                 // real number in (0,1)
  double randDblExc(const double &n);  // real number in (0,n)
  uint32 randInt();                    // integer in [0,2^32-1]
  uint32 randInt(const uint32 &n);     // integer in [0,n] for n < 2^32
  double operator()()
  {
    return rand();
  }  // same as rand()

  // Access to 53-bit random numbers (capacity of IEEE double precision)
  double rand53();  // real number in [0,1)

  // Access to nonuniform random number distributions
  double randNorm(const double &mean = 0.0, const double &variance = 1.0);

  // Re-seeding functions with same behavior as initializers
  void seed(const uint32 oneSeed);
  void seed(uint32 *const bigSeed, const uint32 seedLength = N);
  void seed();

  // Saving and loading generator state
  void save(uint32 *saveArray) const;  // to array of size SAVE
  void load(uint32 *const loadArray);  // from such array
  friend std::ostream &operator<<(std::ostream &os, const MTRand &mtrand);
  friend std::istream &operator>>(std::istream &is, MTRand &mtrand);

 protected:
  void initialize(const uint32 oneSeed);
  void reload();
  uint32 hiBit(const uint32 &u) const
  {
    return u & 0x80000000UL;
  }
  uint32 loBit(const uint32 &u) const
  {
    return u & 0x00000001UL;
  }
  uint32 loBits(const uint32 &u) const
  {
    return u & 0x7fffffffUL;
  }
  uint32 mixBits(const uint32 &u, const uint32 &v) const
  {
    return hiBit(u) | loBits(v);
  }
  uint32 twist(const uint32 &m, const uint32 &s0, const uint32 &s1) const
  {
    return m ^ (mixBits(s0, s1) >> 1) ^ (-loBit(s1) & 0x9908b0dfUL);
  }
  static uint32 hash(time_t t, clock_t c);
};

inline MTRand::MTRand(const uint32 &oneSeed)
{
  seed(oneSeed);
}

inline MTRand::MTRand(uint32 *const bigSeed, const uint32 seedLength)
{
  seed(bigSeed, seedLength);
}

inline MTRand::MTRand()
{
  seed();
}

inline double MTRand::rand()
{
  return double(randInt()) * (1.0 / 4294967295.0);
}

inline double MTRand::rand(const double &n)
{
  return rand() * n;
}

inline double MTRand::randExc()
{
  return double(randInt()) * (1.0 / 4294967296.0);
}

inline double MTRand::randExc(const double &n)
{
  return randExc() * n;
}

inline double MTRand::randDblExc()
{
  return (double(randInt()) + 0.5) * (1.0 / 4294967296.0);
}

inline double MTRand::randDblExc(const double &n)
{
  return randDblExc() * n;
}

inline double MTRand::rand53()
{
  uint32 a = randInt() >> 5, b = randInt() >> 6;
  return (a * 67108864.0 + b) * (1.0 / 9007199254740992.0);  // by Isaku Wada
}

inline double MTRand::randNorm(const double &mean, const double &variance)
{
  // Return a real number from a normal (Gaussian) distribution with given
  // mean and variance by Box-Muller method
  double r = sqrt(-2.0 * log(1.0 - randDblExc())) * variance;
  double phi = 2.0 * 3.14159265358979323846264338328 * randExc();
  return mean + r * cos(phi);
}

inline MTRand::uint32 MTRand::randInt()
{
  // Pull a 32-bit integer from the generator state
  // Every other access function simply transforms the numbers extracted here

  if (left == 0)
    reload();
  --left;

  uint32 s1;
  s1 = *pNext++;
  s1 ^= (s1 >> 11);
  s1 ^= (s1 << 7) & 0x9d2c5680UL;
  s1 ^= (s1 << 15) & 0xefc60000UL;
  return (s1 ^ (s1 >> 18));
}

inline MTRand::uint32 MTRand::randInt(const uint32 &n)
{
  // Find which bits are used in n
  // Optimized by Magnus Jonsson (magnus@smartelectronix.com)
  uint32 used = n;
  used |= used >> 1;
  used |= used >> 2;
  used |= used >> 4;
  used |= used >> 8;
  used |= used >> 16;

  // Draw numbers until one is found in [0,n]
  uint32 i;
  do
    i = randInt() & used;  // toss unused bits to shorten search
  while (i > n);
  return i;
}

inline void MTRand::seed(const uint32 oneSeed)
{
  // Seed the generator with a simple uint32
  initialize(oneSeed);
  reload();
}

inline void MTRand::seed(uint32 *const bigSeed, const uint32 seedLength)
{
  // Seed the generator with an array of uint32's
  // There are 2^19937-1 possible initial states.  This function allows
  // all of those to be accessed by providing at least 19937 bits (with a
  // default seed length of N = 624 uint32's).  Any bits above the lower 32
  // in each element are discarded.
  // Just call seed() if you want to get array from /dev/urandom
  initialize(19650218UL);
  const unsigned int Nenum = N;
  int i = 1;
  uint32 j = 0;
  int k = (Nenum > seedLength ? Nenum : seedLength);
  for (; k; --k) {
    state[i] = state[i] ^ ((state[i - 1] ^ (state[i - 1] >> 30)) * 1664525UL);
    state[i] += (bigSeed[j] & 0xffffffffUL) + j;
    state[i] &= 0xffffffffUL;
    ++i;
    ++j;
    if (i >= N) {
      state[0] = state[N - 1];
      i = 1;
    }
    if (j >= seedLength)
      j = 0;
  }
  for (k = N - 1; k; --k) {
    state[i] = state[i] ^ ((state[i - 1] ^ (state[i - 1] >> 30)) * 1566083941UL);
    state[i] -= i;
    state[i] &= 0xffffffffUL;
    ++i;
    if (i >= N) {
      state[0] = state[N - 1];
      i = 1;
    }
  }
  state[0] = 0x80000000UL;  // MSB is 1, assuring non-zero initial array
  reload();
}

inline void MTRand::seed()
{
  // Seed the generator with an array from /dev/urandom if available
  // Otherwise use a hash of time() and clock() values

  // First try getting an array from /dev/urandom
  FILE *urandom = fopen("/dev/urandom", "rb");
  if (urandom) {
    uint32 bigSeed[N];
    uint32 *s = bigSeed;
    int i = N;
    bool success = true;
    while (success && i--)
      success = fread(s++, sizeof(uint32), 1, urandom);
    fclose(urandom);
    if (success) {
      seed(bigSeed, N);
      return;
    }
  }

  // Was not successful, so use time() and clock() instead
  seed(hash(time(NULL), clock()));
}

inline void MTRand::initialize(const uint32 intseed)
{
  // Initialize generator state with seed
  // See Knuth TAOCP Vol 2, 3rd Ed, p.106 for multiplier.
  // In previous versions, most significant bits (MSBs) of the seed affect
  // only MSBs of the state array.  Modified 9 Jan 2002 by Makoto Matsumoto.
  uint32 *s = state;
  uint32 *r = state;
  int i = 1;
  *s++ = intseed & 0xffffffffUL;
  for (; i < N; ++i) {
    *s++ = (1812433253UL * (*r ^ (*r >> 30)) + i) & 0xffffffffUL;
    r++;
  }
}

inline void MTRand::reload()
{
  // Generate N new values in state
  // Made clearer and faster by Matthew Bellew (matthew.bellew@home.com)
  uint32 *p = state;
  int i;
  for (i = N - M; i--; ++p)
    *p = twist(p[M], p[0], p[1]);
  for (i = M; --i; ++p)
    *p = twist(p[M - N], p[0], p[1]);
  *p = twist(p[M - N], p[0], state[0]);

  left = N, pNext = state;
}

inline MTRand::uint32 MTRand::hash(time_t t, clock_t c)
{
  // Get a uint32 from t and c
  // Better than uint32(x) in case x is floating point in [0,1]
  // Based on code by Lawrence Kirby (fred@genesis.demon.co.uk)

  static uint32 differ = 0;  // guarantee time-based seeds will change

  uint32 h1 = 0;
  unsigned char *p = (unsigned char *)&t;
  for (size_t i = 0; i < sizeof(t); ++i) {
    h1 *= std::numeric_limits<unsigned char>::max() + 2U;
    h1 += p[i];
  }
  uint32 h2 = 0;
  p = (unsigned char *)&c;
  for (size_t j = 0; j < sizeof(c); ++j) {
    h2 *= std::numeric_limits<unsigned char>::max() + 2U;
    h2 += p[j];
  }
  return (h1 + differ++) ^ h2;
}

inline void MTRand::save(uint32 *saveArray) const
{
  uint32 *sa = saveArray;
  const uint32 *s = state;
  int i = N;
  for (; i--; *sa++ = *s++) {
  }
  *sa = left;
}

inline void MTRand::load(uint32 *const loadArray)
{
  uint32 *s = state;
  uint32 *la = loadArray;
  int i = N;
  for (; i--; *s++ = *la++) {
  }
  left = *la;
  pNext = &state[N - left];
}

inline std::ostream &operator<<(std::ostream &os, const MTRand &mtrand)
{
  const MTRand::uint32 *s = mtrand.state;
  int i = mtrand.N;
  for (; i--; os << *s++ << "\t") {
  }
  return os << mtrand.left;
}

inline std::istream &operator>>(std::istream &is, MTRand &mtrand)
{
  MTRand::uint32 *s = mtrand.state;
  int i = mtrand.N;
  for (; i--; is >> *s++) {
  }
  is >> mtrand.left;
  mtrand.pNext = &mtrand.state[mtrand.N - mtrand.left];
  return is;
}

// simple interface to mersenne twister
class RandomStream {
 public:
  inline RandomStream(long seed) : mtr(seed){};
  ~RandomStream()
  {
  }

  /*! get a random number from the stream */
  inline double getDouble(void)
  {
    return mtr.rand();
  };
  inline float getFloat(void)
  {
    return (float)mtr.rand();
  };

  inline float getFloat(float min, float max)
  {
    return mtr.rand(max - min) + min;
  };
  inline float getRandNorm(float mean, float var)
  {
    return mtr.randNorm(mean, var);
  };

#if FLOATINGPOINT_PRECISION == 1
  inline Real getReal()
  {
    return getFloat();
  }

#else
  inline Real getReal()
  {
    return getDouble();
  }
#endif

  inline Vec3 getVec3()
  {
    Real a = getReal(), b = getReal(), c = getReal();
    return Vec3(a, b, c);
  }
  inline Vec3 getVec3Norm()
  {
    Vec3 a = getVec3();
    normalize(a);
    return a;
  }

 private:
  MTRand mtr;
};

}  // namespace Manta

#endif
