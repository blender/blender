/* -*- mode: C++; indent-tabs-mode: nil; -*-
 *
 * This file is a part of LEMON, a generic C++ optimization library.
 *
 * Copyright (C) 2003-2009
 * Egervary Jeno Kombinatorikus Optimalizalasi Kutatocsoport
 * (Egervary Research Group on Combinatorial Optimization, EGRES).
 *
 * Permission to use, modify and distribute this software is granted
 * provided that this copyright notice appears in all copies. For
 * precise terms see the accompanying LICENSE file.
 *
 * This software is provided "AS IS" with no warranty of any kind,
 * express or implied, and with no claim as to its suitability for any
 * purpose.
 *
 */

/*
 * This file contains the reimplemented version of the Mersenne Twister
 * Generator of Matsumoto and Nishimura.
 *
 * See the appropriate copyright notice below.
 *
 * Copyright (C) 1997 - 2002, Makoto Matsumoto and Takuji Nishimura,
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. The names of its contributors may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 * Any feedback is very welcome.
 * http://www.math.sci.hiroshima-u.ac.jp/~m-mat/MT/emt.html
 * email: m-mat @ math.sci.hiroshima-u.ac.jp (remove space)
 */

#ifndef LEMON_RANDOM_H
#define LEMON_RANDOM_H

#include <algorithm>
#include <iterator>
#include <vector>
#include <limits>
#include <fstream>

#include <lemon/math.h>
#include <lemon/dim2.h>

#ifndef WIN32
#include <sys/time.h>
#include <ctime>
#include <sys/types.h>
#include <unistd.h>
#else
#include <lemon/bits/windows.h>
#endif

///\ingroup misc
///\file
///\brief Mersenne Twister random number generator

namespace lemon {

  namespace _random_bits {

    template <typename _Word, int _bits = std::numeric_limits<_Word>::digits>
    struct RandomTraits {};

    template <typename _Word>
    struct RandomTraits<_Word, 32> {

      typedef _Word Word;
      static const int bits = 32;

      static const int length = 624;
      static const int shift = 397;

      static const Word mul = 0x6c078965u;
      static const Word arrayInit = 0x012BD6AAu;
      static const Word arrayMul1 = 0x0019660Du;
      static const Word arrayMul2 = 0x5D588B65u;

      static const Word mask = 0x9908B0DFu;
      static const Word loMask = (1u << 31) - 1;
      static const Word hiMask = ~loMask;


      static Word tempering(Word rnd) {
        rnd ^= (rnd >> 11);
        rnd ^= (rnd << 7) & 0x9D2C5680u;
        rnd ^= (rnd << 15) & 0xEFC60000u;
        rnd ^= (rnd >> 18);
        return rnd;
      }

    };

    template <typename _Word>
    struct RandomTraits<_Word, 64> {

      typedef _Word Word;
      static const int bits = 64;

      static const int length = 312;
      static const int shift = 156;

      static const Word mul = Word(0x5851F42Du) << 32 | Word(0x4C957F2Du);
      static const Word arrayInit = Word(0x00000000u) << 32 |Word(0x012BD6AAu);
      static const Word arrayMul1 = Word(0x369DEA0Fu) << 32 |Word(0x31A53F85u);
      static const Word arrayMul2 = Word(0x27BB2EE6u) << 32 |Word(0x87B0B0FDu);

      static const Word mask = Word(0xB5026F5Au) << 32 | Word(0xA96619E9u);
      static const Word loMask = (Word(1u) << 31) - 1;
      static const Word hiMask = ~loMask;

      static Word tempering(Word rnd) {
        rnd ^= (rnd >> 29) & (Word(0x55555555u) << 32 | Word(0x55555555u));
        rnd ^= (rnd << 17) & (Word(0x71D67FFFu) << 32 | Word(0xEDA60000u));
        rnd ^= (rnd << 37) & (Word(0xFFF7EEE0u) << 32 | Word(0x00000000u));
        rnd ^= (rnd >> 43);
        return rnd;
      }

    };

    template <typename _Word>
    class RandomCore {
    public:

      typedef _Word Word;

    private:

      static const int bits = RandomTraits<Word>::bits;

      static const int length = RandomTraits<Word>::length;
      static const int shift = RandomTraits<Word>::shift;

    public:

      void initState() {
        static const Word seedArray[4] = {
          0x12345u, 0x23456u, 0x34567u, 0x45678u
        };

        initState(seedArray, seedArray + 4);
      }

      void initState(Word seed) {

        static const Word mul = RandomTraits<Word>::mul;

        current = state;

        Word *curr = state + length - 1;
        curr[0] = seed; --curr;
        for (int i = 1; i < length; ++i) {
          curr[0] = (mul * ( curr[1] ^ (curr[1] >> (bits - 2)) ) + i);
          --curr;
        }
      }

      template <typename Iterator>
      void initState(Iterator begin, Iterator end) {

        static const Word init = RandomTraits<Word>::arrayInit;
        static const Word mul1 = RandomTraits<Word>::arrayMul1;
        static const Word mul2 = RandomTraits<Word>::arrayMul2;


        Word *curr = state + length - 1; --curr;
        Iterator it = begin; int cnt = 0;
        int num;

        initState(init);

        num = length > end - begin ? length : end - begin;
        while (num--) {
          curr[0] = (curr[0] ^ ((curr[1] ^ (curr[1] >> (bits - 2))) * mul1))
            + *it + cnt;
          ++it; ++cnt;
          if (it == end) {
            it = begin; cnt = 0;
          }
          if (curr == state) {
            curr = state + length - 1; curr[0] = state[0];
          }
          --curr;
        }

        num = length - 1; cnt = length - (curr - state) - 1;
        while (num--) {
          curr[0] = (curr[0] ^ ((curr[1] ^ (curr[1] >> (bits - 2))) * mul2))
            - cnt;
          --curr; ++cnt;
          if (curr == state) {
            curr = state + length - 1; curr[0] = state[0]; --curr;
            cnt = 1;
          }
        }

        state[length - 1] = Word(1) << (bits - 1);
      }

      void copyState(const RandomCore& other) {
        std::copy(other.state, other.state + length, state);
        current = state + (other.current - other.state);
      }

      Word operator()() {
        if (current == state) fillState();
        --current;
        Word rnd = *current;
        return RandomTraits<Word>::tempering(rnd);
      }

    private:


      void fillState() {
        static const Word mask[2] = { 0x0ul, RandomTraits<Word>::mask };
        static const Word loMask = RandomTraits<Word>::loMask;
        static const Word hiMask = RandomTraits<Word>::hiMask;

        current = state + length;

        Word *curr = state + length - 1;
        long num;

        num = length - shift;
        while (num--) {
          curr[0] = (((curr[0] & hiMask) | (curr[-1] & loMask)) >> 1) ^
            curr[- shift] ^ mask[curr[-1] & 1ul];
          --curr;
        }
        num = shift - 1;
        while (num--) {
          curr[0] = (((curr[0] & hiMask) | (curr[-1] & loMask)) >> 1) ^
            curr[length - shift] ^ mask[curr[-1] & 1ul];
          --curr;
        }
        state[0] = (((state[0] & hiMask) | (curr[length - 1] & loMask)) >> 1) ^
          curr[length - shift] ^ mask[curr[length - 1] & 1ul];

      }


      Word *current;
      Word state[length];

    };


    template <typename Result,
              int shift = (std::numeric_limits<Result>::digits + 1) / 2>
    struct Masker {
      static Result mask(const Result& result) {
        return Masker<Result, (shift + 1) / 2>::
          mask(static_cast<Result>(result | (result >> shift)));
      }
    };

    template <typename Result>
    struct Masker<Result, 1> {
      static Result mask(const Result& result) {
        return static_cast<Result>(result | (result >> 1));
      }
    };

    template <typename Result, typename Word,
              int rest = std::numeric_limits<Result>::digits, int shift = 0,
              bool last = rest <= std::numeric_limits<Word>::digits>
    struct IntConversion {
      static const int bits = std::numeric_limits<Word>::digits;

      static Result convert(RandomCore<Word>& rnd) {
        return static_cast<Result>(rnd() >> (bits - rest)) << shift;
      }

    };

    template <typename Result, typename Word, int rest, int shift>
    struct IntConversion<Result, Word, rest, shift, false> {
      static const int bits = std::numeric_limits<Word>::digits;

      static Result convert(RandomCore<Word>& rnd) {
        return (static_cast<Result>(rnd()) << shift) |
          IntConversion<Result, Word, rest - bits, shift + bits>::convert(rnd);
      }
    };


    template <typename Result, typename Word,
              bool one_word = (std::numeric_limits<Word>::digits <
                               std::numeric_limits<Result>::digits) >
    struct Mapping {
      static Result map(RandomCore<Word>& rnd, const Result& bound) {
        Word max = Word(bound - 1);
        Result mask = Masker<Result>::mask(bound - 1);
        Result num;
        do {
          num = IntConversion<Result, Word>::convert(rnd) & mask;
        } while (num > max);
        return num;
      }
    };

    template <typename Result, typename Word>
    struct Mapping<Result, Word, false> {
      static Result map(RandomCore<Word>& rnd, const Result& bound) {
        Word max = Word(bound - 1);
        Word mask = Masker<Word, (std::numeric_limits<Result>::digits + 1) / 2>
          ::mask(max);
        Word num;
        do {
          num = rnd() & mask;
        } while (num > max);
        return num;
      }
    };

    template <typename Result, int exp>
    struct ShiftMultiplier {
      static const Result multiplier() {
        Result res = ShiftMultiplier<Result, exp / 2>::multiplier();
        res *= res;
        if ((exp & 1) == 1) res *= static_cast<Result>(0.5);
        return res;
      }
    };

    template <typename Result>
    struct ShiftMultiplier<Result, 0> {
      static const Result multiplier() {
        return static_cast<Result>(1.0);
      }
    };

    template <typename Result>
    struct ShiftMultiplier<Result, 20> {
      static const Result multiplier() {
        return static_cast<Result>(1.0/1048576.0);
      }
    };

    template <typename Result>
    struct ShiftMultiplier<Result, 32> {
      static const Result multiplier() {
        return static_cast<Result>(1.0/4294967296.0);
      }
    };

    template <typename Result>
    struct ShiftMultiplier<Result, 53> {
      static const Result multiplier() {
        return static_cast<Result>(1.0/9007199254740992.0);
      }
    };

    template <typename Result>
    struct ShiftMultiplier<Result, 64> {
      static const Result multiplier() {
        return static_cast<Result>(1.0/18446744073709551616.0);
      }
    };

    template <typename Result, int exp>
    struct Shifting {
      static Result shift(const Result& result) {
        return result * ShiftMultiplier<Result, exp>::multiplier();
      }
    };

    template <typename Result, typename Word,
              int rest = std::numeric_limits<Result>::digits, int shift = 0,
              bool last = rest <= std::numeric_limits<Word>::digits>
    struct RealConversion{
      static const int bits = std::numeric_limits<Word>::digits;

      static Result convert(RandomCore<Word>& rnd) {
        return Shifting<Result, shift + rest>::
          shift(static_cast<Result>(rnd() >> (bits - rest)));
      }
    };

    template <typename Result, typename Word, int rest, int shift>
    struct RealConversion<Result, Word, rest, shift, false> {
      static const int bits = std::numeric_limits<Word>::digits;

      static Result convert(RandomCore<Word>& rnd) {
        return Shifting<Result, shift + bits>::
          shift(static_cast<Result>(rnd())) +
          RealConversion<Result, Word, rest-bits, shift + bits>::
          convert(rnd);
      }
    };

    template <typename Result, typename Word>
    struct Initializer {

      template <typename Iterator>
      static void init(RandomCore<Word>& rnd, Iterator begin, Iterator end) {
        std::vector<Word> ws;
        for (Iterator it = begin; it != end; ++it) {
          ws.push_back(Word(*it));
        }
        rnd.initState(ws.begin(), ws.end());
      }

      static void init(RandomCore<Word>& rnd, Result seed) {
        rnd.initState(seed);
      }
    };

    template <typename Word>
    struct BoolConversion {
      static bool convert(RandomCore<Word>& rnd) {
        return (rnd() & 1) == 1;
      }
    };

    template <typename Word>
    struct BoolProducer {
      Word buffer;
      int num;

      BoolProducer() : num(0) {}

      bool convert(RandomCore<Word>& rnd) {
        if (num == 0) {
          buffer = rnd();
          num = RandomTraits<Word>::bits;
        }
        bool r = (buffer & 1);
        buffer >>= 1;
        --num;
        return r;
      }
    };

  }

  /// \ingroup misc
  ///
  /// \brief Mersenne Twister random number generator
  ///
  /// The Mersenne Twister is a twisted generalized feedback
  /// shift-register generator of Matsumoto and Nishimura. The period
  /// of this generator is \f$ 2^{19937} - 1 \f$ and it is
  /// equi-distributed in 623 dimensions for 32-bit numbers. The time
  /// performance of this generator is comparable to the commonly used
  /// generators.
  ///
  /// This implementation is specialized for both 32-bit and 64-bit
  /// architectures. The generators differ sligthly in the
  /// initialization and generation phase so they produce two
  /// completly different sequences.
  ///
  /// The generator gives back random numbers of serveral types. To
  /// get a random number from a range of a floating point type you
  /// can use one form of the \c operator() or the \c real() member
  /// function. If you want to get random number from the {0, 1, ...,
  /// n-1} integer range use the \c operator[] or the \c integer()
  /// method. And to get random number from the whole range of an
  /// integer type you can use the argumentless \c integer() or \c
  /// uinteger() functions. After all you can get random bool with
  /// equal chance of true and false or given probability of true
  /// result with the \c boolean() member functions.
  ///
  ///\code
  /// // The commented code is identical to the other
  /// double a = rnd();                     // [0.0, 1.0)
  /// // double a = rnd.real();             // [0.0, 1.0)
  /// double b = rnd(100.0);                // [0.0, 100.0)
  /// // double b = rnd.real(100.0);        // [0.0, 100.0)
  /// double c = rnd(1.0, 2.0);             // [1.0, 2.0)
  /// // double c = rnd.real(1.0, 2.0);     // [1.0, 2.0)
  /// int d = rnd[100000];                  // 0..99999
  /// // int d = rnd.integer(100000);       // 0..99999
  /// int e = rnd[6] + 1;                   // 1..6
  /// // int e = rnd.integer(1, 1 + 6);     // 1..6
  /// int b = rnd.uinteger<int>();          // 0 .. 2^31 - 1
  /// int c = rnd.integer<int>();           // - 2^31 .. 2^31 - 1
  /// bool g = rnd.boolean();               // P(g = true) = 0.5
  /// bool h = rnd.boolean(0.8);            // P(h = true) = 0.8
  ///\endcode
  ///
  /// LEMON provides a global instance of the random number
  /// generator which name is \ref lemon::rnd "rnd". Usually it is a
  /// good programming convenience to use this global generator to get
  /// random numbers.
  class Random {
  private:

    // Architecture word
    typedef unsigned long Word;

    _random_bits::RandomCore<Word> core;
    _random_bits::BoolProducer<Word> bool_producer;


  public:

    ///\name Initialization
    ///
    /// @{

    /// \brief Default constructor
    ///
    /// Constructor with constant seeding.
    Random() { core.initState(); }

    /// \brief Constructor with seed
    ///
    /// Constructor with seed. The current number type will be converted
    /// to the architecture word type.
    template <typename Number>
    Random(Number seed) {
      _random_bits::Initializer<Number, Word>::init(core, seed);
    }

    /// \brief Constructor with array seeding
    ///
    /// Constructor with array seeding. The given range should contain
    /// any number type and the numbers will be converted to the
    /// architecture word type.
    template <typename Iterator>
    Random(Iterator begin, Iterator end) {
      typedef typename std::iterator_traits<Iterator>::value_type Number;
      _random_bits::Initializer<Number, Word>::init(core, begin, end);
    }

    /// \brief Copy constructor
    ///
    /// Copy constructor. The generated sequence will be identical to
    /// the other sequence. It can be used to save the current state
    /// of the generator and later use it to generate the same
    /// sequence.
    Random(const Random& other) {
      core.copyState(other.core);
    }

    /// \brief Assign operator
    ///
    /// Assign operator. The generated sequence will be identical to
    /// the other sequence. It can be used to save the current state
    /// of the generator and later use it to generate the same
    /// sequence.
    Random& operator=(const Random& other) {
      if (&other != this) {
        core.copyState(other.core);
      }
      return *this;
    }

    /// \brief Seeding random sequence
    ///
    /// Seeding the random sequence. The current number type will be
    /// converted to the architecture word type.
    template <typename Number>
    void seed(Number seed) {
      _random_bits::Initializer<Number, Word>::init(core, seed);
    }

    /// \brief Seeding random sequence
    ///
    /// Seeding the random sequence. The given range should contain
    /// any number type and the numbers will be converted to the
    /// architecture word type.
    template <typename Iterator>
    void seed(Iterator begin, Iterator end) {
      typedef typename std::iterator_traits<Iterator>::value_type Number;
      _random_bits::Initializer<Number, Word>::init(core, begin, end);
    }

    /// \brief Seeding from file or from process id and time
    ///
    /// By default, this function calls the \c seedFromFile() member
    /// function with the <tt>/dev/urandom</tt> file. If it does not success,
    /// it uses the \c seedFromTime().
    /// \return Currently always \c true.
    bool seed() {
#ifndef WIN32
      if (seedFromFile("/dev/urandom", 0)) return true;
#endif
      if (seedFromTime()) return true;
      return false;
    }

    /// \brief Seeding from file
    ///
    /// Seeding the random sequence from file. The linux kernel has two
    /// devices, <tt>/dev/random</tt> and <tt>/dev/urandom</tt> which
    /// could give good seed values for pseudo random generators (The
    /// difference between two devices is that the <tt>random</tt> may
    /// block the reading operation while the kernel can give good
    /// source of randomness, while the <tt>urandom</tt> does not
    /// block the input, but it could give back bytes with worse
    /// entropy).
    /// \param file The source file
    /// \param offset The offset, from the file read.
    /// \return \c true when the seeding successes.
#ifndef WIN32
    bool seedFromFile(const std::string& file = "/dev/urandom", int offset = 0)
#else
    bool seedFromFile(const std::string& file = "", int offset = 0)
#endif
    {
      std::ifstream rs(file.c_str());
      const int size = 4;
      Word buf[size];
      if (offset != 0 && !rs.seekg(offset)) return false;
      if (!rs.read(reinterpret_cast<char*>(buf), sizeof(buf))) return false;
      seed(buf, buf + size);
      return true;
    }

    /// \brief Seding from process id and time
    ///
    /// Seding from process id and time. This function uses the
    /// current process id and the current time for initialize the
    /// random sequence.
    /// \return Currently always \c true.
    bool seedFromTime() {
#ifndef WIN32
      timeval tv;
      gettimeofday(&tv, 0);
      seed(getpid() + tv.tv_sec + tv.tv_usec);
#else
      seed(bits::getWinRndSeed());
#endif
      return true;
    }

    /// @}

    ///\name Uniform Distributions
    ///
    /// @{

    /// \brief Returns a random real number from the range [0, 1)
    ///
    /// It returns a random real number from the range [0, 1). The
    /// default Number type is \c double.
    template <typename Number>
    Number real() {
      return _random_bits::RealConversion<Number, Word>::convert(core);
    }

    double real() {
      return real<double>();
    }

    /// \brief Returns a random real number from the range [0, 1)
    ///
    /// It returns a random double from the range [0, 1).
    double operator()() {
      return real<double>();
    }

    /// \brief Returns a random real number from the range [0, b)
    ///
    /// It returns a random real number from the range [0, b).
    double operator()(double b) {
      return real<double>() * b;
    }

    /// \brief Returns a random real number from the range [a, b)
    ///
    /// It returns a random real number from the range [a, b).
    double operator()(double a, double b) {
      return real<double>() * (b - a) + a;
    }

    /// \brief Returns a random integer from a range
    ///
    /// It returns a random integer from the range {0, 1, ..., b - 1}.
    template <typename Number>
    Number integer(Number b) {
      return _random_bits::Mapping<Number, Word>::map(core, b);
    }

    /// \brief Returns a random integer from a range
    ///
    /// It returns a random integer from the range {a, a + 1, ..., b - 1}.
    template <typename Number>
    Number integer(Number a, Number b) {
      return _random_bits::Mapping<Number, Word>::map(core, b - a) + a;
    }

    /// \brief Returns a random integer from a range
    ///
    /// It returns a random integer from the range {0, 1, ..., b - 1}.
    template <typename Number>
    Number operator[](Number b) {
      return _random_bits::Mapping<Number, Word>::map(core, b);
    }

    /// \brief Returns a random non-negative integer
    ///
    /// It returns a random non-negative integer uniformly from the
    /// whole range of the current \c Number type. The default result
    /// type of this function is <tt>unsigned int</tt>.
    template <typename Number>
    Number uinteger() {
      return _random_bits::IntConversion<Number, Word>::convert(core);
    }

    unsigned int uinteger() {
      return uinteger<unsigned int>();
    }

    /// \brief Returns a random integer
    ///
    /// It returns a random integer uniformly from the whole range of
    /// the current \c Number type. The default result type of this
    /// function is \c int.
    template <typename Number>
    Number integer() {
      static const int nb = std::numeric_limits<Number>::digits +
        (std::numeric_limits<Number>::is_signed ? 1 : 0);
      return _random_bits::IntConversion<Number, Word, nb>::convert(core);
    }

    int integer() {
      return integer<int>();
    }

    /// \brief Returns a random bool
    ///
    /// It returns a random bool. The generator holds a buffer for
    /// random bits. Every time when it become empty the generator makes
    /// a new random word and fill the buffer up.
    bool boolean() {
      return bool_producer.convert(core);
    }

    /// @}

    ///\name Non-uniform Distributions
    ///
    ///@{

    /// \brief Returns a random bool with given probability of true result.
    ///
    /// It returns a random bool with given probability of true result.
    bool boolean(double p) {
      return operator()() < p;
    }

    /// Standard normal (Gauss) distribution

    /// Standard normal (Gauss) distribution.
    /// \note The Cartesian form of the Box-Muller
    /// transformation is used to generate a random normal distribution.
    double gauss()
    {
      double V1,V2,S;
      do {
        V1=2*real<double>()-1;
        V2=2*real<double>()-1;
        S=V1*V1+V2*V2;
      } while(S>=1);
      return std::sqrt(-2*std::log(S)/S)*V1;
    }
    /// Normal (Gauss) distribution with given mean and standard deviation

    /// Normal (Gauss) distribution with given mean and standard deviation.
    /// \sa gauss()
    double gauss(double mean,double std_dev)
    {
      return gauss()*std_dev+mean;
    }

    /// Lognormal distribution

    /// Lognormal distribution. The parameters are the mean and the standard
    /// deviation of <tt>exp(X)</tt>.
    ///
    double lognormal(double n_mean,double n_std_dev)
    {
      return std::exp(gauss(n_mean,n_std_dev));
    }
    /// Lognormal distribution

    /// Lognormal distribution. The parameter is an <tt>std::pair</tt> of
    /// the mean and the standard deviation of <tt>exp(X)</tt>.
    ///
    double lognormal(const std::pair<double,double> &params)
    {
      return std::exp(gauss(params.first,params.second));
    }
    /// Compute the lognormal parameters from mean and standard deviation

    /// This function computes the lognormal parameters from mean and
    /// standard deviation. The return value can direcly be passed to
    /// lognormal().
    std::pair<double,double> lognormalParamsFromMD(double mean,
                                                   double std_dev)
    {
      double fr=std_dev/mean;
      fr*=fr;
      double lg=std::log(1+fr);
      return std::pair<double,double>(std::log(mean)-lg/2.0,std::sqrt(lg));
    }
    /// Lognormal distribution with given mean and standard deviation

    /// Lognormal distribution with given mean and standard deviation.
    ///
    double lognormalMD(double mean,double std_dev)
    {
      return lognormal(lognormalParamsFromMD(mean,std_dev));
    }

    /// Exponential distribution with given mean

    /// This function generates an exponential distribution random number
    /// with mean <tt>1/lambda</tt>.
    ///
    double exponential(double lambda=1.0)
    {
      return -std::log(1.0-real<double>())/lambda;
    }

    /// Gamma distribution with given integer shape

    /// This function generates a gamma distribution random number.
    ///
    ///\param k shape parameter (<tt>k>0</tt> integer)
    double gamma(int k)
    {
      double s = 0;
      for(int i=0;i<k;i++) s-=std::log(1.0-real<double>());
      return s;
    }

    /// Gamma distribution with given shape and scale parameter

    /// This function generates a gamma distribution random number.
    ///
    ///\param k shape parameter (<tt>k>0</tt>)
    ///\param theta scale parameter
    ///
    double gamma(double k,double theta=1.0)
    {
      double xi,nu;
      const double delta = k-std::floor(k);
      const double v0=E/(E-delta);
      do {
        double V0=1.0-real<double>();
        double V1=1.0-real<double>();
        double V2=1.0-real<double>();
        if(V2<=v0)
          {
            xi=std::pow(V1,1.0/delta);
            nu=V0*std::pow(xi,delta-1.0);
          }
        else
          {
            xi=1.0-std::log(V1);
            nu=V0*std::exp(-xi);
          }
      } while(nu>std::pow(xi,delta-1.0)*std::exp(-xi));
      return theta*(xi+gamma(int(std::floor(k))));
    }

    /// Weibull distribution

    /// This function generates a Weibull distribution random number.
    ///
    ///\param k shape parameter (<tt>k>0</tt>)
    ///\param lambda scale parameter (<tt>lambda>0</tt>)
    ///
    double weibull(double k,double lambda)
    {
      return lambda*pow(-std::log(1.0-real<double>()),1.0/k);
    }

    /// Pareto distribution

    /// This function generates a Pareto distribution random number.
    ///
    ///\param k shape parameter (<tt>k>0</tt>)
    ///\param x_min location parameter (<tt>x_min>0</tt>)
    ///
    double pareto(double k,double x_min)
    {
      return exponential(gamma(k,1.0/x_min))+x_min;
    }

    /// Poisson distribution

    /// This function generates a Poisson distribution random number with
    /// parameter \c lambda.
    ///
    /// The probability mass function of this distribusion is
    /// \f[ \frac{e^{-\lambda}\lambda^k}{k!} \f]
    /// \note The algorithm is taken from the book of Donald E. Knuth titled
    /// ''Seminumerical Algorithms'' (1969). Its running time is linear in the
    /// return value.

    int poisson(double lambda)
    {
      const double l = std::exp(-lambda);
      int k=0;
      double p = 1.0;
      do {
        k++;
        p*=real<double>();
      } while (p>=l);
      return k-1;
    }

    ///@}

    ///\name Two Dimensional Distributions
    ///
    ///@{

    /// Uniform distribution on the full unit circle

    /// Uniform distribution on the full unit circle.
    ///
    dim2::Point<double> disc()
    {
      double V1,V2;
      do {
        V1=2*real<double>()-1;
        V2=2*real<double>()-1;

      } while(V1*V1+V2*V2>=1);
      return dim2::Point<double>(V1,V2);
    }
    /// A kind of two dimensional normal (Gauss) distribution

    /// This function provides a turning symmetric two-dimensional distribution.
    /// Both coordinates are of standard normal distribution, but they are not
    /// independent.
    ///
    /// \note The coordinates are the two random variables provided by
    /// the Box-Muller method.
    dim2::Point<double> gauss2()
    {
      double V1,V2,S;
      do {
        V1=2*real<double>()-1;
        V2=2*real<double>()-1;
        S=V1*V1+V2*V2;
      } while(S>=1);
      double W=std::sqrt(-2*std::log(S)/S);
      return dim2::Point<double>(W*V1,W*V2);
    }
    /// A kind of two dimensional exponential distribution

    /// This function provides a turning symmetric two-dimensional distribution.
    /// The x-coordinate is of conditionally exponential distribution
    /// with the condition that x is positive and y=0. If x is negative and
    /// y=0 then, -x is of exponential distribution. The same is true for the
    /// y-coordinate.
    dim2::Point<double> exponential2()
    {
      double V1,V2,S;
      do {
        V1=2*real<double>()-1;
        V2=2*real<double>()-1;
        S=V1*V1+V2*V2;
      } while(S>=1);
      double W=-std::log(S)/S;
      return dim2::Point<double>(W*V1,W*V2);
    }

    ///@}
  };


  extern Random rnd;

}

#endif
