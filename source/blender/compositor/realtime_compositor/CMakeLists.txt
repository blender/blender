# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
  ../../gpu
  ../../nodes
  ../../imbuf
  ../../blenlib
  ../../makesdna
  ../../makesrna
  ../../blenkernel
  ../../gpu/intern
  ../../../../intern/guardedalloc
)


set(SRC
  intern/compile_state.cc
  intern/context.cc
  intern/conversion_operation.cc
  intern/domain.cc
  intern/evaluator.cc
  intern/input_single_value_operation.cc
  intern/node_operation.cc
  intern/operation.cc
  intern/realize_on_domain_operation.cc
  intern/reduce_to_single_value_operation.cc
  intern/result.cc
  intern/scheduler.cc
  intern/shader_node.cc
  intern/shader_operation.cc
  intern/simple_operation.cc
  intern/static_shader_manager.cc
  intern/texture_pool.cc
  intern/utilities.cc

  COM_compile_state.hh
  COM_context.hh
  COM_conversion_operation.hh
  COM_domain.hh
  COM_evaluator.hh
  COM_input_descriptor.hh
  COM_input_single_value_operation.hh
  COM_node_operation.hh
  COM_operation.hh
  COM_realize_on_domain_operation.hh
  COM_reduce_to_single_value_operation.hh
  COM_result.hh
  COM_scheduler.hh
  COM_shader_node.hh
  COM_shader_operation.hh
  COM_simple_operation.hh
  COM_static_shader_manager.hh
  COM_texture_pool.hh
  COM_utilities.hh
)

set(LIB
  bf_gpu
  bf_nodes
  bf_imbuf
  bf_blenlib
  bf_blenkernel
)

blender_add_lib(bf_realtime_compositor "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
