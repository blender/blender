/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <memory>
#include <string>

#include "BLI_map.hh"
#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

#include "COM_context.hh"
#include "COM_domain.hh"
#include "COM_input_descriptor.hh"
#include "COM_result.hh"
#include "COM_static_shader_manager.hh"
#include "COM_texture_pool.hh"

namespace blender::realtime_compositor {

class SimpleOperation;

/* A type representing a vector of simple operations that store the input processors for a
 * particular input. */
using ProcessorsVector = Vector<std::unique_ptr<SimpleOperation>>;

/* ------------------------------------------------------------------------------------------------
 * Operation
 *
 * The operation is the basic unit of the compositor. The evaluator compiles the compositor node
 * tree into an ordered stream of operations which are then executed in order during evaluation.
 * The operation class can be sub-classed to implement a new operation. Operations have a number of
 * inputs and outputs that are declared during construction and are identified by string
 * identifiers. Inputs are declared by calling declare_input_descriptor providing an appropriate
 * descriptor. Those inputs are mapped to the results computed by other operations whose outputs
 * are linked to the inputs. Such mappings are established by the compiler during compilation by
 * calling the map_input_to_result method. Outputs are populated by calling the populate_result
 * method, providing a result of an appropriate type. Upon execution, the operation allocates a
 * result for each of its outputs and computes their value based on its inputs and options.
 *
 * Each input may have one or more input processors, which are simple operations that process the
 * inputs before the operation is executed, see the discussion in COM_simple_operation.hh for more
 * information. And thus the effective input of the operation is the result of the last input
 * processor if one exists. Input processors are added and evaluated by calling the
 * add_and_evaluate_input_processors method, which provides a default implementation that does
 * things like implicit conversion, domain realization, and more. This default implementation can,
 * however, be overridden, extended, or removed. Once the input processors are added and evaluated
 * for the first time, they are stored in the operation and future evaluations can evaluate them
 * directly without having to add them again.
 *
 * The operation is evaluated by calling the evaluate method, which first adds the input processors
 * if they weren't added already and evaluates them, then it resets the results of the operation,
 * then it calls the execute method of the operation, and finally it releases the results mapped to
 * the inputs to declare that they are no longer needed. */
class Operation {
 private:
  /* A reference to the compositor context. This member references the same object in all
   * operations but is included in the class for convenience. */
  Context &context_;
  /* A mapping between each output of the operation identified by its identifier and the result for
   * that output. A result for each output of the operation should be constructed and added to the
   * map during operation construction by calling the populate_result method. The results should be
   * allocated and their contents should be computed in the execute method. */
  Map<std::string, Result> results_;
  /* A mapping between each input of the operation identified by its identifier and its input
   * descriptor. Those descriptors should be declared during operation construction by calling the
   * declare_input_descriptor method. */
  Map<std::string, InputDescriptor> input_descriptors_;
  /* A mapping between each input of the operation identified by its identifier and a pointer to
   * the computed result providing its data. The mapped result is either one that was computed by
   * another operation or one that was internally computed in the operation by the last input
   * processor for that input. It is the responsibility of the evaluator to map the inputs to their
   * linked results before evaluating the operation by calling the map_input_to_result method. */
  Map<StringRef, Result *> results_mapped_to_inputs_;
  /* A mapping between each input of the operation identified by its identifier and an ordered list
   * of simple operations to process that input. This is initialized the first time the input
   * processors are evaluated by calling the add_and_evaluate_input_processors method. Further
   * evaluations will evaluate the processors directly without the need to add them again. The
   * input_processors_added_ member indicates whether the processors were already added and can be
   * evaluated directly or need to be added and evaluated. */
  Map<StringRef, ProcessorsVector> input_processors_;
  /* True if the input processors were already added and can be evaluated directly. False if the
   * input processors are not yet added and needs to be added. */
  bool input_processors_added_ = false;

 public:
  Operation(Context &context);

  virtual ~Operation();

  /* Evaluate the operation by:
   * 1. Evaluating the input processors.
   * 2. Resetting the results of the operation.
   * 3. Calling the execute method of the operation.
   * 4. Releasing the results mapped to the inputs. */
  void evaluate();

  /* Get a reference to the output result identified by the given identifier. */
  Result &get_result(StringRef identifier);

  /* Map the input identified by the given identifier to the result providing its data. See
   * results_mapped_to_inputs_ for more details. This should be called by the evaluator to
   * establish links between different operations. */
  void map_input_to_result(StringRef identifier, Result *result);

 protected:
  /* Compute the operation domain of this operation. By default, this implements a default logic
   * that infers the operation domain from the inputs, which may be overridden for a different
   * logic. See the discussion in COM_domain.hh for the inference logic and more information. */
  virtual Domain compute_domain();

  /* Add and evaluate any needed input processors, which essentially just involves calling the
   * add_and_evaluate_input_processor method with the needed processors. This is called before
   * executing the operation to prepare its inputs. The class defines a default implementation
   * which adds typically needed processors, but derived classes can override the method to have
   * a different implementation, extend the implementation, or remove it entirely. */
  virtual void add_and_evaluate_input_processors();

  /* Given the identifier of an input of the operation and a processor operation:
   * - Add the given processor to the list of input processors for the input.
   * - Map the input of the processor to be the result of the last input processor or the result
   *   mapped to the input if no previous processors exists.
   * - Switch the result mapped to the input to be the output result of the processor.
   * - Evaluate the processor. */
  void add_and_evaluate_input_processor(StringRef identifier, SimpleOperation *processor);

  /* This method should allocate the operation results, execute the operation, and compute the
   * output results. */
  virtual void execute() = 0;

  /* Get a reference to the result connected to the input identified by the given identifier. */
  Result &get_input(StringRef identifier) const;

  /* Switch the result mapped to the input identified by the given identifier with the given
   * result. */
  void switch_result_mapped_to_input(StringRef identifier, Result *result);

  /* Add the given result to the results_ map identified by the given output identifier. This
   * should be called during operation construction for all outputs. The provided result shouldn't
   * be allocated or initialized, this will happen later during execution. */
  void populate_result(StringRef identifier, Result result);

  /* Declare the descriptor of the input identified by the given identifier to be the given
   * descriptor. Adds the given descriptor to the input_descriptors_ map identified by the given
   * input identifier. This should be called during operation constructor for all inputs. */
  void declare_input_descriptor(StringRef identifier, InputDescriptor descriptor);

  /* Get a reference to the descriptor of the input identified by the given identified. */
  InputDescriptor &get_input_descriptor(StringRef identifier);

  /* Returns a reference to the compositor context. */
  Context &context();

  /* Returns a reference to the texture pool of the compositor context. */
  TexturePool &texture_pool() const;

  /* Returns a reference to the shader manager of the compositor context. */
  StaticShaderManager &shader_manager() const;

 private:
  /* Evaluate the input processors. If the input processors were already added they will be
   * evaluated directly. Otherwise, the input processors will be added and evaluated. */
  void evaluate_input_processors();

  /* Resets the results of the operation. See the reset method in the Result class for more
   * information. */
  void reset_results();

  /* Release the results that are mapped to the inputs of the operation. This is called after the
   * evaluation of the operation to declare that the results are no longer needed by this
   * operation. */
  void release_inputs();

  /* Release the results that were allocated in the execute method but are not actually needed.
   * This can be the case if the execute method allocated a dummy texture for an unneeded result,
   * see the description of Result::allocate_texture() for more information. This is called after
   * the evaluation of the operation. */
  void release_unneeded_results();
};

}  // namespace blender::realtime_compositor
