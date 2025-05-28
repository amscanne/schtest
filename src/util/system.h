#pragma once
#include <cassert>
#include <functional>
#include <iostream>
#include <sched.h>
#include <unistd.h>
#include <vector>

#include "util/result.h"

namespace schtest {

class CPUSet {
public:
  virtual ~CPUSet() = default;

  // runs the given function on this CPU set.
  Result<> run(std::function<void()> f) const;

  // Migrates to the given CPU set. When this function returns, the processes
  // may no longer be on this CPU set, but they will have been for at least a
  // short period of time.
  Result<> migrate() const {
    return run([]() {});
  }

protected:
  virtual cpu_set_t mask() const = 0;
};

class Hyperthread : public CPUSet {
public:
  Hyperthread() = delete;
  Hyperthread(const Hyperthread &) = default;

  Hyperthread(int id);
  cpu_set_t mask() const override;
  int id() const;

private:
  int id_;
};

class Core : public CPUSet {
public:
  Core() = delete;
  Core(const Core &) = default;

  Core(int id, std::vector<Hyperthread> &&hyperthreads);
  const std::vector<Hyperthread> &hyperthreads() const;
  cpu_set_t mask() const override;
  int id() const;

private:
  int id_;
  std::vector<Hyperthread> hyperthreads_;
};

class CoreComplex : public CPUSet {
public:
  CoreComplex() = delete;
  CoreComplex(const CoreComplex &) = default;

  CoreComplex(int id, std::vector<Core> &&cores);
  const std::vector<Core> &cores() const;
  cpu_set_t mask() const override;
  int id() const;

private:
  int id_;
  std::vector<Core> cores_;
};

class Node : public CPUSet {
public:
  Node() = delete;
  Node(const Node &) = default;

  Node(int id, std::vector<CoreComplex> &&complexes);
  const std::vector<CoreComplex> &complexes() const;
  std::vector<Core> cores() const;
  cpu_set_t mask() const override;
  int id() const;

private:
  int id_;
  std::vector<CoreComplex> complexes_;
};

class System : public CPUSet {
public:
  static Result<System> load();

  System() = delete;
  System(const System &) = default;

  const std::vector<Node> &nodes() const;
  const std::vector<Core> &cores() const;
  cpu_set_t mask() const override;

  // Returns the number of logical CPUs in the system, aka the number of
  // threads. Note that this is distinct from `Core` above, which refers to
  // a physical core.
  int logical_cpus() const;

private:
  System(std::vector<Node> &&nodes);

  std::vector<Node> nodes_;
  std::vector<Core> all_cores_;
};

std::ostream &operator<<(std::ostream &os, const Hyperthread &ht);
std::ostream &operator<<(std::ostream &os, const Core &core);
std::ostream &operator<<(std::ostream &os, const CoreComplex &complex);
std::ostream &operator<<(std::ostream &os, const Node &node);
std::ostream &operator<<(std::ostream &os, const System &system);

} // namespace schtest
