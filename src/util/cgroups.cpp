#include "util/cgroups.h"

#include <cassert>
#include <cerrno>
#include <fstream>
#include <iostream>
#include <system_error>
#include <unistd.h>

namespace schtest {

Cgroup::~Cgroup() {
  if (!path_.empty()) {
    // Move all processes from this cgroup back to the parent.
    std::filesystem::path parent_path = path_.parent_path();
    std::ifstream tasks_file(path_ / "tasks");
    std::ofstream parent_tasks_file(parent_path / "tasks");

    if (tasks_file && parent_tasks_file) {
      std::string pid;
      while (std::getline(tasks_file, pid)) {
        parent_tasks_file << pid << std::endl;
      }
    }

    // Remove the cgroup directory.
    std::error_code ec;
    std::filesystem::remove_all(path_, ec);
    assert(!ec);
  }
}

Result<Cgroup> Cgroup::create() {
  // Read /proc/self/cgroup to find the current cgroup.
  std::filesystem::path current_cgroup;
  std::ifstream cgroup_file("/proc/self/cgroup");
  if (!cgroup_file) {
    return Error("failed to open /proc/self/cgroup", errno);
  }

  std::string line;
  while (std::getline(cgroup_file, line)) {
    // Parse the line to get the cgroup path
    // Format: hierarchy-ID:controller-list:cgroup-path
    auto pos = line.find_last_of(':');
    if (pos != std::string::npos) {
      current_cgroup = line.substr(pos + 1);
      break;
    }
  }
  if (current_cgroup.empty()) {
    return Error("failed to determine current cgroup", EINVAL);
  }

  // Generate a new unique name, based on a random number.
  std::string name = "schtest-" + std::to_string(rand());

  // Create the new cgroup path.
  std::filesystem::path cgroup_mount = "/sys/fs/cgroup";
  std::filesystem::path new_cgroup_path = cgroup_mount / current_cgroup / name;
  std::error_code ec;
  std::filesystem::create_directories(new_cgroup_path, ec);
  if (ec) {
    return Error("failed to create cgroup directory", ec.value());
  }

  return Cgroup(new_cgroup_path);
}

Result<> Cgroup::enter() {
  // Add the current process to the new cgroup.
  std::ofstream tasks_file(path_ / "tasks");
  if (!tasks_file) {
    return Error("failed to open tasks file", errno);
  }

  // Write the current process ID to the tasks file.
  tasks_file << getpid();
  if (!tasks_file) {
    return Error("failed to write to tasks file", errno);
  }

  return OK();
}

} // namespace schtest
