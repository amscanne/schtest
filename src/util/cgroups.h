#pragma once

#include <filesystem>

#include "util/result.h"

namespace schtest {

// Cgroup is a RAII-style helper for creating and managing cgroups.
// It creates a new cgroup as a sub-cgroup of the current process,
// and tears down this cgroup when the object is destroyed.
class Cgroup {
public:
  static Result<Cgroup> create();

  ~Cgroup();
  Cgroup(Cgroup &&other) = default;
  Cgroup &operator=(Cgroup &&other) = default;

  Cgroup(const Cgroup &) = delete;
  Cgroup &operator=(const Cgroup &) = delete;

  const std::filesystem::path &path() const { return path_; }

  // Enters the cgroup with the current thread.
  Result<> enter();

private:
  explicit Cgroup(std::filesystem::path path) : path_(std::move(path)){};
  std::filesystem::path path_;
};

} // namespace schtest
