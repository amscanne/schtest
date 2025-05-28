#pragma once

#include <gmock/gmock-matchers.h>
#include <gtest/gtest-matchers.h>
#include <gtest/gtest.h>
#include <iostream>
#include <string>
#include <variant>

namespace schtest {

class Error {
public:
  Error() = delete;
  Error(const Error &other) = delete;

  Error &operator=(const Error &other) = delete;
  Error &operator=(Error &&other) = delete;

  Error(std::string &&msg, int err) : err_(err), checked_(false) {
    // This class has pure inline storage to allow to be passed by value
    // between processes. Therefore, we copy the message into the storage,
    // and truncate it if it is too long.
    for (size_t i = 0; i < msg.length() && i < msg_.size(); ++i) {
      msg_[i] = msg[i];
    }
  };
  Error(std::string &&msg, Error &&other)
      : Error(msg + ": " + other.msg_.data(), other.err_){};
  Error(Error &&other) : msg_(std::move(other.msg_)), err_(other.err_) {
    checked_ = other.checked_;
    other.checked_ = true; // Ensure that it does not explode.
  }
  ~Error();

private:
  // See above; for why this is inline.
  std::array<char, 128> msg_ = {};
  int err_ = 0;
  bool checked_ = false;

  // Allow for the printing of the message.
  friend std::ostream &operator<<(std::ostream &os, const Error &e);

  // Allow for setting checked_.
  template <typename T>
  friend class Result;
};

inline std::ostream &operator<<(std::ostream &os, const Error &e) {
  os << e.msg_.data() << " (" << e.err_ << ")";
  return os;
}

class OK {};

template <typename T = OK>
class Result {
public:
  Result(T &&value) : value_(std::move(value)){};
  Result(Error &&error) : value_(std::move(error)){};
  Result(const Result &other) = delete;
  Result(Result &&other) = default;

  operator bool() { return ok(); }
  bool ok() {
    bool v = std::holds_alternative<T>(value_);
    if (!v) {
      // Set that the error has been checked.
      std::get_if<Error>(&value_)->checked_ = true;
    }
    return v;
  }
  bool ok() const { return std::holds_alternative<T>(value_); }
  T *operator->() { return std::get_if<T>(&value_); }
  T &operator*() { return *std::get_if<T>(&value_); }
  const T &operator*() const { return *std::get_if<T>(&value_); }
  Error takeError() { return std::move(*std::get_if<Error>(&value_)); }
  const Error &getError() const { return *std::get_if<Error>(&value_); }

private:
  std::variant<T, Error> value_;
};

std::ostream &operator<<(std::ostream &os, const OK &ok);

template <typename T>
std::ostream &operator<<(std::ostream &os, const Result<T> &r) {
  if (r.ok()) {
    os << *r;
  } else {
    os << r.getError();
  }
  return os;
}

} // namespace schtest
