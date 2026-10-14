#include "third_party/QaMRpp/QaMRpp-Library.h"
#include "qamrpp-lpikorl/lpikorl.h"

static qamrpp_value *
lpikorl_syntax_native (qamrpp_context *ctx, qamrpp_value **argv, size_t argc)
{
  const qamrpp_host_api *host_api
      = ((lpikorl_runtime *)qamrpp_context_get_userdata (ctx))->host_api;

  (void)argv;
  (void)argc;
  return host_api->value_nil (ctx);
}

const qamrpp_native_binding *
lpikorl_syntax_bindings (size_t *count)
{
  static const qamrpp_native_binding bindings[] = {
    { "lpicorl_syntax", lpikorl_syntax_native },
  };

  if (count)
    {
      *count = QAMRPP_ARRAY_COUNT (bindings);
    }
  return bindings;
}
