/* -*- mode: C++; indent-tabs-mode: nil; -*-
 *
 * This file is a part of LEMON, a generic C++ optimization library.
 *
 * Copyright (C) 2003-2013
 * Egervary Jeno Kombinatorikus Optimalizalasi Kutatocsoport
 * (Egervary Research Group on Combinatorial Optimization, EGRES).
 *
 * Permission to use, modify and distribute this software is granted
 * provided that this copyright notice appears in all copies. For
 * precise terms see the accompanying LICENSE file.
 *
 * This software is provided "AS IS" with no warranty of any kind,
 * express or implied, and with no claim as to its suitability for any
 * purpose.
 *
 */

#include <string>

#include <lemon/concepts/digraph.h>
#include <lemon/concepts/graph.h>
#include <lemon/concepts/bpgraph.h>

#include <lemon/list_graph.h>
#include <lemon/smart_graph.h>
#include <lemon/lgf_reader.h>

#include "test_tools.h"

struct ReaderConverter {
  int operator()(const std::string& str) const {
    return str.length();
  }
};

struct WriterConverter {
  std::string operator()(int value) const {
    return std::string(value, '*');
  }
};

void checkDigraphReaderCompile() {
  typedef lemon::concepts::ExtendableDigraphComponent<
    lemon::concepts::Digraph> Digraph;
  Digraph digraph;
  Digraph::NodeMap<int> node_map(digraph);
  Digraph::ArcMap<int> arc_map(digraph);
  Digraph::Node node;
  Digraph::Arc arc;
  int attr;

  lemon::DigraphReader<Digraph> reader(digraph, "filename");
  reader.nodeMap("node_map", node_map);
  reader.nodeMap("node_map", node_map, ReaderConverter());
  reader.arcMap("arc_map", arc_map);
  reader.arcMap("arc_map", arc_map, ReaderConverter());
  reader.attribute("attr", attr);
  reader.attribute("attr", attr, ReaderConverter());
  reader.node("node", node);
  reader.arc("arc", arc);

  reader.nodes("alt_nodes_caption");
  reader.arcs("alt_arcs_caption");
  reader.attributes("alt_attrs_caption");

  reader.useNodes(node_map);
  reader.useNodes(node_map, WriterConverter());
  reader.useArcs(arc_map);
  reader.useArcs(arc_map, WriterConverter());

  reader.skipNodes();
  reader.skipArcs();

  reader.run();

  lemon::DigraphReader<Digraph> reader2(digraph, std::cin);
}

void checkDigraphWriterCompile() {
  typedef lemon::concepts::Digraph Digraph;
  Digraph digraph;
  Digraph::NodeMap<int> node_map(digraph);
  Digraph::ArcMap<int> arc_map(digraph);
  Digraph::Node node;
  Digraph::Arc arc;
  int attr;

  lemon::DigraphWriter<Digraph> writer(digraph, "filename");
  writer.nodeMap("node_map", node_map);
  writer.nodeMap("node_map", node_map, WriterConverter());
  writer.arcMap("arc_map", arc_map);
  writer.arcMap("arc_map", arc_map, WriterConverter());
  writer.attribute("attr", attr);
  writer.attribute("attr", attr, WriterConverter());
  writer.node("node", node);
  writer.arc("arc", arc);

  writer.nodes("alt_nodes_caption");
  writer.arcs("alt_arcs_caption");
  writer.attributes("alt_attrs_caption");

  writer.skipNodes();
  writer.skipArcs();

  writer.run();
}

void checkGraphReaderCompile() {
  typedef lemon::concepts::ExtendableGraphComponent<
    lemon::concepts::Graph> Graph;
  Graph graph;
  Graph::NodeMap<int> node_map(graph);
  Graph::ArcMap<int> arc_map(graph);
  Graph::EdgeMap<int> edge_map(graph);
  Graph::Node node;
  Graph::Arc arc;
  Graph::Edge edge;
  int attr;

  lemon::GraphReader<Graph> reader(graph, "filename");
  reader.nodeMap("node_map", node_map);
  reader.nodeMap("node_map", node_map, ReaderConverter());
  reader.arcMap("arc_map", arc_map);
  reader.arcMap("arc_map", arc_map, ReaderConverter());
  reader.edgeMap("edge_map", edge_map);
  reader.edgeMap("edge_map", edge_map, ReaderConverter());
  reader.attribute("attr", attr);
  reader.attribute("attr", attr, ReaderConverter());
  reader.node("node", node);
  reader.arc("arc", arc);

  reader.nodes("alt_nodes_caption");
  reader.edges("alt_edges_caption");
  reader.attributes("alt_attrs_caption");

  reader.useNodes(node_map);
  reader.useNodes(node_map, WriterConverter());
  reader.useEdges(edge_map);
  reader.useEdges(edge_map, WriterConverter());

  reader.skipNodes();
  reader.skipEdges();

  reader.run();

  lemon::GraphReader<Graph> reader2(graph, std::cin);
}

void checkGraphWriterCompile() {
  typedef lemon::concepts::Graph Graph;
  Graph graph;
  Graph::NodeMap<int> node_map(graph);
  Graph::ArcMap<int> arc_map(graph);
  Graph::EdgeMap<int> edge_map(graph);
  Graph::Node node;
  Graph::Arc arc;
  Graph::Edge edge;
  int attr;

  lemon::GraphWriter<Graph> writer(graph, "filename");
  writer.nodeMap("node_map", node_map);
  writer.nodeMap("node_map", node_map, WriterConverter());
  writer.arcMap("arc_map", arc_map);
  writer.arcMap("arc_map", arc_map, WriterConverter());
  writer.edgeMap("edge_map", edge_map);
  writer.edgeMap("edge_map", edge_map, WriterConverter());
  writer.attribute("attr", attr);
  writer.attribute("attr", attr, WriterConverter());
  writer.node("node", node);
  writer.arc("arc", arc);
  writer.edge("edge", edge);

  writer.nodes("alt_nodes_caption");
  writer.edges("alt_edges_caption");
  writer.attributes("alt_attrs_caption");

  writer.skipNodes();
  writer.skipEdges();

  writer.run();

  lemon::GraphWriter<Graph> writer2(graph, std::cout);
}

void checkBpGraphReaderCompile() {
  typedef lemon::concepts::ExtendableBpGraphComponent<
    lemon::concepts::BpGraph> BpGraph;
  BpGraph graph;
  BpGraph::NodeMap<int> node_map(graph);
  BpGraph::RedNodeMap<int> red_node_map(graph);
  BpGraph::BlueNodeMap<int> blue_node_map(graph);
  BpGraph::ArcMap<int> arc_map(graph);
  BpGraph::EdgeMap<int> edge_map(graph);
  BpGraph::Node node;
  BpGraph::RedNode red_node;
  BpGraph::BlueNode blue_node;
  BpGraph::Arc arc;
  BpGraph::Edge edge;
  int attr;

  lemon::BpGraphReader<BpGraph> reader(graph, "filename");
  reader.nodeMap("node_map", node_map);
  reader.nodeMap("node_map", node_map, ReaderConverter());
  reader.redNodeMap("red_node_map", red_node_map);
  reader.redNodeMap("red_node_map", red_node_map, ReaderConverter());
  reader.blueNodeMap("blue_node_map", blue_node_map);
  reader.blueNodeMap("blue_node_map", blue_node_map, ReaderConverter());
  reader.arcMap("arc_map", arc_map);
  reader.arcMap("arc_map", arc_map, ReaderConverter());
  reader.edgeMap("edge_map", edge_map);
  reader.edgeMap("edge_map", edge_map, ReaderConverter());
  reader.attribute("attr", attr);
  reader.attribute("attr", attr, ReaderConverter());
  reader.node("node", node);
  reader.redNode("red_node", red_node);
  reader.blueNode("blue_node", blue_node);
  reader.arc("arc", arc);

  reader.nodes("alt_nodes_caption");
  reader.edges("alt_edges_caption");
  reader.attributes("alt_attrs_caption");

  reader.useNodes(node_map);
  reader.useNodes(node_map, WriterConverter());
  reader.useEdges(edge_map);
  reader.useEdges(edge_map, WriterConverter());

  reader.skipNodes();
  reader.skipEdges();

  reader.run();

  lemon::BpGraphReader<BpGraph> reader2(graph, std::cin);
}

void checkBpGraphWriterCompile() {
  typedef lemon::concepts::BpGraph BpGraph;
  BpGraph graph;
  BpGraph::NodeMap<int> node_map(graph);
  BpGraph::RedNodeMap<int> red_node_map(graph);
  BpGraph::BlueNodeMap<int> blue_node_map(graph);
  BpGraph::ArcMap<int> arc_map(graph);
  BpGraph::EdgeMap<int> edge_map(graph);
  BpGraph::Node node;
  BpGraph::RedNode red_node;
  BpGraph::BlueNode blue_node;
  BpGraph::Arc arc;
  BpGraph::Edge edge;
  int attr;

  lemon::BpGraphWriter<BpGraph> writer(graph, "filename");
  writer.nodeMap("node_map", node_map);
  writer.nodeMap("node_map", node_map, WriterConverter());
  writer.redNodeMap("red_node_map", red_node_map);
  writer.redNodeMap("red_node_map", red_node_map, WriterConverter());
  writer.blueNodeMap("blue_node_map", blue_node_map);
  writer.blueNodeMap("blue_node_map", blue_node_map, WriterConverter());
  writer.arcMap("arc_map", arc_map);
  writer.arcMap("arc_map", arc_map, WriterConverter());
  writer.edgeMap("edge_map", edge_map);
  writer.edgeMap("edge_map", edge_map, WriterConverter());
  writer.attribute("attr", attr);
  writer.attribute("attr", attr, WriterConverter());
  writer.node("node", node);
  writer.redNode("red_node", red_node);
  writer.blueNode("blue_node", blue_node);
  writer.arc("arc", arc);

  writer.nodes("alt_nodes_caption");
  writer.edges("alt_edges_caption");
  writer.attributes("alt_attrs_caption");

  writer.skipNodes();
  writer.skipEdges();

  writer.run();

  lemon::BpGraphWriter<BpGraph> writer2(graph, std::cout);
}

void checkDigraphReaderWriter() {
  typedef lemon::SmartDigraph Digraph;
  Digraph digraph;
  Digraph::Node n1 = digraph.addNode();
  Digraph::Node n2 = digraph.addNode();
  Digraph::Node n3 = digraph.addNode();

  Digraph::Arc a1 = digraph.addArc(n1, n2);
  Digraph::Arc a2 = digraph.addArc(n2, n3);

  Digraph::NodeMap<int> node_map(digraph);
  node_map[n1] = 11;
  node_map[n2] = 12;
  node_map[n3] = 13;

  Digraph::ArcMap<int> arc_map(digraph);
  arc_map[a1] = 21;
  arc_map[a2] = 22;

  int attr = 100;

  std::ostringstream os;
  lemon::DigraphWriter<Digraph> writer(digraph, os);

  writer.nodeMap("node_map1", node_map);
  writer.nodeMap("node_map2", node_map, WriterConverter());
  writer.arcMap("arc_map1", arc_map);
  writer.arcMap("arc_map2", arc_map, WriterConverter());
  writer.node("node", n2);
  writer.arc("arc", a1);
  writer.attribute("attr1", attr);
  writer.attribute("attr2", attr, WriterConverter());

  writer.run();

  typedef lemon::ListDigraph ExpDigraph;
  ExpDigraph exp_digraph;
  ExpDigraph::NodeMap<int> exp_node_map1(exp_digraph);
  ExpDigraph::NodeMap<int> exp_node_map2(exp_digraph);
  ExpDigraph::ArcMap<int> exp_arc_map1(exp_digraph);
  ExpDigraph::ArcMap<int> exp_arc_map2(exp_digraph);
  ExpDigraph::Node exp_n2;
  ExpDigraph::Arc exp_a1;
  int exp_attr1;
  int exp_attr2;

  std::istringstream is(os.str());
  lemon::DigraphReader<ExpDigraph> reader(exp_digraph, is);

  reader.nodeMap("node_map1", exp_node_map1);
  reader.nodeMap("node_map2", exp_node_map2, ReaderConverter());
  reader.arcMap("arc_map1", exp_arc_map1);
  reader.arcMap("arc_map2", exp_arc_map2, ReaderConverter());
  reader.node("node", exp_n2);
  reader.arc("arc", exp_a1);
  reader.attribute("attr1", exp_attr1);
  reader.attribute("attr2", exp_attr2, ReaderConverter());

  reader.run();

  check(lemon::countNodes(exp_digraph) == 3, "Wrong number of nodes");
  check(lemon::countArcs(exp_digraph) == 2, "Wrong number of arcs");
  check(exp_node_map1[exp_n2] == 12, "Wrong map value");
  check(exp_node_map2[exp_n2] == 12, "Wrong map value");
  check(exp_arc_map1[exp_a1] == 21, "Wrong map value");
  check(exp_arc_map2[exp_a1] == 21, "Wrong map value");
  check(exp_attr1 == 100, "Wrong attr value");
  check(exp_attr2 == 100, "Wrong attr value");
}

void checkGraphReaderWriter() {
  typedef lemon::SmartGraph Graph;
  Graph graph;
  Graph::Node n1 = graph.addNode();
  Graph::Node n2 = graph.addNode();
  Graph::Node n3 = graph.addNode();

  Graph::Edge e1 = graph.addEdge(n1, n2);
  Graph::Edge e2 = graph.addEdge(n2, n3);

  Graph::NodeMap<int> node_map(graph);
  node_map[n1] = 11;
  node_map[n2] = 12;
  node_map[n3] = 13;

  Graph::EdgeMap<int> edge_map(graph);
  edge_map[e1] = 21;
  edge_map[e2] = 22;

  Graph::ArcMap<int> arc_map(graph);
  arc_map[graph.direct(e1, true)] = 211;
  arc_map[graph.direct(e1, false)] = 212;
  arc_map[graph.direct(e2, true)] = 221;
  arc_map[graph.direct(e2, false)] = 222;

  int attr = 100;

  std::ostringstream os;
  lemon::GraphWriter<Graph> writer(graph, os);

  writer.nodeMap("node_map1", node_map);
  writer.nodeMap("node_map2", node_map, WriterConverter());
  writer.edgeMap("edge_map1", edge_map);
  writer.edgeMap("edge_map2", edge_map, WriterConverter());
  writer.arcMap("arc_map1", arc_map);
  writer.arcMap("arc_map2", arc_map, WriterConverter());
  writer.node("node", n2);
  writer.edge("edge", e1);
  writer.arc("arc", graph.direct(e1, false));
  writer.attribute("attr1", attr);
  writer.attribute("attr2", attr, WriterConverter());

  writer.run();

  typedef lemon::ListGraph ExpGraph;
  ExpGraph exp_graph;
  ExpGraph::NodeMap<int> exp_node_map1(exp_graph);
  ExpGraph::NodeMap<int> exp_node_map2(exp_graph);
  ExpGraph::EdgeMap<int> exp_edge_map1(exp_graph);
  ExpGraph::EdgeMap<int> exp_edge_map2(exp_graph);
  ExpGraph::ArcMap<int> exp_arc_map1(exp_graph);
  ExpGraph::ArcMap<int> exp_arc_map2(exp_graph);
  ExpGraph::Node exp_n2;
  ExpGraph::Edge exp_e1;
  ExpGraph::Arc exp_a1;
  int exp_attr1;
  int exp_attr2;

  std::istringstream is(os.str());
  lemon::GraphReader<ExpGraph> reader(exp_graph, is);

  reader.nodeMap("node_map1", exp_node_map1);
  reader.nodeMap("node_map2", exp_node_map2, ReaderConverter());
  reader.edgeMap("edge_map1", exp_edge_map1);
  reader.edgeMap("edge_map2", exp_edge_map2, ReaderConverter());
  reader.arcMap("arc_map1", exp_arc_map1);
  reader.arcMap("arc_map2", exp_arc_map2, ReaderConverter());
  reader.node("node", exp_n2);
  reader.edge("edge", exp_e1);
  reader.arc("arc", exp_a1);
  reader.attribute("attr1", exp_attr1);
  reader.attribute("attr2", exp_attr2, ReaderConverter());

  reader.run();

  check(lemon::countNodes(exp_graph) == 3, "Wrong number of nodes");
  check(lemon::countEdges(exp_graph) == 2, "Wrong number of edges");
  check(lemon::countArcs(exp_graph) == 4, "Wrong number of arcs");
  check(exp_node_map1[exp_n2] == 12, "Wrong map value");
  check(exp_node_map2[exp_n2] == 12, "Wrong map value");
  check(exp_edge_map1[exp_e1] == 21, "Wrong map value");
  check(exp_edge_map2[exp_e1] == 21, "Wrong map value");
  check(exp_arc_map1[exp_a1] == 212, "Wrong map value");
  check(exp_arc_map2[exp_a1] == 212, "Wrong map value");
  check(exp_attr1 == 100, "Wrong attr value");
  check(exp_attr2 == 100, "Wrong attr value");
}

void checkBpGraphReaderWriter() {
  typedef lemon::SmartBpGraph Graph;
  Graph graph;
  Graph::RedNode rn1 = graph.addRedNode();
  Graph::RedNode rn2 = graph.addRedNode();
  Graph::RedNode rn3 = graph.addRedNode();
  Graph::BlueNode bn1 = graph.addBlueNode();
  Graph::BlueNode bn2 = graph.addBlueNode();
  Graph::Node n = bn1;

  Graph::Edge e1 = graph.addEdge(rn1, bn1);
  Graph::Edge e2 = graph.addEdge(rn2, bn1);

  Graph::NodeMap<int> node_map(graph);
  node_map[rn1] = 11;
  node_map[rn2] = 12;
  node_map[rn3] = 13;
  node_map[bn1] = 14;
  node_map[bn2] = 15;

  Graph::NodeMap<int> red_node_map(graph);
  red_node_map[rn1] = 411;
  red_node_map[rn2] = 412;
  red_node_map[rn3] = 413;

  Graph::NodeMap<int> blue_node_map(graph);
  blue_node_map[bn1] = 414;
  blue_node_map[bn2] = 415;

  Graph::EdgeMap<int> edge_map(graph);
  edge_map[e1] = 21;
  edge_map[e2] = 22;

  Graph::ArcMap<int> arc_map(graph);
  arc_map[graph.direct(e1, true)] = 211;
  arc_map[graph.direct(e1, false)] = 212;
  arc_map[graph.direct(e2, true)] = 221;
  arc_map[graph.direct(e2, false)] = 222;

  int attr = 100;

  std::ostringstream os;
  lemon::BpGraphWriter<Graph> writer(graph, os);

  writer.nodeMap("node_map1", node_map);
  writer.nodeMap("node_map2", node_map, WriterConverter());
  writer.nodeMap("red_node_map1", red_node_map);
  writer.nodeMap("red_node_map2", red_node_map, WriterConverter());
  writer.nodeMap("blue_node_map1", blue_node_map);
  writer.nodeMap("blue_node_map2", blue_node_map, WriterConverter());
  writer.edgeMap("edge_map1", edge_map);
  writer.edgeMap("edge_map2", edge_map, WriterConverter());
  writer.arcMap("arc_map1", arc_map);
  writer.arcMap("arc_map2", arc_map, WriterConverter());
  writer.node("node", n);
  writer.redNode("red_node", rn1);
  writer.blueNode("blue_node", bn2);
  writer.edge("edge", e1);
  writer.arc("arc", graph.direct(e1, false));
  writer.attribute("attr1", attr);
  writer.attribute("attr2", attr, WriterConverter());

  writer.run();

  typedef lemon::ListBpGraph ExpGraph;
  ExpGraph exp_graph;
  ExpGraph::NodeMap<int> exp_node_map1(exp_graph);
  ExpGraph::NodeMap<int> exp_node_map2(exp_graph);
  ExpGraph::RedNodeMap<int> exp_red_node_map1(exp_graph);
  ExpGraph::RedNodeMap<int> exp_red_node_map2(exp_graph);
  ExpGraph::BlueNodeMap<int> exp_blue_node_map1(exp_graph);
  ExpGraph::BlueNodeMap<int> exp_blue_node_map2(exp_graph);
  ExpGraph::EdgeMap<int> exp_edge_map1(exp_graph);
  ExpGraph::EdgeMap<int> exp_edge_map2(exp_graph);
  ExpGraph::ArcMap<int> exp_arc_map1(exp_graph);
  ExpGraph::ArcMap<int> exp_arc_map2(exp_graph);
  ExpGraph::Node exp_n;
  ExpGraph::RedNode exp_rn1;
  ExpGraph::BlueNode exp_bn2;
  ExpGraph::Edge exp_e1;
  ExpGraph::Arc exp_a1;
  int exp_attr1;
  int exp_attr2;

  std::istringstream is(os.str());
  lemon::BpGraphReader<ExpGraph> reader(exp_graph, is);

  reader.nodeMap("node_map1", exp_node_map1);
  reader.nodeMap("node_map2", exp_node_map2, ReaderConverter());
  reader.redNodeMap("red_node_map1", exp_red_node_map1);
  reader.redNodeMap("red_node_map2", exp_red_node_map2, ReaderConverter());
  reader.blueNodeMap("blue_node_map1", exp_blue_node_map1);
  reader.blueNodeMap("blue_node_map2", exp_blue_node_map2, ReaderConverter());
  reader.edgeMap("edge_map1", exp_edge_map1);
  reader.edgeMap("edge_map2", exp_edge_map2, ReaderConverter());
  reader.arcMap("arc_map1", exp_arc_map1);
  reader.arcMap("arc_map2", exp_arc_map2, ReaderConverter());
  reader.node("node", exp_n);
  reader.redNode("red_node", exp_rn1);
  reader.blueNode("blue_node", exp_bn2);
  reader.edge("edge", exp_e1);
  reader.arc("arc", exp_a1);
  reader.attribute("attr1", exp_attr1);
  reader.attribute("attr2", exp_attr2, ReaderConverter());

  reader.run();

  check(lemon::countNodes(exp_graph) == 5, "Wrong number of nodes");
  check(lemon::countRedNodes(exp_graph) == 3, "Wrong number of red nodes");
  check(lemon::countBlueNodes(exp_graph) == 2, "Wrong number of blue nodes");
  check(lemon::countEdges(exp_graph) == 2, "Wrong number of edges");
  check(lemon::countArcs(exp_graph) == 4, "Wrong number of arcs");
  check(exp_node_map1[exp_n] == 14, "Wrong map value");
  check(exp_node_map2[exp_n] == 14, "Wrong map value");
  check(exp_red_node_map1[exp_rn1] == 411, "Wrong map value");
  check(exp_red_node_map2[exp_rn1] == 411, "Wrong map value");
  check(exp_blue_node_map1[exp_bn2] == 415, "Wrong map value");
  check(exp_blue_node_map2[exp_bn2] == 415, "Wrong map value");
  check(exp_edge_map1[exp_e1] == 21, "Wrong map value");
  check(exp_edge_map2[exp_e1] == 21, "Wrong map value");
  check(exp_arc_map1[exp_a1] == 212, "Wrong map value");
  check(exp_arc_map2[exp_a1] == 212, "Wrong map value");
  check(exp_attr1 == 100, "Wrong attr value");
  check(exp_attr2 == 100, "Wrong attr value");
}


int main() {
  { // Check digrpah
    checkDigraphReaderWriter();
  }
  { // Check graph
    checkGraphReaderWriter();
  }
  { // Check bipartite graph
    checkBpGraphReaderWriter();
  }
  return 0;
}
