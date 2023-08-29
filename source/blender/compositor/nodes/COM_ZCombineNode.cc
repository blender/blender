/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_ZCombineNode.h"

#include "COM_AntiAliasOperation.h"
#include "COM_MathBaseOperation.h"
#include "COM_ZCombineOperation.h"

namespace blender::compositor {

void ZCombineNode::convert_to_operations(NodeConverter &converter,
                                         const CompositorContext & /*context*/) const
{
  if (this->get_bnode()->custom2) {
    ZCombineOperation *operation = nullptr;
    if (this->get_bnode()->custom1) {
      operation = new ZCombineAlphaOperation();
    }
    else {
      operation = new ZCombineOperation();
    }
    converter.add_operation(operation);

    converter.map_input_socket(get_input_socket(0), operation->get_input_socket(0));
    converter.map_input_socket(get_input_socket(1), operation->get_input_socket(1));
    converter.map_input_socket(get_input_socket(2), operation->get_input_socket(2));
    converter.map_input_socket(get_input_socket(3), operation->get_input_socket(3));
    converter.map_output_socket(get_output_socket(0), operation->get_output_socket());

    MathMinimumOperation *zoperation = new MathMinimumOperation();
    converter.add_operation(zoperation);

    converter.map_input_socket(get_input_socket(1), zoperation->get_input_socket(0));
    converter.map_input_socket(get_input_socket(3), zoperation->get_input_socket(1));
    converter.map_output_socket(get_output_socket(1), zoperation->get_output_socket());
  }
  else {
    /* XXX custom1 is "use_alpha", what on earth is this supposed to do here?!? */
    /* not full anti alias, use masking for Z combine. be aware it uses anti aliasing. */

    /* Step 1 create mask. */
    NodeOperation *maskoperation;
    if (this->get_bnode()->custom1) {
      maskoperation = new MathGreaterThanOperation();
    }
    else {
      maskoperation = new MathLessThanOperation();
    }
    converter.add_operation(maskoperation);

    converter.map_input_socket(get_input_socket(1), maskoperation->get_input_socket(0));
    converter.map_input_socket(get_input_socket(3), maskoperation->get_input_socket(1));

    /* Step 2 anti alias mask bit of an expensive operation, but does the trick. */
    AntiAliasOperation *antialiasoperation = new AntiAliasOperation();
    converter.add_operation(antialiasoperation);

    converter.add_link(maskoperation->get_output_socket(),
                       antialiasoperation->get_input_socket(0));

    /* use mask to blend between the input colors. */
    ZCombineMaskOperation *zcombineoperation = this->get_bnode()->custom1 ?
                                                   new ZCombineMaskAlphaOperation() :
                                                   new ZCombineMaskOperation();
    converter.add_operation(zcombineoperation);

    converter.add_link(antialiasoperation->get_output_socket(),
                       zcombineoperation->get_input_socket(0));
    converter.map_input_socket(get_input_socket(0), zcombineoperation->get_input_socket(1));
    converter.map_input_socket(get_input_socket(2), zcombineoperation->get_input_socket(2));
    converter.map_output_socket(get_output_socket(0), zcombineoperation->get_output_socket());

    MathMinimumOperation *zoperation = new MathMinimumOperation();
    converter.add_operation(zoperation);

    converter.map_input_socket(get_input_socket(1), zoperation->get_input_socket(0));
    converter.map_input_socket(get_input_socket(3), zoperation->get_input_socket(1));
    converter.map_output_socket(get_output_socket(1), zoperation->get_output_socket());
  }
}

}  // namespace blender::compositor
