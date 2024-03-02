/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_node_types.h"
#include "DNA_scene_types.h"

#include "COM_KuwaharaNode.h"

#include "COM_GaussianBlurBaseOperation.h"
#include "COM_KuwaharaAnisotropicOperation.h"
#include "COM_KuwaharaAnisotropicStructureTensorOperation.h"
#include "COM_KuwaharaClassicOperation.h"
#include "COM_SummedAreaTableOperation.h"

namespace blender::compositor {

void KuwaharaNode::convert_to_operations(NodeConverter &converter,
                                         const CompositorContext & /*context*/) const
{
  const bNode *node = this->get_bnode();
  const NodeKuwaharaData *data = (const NodeKuwaharaData *)node->storage;

  switch (data->variation) {
    case CMP_NODE_KUWAHARA_CLASSIC: {
      KuwaharaClassicOperation *kuwahara_classic = new KuwaharaClassicOperation();
      kuwahara_classic->set_high_precision(data->high_precision);
      converter.add_operation(kuwahara_classic);
      converter.map_input_socket(get_input_socket(0), kuwahara_classic->get_input_socket(0));
      converter.map_input_socket(get_input_socket(1), kuwahara_classic->get_input_socket(1));

      SummedAreaTableOperation *sat = new SummedAreaTableOperation();
      sat->set_mode(SummedAreaTableOperation::eMode::Identity);
      converter.add_operation(sat);
      converter.map_input_socket(get_input_socket(0), sat->get_input_socket(0));
      converter.add_link(sat->get_output_socket(0), kuwahara_classic->get_input_socket(2));

      SummedAreaTableOperation *sat_squared = new SummedAreaTableOperation();
      sat_squared->set_mode(SummedAreaTableOperation::eMode::Squared);
      converter.add_operation(sat_squared);
      converter.map_input_socket(get_input_socket(0), sat_squared->get_input_socket(0));
      converter.add_link(sat_squared->get_output_socket(0), kuwahara_classic->get_input_socket(3));

      converter.map_output_socket(get_output_socket(0), kuwahara_classic->get_output_socket(0));
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
      kuwahara_anisotropic_operation->set_sharpness(data->sharpness);
      kuwahara_anisotropic_operation->set_eccentricity(data->eccentricity);

      converter.add_operation(kuwahara_anisotropic_operation);
      converter.map_input_socket(get_input_socket(0),
                                 kuwahara_anisotropic_operation->get_input_socket(0));
      converter.map_input_socket(get_input_socket(1),
                                 kuwahara_anisotropic_operation->get_input_socket(1));
      converter.add_link(blur_y_operation->get_output_socket(0),
                         kuwahara_anisotropic_operation->get_input_socket(2));

      converter.map_output_socket(get_output_socket(0),
                                  kuwahara_anisotropic_operation->get_output_socket(0));

      break;
    }
  }
}

}  // namespace blender::compositor
