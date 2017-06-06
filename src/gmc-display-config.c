/*
 * Copyright (C) 2016-2017  Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include "gmc-display-config.h"
#include "gmc-dbus-display-config.h"

#include <glib-object.h>
#include <stdint.h>

#include <stdio.h>

#define CC_DBUS_DISPLAY_CONFIG_MODE_FLAGS_PREFERRED (1 << 0)
#define CC_DBUS_DISPLAY_CONFIG_MODE_FLAGS_CURRENT (1 << 1)

typedef struct _CcDisplayMode
{
  int resolution_width;
  int resolution_height;
  double refresh_rate;
  double preferred_scale;
  unsigned int flags;
  double *supported_scales;
  int n_supported_scales;
} CcDisplayMode;

typedef struct _CcDisplayMonitor
{
  char *connector;
  char *vendor;
  char *product;
  char *serial;

  GList *modes;
  CcDisplayMode *current_mode;
  CcDisplayMode *preferred_mode;

  char *display_name;
} CcDisplayMonitor;

typedef struct _CcDisplayLogicalMonitor
{
  int x;
  int y;
  double scale;
  CcDisplayTransform transform;
  GList *monitors;
  bool is_primary;
} CcDisplayLogicalMonitor;

struct _CcDisplayState
{
  unsigned int serial;

  GList *monitors;
  GList *logical_monitors;

  gboolean has_max_screen_size;
  int max_screen_width;
  int max_screen_height;
};

typedef struct _CcDisplayMonitorConfig
{
  CcDisplayMonitor *monitor;
  CcDisplayMode *mode;
} CcDisplayMonitorConfig;

typedef struct _CcDisplayLogicalMonitorConfig
{
  int x;
  int y;
  double scale;
  CcDisplayTransform transform;
  GList *monitor_configs;
  bool is_primary;
} CcDisplayLogicalMonitorConfig;

typedef struct _CcDisplayConfig
{
  GList *logical_monitor_configs;
  gboolean layout_mode_set;
  CcDisplayLayoutMode layout_mode;
} CcDisplayConfig;

const char *
cc_display_monitor_get_connector (CcDisplayMonitor *monitor)
{
  return monitor->connector;
}

const char *
cc_display_monitor_get_display_name (CcDisplayMonitor *monitor)
{
  return monitor->display_name;
}

bool
cc_display_monitor_is_active (CcDisplayMonitor *monitor)
{
  return monitor->current_mode != NULL;
}

bool
cc_display_monitor_is_builtin_display (CcDisplayMonitor *monitor)
{
  return false;
}

CcDisplayMode *
cc_display_monitor_get_current_mode (CcDisplayMonitor *monitor)
{
  return monitor->current_mode;
}

CcDisplayMode *
cc_display_monitor_get_preferred_mode (CcDisplayMonitor *monitor)
{
  return monitor->preferred_mode;
}

GList *
cc_display_monitor_get_modes (CcDisplayMonitor *monitor)
{
  return monitor->modes;
}

void
cc_display_mode_get_resolution (CcDisplayMode *mode,
                                int *width,
                                int *height)
{
  *width = mode->resolution_width;
  *height = mode->resolution_height;
}

double
cc_display_mode_get_refresh_rate (CcDisplayMode *mode)
{
  return mode->refresh_rate;
}

double
cc_display_mode_get_preferred_scale (CcDisplayMode *mode)
{
  return mode->preferred_scale;
}

unsigned int
cc_display_state_get_serial (CcDisplayState *state)
{
  return state->serial;
}

GList *
cc_display_state_get_monitors (CcDisplayState *state)
{
  return state->monitors;
}

GList *
cc_display_state_get_logical_monitors (CcDisplayState *state)
{
  return state->logical_monitors;
}

gboolean
cc_display_state_get_max_screen_size (CcDisplayState *state,
                                      int *max_width,
                                      int *max_height)
{
  if (!state->has_max_screen_size)
    return FALSE;

  *max_width = state->max_screen_width;
  *max_height = state->max_screen_height;
  return TRUE;
}

#define MODE_FORMAT "(iiddadu)"
#define MODES_FORMAT "a" MODE_FORMAT
#define MONITOR_SPEC_FORMAT "(ssss)"
#define MONITOR_FORMAT "(" MONITOR_SPEC_FORMAT MODES_FORMAT "@a{sv})"
#define MONITORS_FORMAT "a" MONITOR_FORMAT

#define LOGICAL_MONITOR_FORMAT "(iiduba" MONITOR_SPEC_FORMAT "@a{sv})"
#define LOGICAL_MONITORS_FORMAT "a" LOGICAL_MONITOR_FORMAT

static CcDisplayMode *
cc_display_mode_new_from_variant (GVariant *mode_variant)
{
  CcDisplayMode *mode;
  uint32_t flags;
  int32_t resolution_width;
  int32_t resolution_height;
  double refresh_rate;
  double preferred_scale;
  GVariantIter *supported_scales_iter;
  GVariant *scale_variant;
  int i = 0;

  g_variant_get (mode_variant, MODE_FORMAT,
                 &resolution_width,
                 &resolution_height,
                 &refresh_rate,
                 &preferred_scale,
		 &supported_scales_iter,
                 &flags);

  mode = g_new0 (CcDisplayMode, 1);
  *mode = (CcDisplayMode) {
    .flags = flags,
    .resolution_width = resolution_width,
    .resolution_height = resolution_height,
    .refresh_rate = refresh_rate,
    .preferred_scale = preferred_scale
  };

  mode->n_supported_scales = g_variant_iter_n_children (supported_scales_iter);
  mode->supported_scales = g_new0 (double, mode->n_supported_scales);
  while ((scale_variant = g_variant_iter_next_value (supported_scales_iter)))
    {
      double scale;

      g_variant_get (scale_variant, "d", &scale);
      mode->supported_scales[i++] = scale;
    }

  return mode;
}

static CcDisplayMonitor *
cc_display_monitor_new_from_variant (GVariant *monitor_variant)
{
  CcDisplayMonitor *monitor;
  GVariantIter *modes_iter;
  GVariant *properties_variant;
  GVariant *mode_variant;
  GVariant *display_name_variant;

  monitor = g_new0 (CcDisplayMonitor, 1);

  g_variant_get (monitor_variant, MONITOR_FORMAT,
                 &monitor->connector,
                 &monitor->vendor,
                 &monitor->product,
                 &monitor->serial,
                 &modes_iter,
                 &properties_variant);

  while ((mode_variant = g_variant_iter_next_value (modes_iter)))
    {
      CcDisplayMode *mode;

      mode = cc_display_mode_new_from_variant (mode_variant);
      monitor->modes = g_list_append (monitor->modes, mode);

      if (mode->flags & CC_DBUS_DISPLAY_CONFIG_MODE_FLAGS_PREFERRED)
        {
          g_warn_if_fail (!monitor->preferred_mode);
          monitor->preferred_mode = mode;
        }

      if (mode->flags & CC_DBUS_DISPLAY_CONFIG_MODE_FLAGS_CURRENT)
        {
          g_warn_if_fail (!monitor->current_mode);
          monitor->current_mode = mode;
        }

      g_variant_unref (mode_variant);
    }
  g_variant_iter_free (modes_iter);

  display_name_variant = g_variant_lookup_value (properties_variant,
                                                 "display-name",
                                                 G_VARIANT_TYPE ("s"));
  if (display_name_variant)
    {
      g_variant_get (display_name_variant, "s",
                     &monitor->display_name);
    }

  return monitor;
}

#undef MODE_FORMAT
#undef MODES_FORMAT
#undef MONITOR_FORMAT
#undef MONITORS_FORMAT

static void
get_monitors_from_variant (CcDisplayState *state,
                           GVariant *monitors_variant)
{
  GVariantIter monitor_iter;
  GVariant *monitor_variant;

  g_assert (!state->monitors);

  g_variant_iter_init (&monitor_iter, monitors_variant);
  while ((monitor_variant = g_variant_iter_next_value (&monitor_iter)))
    {
      CcDisplayMonitor *monitor;

      monitor = cc_display_monitor_new_from_variant (monitor_variant);
      state->monitors = g_list_append (state->monitors, monitor);

      g_variant_unref (monitor_variant);
    }
}

static CcDisplayMonitor *
monitor_from_spec (CcDisplayState *state,
                   const char *connector,
                   const char *vendor,
                   const char *product,
                   const char *serial)
{
  GList *l;

  for (l = state->monitors; l; l = l->next)
    {
      CcDisplayMonitor *monitor = l->data;

      if (g_strcmp0 (monitor->connector, connector) == 0 &&
          g_strcmp0 (monitor->vendor, vendor) == 0 &&
          g_strcmp0 (monitor->product, product) == 0 &&
          g_strcmp0 (monitor->serial, serial) == 0)
        return monitor;
    }

  return NULL;
}

static CcDisplayLogicalMonitor *
cc_display_logical_monitor_new_from_variant (CcDisplayState *state,
                                             GVariant *logical_monitor_variant)
{
  CcDisplayLogicalMonitor *logical_monitor;
  GVariantIter *monitor_specs_iter;
  GVariant *monitor_spec_variant;
  int x, y;
  double scale;
  CcDisplayTransform transform;
  gboolean is_primary;
  GVariant *properties;

  logical_monitor = g_new0 (CcDisplayLogicalMonitor, 1);

  g_variant_get (logical_monitor_variant, LOGICAL_MONITOR_FORMAT,
                 &x,
                 &y,
                 &scale,
                 &transform,
                 &is_primary,
                 &monitor_specs_iter,
                 &properties);

  while ((monitor_spec_variant = g_variant_iter_next_value (monitor_specs_iter)))
    {
      CcDisplayMonitor *monitor;
      g_autofree char *connector = NULL;
      g_autofree char *vendor = NULL;
      g_autofree char *product = NULL;
      g_autofree char *serial = NULL;

      g_variant_get (monitor_spec_variant, MONITOR_SPEC_FORMAT,
                     &connector, &vendor, &product, &serial);

      monitor = monitor_from_spec (state, connector, vendor, product, serial);
      if (!monitor)
        {
          g_warning ("Couldn't find monitor given spec: %s, %s, %s, %s\n",
                     connector, vendor, product, serial);
          continue;
        }

      logical_monitor->monitors = g_list_append (logical_monitor->monitors,
                                                 monitor);

      g_variant_unref (monitor_spec_variant);
    }
  g_variant_iter_free (monitor_specs_iter);

  if (!logical_monitor->monitors)
    {
      g_warning ("Got an empty logical monitor, ignoring\n");
      return NULL;
    }

  logical_monitor->x = x;
  logical_monitor->y = y;
  logical_monitor->scale = scale;
  logical_monitor->transform = transform;
  logical_monitor->is_primary = is_primary;

  return logical_monitor;
}

static void
cc_display_logical_monitor_free (CcDisplayLogicalMonitor *logical_monitor)
{
  g_list_free (logical_monitor->monitors);
  g_free (logical_monitor);
}

GList *
cc_display_logical_monitor_get_monitors (CcDisplayLogicalMonitor *logical_monitor)
{
  return logical_monitor->monitors;
}

bool
cc_display_logical_monitor_is_primary (CcDisplayLogicalMonitor *logical_monitor)
{
  return logical_monitor->is_primary;
}

void
cc_display_logical_monitor_calculate_layout (CcDisplayLogicalMonitor *logical_monitor,
                                             cairo_rectangle_int_t *layout)
{
  CcDisplayMonitor *monitor;

  g_return_if_fail (logical_monitor->monitors);

  monitor = logical_monitor->monitors->data;

  *layout = (cairo_rectangle_int_t) {
    .x = logical_monitor->x,
    .y = logical_monitor->y,
    .width = monitor->current_mode->resolution_width,
    .height = monitor->current_mode->resolution_height
  };
}

double
cc_display_logical_monitor_get_scale (CcDisplayLogicalMonitor *logical_monitor)
{
  return logical_monitor->scale;
}

CcDisplayTransform
cc_display_logical_monitor_get_transform (CcDisplayLogicalMonitor *logical_monitor)
{
  return logical_monitor->transform;
}

static void
get_logical_monitors_from_variant (CcDisplayState *state,
                                   GVariant *logical_monitors_variant)
{
  GVariantIter logical_monitor_iter;

  g_variant_iter_init (&logical_monitor_iter, logical_monitors_variant);
  while (true)
    {
      GVariant *logical_monitor_variant =
        g_variant_iter_next_value (&logical_monitor_iter);
      CcDisplayLogicalMonitor *logical_monitor;

      if (!logical_monitor_variant)
        break;

      logical_monitor =
        cc_display_logical_monitor_new_from_variant (state,
                                                     logical_monitor_variant);
      if (!logical_monitor)
        continue;

      state->logical_monitors = g_list_append (state->logical_monitors,
                                               logical_monitor);
    }
}

static void
get_max_screen_size_from_variant (CcDisplayState *state,
                                  GVariant *max_screen_size_variant)
{
  state->has_max_screen_size = TRUE;
  g_variant_get (max_screen_size_variant, "(ii)",
                 &state->max_screen_width,
                 &state->max_screen_height);
}

static bool
get_current_state (CcDisplayState *state,
                   CcDbusDisplayConfig *proxy,
                   GError **error)
{
  unsigned int serial;
  GVariant *monitors_variant;
  GVariant *logical_monitors_variant;
  GVariant *max_screen_size_variant;
  GVariant *properties_variant;

  if (!cc_dbus_display_config_call_get_current_state_sync (proxy,
                                                           &serial,
                                                           &monitors_variant,
                                                           &logical_monitors_variant,
                                                           &properties_variant,
                                                           NULL,
                                                           error))
    return false;

  state->serial = serial;

  get_monitors_from_variant (state, monitors_variant);
  get_logical_monitors_from_variant (state, logical_monitors_variant);

  max_screen_size_variant = g_variant_lookup_value (properties_variant,
                                                    "max-screen-size",
                                                    G_VARIANT_TYPE ("(ii)"));
  if (max_screen_size_variant)
    get_max_screen_size_from_variant (state, max_screen_size_variant);

  return true;
}

static void
cc_display_monitor_free (CcDisplayMonitor *monitor)
{
  g_list_free_full (monitor->modes, g_free);
  g_free (monitor);
}

CcDisplayState *
cc_display_state_new_current (CcDbusDisplayConfig *proxy,
                              GError **error)
{
  g_autofree CcDisplayState *state = NULL;

  state = g_new0 (CcDisplayState, 1);

  if (!get_current_state (state, proxy, error))
    return NULL;

  return g_steal_pointer (&state);
}

void
cc_display_state_free (CcDisplayState *state)
{
  g_list_free_full (state->logical_monitors,
                    (GDestroyNotify) cc_display_logical_monitor_free);
  g_list_free_full (state->monitors,
                    (GDestroyNotify) cc_display_monitor_free);
  g_free (state);
}

double *
cc_display_mode_get_supported_scales (CcDisplayMode *mode,
                                      int           *n_supported_scales)
{
  *n_supported_scales = mode->n_supported_scales;
  return g_memdup (mode->supported_scales,
                   sizeof (double) * mode->n_supported_scales);
}

CcDisplayMonitor *
cc_display_monitor_config_get_monitor (CcDisplayMonitorConfig *monitor_config)
{
  return monitor_config->monitor;
}

CcDisplayMode *
cc_display_monitor_config_get_mode (CcDisplayMonitorConfig *monitor_config)
{
  return monitor_config->mode;
}

void
cc_display_monitor_config_set_mode (CcDisplayMonitorConfig *monitor_config,
                                    CcDisplayMode *mode)
{
  monitor_config->mode = mode;
}

CcDisplayLogicalMonitorConfig *
cc_display_logical_monitor_config_new (void)
{
  return g_new0 (CcDisplayLogicalMonitorConfig, 1);
}

void
cc_display_logical_monitor_config_set_position (CcDisplayLogicalMonitorConfig *logical_monitor_config,
                                                int x,
                                                int y)
{
  logical_monitor_config->x = x;
  logical_monitor_config->y = y;
}

void
cc_display_logical_monitor_config_set_scale (CcDisplayLogicalMonitorConfig *logical_monitor_config,
                                             double scale)
{
  logical_monitor_config->scale = scale;
}

void
cc_display_logical_monitor_config_set_transform (CcDisplayLogicalMonitorConfig *logical_monitor_config,
                                                 CcDisplayTransform             transform)
{
  logical_monitor_config->transform = transform;
}

void
cc_display_logical_monitor_config_set_is_primary (CcDisplayLogicalMonitorConfig *logical_monitor_config,
                                                  bool is_primary)
{
  logical_monitor_config->is_primary = is_primary;
}

void
cc_display_logical_monitor_config_add_monitor (CcDisplayLogicalMonitorConfig *logical_monitor_config,
                                               CcDisplayMonitor *monitor,
                                               CcDisplayMode *mode)
{
  CcDisplayMonitorConfig *monitor_config;

  monitor_config = g_new0 (CcDisplayMonitorConfig, 1);
  *monitor_config = (CcDisplayMonitorConfig) {
    .monitor = monitor,
    .mode = mode
  };
  fprintf(stderr, ":::: %s:%d %s() - \n", __FILE__, __LINE__, __func__);
  logical_monitor_config->monitor_configs =
    g_list_append (logical_monitor_config->monitor_configs, monitor_config);
}

bool
cc_display_logical_monitor_config_is_primary (CcDisplayLogicalMonitorConfig *logical_monitor_config)
{
  return logical_monitor_config->is_primary;
}

double
cc_display_logical_monitor_config_get_scale (CcDisplayLogicalMonitorConfig *logical_monitor_config)
{
  return logical_monitor_config->scale;
}

CcDisplayTransform
cc_display_logical_monitor_config_get_transform (CcDisplayLogicalMonitorConfig *logical_monitor_config)
{
  return logical_monitor_config->transform;
}

void
cc_display_logical_monitor_config_get_position (CcDisplayLogicalMonitorConfig *logical_monitor_config,
                                                int *x,
                                                int *y)
{
  *x = logical_monitor_config->x;
  *y = logical_monitor_config->y;
}

void
cc_display_logical_monitor_config_calculate_layout (CcDisplayLogicalMonitorConfig *logical_monitor_config,
                                                    cairo_rectangle_int_t *layout)
{
  CcDisplayMonitorConfig *monitor_config;

  g_return_if_fail (logical_monitor_config->monitor_configs);

  monitor_config = logical_monitor_config->monitor_configs->data;

  *layout = (cairo_rectangle_int_t) {
    .x = logical_monitor_config->x,
    .y = logical_monitor_config->y,
    .width = monitor_config->mode->resolution_width,
    .height = monitor_config->mode->resolution_height
  };
}

GList *
cc_display_logical_monitor_config_get_monitor_configs (CcDisplayLogicalMonitorConfig *logical_monitor_config)
{
  return logical_monitor_config->monitor_configs;
}

CcDisplayConfig *
cc_display_config_new (void)
{
  CcDisplayConfig *config;

  config = g_new0 (CcDisplayConfig, 1);

  return config;
}

void
cc_display_config_set_layout_mode (CcDisplayConfig *config,
                                   CcDisplayLayoutMode layout_mode)
{
  config->layout_mode_set = TRUE;
  config->layout_mode = layout_mode;
}

gboolean
cc_display_config_get_layout_mode (CcDisplayConfig *config,
                                   CcDisplayLayoutMode *layout_mode)
{
  if (config->layout_mode_set)
    {
      *layout_mode = config->layout_mode;
      return TRUE;
    }
  else
    {
      return FALSE;
    }
}

void
cc_display_config_add_logical_monitor (CcDisplayConfig *config,
                                       CcDisplayLogicalMonitorConfig *logical_monitor_config)
{
  config->logical_monitor_configs =
    g_list_append (config->logical_monitor_configs, logical_monitor_config);
}

GList *
cc_display_config_get_logical_logical_monitor_configs (CcDisplayConfig *config)
{
  return config->logical_monitor_configs;
}
