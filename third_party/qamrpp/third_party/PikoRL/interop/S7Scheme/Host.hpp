#ifndef PIKORL_INTEROP_S7_SCHEME_HOST_HPP
#define PIKORL_INTEROP_S7_SCHEME_HOST_HPP

#include <string>

#include <s7.h>

namespace picorl::interop::s7
{

class Host
{
private:
  s7_scheme *scheme_;

public:
  explicit Host (s7_scheme *scheme) : scheme_ (scheme)
  {
  }

  bool
  evaluate (const std::string &source) const
  {
    if (scheme_ == nullptr)
      return false;
    if (source.empty ())
      return true;
    return s7_eval_c_string (scheme_, source.c_str ()) != nullptr;
  }

  bool
  load_file (const std::string &path) const
  {
    if (scheme_ == nullptr)
      return false;
    return s7_load (scheme_, path.c_str ()) != nullptr;
  }
};

} // namespace picorl::interop::s7

#endif
