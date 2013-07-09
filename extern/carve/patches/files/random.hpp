#pragma once

#include <iostream>
#include <vector>
#include <limits>
#include <stdexcept>
#include <cmath>
#include <algorithm>

#if !defined(_MSC_VER)
#include <stdint.h>
#endif

namespace boost {

// type_traits could help here, but I don't want to depend on type_traits.
template<class T>
struct ptr_helper
{
  typedef T value_type;
  typedef T& reference_type;
  typedef const T& rvalue_type;
  static reference_type ref(T& r) { return r; }
  static const T& ref(const T& r) { return r; }
};

template<class T>
struct ptr_helper<T&>
{
  typedef T value_type;
  typedef T& reference_type;
  typedef T& rvalue_type;
  static reference_type ref(T& r) { return r; }
  static const T& ref(const T& r) { return r; }
};

template<class T>
struct ptr_helper<T*>
{
  typedef T value_type;
  typedef T& reference_type;
  typedef T* rvalue_type;
  static reference_type ref(T * p) { return *p; }
  static const T& ref(const T * p) { return *p; }
};

template<class UniformRandomNumberGenerator>
class pass_through_engine
{
private:
  typedef ptr_helper<UniformRandomNumberGenerator> helper_type;

public:
  typedef typename helper_type::value_type base_type;
  typedef typename base_type::result_type result_type;

  explicit pass_through_engine(UniformRandomNumberGenerator rng)
    // make argument an rvalue to avoid matching Generator& constructor
    : _rng(static_cast<typename helper_type::rvalue_type>(rng))
  { }

  result_type min () const { return (base().min)(); }
  result_type max () const { return (base().max)(); }
  base_type& base() { return helper_type::ref(_rng); }
  const base_type& base() const { return helper_type::ref(_rng); }

  result_type operator()() { return base()(); }

private:
  UniformRandomNumberGenerator _rng;
};

template<class RealType>
class new_uniform_01
{
public:
  typedef RealType input_type;
  typedef RealType result_type;
  // compiler-generated copy ctor and copy assignment are fine
  result_type min () const { return result_type(0); }
  result_type max () const { return result_type(1); }
  void reset() { }

  template<class Engine>
  result_type operator()(Engine& eng) {
    for (;;) {
      typedef typename Engine::result_type base_result;
      result_type factor = result_type(1) /
              (result_type((eng.max)()-(eng.min)()) +
               result_type(std::numeric_limits<base_result>::is_integer ? 1 : 0));
      result_type result = result_type(eng() - (eng.min)()) * factor;
      if (result < result_type(1))
        return result;
    }
  }

  template<class CharT, class Traits>
  friend std::basic_ostream<CharT,Traits>&
  operator<<(std::basic_ostream<CharT,Traits>& os, const new_uniform_01&)
  {
    return os;
  }

  template<class CharT, class Traits>
  friend std::basic_istream<CharT,Traits>&
  operator>>(std::basic_istream<CharT,Traits>& is, new_uniform_01&)
  {
    return is;
  }
};

template<class UniformRandomNumberGenerator, class RealType>
class backward_compatible_uniform_01
{
  typedef ptr_helper<UniformRandomNumberGenerator> traits;
  typedef pass_through_engine<UniformRandomNumberGenerator> internal_engine_type;
public:
  typedef UniformRandomNumberGenerator base_type;
  typedef RealType result_type;

  static const bool has_fixed_range = false;

  explicit backward_compatible_uniform_01(typename traits::rvalue_type rng)
    : _rng(rng),
      _factor(result_type(1) /
              (result_type((_rng.max)()-(_rng.min)()) +
               result_type(std::numeric_limits<base_result>::is_integer ? 1 : 0)))
  {
  }
  // compiler-generated copy ctor and copy assignment are fine

  result_type min () const { return result_type(0); }
  result_type max () const { return result_type(1); }
  typename traits::value_type& base() { return _rng.base(); }
  const typename traits::value_type& base() const { return _rng.base(); }
  void reset() { }

  result_type operator()() {
    for (;;) {
      result_type result = result_type(_rng() - (_rng.min)()) * _factor;
      if (result < result_type(1))
        return result;
    }
  }

  template<class CharT, class Traits>
  friend std::basic_ostream<CharT,Traits>&
  operator<<(std::basic_ostream<CharT,Traits>& os, const backward_compatible_uniform_01& u)
  {
    os << u._rng;
    return os;
  }

  template<class CharT, class Traits>
  friend std::basic_istream<CharT,Traits>&
  operator>>(std::basic_istream<CharT,Traits>& is, backward_compatible_uniform_01& u)
  {
    is >> u._rng;
    return is;
  }

private:
  typedef typename internal_engine_type::result_type base_result;
  internal_engine_type _rng;
  result_type _factor;
};

//  A definition is required even for integral static constants
template<class UniformRandomNumberGenerator, class RealType>
const bool backward_compatible_uniform_01<UniformRandomNumberGenerator, RealType>::has_fixed_range;

template<class UniformRandomNumberGenerator>
struct select_uniform_01
{
  template<class RealType>
  struct apply
  {
    typedef backward_compatible_uniform_01<UniformRandomNumberGenerator, RealType> type;
  };
};

template<>
struct select_uniform_01<float>
{
  template<class RealType>
  struct apply
  {
    typedef new_uniform_01<float> type;
  };
};

template<>
struct select_uniform_01<double>
{
  template<class RealType>
  struct apply
  {
    typedef new_uniform_01<double> type;
  };
};

template<>
struct select_uniform_01<long double>
{
  template<class RealType>
  struct apply
  {
    typedef new_uniform_01<long double> type;
  };
};

// Because it is so commonly used: uniform distribution on the real [0..1)
// range.  This allows for specializations to avoid a costly int -> float
// conversion plus float multiplication
template<class UniformRandomNumberGenerator = double, class RealType = double>
class uniform_01
  : public select_uniform_01<UniformRandomNumberGenerator>::template apply<RealType>::type
{
  typedef typename select_uniform_01<UniformRandomNumberGenerator>::template apply<RealType>::type impl_type;
  typedef ptr_helper<UniformRandomNumberGenerator> traits;
public:

  uniform_01() {}

  explicit uniform_01(typename traits::rvalue_type rng)
    : impl_type(rng)
  {
  }

  template<class CharT, class Traits>
  friend std::basic_ostream<CharT,Traits>&
  operator<<(std::basic_ostream<CharT,Traits>& os, const uniform_01& u)
  {
    os << static_cast<const impl_type&>(u);
    return os;
  }

  template<class CharT, class Traits>
  friend std::basic_istream<CharT,Traits>&
  operator>>(std::basic_istream<CharT,Traits>& is, uniform_01& u)
  {
    is >> static_cast<impl_type&>(u);
    return is;
  }
};

template<class UniformRandomNumberGenerator, class IntType = unsigned long>
class uniform_int_float
{
public:
  typedef UniformRandomNumberGenerator base_type;
  typedef IntType result_type;

  uniform_int_float(base_type rng, IntType min_arg = 0, IntType max_arg = 0xffffffff)
    : _rng(rng), _min(min_arg), _max(max_arg)
  {
    init();
  }

  result_type min () const { return _min; }
  result_type max () const { return _max; }
  base_type& base() { return _rng.base(); }
  const base_type& base() const { return _rng.base(); }

  result_type operator()()
  {
    return static_cast<IntType>(_rng() * _range) + _min;
  }

  template<class CharT, class Traits>
  friend std::basic_ostream<CharT,Traits>&
  operator<<(std::basic_ostream<CharT,Traits>& os, const uniform_int_float& ud)
  {
    os << ud._min << " " << ud._max;
    return os;
  }

  template<class CharT, class Traits>
  friend std::basic_istream<CharT,Traits>&
  operator>>(std::basic_istream<CharT,Traits>& is, uniform_int_float& ud)
  {
    is >> std::ws >> ud._min >> std::ws >> ud._max;
    ud.init();
    return is;
  }

private:
  void init()
  {
    _range = static_cast<base_result>(_max-_min)+1;
  }

  typedef typename base_type::result_type base_result;
  uniform_01<base_type> _rng;
  result_type _min, _max;
  base_result _range;
};


template<class UniformRandomNumberGenerator, class CharT, class Traits>
std::basic_ostream<CharT,Traits>&
operator<<(
    std::basic_ostream<CharT,Traits>& os
    , const pass_through_engine<UniformRandomNumberGenerator>& ud
    )
{
    return os << ud.base();
}

template<class UniformRandomNumberGenerator, class CharT, class Traits>
std::basic_istream<CharT,Traits>&
operator>>(
    std::basic_istream<CharT,Traits>& is
    , const pass_through_engine<UniformRandomNumberGenerator>& ud
    )
{
    return is >> ud.base();
}



template<class RealType = double>
class normal_distribution
{
public:
  typedef RealType input_type;
  typedef RealType result_type;

  explicit normal_distribution(const result_type& mean_arg = result_type(0),
                               const result_type& sigma_arg = result_type(1))
    : _mean(mean_arg), _sigma(sigma_arg), _valid(false)
  {
    //assert(_sigma >= result_type(0));
  }

  // compiler-generated copy constructor is NOT fine, need to purge cache
  normal_distribution(const normal_distribution& other)
    : _mean(other._mean), _sigma(other._sigma), _valid(false)
  {
  }

  // compiler-generated copy ctor and assignment operator are fine

  RealType mean() const { return _mean; }
  RealType sigma() const { return _sigma; }

  void reset() { _valid = false; }

  template<class Engine>
  result_type operator()(Engine& eng)
  {
#ifndef BOOST_NO_STDC_NAMESPACE
    // allow for Koenig lookup
    using std::sqrt; using std::log; using std::sin; using std::cos;
#endif
    if(!_valid) {
      _r1 = eng();
      _r2 = eng();
      _cached_rho = sqrt(-result_type(2) * log(result_type(1)-_r2));
      _valid = true;
    } else {
      _valid = false;
    }
    // Can we have a boost::mathconst please?
    const result_type pi = result_type(3.14159265358979323846);
    
    return _cached_rho * (_valid ?
                          cos(result_type(2)*pi*_r1) :
                          sin(result_type(2)*pi*_r1))
      * _sigma + _mean;
  }

#ifndef BOOST_RANDOM_NO_STREAM_OPERATORS
  template<class CharT, class Traits>
  friend std::basic_ostream<CharT,Traits>&
  operator<<(std::basic_ostream<CharT,Traits>& os, const normal_distribution& nd)
  {
    os << nd._mean << " " << nd._sigma << " "
       << nd._valid << " " << nd._cached_rho << " " << nd._r1;
    return os;
  }

  template<class CharT, class Traits>
  friend std::basic_istream<CharT,Traits>&
  operator>>(std::basic_istream<CharT,Traits>& is, normal_distribution& nd)
  {
    is >> std::ws >> nd._mean >> std::ws >> nd._sigma
       >> std::ws >> nd._valid >> std::ws >> nd._cached_rho
       >> std::ws >> nd._r1;
    return is;
  }
#endif
private:
  result_type _mean, _sigma;
  result_type _r1, _r2, _cached_rho;
  bool _valid;
};

// http://www.math.keio.ac.jp/matumoto/emt.html
template<class UIntType, int w, int n, int m, int r, UIntType a, int u,
  int s, UIntType b, int t, UIntType c, int l, UIntType val>
class mersenne_twister
{
public:
  typedef UIntType result_type;
  static const int word_size = w;
  static const int state_size = n;
  static const int shift_size = m;
  static const int mask_bits = r;
  static const UIntType parameter_a = a;
  static const int output_u = u;
  static const int output_s = s;
  static const UIntType output_b = b;
  static const int output_t = t;
  static const UIntType output_c = c;
  static const int output_l = l;

  static const bool has_fixed_range = false;

  mersenne_twister() { seed(); }

  explicit mersenne_twister(const UIntType& value)
  { seed(value); }
  template<class It> mersenne_twister(It& first, It last) { seed(first,last); }

  template<class Generator>                                           \
  explicit mersenne_twister(Generator& gen)
  { seed(gen); }

  // compiler-generated copy ctor and assignment operator are fine

  void seed() { seed(UIntType(5489)); }

  void seed(const UIntType& value)
  {
    // New seeding algorithm from 
    // http://www.math.sci.hiroshima-u.ac.jp/~m-mat/MT/MT2002/emt19937ar.html
    // In the previous versions, MSBs of the seed affected only MSBs of the
    // state x[].
    const UIntType mask = ~0u;
    x[0] = value & mask;
    for (i = 1; i < n; i++) {
      // See Knuth "The Art of Computer Programming" Vol. 2, 3rd ed., page 106
      x[i] = (1812433253UL * (x[i-1] ^ (x[i-1] >> (w-2))) + i) & mask;
    }
  }

  // For GCC, moving this function out-of-line prevents inlining, which may
  // reduce overall object code size.  However, MSVC does not grok
  // out-of-line definitions of member function templates.
  template<class Generator>                                       \
  void seed(Generator& gen)
  {
/*#ifndef BOOST_NO_LIMITS_COMPILE_TIME_CONSTANTS
    BOOST_STATIC_ASSERT(!std::numeric_limits<result_type>::is_signed);
#endif*/
    // I could have used std::generate_n, but it takes "gen" by value
    for(int j = 0; j < n; j++)
      x[j] = gen();
    i = n;
  }

  template<class It>
  void seed(It& first, It last)
  {
    int j;
    for(j = 0; j < n && first != last; ++j, ++first)
      x[j] = *first;
    i = n;
    if(first == last && j < n)
      throw std::invalid_argument("mersenne_twister::seed");
  }
  
  result_type min () const { return 0; }
  result_type max () const
  {
    // avoid "left shift count >= with of type" warning
    result_type res = 0;
    for(int j = 0; j < w; ++j)
      res |= (1u << j);
    return res;
  }

  result_type operator()();
  static bool validation(result_type v) { return val == v; }

  template<class CharT, class Traits>
  friend std::basic_ostream<CharT,Traits>&
  operator<<(std::basic_ostream<CharT,Traits>& os, const mersenne_twister& mt)
  {
    for(int j = 0; j < mt.state_size; ++j)
      os << mt.compute(j) << " ";
    return os;
  }

  template<class CharT, class Traits>
  friend std::basic_istream<CharT,Traits>&
  operator>>(std::basic_istream<CharT,Traits>& is, mersenne_twister& mt)
  {
    for(int j = 0; j < mt.state_size; ++j)
      is >> mt.x[j] >> std::ws;
    // MSVC (up to 7.1) and Borland (up to 5.64) don't handle the template
    // value parameter "n" available from the class template scope, so use
    // the static constant with the same value
    mt.i = mt.state_size;
    return is;
  }

  friend bool operator==(const mersenne_twister& x, const mersenne_twister& y)
  {
    for(int j = 0; j < state_size; ++j)
      if(x.compute(j) != y.compute(j))
        return false;
    return true;
  }

  friend bool operator!=(const mersenne_twister& x, const mersenne_twister& y)
  { return !(x == y); }

private:
  // returns x(i-n+index), where index is in 0..n-1
  UIntType compute(unsigned int index) const
  {
    // equivalent to (i-n+index) % 2n, but doesn't produce negative numbers
    return x[ (i + n + index) % (2*n) ];
  }
  void twist(int block);

  // state representation: next output is o(x(i))
  //   x[0]  ... x[k] x[k+1] ... x[n-1]     x[n]     ... x[2*n-1]   represents
  //  x(i-k) ... x(i) x(i+1) ... x(i-k+n-1) x(i-k-n) ... x[i(i-k-1)]
  // The goal is to always have x(i-n) ... x(i-1) available for
  // operator== and save/restore.

  UIntType x[2*n]; 
  int i;
};

//  A definition is required even for integral static constants

template<class UIntType, int w, int n, int m, int r, UIntType a, int u,
  int s, UIntType b, int t, UIntType c, int l, UIntType val>
const bool mersenne_twister<UIntType,w,n,m,r,a,u,s,b,t,c,l,val>::has_fixed_range;
template<class UIntType, int w, int n, int m, int r, UIntType a, int u,
  int s, UIntType b, int t, UIntType c, int l, UIntType val>
const int mersenne_twister<UIntType,w,n,m,r,a,u,s,b,t,c,l,val>::state_size;
template<class UIntType, int w, int n, int m, int r, UIntType a, int u,
  int s, UIntType b, int t, UIntType c, int l, UIntType val>
const int mersenne_twister<UIntType,w,n,m,r,a,u,s,b,t,c,l,val>::shift_size;
template<class UIntType, int w, int n, int m, int r, UIntType a, int u,
  int s, UIntType b, int t, UIntType c, int l, UIntType val>
const int mersenne_twister<UIntType,w,n,m,r,a,u,s,b,t,c,l,val>::mask_bits;
template<class UIntType, int w, int n, int m, int r, UIntType a, int u,
  int s, UIntType b, int t, UIntType c, int l, UIntType val>
const UIntType mersenne_twister<UIntType,w,n,m,r,a,u,s,b,t,c,l,val>::parameter_a;
template<class UIntType, int w, int n, int m, int r, UIntType a, int u,
  int s, UIntType b, int t, UIntType c, int l, UIntType val>
const int mersenne_twister<UIntType,w,n,m,r,a,u,s,b,t,c,l,val>::output_u;
template<class UIntType, int w, int n, int m, int r, UIntType a, int u,
  int s, UIntType b, int t, UIntType c, int l, UIntType val>
const int mersenne_twister<UIntType,w,n,m,r,a,u,s,b,t,c,l,val>::output_s;
template<class UIntType, int w, int n, int m, int r, UIntType a, int u,
  int s, UIntType b, int t, UIntType c, int l, UIntType val>
const UIntType mersenne_twister<UIntType,w,n,m,r,a,u,s,b,t,c,l,val>::output_b;
template<class UIntType, int w, int n, int m, int r, UIntType a, int u,
  int s, UIntType b, int t, UIntType c, int l, UIntType val>
const int mersenne_twister<UIntType,w,n,m,r,a,u,s,b,t,c,l,val>::output_t;
template<class UIntType, int w, int n, int m, int r, UIntType a, int u,
  int s, UIntType b, int t, UIntType c, int l, UIntType val>
const UIntType mersenne_twister<UIntType,w,n,m,r,a,u,s,b,t,c,l,val>::output_c;
template<class UIntType, int w, int n, int m, int r, UIntType a, int u,
  int s, UIntType b, int t, UIntType c, int l, UIntType val>
const int mersenne_twister<UIntType,w,n,m,r,a,u,s,b,t,c,l,val>::output_l;

template<class UIntType, int w, int n, int m, int r, UIntType a, int u,
  int s, UIntType b, int t, UIntType c, int l, UIntType val>
void mersenne_twister<UIntType,w,n,m,r,a,u,s,b,t,c,l,val>::twist(int block)
{
  const UIntType upper_mask = (~0u) << r;
  const UIntType lower_mask = ~upper_mask;

  if(block == 0) {
    for(int j = n; j < 2*n; j++) {
      UIntType y = (x[j-n] & upper_mask) | (x[j-(n-1)] & lower_mask);
      x[j] = x[j-(n-m)] ^ (y >> 1) ^ (y&1 ? a : 0);
    }
  } else if (block == 1) {
    // split loop to avoid costly modulo operations
    {  // extra scope for MSVC brokenness w.r.t. for scope
      for(int j = 0; j < n-m; j++) {
        UIntType y = (x[j+n] & upper_mask) | (x[j+n+1] & lower_mask);
        x[j] = x[j+n+m] ^ (y >> 1) ^ (y&1 ? a : 0);
      }
    }
    
    for(int j = n-m; j < n-1; j++) {
      UIntType y = (x[j+n] & upper_mask) | (x[j+n+1] & lower_mask);
      x[j] = x[j-(n-m)] ^ (y >> 1) ^ (y&1 ? a : 0);
    }
    // last iteration
    UIntType y = (x[2*n-1] & upper_mask) | (x[0] & lower_mask);
    x[n-1] = x[m-1] ^ (y >> 1) ^ (y&1 ? a : 0);
    i = 0;
  }
}

template<class UIntType, int w, int n, int m, int r, UIntType a, int u,
  int s, UIntType b, int t, UIntType c, int l, UIntType val>
inline typename mersenne_twister<UIntType,w,n,m,r,a,u,s,b,t,c,l,val>::result_type
mersenne_twister<UIntType,w,n,m,r,a,u,s,b,t,c,l,val>::operator()()
{
  if(i == n)
    twist(0);
  else if(i >= 2*n)
    twist(1);
  // Step 4
  UIntType z = x[i];
  ++i;
  z ^= (z >> u);
  z ^= ((z << s) & b);
  z ^= ((z << t) & c);
  z ^= (z >> l);
  return z;
}

typedef mersenne_twister<uint32_t,32,351,175,19,0xccab8ee7,11,
  7,0x31b6ab00,15,0xffe50000,17, 0xa37d3c92> mt11213b;

// validation by experiment from mt19937.c
typedef mersenne_twister<uint32_t,32,624,397,31,0x9908b0df,11,
  7,0x9d2c5680,15,0xefc60000,18, 3346425566U> mt19937;

  
template<class RealType = double, class Cont = std::vector<RealType> >
class uniform_on_sphere
{
public:
  typedef RealType input_type;
  typedef Cont result_type;

  explicit uniform_on_sphere(int dim = 2) : _container(dim), _dim(dim) { }

  // compiler-generated copy ctor and assignment operator are fine

  void reset() { _normal.reset(); }

  template<class Engine>
  const result_type & operator()(Engine& eng)
  {
    RealType sqsum = 0;
    for(typename Cont::iterator it = _container.begin();
        it != _container.end();
        ++it) {
      RealType val = _normal(eng);
      *it = val;
      sqsum += val * val;
    }
    using std::sqrt;
    // for all i: result[i] /= sqrt(sqsum)
    std::transform(_container.begin(), _container.end(), _container.begin(),
                   std::bind2nd(std::divides<RealType>(), sqrt(sqsum)));
    return _container;
  }

  template<class CharT, class Traits>
  friend std::basic_ostream<CharT,Traits>&
  operator<<(std::basic_ostream<CharT,Traits>& os, const uniform_on_sphere& sd)
  {
    os << sd._dim;
    return os;
  }

  template<class CharT, class Traits>
  friend std::basic_istream<CharT,Traits>&
  operator>>(std::basic_istream<CharT,Traits>& is, uniform_on_sphere& sd)
  {
    is >> std::ws >> sd._dim;
    sd._container.resize(sd._dim);
    return is;
  }

private:
  normal_distribution<RealType> _normal;
  result_type _container;
  int _dim;
};



template<bool have_int, bool want_int>
struct engine_helper;


template<>
struct engine_helper<true, true>
{
  template<class Engine, class DistInputType>
  struct impl
  {
    typedef pass_through_engine<Engine> type;
  };
};

template<>
struct engine_helper<false, false>
{
  template<class Engine, class DistInputType>
  struct impl
  {
    typedef uniform_01<Engine, DistInputType> type;
  };
};

template<>
struct engine_helper<true, false>
{
  template<class Engine, class DistInputType>
  struct impl
  {
    typedef uniform_01<Engine, DistInputType> type;
  };
};

template<>
struct engine_helper<false, true>
{
  template<class Engine, class DistInputType>
  struct impl
  {
    typedef uniform_int_float<Engine, unsigned long> type;
  };
};

template<class Engine, class Distribution>
class variate_generator
{
private:
  typedef pass_through_engine<Engine> decorated_engine;

public:
  typedef typename decorated_engine::base_type engine_value_type;
  typedef Engine engine_type;
  typedef Distribution distribution_type;
  typedef typename Distribution::result_type result_type;

  variate_generator(Engine e, Distribution d)
    : _eng(decorated_engine(e)), _dist(d) { }

  result_type operator()() { return _dist(_eng); }
  template<class T>
  result_type operator()(T value) { return _dist(_eng, value); }

  engine_value_type& engine() { return _eng.base().base(); }
  const engine_value_type& engine() const { return _eng.base().base(); }

  distribution_type& distribution() { return _dist; }
  const distribution_type& distribution() const { return _dist; }

  result_type min () const { return (distribution().min)(); }
  result_type max () const { return (distribution().max)(); }

private:
  enum {
    have_int = std::numeric_limits<typename decorated_engine::result_type>::is_integer,
    want_int = std::numeric_limits<typename Distribution::input_type>::is_integer
  };
  typedef typename engine_helper<have_int, want_int>::template impl<decorated_engine, typename Distribution::input_type>::type internal_engine_type;

  internal_engine_type _eng;
  distribution_type _dist;
};

} // namespace boost
