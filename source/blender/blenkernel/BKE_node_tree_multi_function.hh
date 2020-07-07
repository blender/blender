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

#ifndef __BKE_NODE_TREE_FUNCTION_HH__
#define __BKE_NODE_TREE_FUNCTION_HH__

/** \file
 * \ingroup bke
 *
 * This file allows you to generate a multi-function network from a user-generated node tree.
 */

#include "FN_multi_function_builder.hh"
#include "FN_multi_function_network.hh"

#include "BKE_derived_node_tree.hh"

#include "BLI_resource_collector.hh"

namespace blender {
namespace bke {

/* Maybe this should be moved to BKE_node.h. */
inline bool is_multi_function_data_socket(const bNodeSocket *bsocket)
{
  if (bsocket->typeinfo->get_mf_data_type != nullptr) {
    BLI_assert(bsocket->typeinfo->expand_in_mf_network != nullptr);
    return true;
  }
  return false;
}

/**
 * A MFNetworkTreeMap maps various components of a bke::DerivedNodeTree to components of a
 * fn::MFNetwork. This is necessary for further processing of a multi-function network that has
 * been generated from a node tree.
 */
class MFNetworkTreeMap {
 private:
  /**
   * Store by id instead of using a hash table to avoid unnecessary hash table lookups.
   *
   * Input sockets in a node tree can have multiple corresponding sockets in the generated
   * MFNetwork. This is because nodes are allowed to expand into multiple multi-function nodes.
   */
  Array<Vector<fn::MFSocket *, 1>> m_sockets_by_dsocket_id;
  Array<fn::MFOutputSocket *> m_socket_by_group_input_id;

 public:
  MFNetworkTreeMap(const DerivedNodeTree &tree)
      : m_sockets_by_dsocket_id(tree.sockets().size()),
        m_socket_by_group_input_id(tree.group_inputs().size(), nullptr)
  {
  }

  void add(const DSocket &dsocket, fn::MFSocket &socket)
  {
    BLI_assert(dsocket.is_input() == socket.is_input());
    m_sockets_by_dsocket_id[dsocket.id()].append(&socket);
  }

  void add(const DInputSocket &dsocket, fn::MFInputSocket &socket)
  {
    m_sockets_by_dsocket_id[dsocket.id()].append(&socket);
  }

  void add(const DOutputSocket &dsocket, fn::MFOutputSocket &socket)
  {
    m_sockets_by_dsocket_id[dsocket.id()].append(&socket);
  }

  void add(Span<const DInputSocket *> dsockets, Span<fn::MFInputSocket *> sockets)
  {
    assert_same_size(dsockets, sockets);
    for (uint i : dsockets.index_range()) {
      this->add(*dsockets[i], *sockets[i]);
    }
  }

  void add(Span<const DOutputSocket *> dsockets, Span<fn::MFOutputSocket *> sockets)
  {
    assert_same_size(dsockets, sockets);
    for (uint i : dsockets.index_range()) {
      this->add(*dsockets[i], *sockets[i]);
    }
  }

  void add(const DGroupInput &group_input, fn::MFOutputSocket &socket)
  {
    BLI_assert(m_socket_by_group_input_id[group_input.id()] == nullptr);
    m_socket_by_group_input_id[group_input.id()] = &socket;
  }

  void add_try_match(const DNode &dnode, fn::MFNode &node)
  {
    this->add_try_match(dnode.inputs(), node.inputs());
    this->add_try_match(dnode.outputs(), node.outputs());
  }

  void add_try_match(Span<const DSocket *> dsockets, Span<fn::MFSocket *> sockets)
  {
    uint used_sockets = 0;
    for (const DSocket *dsocket : dsockets) {
      if (!dsocket->is_available()) {
        continue;
      }
      if (!is_multi_function_data_socket(dsocket->bsocket())) {
        continue;
      }
      fn::MFSocket *socket = sockets[used_sockets];
      this->add(*dsocket, *socket);
      used_sockets++;
    }
  }

  fn::MFOutputSocket &lookup(const DGroupInput &group_input)
  {
    fn::MFOutputSocket *socket = m_socket_by_group_input_id[group_input.id()];
    BLI_assert(socket != nullptr);
    return *socket;
  }

  fn::MFOutputSocket &lookup(const DOutputSocket &dsocket)
  {
    auto &sockets = m_sockets_by_dsocket_id[dsocket.id()];
    BLI_assert(sockets.size() == 1);
    return sockets[0]->as_output();
  }

  Span<fn::MFInputSocket *> lookup(const DInputSocket &dsocket)
  {
    return m_sockets_by_dsocket_id[dsocket.id()].as_span().cast<fn::MFInputSocket *>();
  }

  fn::MFInputSocket &lookup_dummy(const DInputSocket &dsocket)
  {
    Span<fn::MFInputSocket *> sockets = this->lookup(dsocket);
    BLI_assert(sockets.size() == 1);
    fn::MFInputSocket &socket = *sockets[0];
    BLI_assert(socket.node().is_dummy());
    return socket;
  }

  fn::MFOutputSocket &lookup_dummy(const DOutputSocket &dsocket)
  {
    fn::MFOutputSocket &socket = this->lookup(dsocket);
    BLI_assert(socket.node().is_dummy());
    return socket;
  }

  bool is_mapped(const DSocket &dsocket) const
  {
    return m_sockets_by_dsocket_id[dsocket.id()].size() >= 1;
  }
};

/**
 * This data is necessary throughout the generation of a MFNetwork from a node tree.
 */
struct CommonMFNetworkBuilderData {
  ResourceCollector &resources;
  fn::MFNetwork &network;
  MFNetworkTreeMap &network_map;
  const DerivedNodeTree &tree;
};

class MFNetworkBuilderBase {
 protected:
  CommonMFNetworkBuilderData &m_common;

 public:
  MFNetworkBuilderBase(CommonMFNetworkBuilderData &common) : m_common(common)
  {
  }

  /**
   * Returns the network that is currently being built.
   */
  fn::MFNetwork &network()
  {
    return m_common.network;
  }

  /**
   * Returns the map between the node tree and the multi-function network that is being built.
   */
  MFNetworkTreeMap &network_map()
  {
    return m_common.network_map;
  }

  /**
   * Returns a resource collector that will only be destructed after the multi-function network is
   * destructed.
   */
  ResourceCollector &resources()
  {
    return m_common.resources;
  }

  /**
   * Constructs a new function that will live at least as long as the MFNetwork.
   */
  template<typename T, typename... Args> T &construct_fn(Args &&... args)
  {
    BLI_STATIC_ASSERT((std::is_base_of_v<fn::MultiFunction, T>), "");
    void *buffer = m_common.resources.linear_allocator().allocate(sizeof(T), alignof(T));
    T *fn = new (buffer) T(std::forward<Args>(args)...);
    m_common.resources.add(destruct_ptr<T>(fn), fn->name().data());
    return *fn;
  }
};

/**
 * This class is used by socket implementations to define how an unlinked input socket is handled
 * in a multi-function network.
 */
class SocketMFNetworkBuilder : public MFNetworkBuilderBase {
 private:
  const DSocket *m_dsocket = nullptr;
  const DGroupInput *m_group_input = nullptr;
  bNodeSocket *m_bsocket;
  fn::MFOutputSocket *m_built_socket = nullptr;

 public:
  SocketMFNetworkBuilder(CommonMFNetworkBuilderData &common, const DSocket &dsocket)
      : MFNetworkBuilderBase(common), m_dsocket(&dsocket), m_bsocket(dsocket.bsocket())
  {
  }

  SocketMFNetworkBuilder(CommonMFNetworkBuilderData &common, const DGroupInput &group_input)
      : MFNetworkBuilderBase(common), m_group_input(&group_input), m_bsocket(group_input.bsocket())
  {
  }

  /**
   * Returns the socket that is currently being built.
   */
  bNodeSocket &bsocket()
  {
    return *m_bsocket;
  }

  /**
   * Utility method that returns bsocket->default_value for the current socket.
   */
  template<typename T> T *socket_default_value()
  {
    return (T *)m_bsocket->default_value;
  }

  /**
   * Builds a function node for that socket that outputs the given constant value.
   */
  template<typename T> void set_constant_value(T value)
  {
    const fn::MultiFunction &fn = this->construct_fn<fn::CustomMF_Constant<T>>(std::move(value));
    this->set_generator_fn(fn);
  }

  /**
   * Uses the first output of the given multi-function as value of the socket.
   */
  void set_generator_fn(const fn::MultiFunction &fn)
  {
    fn::MFFunctionNode &node = m_common.network.add_function(fn);
    this->set_socket(node.output(0));
  }

  /**
   * Define a multi-function socket that outputs the value of the bsocket.
   */
  void set_socket(fn::MFOutputSocket &socket)
  {
    m_built_socket = &socket;
  }

  fn::MFOutputSocket *built_socket()
  {
    return m_built_socket;
  }
};

/**
 * This class is used by node implementations to define how a user-level node expands into
 * multi-function nodes internally.
 */
class NodeMFNetworkBuilder : public MFNetworkBuilderBase {
 private:
  const DNode &m_node;

 public:
  NodeMFNetworkBuilder(CommonMFNetworkBuilderData &common, const DNode &node)
      : MFNetworkBuilderBase(common), m_node(node)
  {
  }

  /**
   * Tells the builder to build a function that corresponds to the node that is being built. It
   * will try to match up sockets.
   */
  template<typename T, typename... Args> void construct_and_set_matching_fn(Args &&... args)
  {
    const fn::MultiFunction &function = this->construct_fn<T>(std::forward<Args>(args)...);
    this->set_matching_fn(function);
  }

  /**
   * Tells the builder that the given function corresponds to the node that is being built. It will
   * try to match up sockets. For that it skips unavailable and non-data sockets.
   */
  void set_matching_fn(const fn::MultiFunction &function)
  {
    fn::MFFunctionNode &node = m_common.network.add_function(function);
    m_common.network_map.add_try_match(m_node, node);
  }

  /**
   * Returns the node that is currently being built.
   */
  bNode &bnode()
  {
    return *m_node.node_ref().bnode();
  }

  /**
   * Returns the node that is currently being built.
   */
  const DNode &dnode() const
  {
    return m_node;
  }
};

MFNetworkTreeMap insert_node_tree_into_mf_network(fn::MFNetwork &network,
                                                  const DerivedNodeTree &tree,
                                                  ResourceCollector &resources);

}  // namespace bke
}  // namespace blender

#endif /* __BKE_NODE_TREE_FUNCTION_HH__ */
