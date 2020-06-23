/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef __FN_MULTI_FUNCTION_NETWORK_HH__
#define __FN_MULTI_FUNCTION_NETWORK_HH__

/** \file
 * \ingroup fn
 *
 * A multi-function network (`MFNetwork`) allows you to connect multiple multi-functions. The
 * `MFNetworkEvaluator` is a multi-function that wraps an entire network into a new multi-function
 * (which can be used in another network and so on).
 *
 * A MFNetwork is a graph data structure with two kinds of nodes:
 * - MFFunctionNode: Represents a multi-function. Its input and output sockets correspond to
 *       parameters of the referenced multi-function.
 * - MFDummyNode: Does not reference a multi-function. Instead it just has sockets that can be
 *       used to represent node group inputs and outputs.
 *
 * Links represent data flow. Unlinked input sockets have no value. In order to execute a function
 * node, all its inputs have to be connected to something.
 *
 * Links are only allowed between sockets with the exact same MFDataType. There are no implicit
 * conversions.
 *
 * Every input and output parameter of a multi-function corresponds to exactly one input or output
 * socket respectively. A multiple parameter belongs to exactly one input AND one output socket.
 *
 * There is an .to_dot() method that generates a graph in dot format for debugging purposes.
 */

#include "FN_multi_function.hh"

#include "BLI_vector_set.hh"

namespace blender {
namespace fn {

class MFNode;
class MFFunctionNode;
class MFDummyNode;
class MFSocket;
class MFInputSocket;
class MFOutputSocket;
class MFNetwork;

class MFNode : NonCopyable, NonMovable {
 protected:
  MFNetwork *m_network;
  Span<MFInputSocket *> m_inputs;
  Span<MFOutputSocket *> m_outputs;
  bool m_is_dummy;
  uint m_id;

  friend MFNetwork;

 public:
  StringRefNull name() const;

  uint id() const;

  MFNetwork &network();
  const MFNetwork &network() const;

  bool is_dummy() const;
  bool is_function() const;

  MFDummyNode &as_dummy();
  const MFDummyNode &as_dummy() const;

  MFFunctionNode &as_function();
  const MFFunctionNode &as_function() const;

  MFInputSocket &input(uint index);
  const MFInputSocket &input(uint index) const;

  MFOutputSocket &output(uint index);
  const MFOutputSocket &output(uint index) const;

  Span<MFInputSocket *> inputs();
  Span<const MFInputSocket *> inputs() const;

  Span<MFOutputSocket *> outputs();
  Span<const MFOutputSocket *> outputs() const;

  template<typename FuncT> void foreach_origin_socket(const FuncT &func) const;

  bool all_inputs_have_origin() const;

 private:
  void destruct_sockets();
};

class MFFunctionNode : public MFNode {
 private:
  const MultiFunction *m_function;
  Span<uint> m_input_param_indices;
  Span<uint> m_output_param_indices;

  friend MFNetwork;

 public:
  StringRefNull name() const;

  const MultiFunction &function() const;

  const MFInputSocket &input_for_param(uint param_index) const;
  const MFOutputSocket &output_for_param(uint param_index) const;
};

class MFDummyNode : public MFNode {
 private:
  StringRefNull m_name;
  MutableSpan<StringRefNull> m_input_names;
  MutableSpan<StringRefNull> m_output_names;

  friend MFNetwork;

 public:
  StringRefNull name() const;

  Span<StringRefNull> input_names() const;
  Span<StringRefNull> output_names() const;
};

class MFSocket : NonCopyable, NonMovable {
 protected:
  MFNode *m_node;
  bool m_is_output;
  uint m_index;
  MFDataType m_data_type;
  uint m_id;
  StringRefNull m_name;

  friend MFNetwork;

 public:
  StringRefNull name() const;

  uint id() const;

  const MFDataType &data_type() const;

  MFNode &node();
  const MFNode &node() const;
};

class MFInputSocket : public MFSocket {
 private:
  MFOutputSocket *m_origin;

  friend MFNetwork;

 public:
  MFOutputSocket *origin();
  const MFOutputSocket *origin() const;
};

class MFOutputSocket : public MFSocket {
 private:
  Vector<MFInputSocket *, 1> m_targets;

  friend MFNetwork;

 public:
  Span<MFInputSocket *> targets();
  Span<const MFInputSocket *> targets() const;
};

class MFNetwork : NonCopyable, NonMovable {
 private:
  LinearAllocator<> m_allocator;

  VectorSet<MFFunctionNode *> m_function_nodes;
  VectorSet<MFDummyNode *> m_dummy_nodes;

  Vector<MFNode *> m_node_or_null_by_id;
  Vector<MFSocket *> m_socket_or_null_by_id;

 public:
  MFNetwork() = default;
  ~MFNetwork();

  MFFunctionNode &add_function(const MultiFunction &function);
  MFDummyNode &add_dummy(StringRef name,
                         Span<MFDataType> input_types,
                         Span<MFDataType> output_types,
                         Span<StringRef> input_names,
                         Span<StringRef> output_names);
  void add_link(MFOutputSocket &from, MFInputSocket &to);

  MFOutputSocket &add_input(StringRef name, MFDataType data_type);
  MFInputSocket &add_output(StringRef name, MFDataType data_type);

  uint max_socket_id() const;

  std::string to_dot() const;
};

/* --------------------------------------------------------------------
 * MFNode inline methods.
 */

inline StringRefNull MFNode::name() const
{
  if (m_is_dummy) {
    return this->as_dummy().name();
  }
  else {
    return this->as_function().name();
  }
}

inline uint MFNode::id() const
{
  return m_id;
}

inline MFNetwork &MFNode::network()
{
  return *m_network;
}

inline const MFNetwork &MFNode::network() const
{
  return *m_network;
}

inline bool MFNode::is_dummy() const
{
  return m_is_dummy;
}

inline bool MFNode::is_function() const
{
  return !m_is_dummy;
}

inline MFDummyNode &MFNode::as_dummy()
{
  BLI_assert(m_is_dummy);
  return *(MFDummyNode *)this;
}

inline const MFDummyNode &MFNode::as_dummy() const
{
  BLI_assert(m_is_dummy);
  return *(const MFDummyNode *)this;
}

inline MFFunctionNode &MFNode::as_function()
{
  BLI_assert(!m_is_dummy);
  return *(MFFunctionNode *)this;
}

inline const MFFunctionNode &MFNode::as_function() const
{
  BLI_assert(!m_is_dummy);
  return *(const MFFunctionNode *)this;
}

inline MFInputSocket &MFNode::input(uint index)
{
  return *m_inputs[index];
}

inline const MFInputSocket &MFNode::input(uint index) const
{
  return *m_inputs[index];
}

inline MFOutputSocket &MFNode::output(uint index)
{
  return *m_outputs[index];
}

inline const MFOutputSocket &MFNode::output(uint index) const
{
  return *m_outputs[index];
}

inline Span<MFInputSocket *> MFNode::inputs()
{
  return m_inputs;
}

inline Span<const MFInputSocket *> MFNode::inputs() const
{
  return m_inputs;
}

inline Span<MFOutputSocket *> MFNode::outputs()
{
  return m_outputs;
}

inline Span<const MFOutputSocket *> MFNode::outputs() const
{
  return m_outputs;
}

template<typename FuncT> void MFNode::foreach_origin_socket(const FuncT &func) const
{
  for (const MFInputSocket *socket : m_inputs) {
    const MFOutputSocket *origin = socket->origin();
    if (origin != nullptr) {
      func(*origin);
    }
  }
}

inline bool MFNode::all_inputs_have_origin() const
{
  for (const MFInputSocket *socket : m_inputs) {
    if (socket->origin() == nullptr) {
      return false;
    }
  }
  return true;
}

/* --------------------------------------------------------------------
 * MFFunctionNode inline methods.
 */

inline StringRefNull MFFunctionNode::name() const
{
  return m_function->name();
}

inline const MultiFunction &MFFunctionNode::function() const
{
  return *m_function;
}

inline const MFInputSocket &MFFunctionNode::input_for_param(uint param_index) const
{
  return this->input(m_input_param_indices.first_index(param_index));
}

inline const MFOutputSocket &MFFunctionNode::output_for_param(uint param_index) const
{
  return this->output(m_output_param_indices.first_index(param_index));
}

/* --------------------------------------------------------------------
 * MFDummyNode inline methods.
 */

inline StringRefNull MFDummyNode::name() const
{
  return m_name;
}

inline Span<StringRefNull> MFDummyNode::input_names() const
{
  return m_input_names;
}

inline Span<StringRefNull> MFDummyNode::output_names() const
{
  return m_output_names;
}

/* --------------------------------------------------------------------
 * MFSocket inline methods.
 */

inline StringRefNull MFSocket::name() const
{
  return m_name;
}

inline uint MFSocket::id() const
{
  return m_id;
}

inline const MFDataType &MFSocket::data_type() const
{
  return m_data_type;
}

inline MFNode &MFSocket::node()
{
  return *m_node;
}

inline const MFNode &MFSocket::node() const
{
  return *m_node;
}

/* --------------------------------------------------------------------
 * MFInputSocket inline methods.
 */

inline MFOutputSocket *MFInputSocket::origin()
{
  return m_origin;
}

inline const MFOutputSocket *MFInputSocket::origin() const
{
  return m_origin;
}

/* --------------------------------------------------------------------
 * MFOutputSocket inline methods.
 */

inline Span<MFInputSocket *> MFOutputSocket::targets()
{
  return m_targets;
}

inline Span<const MFInputSocket *> MFOutputSocket::targets() const
{
  return m_targets.as_span();
}

/* --------------------------------------------------------------------
 * MFNetwork inline methods.
 */

inline uint MFNetwork::max_socket_id() const
{
  return m_socket_or_null_by_id.size() - 1;
}

}  // namespace fn
}  // namespace blender

#endif /* __FN_MULTI_FUNCTION_NETWORK_HH__ */
