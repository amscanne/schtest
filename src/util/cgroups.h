#pragma once

#include <filesystem>
#include <string>

#include "util/result.h"

namespace schtest {

// Cgroup is a RAII-style helper for creating and managing cgroups.
// It creates a new cgroup as a sub-cgroup of the current process,
// and tears down this cgroup when the object is destroyed.
class Cgroup {
public:
  // Delete copy constructor and assignment operator
  Cgroup(const Cgroup &) = delete;
  Cgroup &operator=(const Cgroup &) = delete;

  // Move constructor and assignment operator
  Cgroup(Cgroup &&other) noexcept;
  Cgroup &operator=(Cgroup &&other) noexcept;

  // Destructor - tears down the cgroup
  ~Cgroup();

  // Get the path to the cgroup
  const std::filesystem::path &path() const;

  // Static factory method to create a new cgroup
  // Returns Result<Cgroup> to handle errors on the creation path
  static Result<Cgroup> create(const std::string &name);

private:
  // Private constructor - only called by the static factory method
  explicit Cgroup(std::filesystem::path path);

  // Clean up the cgroup
  void cleanup();

  std::filesystem::path path_; // Path to the cgroup
};

} // namespace schtest
