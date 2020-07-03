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

namespace blender::fn {

class MFNode;
class MFFunctionNode;
class MFDummyNode;
class MFSocket;
class MFInputSocket;
class MFOutputSocket;
class MFNetwork;

class MFNode : NonCopyable, NonMovable {
 protected:
  MFNetwork *network_;
  Span<MFInputSocket *> inputs_;
  Span<MFOutputSocket *> outputs_;
  bool is_dummy_;
  uint id_;

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
  const MultiFunction *function_;
  Span<uint> input_param_indices_;
  Span<uint> output_param_indices_;

  friend MFNetwork;

 public:
  StringRefNull name() const;

  const MultiFunction &function() const;

  const MFInputSocket &input_for_param(uint param_index) const;
  const MFOutputSocket &output_for_param(uint param_index) const;
};

class MFDummyNode : public MFNode {
 private:
  StringRefNull name_;
  MutableSpan<StringRefNull> input_names_;
  MutableSpan<StringRefNull> output_names_;

  friend MFNetwork;

 public:
  StringRefNull name() const;

  Span<StringRefNull> input_names() const;
  Span<StringRefNull> output_names() const;
};

class MFSocket : NonCopyable, NonMovable {
 protected:
  MFNode *node_;
  bool is_output_;
  uint index_;
  MFDataType data_type_;
  uint id_;
  StringRefNull name_;

  friend MFNetwork;

 public:
  StringRefNull name() const;

  uint id() const;

  const MFDataType &data_type() const;

  MFNode &node();
  const MFNode &node() const;

  bool is_input() const;
  bool is_output() const;

  MFInputSocket &as_input();
  const MFInputSocket &as_input() const;

  MFOutputSocket &as_output();
  const MFOutputSocket &as_output() const;
};

class MFInputSocket : public MFSocket {
 private:
  MFOutputSocket *origin_;

  friend MFNetwork;

 public:
  MFOutputSocket *origin();
  const MFOutputSocket *origin() const;
};

class MFOutputSocket : public MFSocket {
 private:
  Vector<MFInputSocket *, 1> targets_;

  friend MFNetwork;

 public:
  Span<MFInputSocket *> targets();
  Span<const MFInputSocket *> targets() const;
};

class MFNetwork : NonCopyable, NonMovable {
 private:
  LinearAllocator<> allocator_;

  VectorSet<MFFunctionNode *> function_nodes_;
  VectorSet<MFDummyNode *> dummy_nodes_;

  Vector<MFNode *> node_or_null_by_id_;
  Vector<MFSocket *> socket_or_null_by_id_;

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

  void relink(MFOutputSocket &old_output, MFOutputSocket &new_output);

  void remove(MFNode &node);

  uint max_socket_id() const;

  std::string to_dot() const;
};

/* --------------------------------------------------------------------
 * MFNode inline methods.
 */

inline StringRefNull MFNode::name() const
{
  if (is_dummy_) {
    return this->as_dummy().name();
  }
  else {
    return this->as_function().name();
  }
}

inline uint MFNode::id() const
{
  return id_;
}

inline MFNetwork &MFNode::network()
{
  return *network_;
}

inline const MFNetwork &MFNode::network() const
{
  return *network_;
}

inline bool MFNode::is_dummy() const
{
  return is_dummy_;
}

inline bool MFNode::is_function() const
{
  return !is_dummy_;
}

inline MFDummyNode &MFNode::as_dummy()
{
  BLI_assert(is_dummy_);
  return *(MFDummyNode *)this;
}

inline const MFDummyNode &MFNode::as_dummy() const
{
  BLI_assert(is_dummy_);
  return *(const MFDummyNode *)this;
}

inline MFFunctionNode &MFNode::as_function()
{
  BLI_assert(!is_dummy_);
  return *(MFFunctionNode *)this;
}

inline const MFFunctionNode &MFNode::as_function() const
{
  BLI_assert(!is_dummy_);
  return *(const MFFunctionNode *)this;
}

inline MFInputSocket &MFNode::input(uint index)
{
  return *inputs_[index];
}

inline const MFInputSocket &MFNode::input(uint index) const
{
  return *inputs_[index];
}

inline MFOutputSocket &MFNode::output(uint index)
{
  return *outputs_[index];
}

inline const MFOutputSocket &MFNode::output(uint index) const
{
  return *outputs_[index];
}

inline Span<MFInputSocket *> MFNode::inputs()
{
  return inputs_;
}

inline Span<const MFInputSocket *> MFNode::inputs() const
{
  return inputs_;
}

inline Span<MFOutputSocket *> MFNode::outputs()
{
  return outputs_;
}

inline Span<const MFOutputSocket *> MFNode::outputs() const
{
  return outputs_;
}

template<typename FuncT> void MFNode::foreach_origin_socket(const FuncT &func) const
{
  for (const MFInputSocket *socket : inputs_) {
    const MFOutputSocket *origin = socket->origin();
    if (origin != nullptr) {
      func(*origin);
    }
  }
}

inline bool MFNode::all_inputs_have_origin() const
{
  for (const MFInputSocket *socket : inputs_) {
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
  return function_->name();
}

inline const MultiFunction &MFFunctionNode::function() const
{
  return *function_;
}

inline const MFInputSocket &MFFunctionNode::input_for_param(uint param_index) const
{
  return this->input(input_param_indices_.first_index(param_index));
}

inline const MFOutputSocket &MFFunctionNode::output_for_param(uint param_index) const
{
  return this->output(output_param_indices_.first_index(param_index));
}

/* --------------------------------------------------------------------
 * MFDummyNode inline methods.
 */

inline StringRefNull MFDummyNode::name() const
{
  return name_;
}

inline Span<StringRefNull> MFDummyNode::input_names() const
{
  return input_names_;
}

inline Span<StringRefNull> MFDummyNode::output_names() const
{
  return output_names_;
}

/* --------------------------------------------------------------------
 * MFSocket inline methods.
 */

inline StringRefNull MFSocket::name() const
{
  return name_;
}

inline uint MFSocket::id() const
{
  return id_;
}

inline const MFDataType &MFSocket::data_type() const
{
  return data_type_;
}

inline MFNode &MFSocket::node()
{
  return *node_;
}

inline const MFNode &MFSocket::node() const
{
  return *node_;
}

inline bool MFSocket::is_input() const
{
  return !is_output_;
}

inline bool MFSocket::is_output() const
{
  return is_output_;
}

inline MFInputSocket &MFSocket::as_input()
{
  BLI_assert(this->is_input());
  return *(MFInputSocket *)this;
}

inline const MFInputSocket &MFSocket::as_input() const
{
  BLI_assert(this->is_input());
  return *(const MFInputSocket *)this;
}

inline MFOutputSocket &MFSocket::as_output()
{
  BLI_assert(this->is_output());
  return *(MFOutputSocket *)this;
}

inline const MFOutputSocket &MFSocket::as_output() const
{
  BLI_assert(this->is_output());
  return *(const MFOutputSocket *)this;
}

/* --------------------------------------------------------------------
 * MFInputSocket inline methods.
 */

inline MFOutputSocket *MFInputSocket::origin()
{
  return origin_;
}

inline const MFOutputSocket *MFInputSocket::origin() const
{
  return origin_;
}

/* --------------------------------------------------------------------
 * MFOutputSocket inline methods.
 */

inline Span<MFInputSocket *> MFOutputSocket::targets()
{
  return targets_;
}

inline Span<const MFInputSocket *> MFOutputSocket::targets() const
{
  return targets_.as_span();
}

/* --------------------------------------------------------------------
 * MFNetwork inline methods.
 */

inline uint MFNetwork::max_socket_id() const
{
  return socket_or_null_by_id_.size() - 1;
}

}  // namespace blender::fn

#endif /* __FN_MULTI_FUNCTION_NETWORK_HH__ */
