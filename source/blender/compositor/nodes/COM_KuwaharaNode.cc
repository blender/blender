/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_KuwaharaNode.h"

#include "COM_ConvertOperation.h"
#include "COM_ConvolutionFilterOperation.h"
#include "COM_FastGaussianBlurOperation.h"
#include "COM_KuwaharaAnisotropicOperation.h"
#include "COM_KuwaharaClassicOperation.h"
#include "COM_MathBaseOperation.h"
#include "COM_SetValueOperation.h"

namespace blender::compositor {

void KuwaharaNode::convert_to_operations(NodeConverter &converter,
                                         const CompositorContext & /*context*/) const
{
  const bNode *node = this->get_bnode();
  const NodeKuwaharaData *data = (const NodeKuwaharaData *)node->storage;

  switch (data->variation) {
    case CMP_NODE_KUWAHARA_CLASSIC: {
      KuwaharaClassicOperation *operation = new KuwaharaClassicOperation();
      operation->set_kernel_size(data->size);

      converter.add_operation(operation);
      converter.map_input_socket(get_input_socket(0), operation->get_input_socket(0));
      converter.map_output_socket(get_output_socket(0), operation->get_output_socket());
      break;
    }

    case CMP_NODE_KUWAHARA_ANISOTROPIC: {
      /* Edge detection on luminance. */
      auto rgb_to_lum = new ConvertColorToBWOperation();
      converter.add_operation(rgb_to_lum);
      converter.map_input_socket(get_input_socket(0), rgb_to_lum->get_input_socket(0));

      auto const_fact = new SetValueOperation();
      const_fact->set_value(1.0f);
      converter.add_operation(const_fact);

      auto sobel_x = new ConvolutionFilterOperation();
      sobel_x->set3x3Filter(1, 0, -1, 2, 0, -2, 1, 0, -1);
      converter.add_operation(sobel_x);
      converter.add_link(rgb_to_lum->get_output_socket(0), sobel_x->get_input_socket(0));
      converter.add_link(const_fact->get_output_socket(0), sobel_x->get_input_socket(1));

      auto sobel_y = new ConvolutionFilterOperation();
      sobel_y->set3x3Filter(1, 2, 1, 0, 0, 0, -1, -2, -1);
      converter.add_operation(sobel_y);
      converter.add_link(rgb_to_lum->get_output_socket(0), sobel_y->get_input_socket(0));
      converter.add_link(const_fact->get_output_socket(0), sobel_y->get_input_socket(1));

      /* Compute intensity of edges. */
      auto sobel_xx = new MathMultiplyOperation();
      auto sobel_yy = new MathMultiplyOperation();
      auto sobel_xy = new MathMultiplyOperation();
      converter.add_operation(sobel_xx);
      converter.add_operation(sobel_yy);
      converter.add_operation(sobel_xy);

      converter.add_link(sobel_x->get_output_socket(0), sobel_xx->get_input_socket(0));
      converter.add_link(sobel_x->get_output_socket(0), sobel_xx->get_input_socket(1));

      converter.add_link(sobel_y->get_output_socket(0), sobel_yy->get_input_socket(0));
      converter.add_link(sobel_y->get_output_socket(0), sobel_yy->get_input_socket(1));

      converter.add_link(sobel_x->get_output_socket(0), sobel_xy->get_input_socket(0));
      converter.add_link(sobel_y->get_output_socket(0), sobel_xy->get_input_socket(1));

      /* Blurring for more robustness. */
      const int sigma = data->smoothing;

      auto blur_sobel_xx = new FastGaussianBlurOperation();
      auto blur_sobel_yy = new FastGaussianBlurOperation();
      auto blur_sobel_xy = new FastGaussianBlurOperation();

      blur_sobel_yy->set_size(sigma, sigma);
      blur_sobel_xx->set_size(sigma, sigma);
      blur_sobel_xy->set_size(sigma, sigma);

      converter.add_operation(blur_sobel_xx);
      converter.add_operation(blur_sobel_yy);
      converter.add_operation(blur_sobel_xy);

      converter.add_link(sobel_xx->get_output_socket(0), blur_sobel_xx->get_input_socket(0));
      converter.add_link(sobel_yy->get_output_socket(0), blur_sobel_yy->get_input_socket(0));
      converter.add_link(sobel_xy->get_output_socket(0), blur_sobel_xy->get_input_socket(0));

      /* Apply anisotropic Kuwahara filter. */
      KuwaharaAnisotropicOperation *aniso = new KuwaharaAnisotropicOperation();
      aniso->set_kernel_size(data->size + 4);
      converter.map_input_socket(get_input_socket(0), aniso->get_input_socket(0));
      converter.add_operation(aniso);

      converter.add_link(blur_sobel_xx->get_output_socket(0), aniso->get_input_socket(1));
      converter.add_link(blur_sobel_yy->get_output_socket(0), aniso->get_input_socket(2));
      converter.add_link(blur_sobel_xy->get_output_socket(0), aniso->get_input_socket(3));

      converter.map_output_socket(get_output_socket(0), aniso->get_output_socket(0));

      break;
    }
  }
}

}  // namespace blender::compositor
