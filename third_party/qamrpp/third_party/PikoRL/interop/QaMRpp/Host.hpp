#ifndef PIKORL_INTEROP_QAMRPP_HOST_HPP
#define PIKORL_INTEROP_QAMRPP_HOST_HPP

#include <string>

#include <QaMRpp.hpp>

namespace picorl::interop::qamrpp
{

class Host
{
private:
  ::qamrpp::Context *context_;

public:
  explicit Host (::qamrpp::Context &context) : context_ (&context)
  {
  }

  bool
  evaluate (const std::string &source) const
  {
    if (source.empty ())
      return true;
    try
      {
        (void)context_->run (source);
        return true;
      }
    catch (...)
      {
        return false;
      }
  }

  bool
  load_plugin (const std::string &path) const
  {
    return context_->load_library (path);
  }
};

} // namespace picorl::interop::qamrpp

#endif
