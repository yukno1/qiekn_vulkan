module;

// #include <iostream>
// #include <print>
// #include <string>

export module greeting;

import std;

export void greet(const std::string &name) {
  std::print("Hello\n");
  std::cout << "Hello, " << name << std::endl;
}