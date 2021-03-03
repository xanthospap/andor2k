#include "parameter_file.hpp"
#include <iostream>

int main(int argc, char *argv[]) {
  if (argc != 2) {
    std::cerr << "\n[ERROR] Need to provide a PARAMETER FILE as input";
    return 1;
  }

  std::map<std::string, std::string> parameter_map;
  if (aristarchos::read_parameter_file(argv[1], parameter_map)) {
    std::cerr << "\n[ERROR] Failed to read parameter file";
    return 1;
  }

  std::cout << "\nParameters read off from input file:";
  for (const auto &[key, value] : parameter_map) {
    std::cout << "\n" << key << "->" << value;
  }

  std::cout << "\n";
  return 0;
}
