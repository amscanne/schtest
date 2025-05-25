#include <iostream>
#include <sstream>

#include "util/clock.h"

namespace schtest {

std::string fmt(const std::chrono::nanoseconds &d) {
  using namespace std::chrono;

  std::stringstream ss;
  if (d >= hours(10)) {
    auto h = duration_cast<hours>(d);
    auto m = duration_cast<minutes>(d - h);
    ss << std::to_string(h.count()) << "." << std::setw(3) << std::setfill('0')
       << static_cast<long>(1000.0 * static_cast<double>(m.count()) / 60.0)
       << "h";
  } else if (d >= minutes(10)) {
    auto m = duration_cast<minutes>(d);
    auto s = duration_cast<seconds>(d - m);
    ss << std::to_string(m.count()) << "." << std::setw(3) << std::setfill('0')
       << static_cast<long>(1000.0 * static_cast<double>(s.count()) / 60.0)
       << "m";
  } else if (d >= seconds(10)) {
    auto s = duration_cast<seconds>(d);
    auto ms = duration_cast<milliseconds>(d - s);
    ss << std::to_string(s.count()) << "." << std::setw(3) << std::setfill('0')
       << static_cast<long>(1000.0 * static_cast<double>(ms.count()) / 1000.0)
       << "s";
  } else if (d >= milliseconds(10)) {
    auto ms = duration_cast<milliseconds>(d);
    auto us = duration_cast<microseconds>(d - ms);
    ss << std::to_string(ms.count()) << "." << std::setw(3) << std::setfill('0')
       << static_cast<long>(1000.0 * static_cast<double>(us.count()) / 1000.0)
       << "ms";
  } else if (d >= microseconds(10)) {
    auto us = duration_cast<microseconds>(d);
    auto ns = duration_cast<nanoseconds>(d - us);
    ss << std::to_string(us.count()) << "." << std::setw(3) << std::setfill('0')
       << static_cast<long>(1000.0 * static_cast<double>(ns.count()) / 1000.0)
       << "μs";
  } else {
    auto ns = duration_cast<nanoseconds>(d);
    ss << std::to_string(ns.count()) + "ns";
  }
  return ss.str();
}

} // namespace schtest
