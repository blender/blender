/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Blur the input horizontally by applying a fourth order IIR filter approximating a Gaussian
 * filter using Deriche's design method. This is based on the following paper:
 *
 *   Deriche, Rachid. Recursively implementating the Gaussian and its derivatives. Diss. INRIA,
 *   1993.
 *
 * We run two filters per row in parallel, one for the causal filter and one for the non causal
 * filter, storing the result of each separately. See the DericheGaussianCoefficients class and the
 * implementation for more information. */

#pragma BLENDER_REQUIRE(gpu_shader_compositor_texture_utilities.glsl)

#define FILTER_ORDER 4

void main()
{
  /* The shader runs parallel across rows but serially across columns. */
  int y = int(gl_GlobalInvocationID.x);
  int width = texture_size(input_tx).x;

  /* The second dispatch dimension is two dispatches, one for the causal filter and one for the non
   * causal one. */
  bool is_causal = gl_GlobalInvocationID.y == 0;
  vec4 feedforward_coefficients = is_causal ? causal_feedforward_coefficients :
                                              non_causal_feedforward_coefficients;
  float boundary_coefficient = is_causal ? causal_boundary_coefficient :
                                           non_causal_boundary_coefficient;

  /* Create an array that holds the last FILTER_ORDER inputs along with the current input. The
   * current input is at index 0 and the oldest input is at index FILTER_ORDER. We assume Neumann
   * boundary condition, so we initialize all inputs by the boundary pixel. */
  ivec2 boundary_texel = is_causal ? ivec2(0, y) : ivec2(width - 1, y);
  vec4 input_boundary = texture_load(input_tx, boundary_texel);
  vec4 inputs[FILTER_ORDER + 1] = vec4[](
      input_boundary, input_boundary, input_boundary, input_boundary, input_boundary);

  /* Create an array that holds the last FILTER_ORDER outputs along with the current output. The
   * current output is at index 0 and the oldest output is at index FILTER_ORDER. We assume Neumann
   * boundary condition, so we initialize all outputs by the boundary pixel multiplied by the
   * boundary coefficient. See the DericheGaussianCoefficients class for more information on the
   * boundary handing. */
  vec4 output_boundary = input_boundary * boundary_coefficient;
  vec4 outputs[FILTER_ORDER + 1] = vec4[](
      output_boundary, output_boundary, output_boundary, output_boundary, output_boundary);

  for (int x = 0; x < width; x++) {
    /* Run forward across rows for the causal filter and backward for the non causal filter. */
    ivec2 texel = is_causal ? ivec2(x, y) : ivec2(width - 1 - x, y);
    inputs[0] = texture_load(input_tx, texel);

    /* Compute Equation (28) for the causal filter or Equation (29) for the non causal filter. The
     * only difference is that the non causal filter ignores the current value and starts from the
     * previous input, as can be seen in the subscript of the first input term in both equations.
     * So add one while indexing the non causal inputs. */
    outputs[0] = vec4(0.0);
    int first_input_index = is_causal ? 0 : 1;
    for (int i = 0; i < FILTER_ORDER; i++) {
      outputs[0] += feedforward_coefficients[i] * inputs[first_input_index + i];
      outputs[0] -= feedback_coefficients[i] * outputs[i + 1];
    }

    /* Store the causal and non causal outputs independently, then sum them in a separate shader
     * dispatch for better parallelism. */
    if (is_causal) {
      imageStore(causal_output_img, texel, outputs[0]);
    }
    else {
      imageStore(non_causal_output_img, texel, outputs[0]);
    }

    /* Shift the inputs temporally by one. The oldest input is discarded, while the current input
     * will retain its value but will be overwritten with the new current value in the next
     * iteration. */
    for (int i = FILTER_ORDER; i >= 1; i--) {
      inputs[i] = inputs[i - 1];
    }

    /* Shift the outputs temporally by one. The oldest output is discarded, while the current
     * output will retain its value but will be overwritten with the new current value in the next
     * iteration. */
    for (int i = FILTER_ORDER; i >= 1; i--) {
      outputs[i] = outputs[i - 1];
    }
  }
}
