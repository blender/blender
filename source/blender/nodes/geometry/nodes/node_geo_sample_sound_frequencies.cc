/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_sound_sample.hh"

#include "NOD_socket_usage_inference.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_sample_sound_frequencies_cc {

enum class FFTSize {
  _128 = 128,
  _256 = 256,
  _512 = 512,
  _1024 = 1024,
  _2048 = 2048,
  _4096 = 4096,
  _8192 = 8192,
  _16384 = 16384,
  _32768 = 32768,
};

enum class WindowFunction {
  Hann = 0,
  Hamming = 1,
  Blackman = 2,
  Rectangular = 3,
};

static const EnumPropertyItem fft_size_items[] = {
    {int(FFTSize::_128), "128", 0, "128", ""},
    {int(FFTSize::_256), "256", 0, "256", ""},
    {int(FFTSize::_512), "512", 0, "512", ""},
    {int(FFTSize::_1024), "1024", 0, "1024", ""},
    {int(FFTSize::_2048), "2048", 0, "2048", ""},
    {int(FFTSize::_4096), "4096", 0, "4096", ""},
    {int(FFTSize::_8192), "8192", 0, "8192", ""},
    {int(FFTSize::_16384), "16384", 0, "16384", ""},
    {int(FFTSize::_32768), "32768", 0, "32768", ""},
    {},
};

static const EnumPropertyItem window_function_items[] = {
    {int(WindowFunction::Hann), "Hann", 0, "Hann", ""},
    {int(WindowFunction::Hamming), "Hamming", 0, "Hamming", ""},
    {int(WindowFunction::Blackman), "Blackman", 0, "Blackman", ""},
    {int(WindowFunction::Rectangular),
     "Rectangular",
     0,
     "Rectangular",
     "Equivalent to having no window function"},
    {},
};

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();

  b.add_output<decl::Float>("Amplitude"_ustr)
      .reference_pass_all()
      .description("Sum of amplitudes of the frequencies in the given range")
      .structure_type(StructureType::Dynamic);
  b.add_input<decl::Sound>("Sound"_ustr).optional_label().description("Sound to sample");
  b.add_input<decl::Float>("Time"_ustr)
      .subtype(PROP_TIME_ABSOLUTE)
      .supports_field()
      .structure_type(StructureType::Dynamic)
      .description("Time in seconds of the sound to sample at");
  b.add_input<decl::Bool>("All Channels"_ustr)
      .default_value(true)
      .supports_field()
      .structure_type(StructureType::Dynamic)
      .description("Mix all channels before sampling the sound (e.g. stereo to mono)");
  b.add_input<decl::Int>("Channel"_ustr)
      .min(0)
      .usage_inference(
          [](const socket_usage_inference::SocketUsageParams &params) -> std::optional<bool> {
            if (const std::optional<bool> any_output_used = params.any_output_is_used()) {
              if (!*any_output_used) {
                return false;
              }
            }
            else {
              return std::nullopt;
            }
            const std::optional<bool> all_channels =
                params.get_input("All Channels"_ustr).get_if_primitive<bool>();
            if (!all_channels.has_value()) {
              return true;
            }
            return !*all_channels;
          })
      .supports_field()
      .structure_type(StructureType::Dynamic)
      .description("The channel to sample unless 'All Channels' is checked");
  b.add_input<decl::Float>("Low"_ustr)
      .subtype(PROP_FREQUENCY)
      .default_value(0.0f)
      .min(0.0f)
      .supports_field()
      .structure_type(StructureType::Dynamic)
      .description("Lower bound of the sampled frequency range");
  b.add_input<decl::Float>("High"_ustr)
      .subtype(PROP_FREQUENCY)
      .default_value(10'000.0f)
      .min(0.0f)
      .supports_field()
      .structure_type(StructureType::Dynamic)
      .description("Upper bound of the sampled frequency range");

  {
    auto &p = b.add_panel("FFT"_ustr)
                  .default_closed(true)
                  .description("Configure details of the fourier transformation");
    p.add_input<decl::Menu>("FFT Size"_ustr)
        .static_items(fft_size_items)
        .default_value(FFTSize::_4096)
        .optional_label()
        .description(
            "Number of samples to process in the discrete fourier transformation at once. Higher "
            "values have higher frequency but lower time resolution and vice versa");
    p.add_input<decl::Menu>("Window Function"_ustr)
        .static_items(window_function_items)
        .default_value(WindowFunction::Hann)
        .optional_label()
        .description(
            "Applies a tapering function to the windowed samples to minimize discontinuities at "
            "the edges, improving frequency resolution and reducing artifacts");
  }
}

class SampleSoundFunction : public mf::MultiFunction {
 private:
  const bSound &sound_;
  const int fft_size_;
  const bke::bSoundFrequencySampler::WindowFunction window_function_;

 public:
  SampleSoundFunction(bSound &sound,
                      const int fft_size,
                      const bke::bSoundFrequencySampler::WindowFunction window_function)
      : sound_(sound), fft_size_(fft_size), window_function_(window_function)
  {
    static const mf::Signature signature = []() {
      mf::Signature signature;
      mf::SignatureBuilder builder("Sample Sound", signature);
      builder.single_input<float>("Time");
      builder.single_input<bool>("All Channels");
      builder.single_input<int>("Channel");
      builder.single_input<float>("Low");
      builder.single_input<float>("High");
      builder.single_output<float>("Amplitude");
      return signature;
    }();
    this->set_signature(&signature);
    BLI_assert(is_power_of_2(fft_size_));
  }

  void call(const IndexMask &mask, mf::Params params, mf::Context /*context*/) const override
  {
    const VArray<float> &times = params.readonly_single_input<float>(0, "Time");
    const VArray<bool> &all_channels_varray = params.readonly_single_input<bool>(1,
                                                                                 "All Channels");
    const VArray<int> &channels = params.readonly_single_input<int>(2, "Channel");
    const VArray<float> &lows = params.readonly_single_input<float>(3, "Low");
    const VArray<float> &highs = params.readonly_single_input<float>(4, "High");
    MutableSpan<float> amplitudes = params.uninitialized_single_output<float>(5, "Amplitude");

    const std::optional<bool> all_channels_value = all_channels_varray.get_if_single();
    const std::optional<float> channel_value = channels.get_if_single();
    const bool constant_channel = all_channels_value == true ||
                                  (all_channels_value.has_value() && channel_value.has_value());

    const auto time_interpolation = bke::bSoundFrequencySampler::InterpolationMethod::BSpline;
    const auto frequency_interpolation = bke::bSoundFrequencySampler::InterpolationMethod::BSpline;

    /* Optimize the case when all indices sample the same channel. */
    if (constant_channel) {
      bke::bSoundFrequencySampler::Key key;
      key.window_function = window_function_;
      key.fft_size = fft_size_;
      key.channel = *all_channels_value ? std::nullopt : channel_value;
      const bke::bSoundFrequencySampler *sampler = bke::bSoundFrequencySampler::get_cached(sound_,
                                                                                           key);
      if (!sampler) {
        index_mask::masked_fill(amplitudes, 0.0f, mask);
        return;
      }
      mask.foreach_index([&](const int i) {
        const float time = times[i];
        const float low = lows[i];
        const float high = highs[i];
        const float amplitude = sampler->sample(
            time, low, high, time_interpolation, frequency_interpolation);
        amplitudes[i] = amplitude;
      });
      return;
    }

    /* Sort indices by channel, optimizing for the common case that the channel number is typically
     * very low. */
    constexpr int channel_array_size = 6;
    std::array<Vector<int>, channel_array_size> indices_by_channel;
    MultiValueMap<int, int> indices_with_different_channel;
    Vector<int> indices_with_all_channels;
    mask.foreach_index([&](const int i) {
      const bool all_channels = all_channels_varray[i];
      if (all_channels) {
        indices_with_all_channels.append(i);
      }
      else {
        const int channel = channels[i];
        if (channel >= 0 && channel < channel_array_size) {
          indices_by_channel[channel].append(i);
        }
        else {
          indices_with_different_channel.add(channel, i);
        }
      }
    });

    /* Actually sample the indices by channel. */
    auto sample_indices_in_channel = [&](const Span<int> indices,
                                         const std::optional<int> channel) {
      if (indices.is_empty()) {
        return;
      }
      bke::bSoundFrequencySampler::Key key;
      key.window_function = window_function_;
      key.fft_size = fft_size_;
      key.channel = channel;
      const bke::bSoundFrequencySampler *sampler = bke::bSoundFrequencySampler::get_cached(sound_,
                                                                                           key);
      if (!sampler) {
        amplitudes.fill_indices(indices, 0.0f);
        return;
      }
      for (const int i : indices) {
        const float time = times[i];
        const float low = lows[i];
        const float high = highs[i];
        const float amplitude = sampler->sample(
            time, low, high, time_interpolation, frequency_interpolation);
        amplitudes[i] = amplitude;
      }
    };

    sample_indices_in_channel(indices_with_all_channels, std::nullopt);
    for (const int channel : IndexRange(channel_array_size)) {
      sample_indices_in_channel(indices_by_channel[channel], channel);
    }
    for (const auto item : indices_with_different_channel.items()) {
      sample_indices_in_channel(item.value, item.key);
    }
  }
};

static int to_fft_size_int(const FFTSize fft_size)
{
  switch (fft_size) {
    case FFTSize::_128:
      return 128;
    case FFTSize::_256:
      return 256;
    case FFTSize::_512:
      return 512;
    case FFTSize::_1024:
      return 1024;
    case FFTSize::_2048:
      return 2048;
    case FFTSize::_4096:
      return 4096;
    case FFTSize::_8192:
      return 8192;
    case FFTSize::_16384:
      return 16384;
    case FFTSize::_32768:
      return 32768;
  }
  return 4096;
}

static bke::bSoundFrequencySampler::WindowFunction to_window_function(
    const WindowFunction window_function)
{
  switch (window_function) {
    case WindowFunction::Hann:
      return bke::bSoundFrequencySampler::WindowFunction::Hann;
    case WindowFunction::Hamming:
      return bke::bSoundFrequencySampler::WindowFunction::Hamming;
    case WindowFunction::Blackman:
      return bke::bSoundFrequencySampler::WindowFunction::Blackman;
    case WindowFunction::Rectangular:
      return bke::bSoundFrequencySampler::WindowFunction::Rectangular;
  }
  return bke::bSoundFrequencySampler::WindowFunction::Hann;
}

static void node_geo_exec(GeoNodeExecParams params)
{
  bSound *sound = params.extract_input<bSound *>("Sound"_ustr);
  if (!sound) {
    params.set_default_remaining_outputs();
    return;
  }

  const FFTSize fft_size = params.extract_input<FFTSize>("FFT Size"_ustr);
  const WindowFunction window_function = params.extract_input<WindowFunction>(
      "Window Function"_ustr);

  SocketValueVariant times = params.extract_input<SocketValueVariant>("Time"_ustr);
  SocketValueVariant all_channels = params.extract_input<SocketValueVariant>("All Channels"_ustr);
  SocketValueVariant channels = params.extract_input<SocketValueVariant>("Channel"_ustr);
  SocketValueVariant lows = params.extract_input<SocketValueVariant>("Low"_ustr);
  SocketValueVariant highs = params.extract_input<SocketValueVariant>("High"_ustr);

  auto sample_fn = std::make_shared<SampleSoundFunction>(
      *sound, to_fft_size_int(fft_size), to_window_function(window_function));

  SocketValueVariant amplitudes;
  std::string error_message;
  if (!execute_multi_function_on_value_variant(std::move(sample_fn),
                                               {&times, &all_channels, &channels, &lows, &highs},
                                               {&amplitudes},
                                               params.user_data(),
                                               error_message))
  {
    params.set_default_remaining_outputs();
    params.error_message_add(NodeWarningType::Error, std::move(error_message));
    return;
  }

  params.set_output("Amplitude"_ustr, std::move(amplitudes));
}

static void node_register()
{
  static bke::bNodeType ntype;
  geo_node_type_base(&ntype, "GeometryNodeSampleSoundFrequencies"_ustr);
  ntype.ui_name = "Sample Sound Frequencies";
  ntype.ui_description =
      "Retrieve the amplitude from a sound data-block of a frequency range at a given time";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  bke::node_type_size(ntype, 180, 100, NODE_DEFAULT_MAX_WIDTH);
  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_sample_sound_frequencies_cc
