#include "util/cgroups.h"

#include <cerrno>
#include <fstream>
#include <iostream>
#include <system_error>
#include <unistd.h>

namespace schtest {

// Move constructor
Cgroup::Cgroup(Cgroup &&other) noexcept : path_(std::move(other.path_)) {
  other.path_.clear(); // Ensure the other Cgroup doesn't clean up
}

// Move assignment operator
Cgroup &Cgroup::operator=(Cgroup &&other) noexcept {
  if (this != &other) {
    cleanup();
    path_ = std::move(other.path_);
    other.path_.clear(); // Ensure the other Cgroup doesn't clean up
  }
  return *this;
}

// Destructor
Cgroup::~Cgroup() { cleanup(); }

// Get the path to the cgroup
const std::filesystem::path &Cgroup::path() const { return path_; }

// Private constructor
Cgroup::Cgroup(std::filesystem::path path) : path_(std::move(path)) {}

// Clean up the cgroup
void Cgroup::cleanup() {
  if (!path_.empty()) {
    // Move all processes from this cgroup back to the parent
    std::filesystem::path parent_path = path_.parent_path();
    std::ifstream tasks_file(path_ / "tasks");
    std::ofstream parent_tasks_file(parent_path / "tasks");

    if (tasks_file && parent_tasks_file) {
      std::string pid;
      while (std::getline(tasks_file, pid)) {
        parent_tasks_file << pid << std::endl;
      }
    }

    // Remove the cgroup directory
    std::error_code ec;
    std::filesystem::remove_all(path_, ec);
    // Ignore errors during cleanup
  }
}

// Static factory method to create a new cgroup
Result<Cgroup> Cgroup::create(const std::string &name) {
  // Get the current process's cgroup
  std::filesystem::path current_cgroup;

  // Read /proc/self/cgroup to find the current cgroup
  std::ifstream cgroup_file("/proc/self/cgroup");
  if (!cgroup_file) {
    return Error("Failed to open /proc/self/cgroup", errno);
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
    return Error("Failed to determine current cgroup", EINVAL);
  }

  // Create the new cgroup path
  std::filesystem::path cgroup_mount = "/sys/fs/cgroup";
  std::filesystem::path new_cgroup_path = cgroup_mount / current_cgroup / name;

  // Create the new cgroup directory
  std::error_code ec;
  std::filesystem::create_directories(new_cgroup_path, ec);
  if (ec) {
    return Error("Failed to create cgroup directory", ec.value());
  }

  // Add the current process to the new cgroup
  std::ofstream tasks_file(new_cgroup_path / "tasks");
  if (!tasks_file) {
    // Clean up the created directory
    std::filesystem::remove(new_cgroup_path, ec);
    return Error("Failed to open tasks file", errno);
  }

  // Write the current process ID to the tasks file
  tasks_file << getpid();
  if (!tasks_file) {
    // Clean up the created directory
    std::filesystem::remove(new_cgroup_path, ec);
    return Error("Failed to write to tasks file", errno);
  }

  // Return the new Cgroup object
  return Cgroup(new_cgroup_path);
}

} // namespace schtest
