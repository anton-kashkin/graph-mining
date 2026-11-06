// terahac_cluster.cc — minimal TeraHAC clustering tool
//
// Usage:
//   ./terahac_cluster [epsilon] [weight_threshold] < edges.txt
//   ./terahac_cluster [epsilon] [weight_threshold] edges.txt
//
// Input: "node_a node_b weight" per line (0-based node ids)
// Output: "node_id cluster_id" per line (tab-separated)

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "absl/log/initialize.h"
#include "absl/status/status.h"
#include "in_memory/clustering/config.pb.h"
#include "in_memory/clustering/graph.h"
#include "in_memory/clustering/hac/terahac.h"
#include "in_memory/status_macros.h"

using graph_mining::in_memory::TeraHacClusterer;
using graph_mining::in_memory::SimpleUndirectedGraph;
using graph_mining::in_memory::ClustererConfig;

absl::Status Run(double epsilon, double weight_threshold, std::istream& in) {
  // 1. Build graph from edge list
  SimpleUndirectedGraph graph;
  int u, v;
  double w;
  while (in >> u >> v >> w) {
    RETURN_IF_ERROR(graph.AddEdge(u, v, static_cast<float>(w)));
  }
  int num_nodes = graph.NumNodes();
  std::cerr << "Loaded graph: " << num_nodes << " nodes, "
            << graph.NumDirectedEdges() / 2 << " undirected edges\n";

  // 2. Copy into TeraHAC's internal GbbsGraph
  TeraHacClusterer clusterer;
  RETURN_IF_ERROR(CopyGraph(graph, clusterer.MutableGraph()));

  // 3. Configure: epsilon controls approximation quality (default 0.1 = (1.1)^1-approx)
  //               weight_threshold is the similarity cut level (edges below this are pruned
  //               during the run AND the final dendrogram is cut here to produce flat clusters)
  ClustererConfig config;
  auto* tc = config.mutable_terahac_clusterer_config();
  tc->set_epsilon(epsilon);
  tc->set_weight_threshold(weight_threshold);

  ASSIGN_OR_RETURN(auto clustering, clusterer.Cluster(config));

  // 4. Invert: node -> cluster_id
  std::vector<int> node_to_cluster(num_nodes, -1);
  for (int cluster_id = 0; cluster_id < (int)clustering.size(); ++cluster_id) {
    for (int node : clustering[cluster_id]) {
      node_to_cluster[node] = cluster_id;
    }
  }

  // 5. Output: node_id TAB cluster_id, one per line
  for (int node = 0; node < num_nodes; ++node) {
    std::cout << node << "\t" << node_to_cluster[node] << "\n";
  }
  std::cerr << "Produced " << clustering.size() << " clusters from "
            << num_nodes << " nodes\n";
  return absl::OkStatus();
}

int main(int argc, char* argv[]) {
  absl::InitializeLog();

  double epsilon = 0.1;
  double weight_threshold = 0.5;
  int file_arg = -1;

  if (argc >= 3) {
    epsilon = std::atof(argv[1]);
    weight_threshold = std::atof(argv[2]);
    if (argc >= 4) file_arg = 3;
  } else if (argc == 2) {
    // try to parse as a number; if not, treat as filename
    char* end;
    double val = std::strtod(argv[1], &end);
    if (*end == '\0') {
      weight_threshold = val;
    } else {
      file_arg = 1;
    }
  }

  absl::Status status;
  if (file_arg >= 0) {
    std::ifstream fin(argv[file_arg]);
    if (!fin) {
      std::cerr << "Cannot open: " << argv[file_arg] << "\n";
      return 1;
    }
    status = Run(epsilon, weight_threshold, fin);
  } else {
    status = Run(epsilon, weight_threshold, std::cin);
  }

  if (!status.ok()) {
    std::cerr << "Error: " << status.ToString() << "\n";
    return 1;
  }
  return 0;
}
