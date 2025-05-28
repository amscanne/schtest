#include <algorithm>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sched.h>
#include <string>

#include "util/system.h"

namespace schtest {

Result<> CPUSet::run(std::function<void()> fn) const {
  cpu_set_t orig;
  cpu_set_t target = mask();

  // Retrieve the current affinity.
  CPU_ZERO(&orig);
  int rc = sched_getaffinity(0, sizeof(cpu_set_t), &orig);
  if (rc < 0) {
    return Error("unable to get current CPU mask", errno);
  }

  // Set new affinity; when this returns, we will be executing on these cores.
  // We can't control what happens when we restore, but we will be running for
  // some amount of time here.
  rc = sched_setaffinity(0, sizeof(cpu_set_t), &target);
  if (rc < 0) {
    return Error("unable to set CPU mask", errno);
  }

  // Run the required function.
  fn();

  // Restore original affinity.
  rc = sched_setaffinity(0, sizeof(cpu_set_t), &orig);
  if (rc < 0) {
    return Error("unable to restore CPU mask", errno);
  }

  return OK();
}

Hyperthread::Hyperthread(int id) : id_(id) {}

cpu_set_t Hyperthread::mask() const {
  cpu_set_t mask;
  CPU_ZERO(&mask);
  CPU_SET(id_, &mask);
  return mask;
}

int Hyperthread::id() const { return id_; }

Core::Core(int id, std::vector<Hyperthread> &&hyperthreads)
    : id_(id), hyperthreads_(std::move(hyperthreads)) {}

int Core::id() const { return id_; }

const std::vector<Hyperthread> &Core::hyperthreads() const {
  return hyperthreads_;
}

cpu_set_t Core::mask() const {
  cpu_set_t mask;
  CPU_ZERO(&mask);
  for (const auto &ht : hyperthreads_) {
    auto ht_mask = ht.mask();
    CPU_OR(&mask, &mask, &ht_mask);
  }
  return mask;
}

CoreComplex::CoreComplex(int id, std::vector<Core> &&cores)
    : id_(id), cores_(std::move(cores)) {}

int CoreComplex::id() const { return id_; }

const std::vector<Core> &CoreComplex::cores() const { return cores_; }

cpu_set_t CoreComplex::mask() const {
  cpu_set_t mask;
  CPU_ZERO(&mask);
  for (const auto &core : cores_) {
    cpu_set_t core_mask = core.mask();
    CPU_OR(&mask, &mask, &core_mask);
  }
  return mask;
}

Node::Node(int id, std::vector<CoreComplex> &&complexes)
    : id_(id), complexes_(std::move(complexes)) {}

int Node::id() const { return id_; }

const std::vector<CoreComplex> &Node::complexes() const { return complexes_; }

std::vector<Core> Node::cores() const {
  std::vector<Core> all_cores;
  for (const auto &complex : complexes_) {
    const auto &complex_cores = complex.cores();
    all_cores.insert(all_cores.end(), complex_cores.begin(),
                     complex_cores.end());
  }
  return all_cores;
}

cpu_set_t Node::mask() const {
  cpu_set_t mask;
  CPU_ZERO(&mask);
  for (const auto &complex : complexes_) {
    cpu_set_t complex_mask = complex.mask();
    CPU_OR(&mask, &mask, &complex_mask);
  }
  return mask;
}

System::System(std::vector<Node> &&nodes) : nodes_(std::move(nodes)) {
  // Build the flat list of all cores for quick lookup.
  for (const auto &node : nodes_) {
    auto node_cores = node.cores();
    all_cores_.insert(all_cores_.end(), node_cores.begin(), node_cores.end());
  }
  std::sort(all_cores_.begin(), all_cores_.end(),
            [](const Core &a, const Core &b) { return a.id() < b.id(); });
}

const std::vector<Node> &System::nodes() const { return nodes_; }

const std::vector<Core> &System::cores() const { return all_cores_; }

int System::logical_cpus() const {
  int count = 0;
  for (const auto &core : all_cores_) {
    count += core.hyperthreads().size();
  }
  return count;
}

cpu_set_t System::mask() const {
  cpu_set_t mask;
  CPU_ZERO(&mask);
  for (const auto &node : nodes_) {
    cpu_set_t node_mask = node.mask();
    CPU_OR(&mask, &mask, &node_mask);
  }
  return mask;
}

Result<System> System::load() {
  // Maps to store the topology information.
  std::map<int, std::map<int, std::map<int, std::vector<int>>>> topology;

  // Get the number of CPUs in the system.
  int num_cpus = sysconf(_SC_NPROCESSORS_CONF);
  if (num_cpus <= 0) {
    return Error("failed to get number of CPUs", errno);
  }

  // Process each CPU.
  const std::filesystem::path base_path = "/sys/devices/system/cpu";
  for (int cpu_id = 0; cpu_id < num_cpus; cpu_id++) {
    std::filesystem::path cpu_path =
        base_path / ("cpu" + std::to_string(cpu_id));

    // Skip if the CPU directory doesn't exist.
    if (!std::filesystem::exists(cpu_path)) {
      continue;
    }

    // Try to read from topology/physical_package_id.
    uint32_t node_id = 0; // Default to zero.
    std::filesystem::path package_path =
        cpu_path / "topology" / "physical_package_id";
    if (std::filesystem::exists(package_path)) {
      std::ifstream package_file(package_path);
      package_file >> node_id;
    }
    int core_id = cpu_id;
    std::filesystem::path core_path = cpu_path / "topology" / "core_id";
    if (std::filesystem::exists(core_path)) {
      std::ifstream core_file(core_path);
      core_file >> core_id;
    }
    int complex_id = 0; // Default to 0 if not found.
    std::filesystem::path die_path = cpu_path / "topology" / "die_id";
    if (std::filesystem::exists(die_path)) {
      std::ifstream die_file(die_path);
      die_file >> complex_id;
    } else {
      // Try to use L3 cache as a proxy for complex.
      std::filesystem::path cache_path =
          base_path / "cpu" / ("cpu" + std::to_string(cpu_id)) / "cache";
      if (std::filesystem::exists(cache_path)) {
        // Look for L3 cache.
        for (const auto &entry :
             std::filesystem::directory_iterator(cache_path)) {
          std::filesystem::path index_path = entry.path() / "level";
          if (std::filesystem::exists(index_path)) {
            std::ifstream index_file(index_path);
            int level;
            if (index_file >> level && level == 3) {
              // Found L3 cache, use its ID as complex ID.
              std::filesystem::path id_path = entry.path() / "id";
              if (std::filesystem::exists(id_path)) {
                std::ifstream id_file(id_path);
                if (id_file >> complex_id) {
                  // Successfully read the L3 cache ID.
                  break;
                }
              }
            }
          }
        }
      }
    }

    // Add this CPU to the topology map.
    topology[node_id][complex_id][core_id].push_back(cpu_id);
  }

  // Build the system topology from the collected information.
  std::vector<Node> nodes;
  for (const auto &node_entry : topology) {
    int node_id = node_entry.first;
    std::vector<CoreComplex> complexes;
    for (const auto &complex_entry : node_entry.second) {
      int complex_id = complex_entry.first;
      std::vector<Core> cores;
      for (const auto &core_entry : complex_entry.second) {
        int core_id = core_entry.first;
        std::vector<Hyperthread> hyperthreads;

        for (int cpu_id : core_entry.second) {
          hyperthreads.emplace_back(cpu_id);
        }
        cores.emplace_back(core_id, std::move(hyperthreads));
      }
      complexes.emplace_back(complex_id, std::move(cores));
    }
    nodes.emplace_back(node_id, std::move(complexes));
  }

  return System(std::move(nodes));
}

std::ostream &operator<<(std::ostream &os, const Hyperthread &ht) {
  os << "Hyperthread{id=" << ht.id() << "}";
  return os;
}

std::ostream &operator<<(std::ostream &os, const Core &core) {
  os << "Core{id=" << core.id() << ", hyperthreads=[";
  const auto &hts = core.hyperthreads();
  for (size_t i = 0; i < hts.size(); i++) {
    if (i > 0) {
      os << ", ";
    }
    os << hts[i];
  }
  os << "]}";
  return os;
}

std::ostream &operator<<(std::ostream &os, const CoreComplex &complex) {
  os << "CoreComplex{id=" << complex.id() << ", cores=[";
  const auto &cores = complex.cores();
  for (size_t i = 0; i < cores.size(); i++) {
    if (i > 0) {
      os << ", ";
    }
    os << cores[i];
  }
  os << "]}";
  return os;
}

std::ostream &operator<<(std::ostream &os, const Node &node) {
  os << "Node{id=" << node.id() << ", complexes=[";
  const auto &complexes = node.complexes();
  for (size_t i = 0; i < complexes.size(); i++) {
    if (i > 0) {
      os << ", ";
    }
    os << complexes[i];
  }
  os << "]}";
  return os;
}

std::ostream &operator<<(std::ostream &os, const System &system) {
  os << "System{nodes=[";
  const auto &nodes = system.nodes();
  for (size_t i = 0; i < nodes.size(); i++) {
    if (i > 0) {
      os << ", ";
    }
    os << nodes[i];
  }
  os << "], logical_cpus=" << system.logical_cpus() << "}";
  return os;
}

} // namespace schtest
