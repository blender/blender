#ifndef FLOW_H_
#define FLOW_H_

#include <Eigen/Core>
#include <list>
#include <map>
#include <vector>

#include "config.hpp"
#include "../patches/boykov_kolmogorov_max_flow.hpp"

#include <lemon/network_simplex.h>
#include <lemon/preflow.h>
#include <lemon/smart_graph.h>

using namespace Eigen;

namespace qflow {

class MaxFlowHelper {
   public:
    MaxFlowHelper() {}
    virtual ~MaxFlowHelper(){};
    virtual void resize(int n, int m) = 0;
    virtual void addEdge(int x, int y, int c, int rc, int v, int cost = 1) = 0;
    virtual int compute() = 0;
    virtual void applyTo(std::vector<Vector2i>& edge_diff) = 0;
};

class BoykovMaxFlowHelper : public MaxFlowHelper {
   public:
    BoykovMaxFlowHelper() = default;
    void resize(int n, int m) override {
        num_verts = n;
        num_edges = 0;
        flow.resize(num_verts, m * 2);
    }
    int compute() override {
        return flow.max_flow(0, num_verts - 1);
    }
    void addEdge(int x, int y, int c, int rc, int v, int cost = 1) override {
        const int e1 = num_edges++;
        const int e2 = num_edges++;
        flow.set_edge(e1, e2, x, y, c);
        flow.set_edge(e2, e1, y, x, rc);
        if (v != -1) {
            edge_to_variables.emplace_back(v, -1);
            edge_to_variables.emplace_back(v, 1);
        }
        else {
            edge_to_variables.emplace_back(-1, -1);
            edge_to_variables.emplace_back(-1, -1);
        }
    }
    void applyTo(std::vector<Vector2i>& edge_diff) override {
        for (int vert = 0; vert < num_verts; vert++) {
            for (int edge : flow.vertex_out_edges(vert)) {
                const int capacity = flow.edge_capacity(edge);
                const int residual_capacity = flow.edge_residual_capacity(edge);
                if (capacity > 0) {
                    int flow = (capacity - residual_capacity);
                    if (flow > 0) {
                        std::pair<int, int> e2v = edge_to_variables[edge];
                        if (e2v.first != -1) {
                            edge_diff[e2v.first / 2][e2v.first % 2] += e2v.second * flow;
                        }
                    }
                }
            }
        }
    }

   private:
    BoykovKolmogorovMaxFlow flow;
    std::vector<std::pair<int, int>> edge_to_variables;
    int num_verts = 0;
    int num_edges = 0;
};

class NetworkSimplexFlowHelper : public MaxFlowHelper {
   public:
    using Weight = int;
    using Capacity = int;
    using Graph = lemon::SmartDigraph;
    using Node = Graph::Node;
    using Arc = Graph::Arc;
    template <typename ValueType>
    using ArcMap = lemon::SmartDigraph::ArcMap<ValueType>;
    using Preflow = lemon::Preflow<lemon::SmartDigraph, ArcMap<Capacity>>;
    using NetworkSimplex = lemon::NetworkSimplex<lemon::SmartDigraph, Capacity, Weight>;

   public:
    NetworkSimplexFlowHelper() : cost(graph), capacity(graph), flow(graph), variable(graph) {}
    ~NetworkSimplexFlowHelper(){};
    void resize(int n, int m) {
        nodes.reserve(n);
        for (int i = 0; i < n; ++i) nodes.push_back(graph.addNode());
    }
    void addEdge(int x, int y, int c, int rc, int v, int cst = 1) {
        assert(x >= 0);
        assert(v >= -1);
        if (c) {
            auto e1 = graph.addArc(nodes[x], nodes[y]);
            cost[e1] = cst;
            capacity[e1] = c;
            variable[e1] = std::make_pair(v, 1);
        }

        if (rc) {
            auto e2 = graph.addArc(nodes[y], nodes[x]);
            cost[e2] = cst;
            capacity[e2] = rc;
            variable[e2] = std::make_pair(v, -1);
        }
    }
    int compute() {
        Preflow pf(graph, capacity, nodes.front(), nodes.back());
        NetworkSimplex ns(graph);

        // Run preflow to find maximum flow
        lprintf("push-relabel flow... ");
        pf.runMinCut();
        int maxflow = pf.flowValue();

        // Run network simplex to find minimum cost maximum flow
        ns.costMap(cost).upperMap(capacity).stSupply(nodes.front(), nodes.back(), maxflow);
        auto status = ns.run();
        switch (status) {
            case NetworkSimplex::OPTIMAL:
                ns.flowMap(flow);
                break;
            case NetworkSimplex::INFEASIBLE:
                lputs("NetworkSimplex::INFEASIBLE");
                assert(0);
                break;
            default:
                lputs("Unknown: NetworkSimplex::Default");
                assert(0);
                break;
        }

        return maxflow;
    }
    void applyTo(std::vector<Vector2i>& edge_diff) {
        for (Graph::ArcIt e(graph); e != lemon::INVALID; ++e) {
            int var = variable[e].first;
            if (var == -1) continue;
            int sgn = variable[e].second;
            edge_diff[var / 2][var % 2] -= sgn * flow[e];
        }
    }

   private:
    Graph graph;
    ArcMap<Weight> cost;
    ArcMap<Capacity> capacity;
    ArcMap<Capacity> flow;
    ArcMap<std::pair<int, int>> variable;
    std::vector<Node> nodes;
    std::vector<Arc> edges;
};

#ifdef WITH_GUROBI

#include <gurobi_c++.h>

class GurobiFlowHelper : public MaxFlowHelper {
   public:
    GurobiFlowHelper() {}
    virtual ~GurobiFlowHelper(){};
    virtual void resize(int n, int m) {
        nodes.resize(n * 2);
        edges.resize(m);
    }
    virtual void addEdge(int x, int y, int c, int rc, int v, int cost = 1) {
        nodes[x * 2 + 0].push_back(vars.size());
        nodes[y * 2 + 1].push_back(vars.size());
        vars.push_back(model.addVar(0, c, 0, GRB_INTEGER));
        edges.push_back(std::make_pair(v, 1));

        nodes[y * 2 + 0].push_back(vars.size());
        nodes[x * 2 + 1].push_back(vars.size());
        vars.push_back(model.addVar(0, rc, 0, GRB_INTEGER));
        edges.push_back(std::make_pair(v, -1));
    }
    virtual int compute() {
        std::cerr << "compute" << std::endl;
        int ns = nodes.size() / 2;

        int flow;
        for (int i = 1; i < ns - 1; ++i) {
            GRBLinExpr cons = 0;
            for (auto n : nodes[2 * i + 0]) cons += vars[n];
            for (auto n : nodes[2 * i + 1]) cons -= vars[n];
            model.addConstr(cons == 0);
        }

        // first pass, maximum flow
        GRBLinExpr outbound = 0;
        {
            lprintf("first pass\n");
            for (auto& n : nodes[0]) outbound += vars[n];
            for (auto& n : nodes[1]) outbound -= vars[n];
            model.setObjective(outbound, GRB_MAXIMIZE);
            model.optimize();

            flow = (int)model.get(GRB_DoubleAttr_ObjVal);
            lprintf("Gurobi result: %d\n", flow);
        }

        // second pass, minimum cost flow
        {
            lprintf("second pass\n");
            model.addConstr(outbound == flow);
            GRBLinExpr cost = 0;
            for (auto& v : vars) cost += v;
            model.setObjective(cost, GRB_MINIMIZE);
            model.optimize();

            double optimal_cost = (int)model.get(GRB_DoubleAttr_ObjVal);
            lprintf("Gurobi result: %.3f\n", optimal_cost);
        }
        return flow;
    }
    virtual void applyTo(std::vector<Vector2i>& edge_diff) { assert(0); };

   private:
    GRBEnv env = GRBEnv();
    GRBModel model = GRBModel(env);
    std::vector<GRBVar> vars;
    std::vector<std::pair<int, int>> edges;
    std::vector<std::vector<int>> nodes;
};

#endif

class ECMaxFlowHelper : public MaxFlowHelper {
   public:
    struct FlowInfo {
        int id;
        int capacity, flow;
        int v, d;
        FlowInfo* rev;
    };
    struct SearchInfo {
        SearchInfo(int _id, int _prev_id, FlowInfo* _info)
            : id(_id), prev_id(_prev_id), info(_info) {}
        int id;
        int prev_id;
        FlowInfo* info;
    };
    ECMaxFlowHelper() { num = 0; }
    int num;
    std::vector<FlowInfo*> variable_to_edge;
    void resize(int n, int m) {
        graph.resize(n);
        variable_to_edge.resize(m, 0);
        num = n;
    }
    void addEdge(int x, int y, int c, int rc, int v, int cost = 0) {
        FlowInfo flow;
        flow.id = y;
        flow.capacity = c;
        flow.flow = 0;
        flow.v = v;
        flow.d = -1;
        graph[x].push_back(flow);
        auto& f1 = graph[x].back();
        flow.id = x;
        flow.capacity = rc;
        flow.flow = 0;
        flow.v = v;
        flow.d = 1;
        graph[y].push_back(flow);
        auto& f2 = graph[y].back();
        f2.rev = &f1;
        f1.rev = &f2;
    }
    int compute() {
        int total_flow = 0;
        int count = 0;
        while (true) {
            count += 1;
            std::vector<int> vhash(num, 0);
            std::vector<SearchInfo> q;
            q.push_back(SearchInfo(0, -1, 0));
            vhash[0] = 1;
            int q_front = 0;
            bool found = false;
            while (q_front < q.size()) {
                int vert = q[q_front].id;
                for (auto& l : graph[vert]) {
                    if (vhash[l.id] || l.capacity <= l.flow) continue;
                    q.push_back(SearchInfo(l.id, q_front, &l));
                    vhash[l.id] = 1;
                    if (l.id == num - 1) {
                        found = true;
                        break;
                    }
                }
                if (found) break;
                q_front += 1;
            }
            if (q_front == q.size()) break;
            int loc = q.size() - 1;
            while (q[loc].prev_id != -1) {
                q[loc].info->flow += 1;
                q[loc].info->rev->flow -= 1;
                loc = q[loc].prev_id;
                // int prev_v = q[loc].id;
                // applyFlow(prev_v, current_v, 1);
                // applyFlow(current_v, prev_v, -1);
            }
            total_flow += 1;
        }
        return total_flow;
    }
    void applyTo(std::vector<Vector2i>& edge_diff) {
        for (int i = 0; i < graph.size(); ++i) {
            for (auto& flow : graph[i]) {
                if (flow.flow > 0 && flow.v != -1) {
                    if (flow.flow > 0) {
                        edge_diff[flow.v / 2][flow.v % 2] += flow.d * flow.flow;
                        if (abs(edge_diff[flow.v / 2][flow.v % 2]) > 2) {
                        }
                    }
                }
            }
        }
    }
    void applyFlow(int v1, int v2, int flow) {
        for (auto& it : graph[v1]) {
            if (it.id == v2) {
                it.flow += flow;
                break;
            }
        }
    }
    std::vector<std::list<FlowInfo>> graph;
};

} // namespace qflow

#endif
