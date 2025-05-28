#pragma

#include <atomic>

namespace schtest::workloads {

class Context;

// Atomic is a shared object across processes.
template <typename T>
class Atomic : public std::atomic<T> {
public:
  Atomic(Context &ctx){};
};

} // namespace schtest::workloads
