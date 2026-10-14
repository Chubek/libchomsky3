#include "Klyspec-Profiles.hpp"

#include <cassert>

int main() {
  auto json = klyspec::KlyProfileLoader::load(klyspec::KlyProfileLoader::Format::json, R"({"file":"a.json","profile":"release"})");
  assert(json.has_value());
  assert(json->at("file") == "a.json");

  auto yaml = klyspec::KlyProfileLoader::load(klyspec::KlyProfileLoader::Format::yaml, "file: a.yaml\nprofile: debug\n");
  assert(yaml.has_value());
  assert(yaml->at("profile") == "debug");
  return 0;
}
