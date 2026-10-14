#include "third_party/QaMRpp/QaMRpp-Library.h"
#include "qamrpp-lpikorl/lpikorl.h"

static lpikorl_runtime g_lpikorl_runtime = { 0 };

static qamrpp_native_binding g_lpikorl_bindings[16];

int
lpikorl_on_load (qamrpp_context *ctx, const qamrpp_host_api *host_api)
{
  size_t offset = 0;
  size_t count = 0;
  const qamrpp_native_binding *bindings = 0;

  if (!host_api)
    {
      return 0;
    }

  g_lpikorl_runtime.host_api = host_api;
  host_api->context_set_userdata (ctx, &g_lpikorl_runtime);

  bindings = lpikorl_prompt_bindings (&count);
  if (bindings && count > 0)
    {
      size_t i = 0;
      for (; i < count; ++i)
        {
          g_lpikorl_bindings[offset + i] = bindings[i];
        }
      offset += count;
    }

  bindings = lpikorl_directive_bindings (&count);
  if (bindings && count > 0)
    {
      size_t i = 0;
      for (; i < count; ++i)
        {
          g_lpikorl_bindings[offset + i] = bindings[i];
        }
      offset += count;
    }

  bindings = lpikorl_syntax_bindings (&count);
  if (bindings && count > 0)
    {
      size_t i = 0;
      for (; i < count; ++i)
        {
          g_lpikorl_bindings[offset + i] = bindings[i];
        }
      offset += count;
    }

  bindings = lpikorl_completion_bindings (&count);
  if (bindings && count > 0)
    {
      size_t i = 0;
      for (; i < count; ++i)
        {
          g_lpikorl_bindings[offset + i] = bindings[i];
        }
      offset += count;
    }

  bindings = lpikorl_help_bindings (&count);
  if (bindings && count > 0)
    {
      size_t i = 0;
      for (; i < count; ++i)
        {
          g_lpikorl_bindings[offset + i] = bindings[i];
        }
      offset += count;
    }

  bindings = lpikorl_history_bindings (&count);
  if (bindings && count > 0)
    {
      size_t i = 0;
      for (; i < count; ++i)
        {
          g_lpikorl_bindings[offset + i] = bindings[i];
        }
      offset += count;
    }

  bindings = lpikorl_idl_bindings (&count);
  if (bindings && count > 0)
    {
      size_t i = 0;
      for (; i < count; ++i)
        {
          g_lpikorl_bindings[offset + i] = bindings[i];
        }
      offset += count;
    }

  bindings = lpikorl_macros_bindings (&count);
  if (bindings && count > 0)
    {
      size_t i = 0;
      for (; i < count; ++i)
        {
          g_lpikorl_bindings[offset + i] = bindings[i];
        }
      offset += count;
    }

  bindings = lpikorl_menu_bindings (&count);
  if (bindings && count > 0)
    {
      size_t i = 0;
      for (; i < count; ++i)
        {
          g_lpikorl_bindings[offset + i] = bindings[i];
        }
      offset += count;
    }

  bindings = lpikorl_page_bindings (&count);
  if (bindings && count > 0)
    {
      size_t i = 0;
      for (; i < count; ++i)
        {
          g_lpikorl_bindings[offset + i] = bindings[i];
        }
      offset += count;
    }

  bindings = lpikorl_sandboxing_bindings (&count);
  if (bindings && count > 0)
    {
      size_t i = 0;
      for (; i < count; ++i)
        {
          g_lpikorl_bindings[offset + i] = bindings[i];
        }
      offset += count;
    }

  bindings = lpikorl_theme_bindings (&count);
  if (bindings && count > 0)
    {
      size_t i = 0;
      for (; i < count; ++i)
        {
          g_lpikorl_bindings[offset + i] = bindings[i];
        }
      offset += count;
    }

  bindings = lpikorl_colors_bindings (&count);
  if (bindings && count > 0)
    {
      size_t i = 0;
      for (; i < count; ++i)
        {
          g_lpikorl_bindings[offset + i] = bindings[i];
        }
      offset += count;
    }

  bindings = lpikorl_web_bindings (&count);
  if (bindings && count > 0)
    {
      size_t i = 0;
      for (; i < count; ++i)
        {
          g_lpikorl_bindings[offset + i] = bindings[i];
        }
      offset += count;
    }

  return 1;
}

void
lpikorl_on_unload (qamrpp_context *ctx)
{
  (void)ctx;
}

static const qamrpp_library_descriptor g_lpikorl_descriptor = {
  QAMRPP_LIBRARY_API_VERSION, "lpikorl", g_lpikorl_bindings, 14,
  lpikorl_on_load,            lpikorl_on_unload
};

QAMRPP_LIBRARY_EXPORT_DESCRIPTOR (g_lpikorl_descriptor)
