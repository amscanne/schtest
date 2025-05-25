#include "util/output.h"

namespace schtest::output {

namespace {

class ListenerImpl : public Output {
public:
  ListenerImpl(testing::TestEventListener *listener) : out_(listener) {}

  void emit(std::string &&s) override;

private:
  testing::TestEventListener *out_;
};

class StreamImpl : public Output {
public:
  StreamImpl(std::ostream &out) : out_(std::ref(out)) {}

  void emit(std::string &&s) override;

private:
  std::ostream &out_;
};

} // namespace

void ListenerImpl::emit(std::string &&s) {
  std::cout << "[          ] " << s << std::endl;
}

void StreamImpl::emit(std::string &&s) {
  out_ << s << std::endl;
  out_.flush();
}

static std::unique_ptr<Output> output = nullptr;

Output &Output::get() { return *output.get(); }

testing::TestEventListener *
Output::setup(std::unique_ptr<testing::TestEventListener> &&listener) {
  auto ptr = listener.release();
  auto *impl = new ListenerImpl(ptr);
  output.reset(impl);
  return ptr;
}

void Output::setup(std::ostream &out) { output.reset(new StreamImpl(out)); }

} // namespace schtest::output