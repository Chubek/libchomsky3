#ifndef LPIKORL_H
#define LPIKORL_H

#include "third_party/QaMRpp/QaMRpp-Library.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct lpikorl_runtime
{
  const qamrpp_host_api *host_api;
} lpikorl_runtime;

int lpikorl_on_load (qamrpp_context *ctx, const qamrpp_host_api *host_api);
void lpikorl_on_unload (qamrpp_context *ctx);

const qamrpp_native_binding *lpikorl_prompt_bindings (size_t *count);
const qamrpp_native_binding *lpikorl_directive_bindings (size_t *count);
const qamrpp_native_binding *lpikorl_syntax_bindings (size_t *count);
const qamrpp_native_binding *lpikorl_completion_bindings (size_t *count);
const qamrpp_native_binding *lpikorl_help_bindings (size_t *count);
const qamrpp_native_binding *lpikorl_history_bindings (size_t *count);
const qamrpp_native_binding *lpikorl_idl_bindings (size_t *count);
const qamrpp_native_binding *lpikorl_macros_bindings (size_t *count);
const qamrpp_native_binding *lpikorl_menu_bindings (size_t *count);
const qamrpp_native_binding *lpikorl_page_bindings (size_t *count);
const qamrpp_native_binding *lpikorl_sandboxing_bindings (size_t *count);
const qamrpp_native_binding *lpikorl_theme_bindings (size_t *count);
const qamrpp_native_binding *lpikorl_colors_bindings (size_t *count);
const qamrpp_native_binding *lpikorl_web_bindings (size_t *count);

#ifdef __cplusplus
}
#endif

#endif
