/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <optional>

#include "BLI_array.hh"
#include "BLI_cache_mutex.hh"
#include "BLI_hash.hh"

#include "BKE_sound.hh"

namespace blender::bke {

/**
 * This class allows efficiently sampling an arbitrary frequency range at an arbitrary point in
 * time. This is achieved by caching the result of the fourier transform for various windows and
 * interpolating between the cached values.
 *
 * Used by the Sample Sound node in Geometry Nodes.
 */
class bSoundFrequencySampler {
 public:
  /**
   * Window function that is applied before computing the FFT. It avoids spectral leakage and
   * generally leads to better results. Using #Rectangular mode is the same as not using any window
   * function.
   */
  enum class WindowFunction {
    Hann,
    Hamming,
    Blackman,
    Rectangular,
  };

  enum class InterpolationMethod {
    /**
     * Fastest method but a resulting spectrum visualization may not be continuous, especially for
     * lower frequencies.
     */
    Linear,
    /** Slower but produces a continuous spectrum. It may not be entirely smooth though. */
    CatmullRom,
    /** Yet slower but produces a continuous spectrum that is smooth. */
    BSpline,
  };

  struct Key {
    WindowFunction window_function;
    /** This has to be a power of two. */
    int fft_size;
    /** If nullopt, the all channels are mixed. */
    std::optional<int> channel;

    uint64_t hash() const
    {
      return get_default_hash(this->window_function, this->fft_size, this->channel.value_or(-1));
    }

    friend bool operator==(const Key &a, const Key &b) = default;
  };

  /** Precomputed weights for a specific window function and FFT size. */
  struct WindowWeights {
    Array<float> weights;
    float weights_sum;
  };

 private:
  /** Cache for a single window. The frequencies are computed lazily when necessary. */
  struct WindowCache {
    mutable CacheMutex mutex;
    /**
     * Stores the amplitudes of the individual frequencies in this window using a prefix-sum. This
     * allows constant time lookup for a range of frequencies.
     */
    mutable std::optional<Array<float, 0>> cumulative_amplitudes;
  };

  AUD_Sound sound_;
  Key key_;
  /** Derived from the sound. */
  int samples_per_second_;
  /**
   * Determines the offset of one window to the next in samples.
   *
   * The larger the value, the fewer FFTs have to be computed resulting in faster playback at the
   * cost of resolution on the time domain. Small values also increase the memory usage.
   */
  int window_cache_stride_;
  Array<WindowCache> window_caches_;

  /** Cached weights of the selected window function. */
  const WindowWeights &window_weights_;

 public:
  /** Construct a new sampler, prefer using #get_cached instead. */
  bSoundFrequencySampler(AUD_Sound sound, const Key &key);

  /** Access a reusable frequency sampler for the given sound.  */
  static const bSoundFrequencySampler *get_cached(const bSound &sound, const Key &key);

  /** Sample the amplitude a the given time and frequency range. */
  float sample(float time,
               float low,
               float high,
               InterpolationMethod time_interpolation,
               InterpolationMethod frequency_interpolation) const;

 private:
  float sample_frequency_range_in_window(int window_i,
                                         float low,
                                         float high,
                                         InterpolationMethod method) const;
  float sample_cumulative_frequency(Span<float> window_values,
                                    float frequency,
                                    InterpolationMethod method) const;
  std::optional<Span<float>> ensure_window_cache(int window_i) const;
  std::optional<Array<float>> compute_fft(int start_sample) const;
};

}  // namespace blender::bke
