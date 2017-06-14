/*
 * Copyright (C) 2016  Red Hat, Inc.
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

#include "gmc-display-config-manager.h"

#include <stdint.h>
#include <stdio.h>
#include <math.h>

#include "gmc-display-config.h"
#include "gmc-dbus-display-config.h"

struct _CcDisplayConfigManager
{
  GObject parent;
  CcDbusDisplayConfig *proxy;
};

static void
cc_display_config_manager_initable_init_iface (GInitableIface *iface);

G_DEFINE_TYPE_WITH_CODE (CcDisplayConfigManager, cc_display_config_manager,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                cc_display_config_manager_initable_init_iface));

CcDisplayState *
cc_display_config_manager_new_current_state (CcDisplayConfigManager *manager,
                                             GError **error)
{
  return cc_display_state_new_current (manager->proxy, error);
}

#define MONITOR_CONFIG_FORMAT "(ssa{sv})"
#define MONITOR_CONFIGS_FORMAT "a" MONITOR_CONFIG_FORMAT

#define LOGICAL_MONITOR_CONFIG_FORMAT "(iidub" MONITOR_CONFIGS_FORMAT ")"

#define CONFIG_FORMAT "a" LOGICAL_MONITOR_CONFIG_FORMAT

static double
find_nearest_scale (CcDisplayMonitor *monitor,
                    CcDisplayMode *mode,
                    double configured_scale)
{
  double *supported_scales;
  int n_supported_scales;
  int i;
  double closest_scale = 0.0;
  double closest_scale_diff = DBL_MAX;

  supported_scales =
    cc_display_mode_get_supported_scales (mode, &n_supported_scales);

  for (i = 0; i < n_supported_scales; i++)
    {
      double scale = supported_scales[i];
      double scale_diff;

      scale_diff = fabs (configured_scale - scale);
      if (scale_diff < closest_scale_diff)
        {
          closest_scale_diff = scale_diff;
          closest_scale = scale;
        }
    }

  return closest_scale;
}

static GVariant *
create_monitors_config_variant (CcDisplayState *state,
                                CcDisplayConfig *config)
{
  GVariantBuilder config_builder;
  GList *logical_monitor_configs;
  GList *l;

  g_variant_builder_init (&config_builder, G_VARIANT_TYPE (CONFIG_FORMAT));

  logical_monitor_configs =
    cc_display_config_get_logical_logical_monitor_configs (config);
  for (l = logical_monitor_configs; l; l = l->next)
    {
      CcDisplayLogicalMonitorConfig *logical_monitor_config = l->data;
      GVariantBuilder monitor_configs_builder;
      GList *monitor_configs;
      GList *k;
      int x, y;
      bool scale_calculated = false;
      double scale;
      CcDisplayTransform transform;
      gboolean is_primary;

      g_variant_builder_init (&monitor_configs_builder,
                              G_VARIANT_TYPE (MONITOR_CONFIGS_FORMAT));
      monitor_configs =
        cc_display_logical_monitor_config_get_monitor_configs (logical_monitor_config);
      for (k = monitor_configs; k; k = k->next)
        {
          CcDisplayMonitorConfig *monitor_config = k->data;
          CcDisplayMonitor *monitor;
          CcDisplayMode *mode;
          const char *connector;
          int resolution_width;
          int resolution_height;
          double refresh_rate;

          monitor = cc_display_monitor_config_get_monitor (monitor_config);
          connector = cc_display_monitor_get_connector (monitor);
          mode = cc_display_monitor_config_get_mode (monitor_config);
          cc_display_mode_get_resolution (mode,
                                          &resolution_width, &resolution_height);
          refresh_rate = cc_display_mode_get_refresh_rate (mode);

          if (!scale_calculated)
            {
              double configured_scale =
                cc_display_logical_monitor_config_get_scale (logical_monitor_config);

              scale = find_nearest_scale (monitor, mode, configured_scale);
              scale_calculated = true;
            }

          g_variant_builder_add (&monitor_configs_builder, MONITOR_CONFIG_FORMAT,
                                 connector,
                                 (int32_t) resolution_width,
                                 (int32_t) resolution_height,
                                 refresh_rate,
                                 NULL);
        }

      cc_display_logical_monitor_config_get_position (logical_monitor_config,
                                                      &x, &y);
      transform =
        cc_display_logical_monitor_config_get_transform (logical_monitor_config);
      is_primary =
        cc_display_logical_monitor_config_is_primary (logical_monitor_config);

      g_variant_builder_add (&config_builder, LOGICAL_MONITOR_CONFIG_FORMAT,
                             (int32_t) x,
                             (int32_t) y,
                             scale,
                             (uint32_t) transform,
                             is_primary,
                             &monitor_configs_builder);
    }

  return g_variant_builder_end (&config_builder);
}

gboolean
cc_display_config_manager_apply (CcDisplayConfigManager *manager,
                                 CcDisplayState *state,
                                 CcDisplayConfig *config,
                                 CcDisplayConfigMethod method,
                                 GError **error)
{
  CcDbusDisplayConfig *proxy = manager->proxy;
  unsigned int serial;
  GVariant *logical_monitor_configs_variant;
  GVariantBuilder properties_builder;
  CcDisplayLayoutMode layout_mode;

  serial = cc_display_state_get_serial (state);
  logical_monitor_configs_variant =
    create_monitors_config_variant (state, config);

  g_variant_builder_init (&properties_builder, G_VARIANT_TYPE ("a{sv}"));
  if (cc_display_config_get_layout_mode (config, &layout_mode))
    {
      g_variant_builder_add (&properties_builder,
                             "{sv}", "layout-mode",
                             g_variant_new_uint32 (layout_mode));
    }

  g_print ("%s\n", g_variant_print (logical_monitor_configs_variant, TRUE));

  return cc_dbus_display_config_call_apply_monitors_config_sync (
    proxy,
    serial,
    method,
    logical_monitor_configs_variant,
    g_variant_builder_end (&properties_builder),
    NULL,
    error);
}

CcDisplayConfigManager *
cc_display_config_manager_new (GError **error)
{
  g_autoptr(CcDisplayConfigManager) manager = NULL;

  manager = g_object_new (CC_TYPE_DISPLAY_CONFIG_MANAGER, NULL);
  if (!g_initable_init (G_INITABLE (manager), NULL, error))
    return NULL;

  return g_steal_pointer (&manager);
}

static gboolean
cc_display_config_manager_initable_init (GInitable *initable,
                                         GCancellable *cancellable,
                                         GError **error)
{
  CcDisplayConfigManager *manager = CC_DISPLAY_CONFIG_MANAGER (initable);
  CcDbusDisplayConfig *proxy;

  proxy = cc_dbus_display_config_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                                         G_DBUS_PROXY_FLAGS_NONE,
                                                         "org.gnome.Mutter.DisplayConfig",
                                                         "/org/gnome/Mutter/DisplayConfig",
                                                         NULL, error);
  if (!proxy)
    return FALSE;

  manager->proxy = proxy;

  return TRUE;
}

static void
cc_display_config_manager_initable_init_iface (GInitableIface *iface)
{
  iface->init = cc_display_config_manager_initable_init;
}

static void
cc_display_config_manager_init (CcDisplayConfigManager *config_manager)
{
}

static void
cc_display_config_manager_class_init (CcDisplayConfigManagerClass *klass)
{
}
