%module PikoRL

%{
#include "PikoRL.hpp"
%}

%include "std_string.i"

namespace picorl {
class REPL {
public:
  REPL();
  void run();
  bool load_example_bundle(const std::string &bundle_dir);
};
}
