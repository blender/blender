#include <cassert>
#include <cmath>
#include <vector>

namespace boost {
#if __cplusplus > 199711L
#  include <random>
typedef std::mt19937 mt19937;
#else
#  include <stdlib.h>
struct mt19937 {
  int operator()() {
    return rand();
  }

  int max() {
    return RAND_MAX;
  }
};
#endif

template<typename T>
struct uniform_on_sphere {
  typedef std::vector<T> result_type;

  uniform_on_sphere(int dimension) {
    assert(dimension == 3);
  }

  std::vector<T>
  operator()(float u1, float u2) {
    T z = 1.0 - 2.0*u1;
    T r = std::sqrt(std::max(0.0, 1.0 - z*z));
    T phi = 2.0*M_PI*u2;
    T x = r*std::cos(phi);
    T y = r*std::sin(phi);
    std::vector<T> result;
    result.push_back(x);
    result.push_back(y);
    result.push_back(z);
    return result;
  }
};

template<typename RNG, typename DISTR>
struct variate_generator {

  variate_generator(RNG rng, DISTR distr)
    : rng_(rng), distr_(distr) {}

  typename DISTR::result_type
  operator()() {
    float rng_max_inv = 1.0 / rng_.max();
    return distr_(rng_() * rng_max_inv, rng_() * rng_max_inv);
  }

  RNG rng_;
  DISTR distr_;
};

}
