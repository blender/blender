/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_node_types.h"
#include "DNA_scene_types.h"

#include "COM_KuwaharaNode.h"

#include "COM_GaussianXBlurOperation.h"
#include "COM_GaussianYBlurOperation.h"
#include "COM_KuwaharaAnisotropicOperation.h"
#include "COM_KuwaharaAnisotropicStructureTensorOperation.h"
#include "COM_KuwaharaClassicOperation.h"

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
      KuwaharaAnisotropicStructureTensorOperation *structure_tensor_operation =
          new KuwaharaAnisotropicStructureTensorOperation();
      converter.add_operation(structure_tensor_operation);
      converter.map_input_socket(get_input_socket(0),
                                 structure_tensor_operation->get_input_socket(0));

      NodeBlurData blur_data;
      blur_data.sizex = data->uniformity;
      blur_data.sizey = data->uniformity;
      blur_data.relative = false;
      blur_data.filtertype = R_FILTER_GAUSS;

      GaussianXBlurOperation *blur_x_operation = new GaussianXBlurOperation();
      blur_x_operation->set_data(&blur_data);
      blur_x_operation->set_size(1.0f);

      converter.add_operation(blur_x_operation);
      converter.add_link(structure_tensor_operation->get_output_socket(0),
                         blur_x_operation->get_input_socket(0));

      GaussianYBlurOperation *blur_y_operation = new GaussianYBlurOperation();
      blur_y_operation->set_data(&blur_data);
      blur_y_operation->set_size(1.0f);

      converter.add_operation(blur_y_operation);
      converter.add_link(blur_x_operation->get_output_socket(0),
                         blur_y_operation->get_input_socket(0));

      KuwaharaAnisotropicOperation *kuwahara_anisotropic_operation =
          new KuwaharaAnisotropicOperation();
      kuwahara_anisotropic_operation->data = *data;

      converter.add_operation(kuwahara_anisotropic_operation);
      converter.map_input_socket(get_input_socket(0),
                                 kuwahara_anisotropic_operation->get_input_socket(0));
      converter.add_link(blur_y_operation->get_output_socket(0),
                         kuwahara_anisotropic_operation->get_input_socket(1));

      converter.map_output_socket(get_output_socket(0),
                                  kuwahara_anisotropic_operation->get_output_socket(0));

      break;
    }
  }
}

}  // namespace blender::compositor
