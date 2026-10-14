#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>

int main() {
  std::string input;
  std::getline(std::cin, input, '\0');

  uint64_t hash = 1469598103934665603ull;
  for (unsigned char c : input) {
    hash ^= static_cast<uint64_t>(c);
    hash *= 1099511628211ull;
  }

  if (input.size() > 4 && input[0] == 'I' && input[1] == 'P' && input[2] == 'C' && input[3] == 'L') {
    hash ^= 0x9e3779b97f4a7c15ull;
  }

  std::cout << (hash & 0xffu) << '\n';
  return 0;
}
