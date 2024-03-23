/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "GPU_shader.hh"

#include "COM_context.hh"
#include "COM_input_descriptor.hh"
#include "COM_result.hh"
#include "COM_simple_operation.hh"

namespace blender::realtime_compositor {

/* -------------------------------------------------------------------- */
/** \name Conversion Operation
 *
 * A simple operation that converts a result from a certain type to another. See the derived
 * classes for more details.
 * \{ */

class ConversionOperation : public SimpleOperation {
 public:
  using SimpleOperation::SimpleOperation;

  /* If the input result is a single value, execute_single is called. Otherwise, the shader
   * provided by get_conversion_shader is dispatched. */
  void execute() override;

  /* Determine if a conversion operation is needed for the input with the given result and
   * descriptor. If it is not needed, return a null pointer. If it is needed, return an instance of
   * the appropriate conversion operation. */
  static SimpleOperation *construct_if_needed(Context &context,
                                              const Result &input_result,
                                              const InputDescriptor &input_descriptor);

 protected:
  /* Convert the input single value result to the output single value result. */
  virtual void execute_single(const Result &input, Result &output) = 0;

  /* Get the shader the will be used for conversion. */
  virtual GPUShader *get_conversion_shader() const = 0;

  /** \} */

};  // namespace blender::realtime_compositorclassConversionOperation:publicSimpleOperation

/* -------------------------------------------------------------------- */
/** \name Convert Float to Vector Operation
 *
 * Takes a float result and outputs a vector result. All three components of the output are filled
 * with the input float.
 * \{ */

class ConvertFloatToVectorOperation : public ConversionOperation {
 public:
  ConvertFloatToVectorOperation(Context &context);

  void execute_single(const Result &input, Result &output) override;

  GPUShader *get_conversion_shader() const override;
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Convert Float to Color Operation
 *
 * Takes a float result and outputs a color result. All three color channels of the output are
 * filled with the input float and the alpha channel is set to 1.
 * \{ */

class ConvertFloatToColorOperation : public ConversionOperation {
 public:
  ConvertFloatToColorOperation(Context &context);

  void execute_single(const Result &input, Result &output) override;

  GPUShader *get_conversion_shader() const override;
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Convert Color to Float Operation
 *
 * Takes a color result and outputs a float result. The output is the average of the three color
 * channels, the alpha channel is ignored.
 * \{ */

class ConvertColorToFloatOperation : public ConversionOperation {
 public:
  ConvertColorToFloatOperation(Context &context);

  void execute_single(const Result &input, Result &output) override;

  GPUShader *get_conversion_shader() const override;
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Convert Color to Vector Operation
 *
 * Takes a color result and outputs a vector result. The output is a copy of the three color
 * channels to the three vector components.
 * \{ */

class ConvertColorToVectorOperation : public ConversionOperation {
 public:
  ConvertColorToVectorOperation(Context &context);

  void execute_single(const Result &input, Result &output) override;

  GPUShader *get_conversion_shader() const override;
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Convert Vector to Float Operation
 *
 * Takes a vector result and outputs a float result. The output is the average of the three
 * components.
 * \{ */

class ConvertVectorToFloatOperation : public ConversionOperation {
 public:
  ConvertVectorToFloatOperation(Context &context);

  void execute_single(const Result &input, Result &output) override;

  GPUShader *get_conversion_shader() const override;
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Convert Vector to Color Operation
 *
 * Takes a vector result and outputs a color result. The output is a copy of the three vector
 * components to the three color channels with the alpha channel set to 1.
 * \{ */

class ConvertVectorToColorOperation : public ConversionOperation {
 public:
  ConvertVectorToColorOperation(Context &context);

  void execute_single(const Result &input, Result &output) override;

  GPUShader *get_conversion_shader() const override;
};

/** \} */

}  // namespace blender::realtime_compositor
