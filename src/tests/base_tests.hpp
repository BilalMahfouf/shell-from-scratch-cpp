#pragma once
#include <iostream>
#include <string>
#include <type_traits>

class TestRunner {
private:
  int passed = 0;
  int failed = 0;

public:
  void assertTrue(bool condition, const std::string &name) {
    if (condition) {
      std::cout << "[PASS] " << name << "\n";
      passed++;
    } else {
      std::cout << "[FAIL] " << name << "\n";
      failed++;
    }
  }

  template <typename A, typename B>
  void assertEq(const A &a, const B &b, const std::string &name) {
    using C = std::common_type_t<A, B>;

    if (static_cast<C>(a) == static_cast<C>(b)) {
      std::cout << "[PASS] " << name << "\n";
      passed++;
    } else {
      std::cout << "[FAIL] " << name << " | expected: " << b << " got: " << a
                << "\n";
      failed++;
    }
  }

  void summary() {
    std::cout << "\n====================\n";
    std::cout << "Passed: " << passed << "\n";
    std::cout << "Failed: " << failed << "\n";
    std::cout << "====================\n";
  }
};
