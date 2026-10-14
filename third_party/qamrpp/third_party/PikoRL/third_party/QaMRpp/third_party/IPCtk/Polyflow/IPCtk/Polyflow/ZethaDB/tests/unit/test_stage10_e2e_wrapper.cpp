#include <cstdlib>
#include <iostream>
#include <string>

int main() {
  const std::string cmd = "bash " + std::string(ZETHADB_SOURCE_DIR) + "/tests/e2e/smoke_stage10.sh";
  const int rc = std::system(cmd.c_str());
  if (rc != 0) {
    std::cerr << "stage10_e2e wrapper failed rc=" << rc << "\n";
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
