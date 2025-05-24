#pragma once

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

  Error(std::string &&msg, int err)
      : msg_(std::move(msg)), err_(err), checked_(false) {};
  Error(std::string &&msg, Error &&other)
      : msg_(std::move(msg) + ": " + other.msg_), err_(other.err_),
        checked_(false) {}
  Error(Error &&other) : msg_(std::move(other.msg_)), err_(other.err_) {
    checked_ = other.checked_;
    other.checked_ = true; // Ensure that it does not explode.
  }
  ~Error();

private:
  std::string msg_;
  int err_;
  bool checked_;

  // Allow for the printing of the message.
  friend std::ostream &operator<<(std::ostream &os, const Error &e);

  // Allow for setting checked_.
  template <typename T>
  friend class Result;
};

inline std::ostream &operator<<(std::ostream &os, const Error &e) {
  os << e.msg_ << " (" << e.err_ << ")";
  return os;
}

class OK {};

template <typename T = OK>
class Result {
public:
  Result(T &&value) : value_(std::move(value)) {};
  Result(Error &&error) : value_(std::move(error)) {};

  operator bool() { return ok(); }
  bool ok() {
    bool v = std::holds_alternative<T>(value_);
    if (!v) {
      // Set that the error has been checked.
      std::get_if<Error>(&value_)->checked_ = true;
    }
    return v;
  }
  T *operator->() { return std::get_if<T>(&value_); }
  T &operator*() { return *std::get_if<T>(&value_); }
  Error takeError() { return std::move(*std::get_if<Error>(&value_)); }

private:
  std::variant<T, Error> value_;
};

} // namespace schtest
