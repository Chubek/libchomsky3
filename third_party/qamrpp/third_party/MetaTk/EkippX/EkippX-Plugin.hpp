/**
 * @file EkippX-Plugin.hpp
 * @brief Compatibility include for the EkippX plugin API.
 *
 * Include this header from simple plugins that only need the public plugin
 * registration surface. It forwards to EkippX-PluginAPI.hpp and preserves a
 * stable include path for downstream examples.
 */
#pragma once

#include "EkippX-PluginAPI.hpp"
