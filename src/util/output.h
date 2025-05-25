#pragma once

#include <gtest/gtest.h>
#include <string>

namespace schtest::output {

class Output {
public:
  virtual ~Output() = default;

  // Gets the current singleton output instance.
  static Output &get();

  // Sets up the output to write to the test listener. The caller will take
  // ownership of the returned listener, it should not be deleted.
  static testing::TestEventListener *
  setup(std::unique_ptr<testing::TestEventListener> &&listener);

  // Sets up the output to write to a stream.
  static void setup(std::ostream &output);

  // Outputs must implement an emit method.
  virtual void emit(std::string &&s) = 0;
};

class Message {
public:
  Message(Output &output) : output_(output) {};
  ~Message() {
    std::string line;
    while (std::getline(buffer_, line)) {
      output_.emit(std::move(line));
    }
  }

  template <typename T>
  Message &operator<<(const T &value) {
    buffer_ << value;
    return *this;
  }

private:
  Output &output_;
  std::stringstream buffer_;
};

// Log a message to the static output.
inline Message log() { return Message(Output::get()); }

} // namespace schtest::output