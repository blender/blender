// SPDX-FileCopyrightText: 2006 Stephan Diederich
// SPDX-FileCopyrightText: 2024 Blender Authors
// SPDX-License-Identifier: MIT
//
// Adapted from boost::graph

#include <algorithm>
#include <cassert>
#include <list>
#include <queue>
#include <tuple>
#include <vector>

namespace qflow {

class BoykovKolmogorovMaxFlow {
  // Types
  enum class Color { white, black, gray };

  struct Edge {
    int source;
    int target;
  };

  const int NULL_VERTEX = -1;
  const int NULL_EDGE = -1;

 public:
  BoykovKolmogorovMaxFlow() = default;

  void resize(int num_verts, int num_edges)
  {
    m_graph_edges.resize(num_edges);
    m_graph_out_edges.resize(num_verts);
    m_cap_map.resize(num_edges);
    m_res_cap_map.resize(num_edges);
    m_rev_edge_map.resize(num_edges);
    m_pre_map.resize(num_verts, 0);
    m_tree_map.resize(num_verts, Color::gray);
    m_dist_map.resize(num_verts, 0);
    m_in_active_list_map.resize(num_verts, false);
    m_has_parent_map.resize(num_verts, false);
    m_time_map.resize(num_verts, 0);
  }

  void set_edge(const int edge,
                const int reverse_edge,
                const int source,
                const int target,
                const int capacity)
  {
    assert(edge < m_graph_edges.size());

    m_graph_edges[edge] = Edge{source, target};
    m_graph_out_edges[source].push_back(edge);
    m_rev_edge_map[edge] = reverse_edge;

    // Initialize flow to zero which means initializing
    // the residual capacity equal to the capacity
    m_cap_map[edge] = capacity;
    m_res_cap_map[edge] = capacity;
  }

  int edge_capacity(const int edge)
  {
    return m_cap_map[edge];
  }

  int edge_residual_capacity(const int edge)
  {
    return m_res_cap_map[edge];
  }

  const std::vector<int> &vertex_out_edges(const int vertex)
  {
    return m_graph_out_edges[vertex];
  }

  int max_flow(int src, int sink)
  {
    m_source = src;
    m_sink = sink;

    // init the search trees with the two terminals
    m_tree_map[m_source] = Color::black;
    m_tree_map[m_sink] = Color::white;
    m_time_map[m_source] = 1;
    m_time_map[m_sink] = 1;

    // augment direct paths from SOURCE->SINK and SOURCE->VERTEX->SINK
    augment_direct_paths();
    // start the main-loop
    while (true) {
      bool path_found;
      int connecting_edge;
      std::tie(connecting_edge, path_found) = grow();  // find a path from source to sink
      if (!path_found) {
        // we're finished, no more paths were found
        break;
      }
      ++m_time;
      augment(connecting_edge);  // augment that path
      adopt();                   // rebuild search tree structure
    }
    return m_flow;
  }

 protected:
  int lookup_edge(int source, int target)
  {
    for (const int e : m_graph_out_edges[source]) {
      if (m_graph_edges[e].target == target) {
        return e;
      }
    }
    return NULL_EDGE;
  }

  void augment_direct_paths()
  {
    // in a first step, we augment all direct paths from
    // source->NODE->sink and additionally paths from source->sink. This
    // improves especially graphcuts for segmentation, as most of the
    // nodes have source/sink connects but shouldn't have an impact on
    // other maxflow problems (this is done in grow() anyway)
    for (const int ei : m_graph_out_edges[m_source]) {
      int from_source = ei;
      int current_node = m_graph_edges[from_source].target;
      if (current_node == m_sink) {
        int cap = m_res_cap_map[from_source];
        m_res_cap_map[from_source] = 0;
        m_flow += cap;
        continue;
      }
      const int to_sink = lookup_edge(current_node, m_sink);
      if (to_sink != NULL_EDGE) {
        int cap_from_source = m_res_cap_map[from_source];
        int cap_to_sink = m_res_cap_map[to_sink];
        if (cap_from_source > cap_to_sink) {
          m_tree_map[current_node] = Color::black;
          add_active_node(current_node);
          set_edge_to_parent(current_node, from_source);
          m_dist_map[current_node] = 1;
          m_time_map[current_node] = 1;
          // add stuff to flow and update residuals. we dont need
          // to update reverse_edges, as incoming/outgoing edges
          // to/from source/sink don't count for max-flow
          m_res_cap_map[from_source] = m_res_cap_map[from_source] - cap_to_sink;
          m_res_cap_map[to_sink] = 0;
          m_flow += cap_to_sink;
        }
        else if (cap_to_sink > 0) {
          m_tree_map[current_node] = Color::white;
          add_active_node(current_node);
          set_edge_to_parent(current_node, to_sink);
          m_dist_map[current_node] = 1;
          m_time_map[current_node] = 1;
          // add stuff to flow and update residuals. we dont need
          // to update reverse_edges, as incoming/outgoing edges
          // to/from source/sink don't count for max-flow
          m_res_cap_map[to_sink] = m_res_cap_map[to_sink] - cap_from_source;
          m_res_cap_map[from_source] = 0;
          m_flow += cap_from_source;
        }
      }
      else if (m_res_cap_map[from_source]) {
        // there is no sink connect, so we can't augment this path,
        // but to avoid adding m_source to the active nodes, we just
        // activate this node and set the approciate things
        m_tree_map[current_node] = Color::black;
        set_edge_to_parent(current_node, from_source);
        m_dist_map[current_node] = 1;
        m_time_map[current_node] = 1;
        add_active_node(current_node);
      }
    }
    for (const int ei : m_graph_out_edges[m_sink]) {
      int to_sink = m_rev_edge_map[ei];
      int current_node = m_graph_edges[to_sink].source;
      if (m_res_cap_map[to_sink]) {
        m_tree_map[current_node] = Color::white;
        set_edge_to_parent(current_node, to_sink);
        m_dist_map[current_node] = 1;
        m_time_map[current_node] = 1;
        add_active_node(current_node);
      }
    }
  }

  /**
   * Returns a pair of an edge and a boolean. if the bool is true, the
   * edge is a connection of a found path from s->t , read "the link" and
   * m_graph_edges[returnVal].source is the end of the path found in the
   * source-tree m_graph_edges[returnVal].target is the beginning of the path found
   * in the sink-tree
   */
  std::pair<int, bool> grow()
  {
    assert(m_orphans.empty());
    int current_node;
    while ((current_node = get_next_active_node()) != NULL_VERTEX) {  // if there is one
      assert(m_tree_map[current_node] != Color::gray &&
             (has_parent(current_node) || current_node == m_source || current_node == m_sink));

      if (m_tree_map[current_node] == Color::black) {
        // source tree growing
        if (current_node != m_last_grow_vertex) {
          m_last_grow_vertex = current_node;
          m_last_grow_out_edge = 0;
        }
        const std::vector<int> &out_edges = m_graph_out_edges[m_last_grow_vertex];
        for (; m_last_grow_out_edge < out_edges.size(); m_last_grow_out_edge++) {
          int out_edge = out_edges[m_last_grow_out_edge];
          if (m_res_cap_map[out_edge] > 0) {  // check if we have capacity left on this edge
            int other_node = m_graph_edges[out_edge].target;
            if (m_tree_map[other_node] == Color::gray) {  // it's a free node
              // aquire other node to our search tree
              m_tree_map[other_node] = Color::black;
              set_edge_to_parent(other_node, out_edge);               // set us as parent
              m_dist_map[other_node] = m_dist_map[current_node] + 1;  // and update the
                                                                      // distance-heuristic
              m_time_map[other_node] = m_time_map[current_node];
              add_active_node(other_node);
            }
            else if (m_tree_map[other_node] == Color::black) {
              // we do this to get shorter paths. check if we
              // are nearer to the source as its parent is
              if (is_closer_to_terminal(current_node, other_node)) {
                set_edge_to_parent(other_node, out_edge);
                m_dist_map[other_node] = m_dist_map[current_node] + 1;
                m_time_map[other_node] = m_time_map[current_node];
              }
            }
            else {
              assert(m_tree_map[other_node] == Color::white);
              // kewl, found a path from one to the other
              // search tree, return
              // the connecting edge in src->sink dir
              return std::make_pair(out_edge, true);
            }
          }
        }  // for all out-edges
      }  // source-tree-growing
      else {
        assert(m_tree_map[current_node] == Color::white);
        if (current_node != m_last_grow_vertex) {
          m_last_grow_vertex = current_node;
          m_last_grow_out_edge = 0;
        }
        const std::vector<int> &out_edges = m_graph_out_edges[m_last_grow_vertex];
        for (; m_last_grow_out_edge < out_edges.size(); m_last_grow_out_edge++) {
          int in_edge = m_rev_edge_map[out_edges[m_last_grow_out_edge]];
          if (m_res_cap_map[in_edge] > 0) {  // check if there is capacity left
            int other_node = m_graph_edges[in_edge].source;
            if (m_tree_map[other_node] == Color::gray) {  // it's a free node
              // aquire other node to our search tree
              m_tree_map[other_node] = Color::white;
              set_edge_to_parent(other_node, in_edge);                // set us as parent
              add_active_node(other_node);                            // activate that node
              m_dist_map[other_node] = m_dist_map[current_node] + 1;  // set its distance
              m_time_map[other_node] = m_time_map[current_node];      // and time
            }
            else if (m_tree_map[other_node] == Color::white) {
              if (is_closer_to_terminal(current_node, other_node)) {
                // we are closer to the sink than its parent
                // is, so we "adopt" him
                set_edge_to_parent(other_node, in_edge);
                m_dist_map[other_node] = m_dist_map[current_node] + 1;
                m_time_map[other_node] = m_time_map[current_node];
              }
            }
            else {
              assert(m_tree_map[other_node] == Color::black);
              // kewl, found a path from one to the other
              // search tree,
              // return the connecting edge in src->sink dir
              return std::make_pair(in_edge, true);
            }
          }
        }  // for all out-edges
      }  // sink-tree growing

      // all edges of that node are processed, and no more paths were
      // found.
      // remove if from the front of the active queue
      finish_node(current_node);
    }  // while active_nodes not empty

    // no active nodes anymore and no path found, we're done
    return std::make_pair(int(), false);
  }

  /**
   * augments path from s->t and updates residual graph
   * m_graph_edges[e].source is the end of the path found in the source-tree
   * m_graph_edges[e].target is the beginning of the path found in the sink-tree
   * this phase generates orphans on satured edges, if the attached verts
   * are from different search-trees orphans are ordered in distance to
   * sink/source. first the farest from the source are front_inserted into
   * the orphans list, and after that the sink-tree-orphans are
   * front_inserted. when going to adoption stage the orphans are
   * popped_front, and so we process the nearest verts to the terminals
   * first
   */
  void augment(int e)
  {
    assert(m_tree_map[m_graph_edges[e].target] == Color::white);
    assert(m_tree_map[m_graph_edges[e].source] == Color::black);
    assert(m_orphans.empty());

    const int bottleneck = find_bottleneck(e);
    // now we push the found flow through the path
    // for each edge we saturate we have to look for the verts that
    // belong to that edge, one of them becomes an orphans now process
    // the connecting edge
    m_res_cap_map[e] = m_res_cap_map[e] - bottleneck;
    assert(m_res_cap_map[e] >= 0);
    m_res_cap_map[m_rev_edge_map[e]] = m_res_cap_map[m_rev_edge_map[e]] + bottleneck;

    // now we follow the path back to the source
    int current_node = m_graph_edges[e].source;
    while (current_node != m_source) {
      int pred = get_edge_to_parent(current_node);
      m_res_cap_map[pred] = m_res_cap_map[pred] - bottleneck;
      assert(m_res_cap_map[pred] >= 0);
      m_res_cap_map[m_rev_edge_map[pred]] = m_res_cap_map[m_rev_edge_map[pred]] + bottleneck;
      if (m_res_cap_map[pred] == 0) {
        set_no_parent(current_node);
        m_orphans.push_front(current_node);
      }
      current_node = m_graph_edges[pred].source;
    }
    // then go forward in the sink-tree
    current_node = m_graph_edges[e].target;
    while (current_node != m_sink) {
      int pred = get_edge_to_parent(current_node);
      m_res_cap_map[pred] = m_res_cap_map[pred] - bottleneck;
      assert(m_res_cap_map[pred] >= 0);
      m_res_cap_map[m_rev_edge_map[pred]] = m_res_cap_map[m_rev_edge_map[pred]] + bottleneck;
      if (m_res_cap_map[pred] == 0) {
        set_no_parent(current_node);
        m_orphans.push_front(current_node);
      }
      current_node = m_graph_edges[pred].target;
    }
    // and add it to the max-flow
    m_flow += bottleneck;
  }

  /**
   * returns the bottleneck of a s->t path (end_of_path is last vertex in
   * source-tree, begin_of_path is first vertex in sink-tree)
   */
  int find_bottleneck(int e)
  {
    int minimum_cap = m_res_cap_map[e];
    int current_node = m_graph_edges[e].source;
    // first go back in the source tree
    while (current_node != m_source) {
      int pred = get_edge_to_parent(current_node);
      minimum_cap = std::min(minimum_cap, m_res_cap_map[pred]);
      current_node = m_graph_edges[pred].source;
    }
    // then go forward in the sink-tree
    current_node = m_graph_edges[e].target;
    while (current_node != m_sink) {
      int pred = get_edge_to_parent(current_node);
      minimum_cap = std::min(minimum_cap, m_res_cap_map[pred]);
      current_node = m_graph_edges[pred].target;
    }
    return minimum_cap;
  }

  /**
   * rebuild search trees
   * empty the queue of orphans, and find new parents for them or just
   * drop them from the search trees
   */
  void adopt()
  {
    while (!m_orphans.empty() || !m_child_orphans.empty()) {
      int current_node;
      if (m_child_orphans.empty()) {
        // get the next orphan from the main-queue  and remove it
        current_node = m_orphans.front();
        m_orphans.pop_front();
      }
      else {
        current_node = m_child_orphans.front();
        m_child_orphans.pop();
      }
      if (m_tree_map[current_node] == Color::black) {
        // we're in the source-tree
        int min_distance = (std::numeric_limits<int>::max)();
        int new_parent_edge;
        for (const int ei : m_graph_out_edges[current_node]) {
          const int in_edge = m_rev_edge_map[ei];
          assert(m_graph_edges[in_edge].target == current_node);  // we should be the target of
                                                                  // this edge
          if (m_res_cap_map[in_edge] > 0) {
            int other_node = m_graph_edges[in_edge].source;
            if (m_tree_map[other_node] == Color::black && has_source_connect(other_node)) {
              if (m_dist_map[other_node] < min_distance) {
                min_distance = m_dist_map[other_node];
                new_parent_edge = in_edge;
              }
            }
          }
        }
        if (min_distance != (std::numeric_limits<int>::max)()) {
          set_edge_to_parent(current_node, new_parent_edge);
          m_dist_map[current_node] = min_distance + 1;
          m_time_map[current_node] = m_time;
        }
        else {
          m_time_map[current_node] = 0;
          for (const int ei : m_graph_out_edges[current_node]) {
            int in_edge = m_rev_edge_map[ei];
            int other_node = m_graph_edges[in_edge].source;
            if (m_tree_map[other_node] == Color::black && other_node != m_source) {
              if (m_res_cap_map[in_edge] > 0) {
                add_active_node(other_node);
              }
              if (has_parent(other_node) &&
                  m_graph_edges[get_edge_to_parent(other_node)].source == current_node)
              {
                // we are the parent of that node
                // it has to find a new parent, too
                set_no_parent(other_node);
                m_child_orphans.push(other_node);
              }
            }
          }
          m_tree_map[current_node] = Color::gray;
        }  // no parent found
      }  // source-tree-adoption
      else {
        // now we should be in the sink-tree, check that...
        assert(m_tree_map[current_node] == Color::white);
        int new_parent_edge;
        int min_distance = (std::numeric_limits<int>::max)();
        for (const int ei : m_graph_out_edges[current_node]) {
          const int out_edge = ei;
          if (m_res_cap_map[out_edge] > 0) {
            const int other_node = m_graph_edges[out_edge].target;
            if (m_tree_map[other_node] == Color::white && has_sink_connect(other_node)) {
              if (m_dist_map[other_node] < min_distance) {
                min_distance = m_dist_map[other_node];
                new_parent_edge = out_edge;
              }
            }
          }
        }
        if (min_distance != (std::numeric_limits<int>::max)()) {
          set_edge_to_parent(current_node, new_parent_edge);
          m_dist_map[current_node] = min_distance + 1;
          m_time_map[current_node] = m_time;
        }
        else {
          m_time_map[current_node] = 0;
          for (const int ei : m_graph_out_edges[current_node]) {
            const int out_edge = ei;
            const int other_node = m_graph_edges[out_edge].target;
            if (m_tree_map[other_node] == Color::white && other_node != m_sink) {
              if (m_res_cap_map[out_edge] > 0) {
                add_active_node(other_node);
              }
              if (has_parent(other_node) &&
                  m_graph_edges[get_edge_to_parent(other_node)].target == current_node)
              {
                // we were it's parent, so it has to find a
                // new one, too
                set_no_parent(other_node);
                m_child_orphans.push(other_node);
              }
            }
          }
          m_tree_map[current_node] = Color::gray;
        }  // no parent found
      }  // sink-tree adoption
    }  // while !orphans.empty()
  }  // adopt

  /**
   * return next active vertex if there is one, otherwise a null_vertex
   */
  int get_next_active_node()
  {
    while (true) {
      if (m_active_nodes.empty()) {
        return NULL_VERTEX;
      }
      int v = m_active_nodes.front();

      // if it has no parent, this node can't be active (if its not
      // source or sink)
      if (!has_parent(v) && v != m_source && v != m_sink) {
        m_active_nodes.pop();
        m_in_active_list_map[v] = false;
      }
      else {
        assert(m_tree_map[v] == Color::black || m_tree_map[v] == Color::white);
        return v;
      }
    }
  }

  /**
   * adds v as an active vertex, but only if its not in the list already
   */
  void add_active_node(int v)
  {
    assert(m_tree_map[v] != Color::gray);
    if (m_in_active_list_map[v]) {
      if (m_last_grow_vertex == v) {
        m_last_grow_vertex = NULL_VERTEX;
      }
      return;
    }

    m_in_active_list_map[v] = true;
    m_active_nodes.push(v);
  }

  /**
   * finish_node removes a node from the front of the active queue (its
   * called in grow phase, if no more paths can be found using this node)
   */
  void finish_node(int v)
  {
    assert(m_active_nodes.front() == v);
    m_active_nodes.pop();
    m_in_active_list_map[v] = false;
    m_last_grow_vertex = NULL_VERTEX;
  }

  /**
   * returns edge to parent vertex of v;
   */
  int get_edge_to_parent(int v) const
  {
    return m_pre_map[v];
  }

  /**
   * returns true if the edge stored in m_pre_map[v] is a valid entry
   */
  bool has_parent(int v) const
  {
    return m_has_parent_map[v];
  }

  /**
   * sets edge to parent vertex of v;
   */
  void set_edge_to_parent(int v, int f_edge_to_parent)
  {
    assert(m_res_cap_map[f_edge_to_parent] > 0);
    m_pre_map[v] = f_edge_to_parent;
    m_has_parent_map[v] = true;
  }

  /**
   * removes the edge to parent of v (this is done by invalidating the
   * entry an additional map)
   */
  void set_no_parent(int v)
  {
    m_has_parent_map[v] = false;
  }

  /**
   * checks if vertex v has a connect to the sink-vertex (@var m_sink)
   * @param v the vertex which is checked
   * @return true if a path to the sink was found, false if not
   */
  bool has_sink_connect(int v)
  {
    int current_distance = 0;
    int current_vertex = v;
    while (true) {
      if (m_time_map[current_vertex] == m_time) {
        // we found a node which was already checked this round. use
        // it for distance calculations
        current_distance += m_dist_map[current_vertex];
        break;
      }
      if (current_vertex == m_sink) {
        m_time_map[m_sink] = m_time;
        break;
      }
      if (has_parent(current_vertex)) {
        // it has a parent, so get it
        current_vertex = m_graph_edges[get_edge_to_parent(current_vertex)].target;
        ++current_distance;
      }
      else {
        // no path found
        return false;
      }
    }
    current_vertex = v;
    while (m_time_map[current_vertex] != m_time) {
      m_dist_map[current_vertex] = current_distance;
      --current_distance;
      m_time_map[current_vertex] = m_time;
      current_vertex = m_graph_edges[get_edge_to_parent(current_vertex)].target;
    }
    return true;
  }

  /**
   * checks if vertex v has a connect to the source-vertex (@var m_source)
   * @param v the vertex which is checked
   * @return true if a path to the source was found, false if not
   */
  bool has_source_connect(int v)
  {
    int current_distance = 0;
    int current_vertex = v;
    while (true) {
      if (m_time_map[current_vertex] == m_time) {
        // we found a node which was already checked this round. use
        // it for distance calculations
        current_distance += m_dist_map[current_vertex];
        break;
      }
      if (current_vertex == m_source) {
        m_time_map[m_source] = m_time;
        break;
      }
      if (has_parent(current_vertex)) {
        // it has a parent, so get it
        current_vertex = m_graph_edges[get_edge_to_parent(current_vertex)].source;
        ++current_distance;
      }
      else {
        // no path found
        return false;
      }
    }
    current_vertex = v;
    while (m_time_map[current_vertex] != m_time) {
      m_dist_map[current_vertex] = current_distance;
      --current_distance;
      m_time_map[current_vertex] = m_time;
      current_vertex = m_graph_edges[get_edge_to_parent(current_vertex)].source;
    }
    return true;
  }

  /**
   * returns true, if p is closer to a terminal than q
   */
  bool is_closer_to_terminal(int p, int q)
  {
    // checks the timestamps first, to build no cycles, and after that
    // the real distance
    return (m_time_map[q] <= m_time_map[p] && m_dist_map[q] > m_dist_map[p] + 1);
  }

  // Member variables
  std::vector<Edge> m_graph_edges;
  std::vector<std::vector<int>> m_graph_out_edges;

  std::vector<int> m_cap_map;
  std::vector<int> m_res_cap_map;
  std::vector<int> m_rev_edge_map;
  std::vector<int> m_pre_map;  // stores paths found in the growth stage
  std::vector<Color> m_tree_map;  // maps each vertex into white, black or gray
  std::vector<int> m_dist_map;  // stores distance to source/sink nodes

  int m_source;
  int m_sink;

  std::queue<int> m_active_nodes;
  std::vector<bool> m_in_active_list_map;

  std::list<int> m_orphans;
  std::queue<int> m_child_orphans;  // we use a second queuqe for child orphans, as
                                    // they are FIFO processed
  std::vector<bool> m_has_parent_map;

  std::vector<int> m_time_map;  // timestamp of each node, used for
                                // sink/source-path calculations
  int m_flow = 0;
  int m_time = 1;

  int m_last_grow_vertex = NULL_VERTEX;
  int m_last_grow_out_edge = 0;
};

}  // namespace qflow
