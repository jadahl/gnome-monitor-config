/*
 * Copyright (C) 2016-2017 Red Hat, Inc.
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

#define _GNU_SOURCE

#include "gmc-display-config.h"

#include <errno.h>
#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "gmc-display-config-manager.h"

static CcDisplayState *current_state = NULL;

static CcDisplayConfig *pending_config = NULL;
static gboolean pending_layout_mode_set = FALSE;
static CcDisplayLayoutMode pending_layout_mode = CC_DISPLAY_LAYOUT_MODE_LOGICAL;
static CcDisplayLogicalMonitorConfig *pending_logical_monitor_config = NULL;
static CcDisplayMonitor *current_monitor;
static int pending_logical_monitor_x;
static int pending_logical_monitor_y;
static double pending_logical_monitor_scale;
static CcDisplayTransform pending_logical_monitor_transform;

static void
print_usage (FILE *stream)
{
  fprintf (stream, "Usage: %s [OPTIONS...] COMMAND [COMMAND OPTIONS...]\n",
           g_get_prgname ());
}

static void
print_help (void)
{
  print_usage (stdout);
  printf ("Options:\n"
          " -h, --help                  Print help text\n"
          "\n"
          "Commands:\n"
          "  list                       List current monitors and current configuration\n"
          "  set                        Set new configuration\n"
          "  show                       Show monitor labels\n"
          "\n"
          "Options for 'set':\n"
          " -L, --logical-monitor       Add logical monitor\n"
          " -x, --x=X                   Set x position of newly added logical monitor\n"
          " -y, --y=Y                   Set y position of newly added logical monitor\n"
          " -s, --scale=SCALE           Set scale of newly added logical monitor\n"
          " -t, --transform=TRANSFORM   Set transform (normal, left, right, flip)\n"
          " -p, --primary               Mark the newly added logical monitor as primary\n"
          " -m, --mode                  Set the display resolution and refresh rate. ex: 1920x1080@60\n"
          " -M, --monitor=CONNECTOR     Add a monitor (given its connector) to newly added\n"
          "                             logical monitor\n"
          " -P, --persistent            Set the configuration as persistent\n"
          " -V, --verify                Set the configuration as persistent and skip verification dialogue\n"
          " --logical-layout-mode       Set logical layout mode\n"
          " --physical-layout-mode      Set physical layout mode\n"
          );
}

static void
list_modes (CcDisplayMonitor *monitor)
{
  CcDisplayMode *preferred_mode, *current_mode;
  GList *modes, *l;

  preferred_mode = cc_display_monitor_get_preferred_mode (monitor);
  current_mode = cc_display_monitor_get_current_mode (monitor);
  modes = cc_display_monitor_get_modes (monitor);

  for (l = modes; l; l = l->next)
    {
      CcDisplayMode *mode = l->data;
      int resolution_width, resolution_height;
      double refresh_rate;
      double preferred_scale;
      double *supported_scales;
      int n_supported_scales;
      int i;

      cc_display_mode_get_resolution (mode,
                                      &resolution_width, &resolution_height);
      refresh_rate = cc_display_mode_get_refresh_rate (mode);
      preferred_scale = cc_display_mode_get_preferred_scale (mode);

      g_print ("  %dx%d@%g [id: '%s'] [preferred scale = %g (",
               resolution_width, resolution_height,
               refresh_rate,
               cc_display_mode_get_id (mode),
               preferred_scale);

      supported_scales =
        cc_display_mode_get_supported_scales (mode, &n_supported_scales);
      for (i = 0; i < n_supported_scales; i++)
        g_print ("%s%g", i == 0 ? "" : " ", supported_scales[i]);
      g_free (supported_scales);

      g_print (")]%s%s\n",
               mode == preferred_mode ? " PREFERRED" : "",
               mode == current_mode ? " CURRENT" : "");
    }
}

static void
list_logical_monitor_monitors (CcDisplayLogicalMonitor *logical_monitor)
{
  GList *monitors;
  GList *l;

  monitors = cc_display_logical_monitor_get_monitors (logical_monitor);
  for (l = monitors; l; l = l->next)
    {
      CcDisplayMonitor *monitor = l->data;
      const char *connector = cc_display_monitor_get_connector (monitor);

      g_print ("  %s\n", connector);
    }
}

static const char *
transform_to_str (CcDisplayTransform transform)
{
  switch (transform)
    {
    case CC_DISPLAY_TRANSFORM_NORMAL:
      return "normal";
    case CC_DISPLAY_TRANSFORM_90:
      return "left";
    case CC_DISPLAY_TRANSFORM_180:
      return "upside down";
    case CC_DISPLAY_TRANSFORM_270:
      return "right";
    case CC_DISPLAY_TRANSFORM_FLIPPED:
      return "flipped";
    case CC_DISPLAY_TRANSFORM_FLIPPED_90:
      return "left flipped";
    case CC_DISPLAY_TRANSFORM_FLIPPED_180:
      return "upside down flipped";
    case CC_DISPLAY_TRANSFORM_FLIPPED_270:
      return "right flipped";
    }

  g_assert_not_reached ();
}

static gboolean
list_monitors (GError **error)
{
  CcDisplayConfigManager *config_manager;
  CcDisplayState *state;
  GList *l;
  int max_screen_width, max_screen_height;

  config_manager = cc_display_config_manager_new (error);
  if (!config_manager)
    return FALSE;

  state = cc_display_config_manager_new_current_state (config_manager, error);
  if (!state)
    return FALSE;

  for (l = cc_display_state_get_monitors (state); l; l = l->next)
    {
      CcDisplayMonitor *monitor = l->data;
      const char *connector = cc_display_monitor_get_connector (monitor);
      bool is_active = cc_display_monitor_is_active (monitor);
      bool is_builtin_display = cc_display_monitor_is_builtin_display (monitor);
      const char *display_name = cc_display_monitor_get_display_name (monitor);

      g_print ("Monitor [ %s ] %s%s\n",
               connector,
               is_active ? "ON" : "OFF",
               is_builtin_display ? " BUILTIN" : "");
      if (display_name)
        g_print ("  display-name: %s\n", display_name);
      list_modes (monitor);
    }

  for (l = cc_display_state_get_logical_monitors (state); l; l = l->next)
    {
      CcDisplayLogicalMonitor *logical_monitor = l->data;
      cairo_rectangle_int_t layout;
      bool is_primary;
      double scale;
      CcDisplayTransform transform;

      cc_display_logical_monitor_calculate_layout (logical_monitor, &layout);
      is_primary = cc_display_logical_monitor_is_primary (logical_monitor);
      scale = cc_display_logical_monitor_get_scale (logical_monitor);
      transform = cc_display_logical_monitor_get_transform (logical_monitor);

      g_print ("Logical monitor [ %dx%d+%d+%d ]%s, scale = %g, transform = %s\n",
               layout.width, layout.height, layout.x, layout.y,
               is_primary ? ", PRIMARY" : "",
               scale,
               transform_to_str (transform));
      list_logical_monitor_monitors (logical_monitor);
    }

  if (cc_display_state_get_max_screen_size (state,
                                            &max_screen_width,
                                            &max_screen_height))
    g_print ("Max screen size: %dx%d\n", max_screen_width, max_screen_height);
  else
    g_print ("Max screen size: unlimited\n");

  return TRUE;
}

static gboolean
finalize_pending_logical_monitor_config (void)
{
  if (!pending_config)
    return FALSE;

  if (!pending_logical_monitor_config)
    return FALSE;

  if (pending_layout_mode_set)
    cc_display_config_set_layout_mode (pending_config,
                                       pending_layout_mode);
  cc_display_logical_monitor_config_set_position (pending_logical_monitor_config,
                                                  pending_logical_monitor_x,
                                                  pending_logical_monitor_y);
  cc_display_logical_monitor_config_set_scale (pending_logical_monitor_config,
                                               pending_logical_monitor_scale);
  cc_display_logical_monitor_config_set_transform (pending_logical_monitor_config,
                                                   pending_logical_monitor_transform);

  cc_display_config_add_logical_monitor (pending_config,
                                         pending_logical_monitor_config);

  pending_logical_monitor_config = NULL;

  return TRUE;
}

static gboolean
handle_logical_monitor_arg (GError **error)
{
  if (pending_logical_monitor_config)
    {
      if (!finalize_pending_logical_monitor_config ())
        {
          g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                       "Confugiration incomplete");
          return FALSE;
        }
    }

  pending_logical_monitor_config = cc_display_logical_monitor_config_new ();
  pending_logical_monitor_x = 0;
  pending_logical_monitor_y = 0;
  pending_logical_monitor_scale = 1.0;

  return TRUE;
}

static gboolean
handle_logical_monitor_property_arg (int option,
                                     const char *value,
                                     GError **error)
{
  switch (option)
    {
    case 'x':
      {
        int64_t x;

        errno = 0;
        x = g_ascii_strtoll (value, NULL, 10);
        if (errno || x > INT32_MAX || x < INT32_MIN)
          {
            g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                         "Invalid x value %s", value);
            return FALSE;
          }

        pending_logical_monitor_x = x;

        break;
      }
    case 'y':
      {
        int64_t y;

        errno = 0;
        y = g_ascii_strtoll (value, NULL, 10);
        if (errno || y > INT32_MAX || y < INT32_MIN)
          {
            g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                         "Invalid y value %s", value);
            return FALSE;
          }

        pending_logical_monitor_y = y;

        break;
      }
    case 's':
      {
        double scale;

        scale = g_ascii_strtod (value, NULL);
        if (errno)
          {
            g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                         "Invalid scale %s", value);
            return FALSE;
          }

        pending_logical_monitor_scale = scale;

        break;
      }
    case 't':
      {
        CcDisplayTransform transform;

        if (g_str_equal (value, "normal"))
          {
            transform = CC_DISPLAY_TRANSFORM_NORMAL;
          }
        else if (g_str_equal (value, "right"))
          {
            transform = CC_DISPLAY_TRANSFORM_270;
          }
        else if (g_str_equal (value, "left"))
          {
            transform = CC_DISPLAY_TRANSFORM_90;
          }
        else if (g_str_equal (value, "flip"))
          {
            transform = CC_DISPLAY_TRANSFORM_180;
          }
        else
          {
            g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                         "Invalid transform '%s'", value);
            return FALSE;
          }

        pending_logical_monitor_transform = transform;
        break;
      }
    default:
      g_printerr ("Unhandled option %c\n", option);
      g_assert_not_reached ();
    }

  return TRUE;
}

static gboolean
handle_logical_monitor_primary_arg (GError **error)
{
  if (!pending_logical_monitor_config)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Made primary without logical monitor");
      return FALSE;
    }

  cc_display_logical_monitor_config_set_is_primary (pending_logical_monitor_config,
                                                    TRUE);

  return TRUE;
}

static gboolean
add_monitor_with_preferred_mode (CcDisplayMonitor *monitor,
                                 GError **error)
{
  CcDisplayMode *mode;

  if (!pending_logical_monitor_config)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Tried to add monitor without logical monitor");
      return FALSE;
    }

  mode = cc_display_monitor_get_preferred_mode (monitor);
  cc_display_logical_monitor_config_add_monitor (pending_logical_monitor_config,
                                                 monitor, mode);

  return TRUE;
}

static gboolean
handle_monitor_arg (const char *value,
                    GError **error)
{
  GList *l;
  const char *connector = value;

  for (l = cc_display_state_get_monitors (current_state); l; l = l->next)
    {
      CcDisplayMonitor *monitor = l->data;

      if (g_str_equal (cc_display_monitor_get_connector (monitor),
                       connector))
        {
          current_monitor = monitor;
          return add_monitor_with_preferred_mode (monitor, error);
        }
    }

  g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
               "Tried to add unknown monitor %s", value);
  return FALSE;
}

static gboolean
handle_mode_arg (const char *optarg,
                 GError **error)
{
  CcDisplayMode *mode;
  GList *monitor_configs;
  CcDisplayMonitorConfig *monitor_config;

  if (!current_monitor)
  {
    g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                 "No current monitor");
    return FALSE;
  }

  mode = cc_display_monitor_lookup_mode (current_monitor, optarg);
  if (!mode)
  {
    g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                 "Invalid mode '%s'", optarg);
    return FALSE;
  }

  monitor_configs =
    cc_display_logical_monitor_config_get_monitor_configs (pending_logical_monitor_config);
  monitor_config = g_list_last (monitor_configs)->data;
  cc_display_monitor_config_set_mode (monitor_config, mode);

  return TRUE;
}

static void
print_pending_configuration (void)
{
  GList *logical_monitor_configs;
  GList *l;

  logical_monitor_configs =
    cc_display_config_get_logical_logical_monitor_configs (pending_config);
  for (l = logical_monitor_configs; l; l = l->next)
    {
      CcDisplayLogicalMonitorConfig *logical_monitor_config = l->data;
      cairo_rectangle_int_t layout;
      bool is_primary;
      double scale;
      GList *monitor_configs;
      GList *k;

      cc_display_logical_monitor_config_calculate_layout (logical_monitor_config,
                                                          &layout);
      is_primary =
        cc_display_logical_monitor_config_is_primary (logical_monitor_config);
      scale =
        cc_display_logical_monitor_config_get_scale (logical_monitor_config);
      g_print ("Logical monitor [ %dx%d+%d+%d ]%s, scale = %g\n",
               layout.width, layout.height, layout.x, layout.y,
               is_primary ? ", PRIMARY" : "",
               scale);

      monitor_configs =
        cc_display_logical_monitor_config_get_monitor_configs (logical_monitor_config);
      for (k = monitor_configs; k; k = k->next)
        {
          CcDisplayMonitorConfig *monitor_config = k->data;
          CcDisplayMonitor *monitor;
          CcDisplayMode *mode;
          const char *connector;
          int resolution_width, resolution_height;
          double refresh_rate;

          monitor = cc_display_monitor_config_get_monitor (monitor_config);
          connector = cc_display_monitor_get_connector (monitor);
          mode = cc_display_monitor_config_get_mode (monitor_config);
          cc_display_mode_get_resolution (mode,
                                          &resolution_width, &resolution_height);
          refresh_rate = cc_display_mode_get_refresh_rate (mode);

          g_print ("  Monitor [ %s ] %dx%d@%g\n",
                   connector,
                   resolution_width, resolution_height,
                   refresh_rate);
        }
    }
}

static int
set_monitors (int argc,
              char **argv,
              GError **error)
{
  struct option options[] = {
    { "logical-monitor", no_argument, 0, 'L' },
    { "persistent", no_argument, 0, 'P' },
    { "verify", no_argument, 0, 'V' },
    { "x", required_argument, 0, 'x' },
    { "y", required_argument, 0, 'y' },
    { "scale", required_argument, 0, 's' },
    { "transform", required_argument, 0, 't' },
    { "primary", no_argument, 0, 'p' },
    { "monitor", required_argument, 0, 'M' },
    { "mode", required_argument, 0, 'm' },
    { "logical-layout-mode", no_argument, 0, 0 },
    { "physical-layout-mode", no_argument, 0, 0 },
    { "help", no_argument, 0, 'h' },
    { }
  };
  CcDisplayConfigManager *config_manager;
  CcDisplayConfigMethod method = CC_DISPLAY_METHOD_TEMPORARY;

  config_manager = cc_display_config_manager_new (error);
  if (!config_manager)
    return FALSE;

  current_state = cc_display_config_manager_new_current_state (config_manager,
                                                               error);
  if (!current_state)
    return FALSE;

  pending_config = cc_display_config_new ();

  while (true)
    {
      int c;
      int option_index;

      c = getopt_long (argc, argv, "LPx:dy:ds:dt:spM:sm:dh",
                       options, &option_index);

      if (c < 0)
        break;

      switch (c)
        {
        case 0:
          if (g_str_equal (options[option_index].name,
                           "logical-layout-mode"))
            {
              pending_layout_mode = CC_DISPLAY_LAYOUT_MODE_LOGICAL;
              pending_layout_mode_set = TRUE;
            }
          else if (g_str_equal (options[option_index].name,
                                "physical-layout-mode"))
            {
              pending_layout_mode = CC_DISPLAY_LAYOUT_MODE_PHYSICAL;
              pending_layout_mode_set = TRUE;
            }
          else
            {
              g_assert_not_reached ();
            }

          break;
        case 'L':
          if (!handle_logical_monitor_arg (error))
            return FALSE;
          break;

        case 'P':
          method = CC_DISPLAY_METHOD_PERSISTENT;
          break;
        case 'V':
          method = CC_DISPLAY_METHOD_VERIFY;
          break;

        case 'x':
        case 'y':
        case 's':
        case 't':
          if (!handle_logical_monitor_property_arg (c, optarg, error))
            return FALSE;
          break;

        case 'p':
          if (!handle_logical_monitor_primary_arg (error))
            return FALSE;
          break;

        case 'M':
          if (!handle_monitor_arg (optarg, error))
            return FALSE;
          break;

	case 'm':
	  if (!handle_mode_arg (optarg, error))
            return FALSE;
          break;

        case 'h':
          print_help ();
          return TRUE;
        }
    }

  if (!finalize_pending_logical_monitor_config ())
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Configuration incomplete");
      return FALSE;
    }

  print_pending_configuration ();

  return cc_display_config_manager_apply (config_manager,
                                          current_state,
                                          pending_config,
                                          method,
                                          error);
}

static gboolean
show_monitor_labels (GError **error)
{
  GDBusProxy *proxy;
  GVariantBuilder builder;
  CcDisplayConfigManager *config_manager;
  GList *l;
  int number;

  proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                         G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES |
                                         G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS |
                                         G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START,
                                         NULL,
                                         "org.gnome.Shell",
                                         "/org/gnome/Shell",
                                         "org.gnome.Shell",
                                         NULL,
                                         error);
  if (!proxy)
    return FALSE;

  config_manager = cc_display_config_manager_new (error);
  if (!config_manager)
    return FALSE;

  current_state = cc_display_config_manager_new_current_state (config_manager,
                                                               error);
  if (!current_state)
    return FALSE;

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a{sv}"));

  number = 1;
  for (l = cc_display_state_get_monitors (current_state); l; l = l->next)
    {
      CcDisplayMonitor *monitor = l->data;

      g_variant_builder_add (&builder, "{sv}",
                             cc_display_monitor_get_connector (monitor),
                             g_variant_new_int32 (number));

      number++;
    }

  g_dbus_proxy_call (proxy,
                     "ShowMonitorLabels2",
                     g_variant_new ("(a{sv})", &builder),
                     G_DBUS_CALL_FLAGS_NONE,
                     -1, NULL, NULL, NULL);

  /*
   * The shell will hide the labels if the D-Bus client goes away, so linger a
   * bit before they go away.
   */
  sleep(20);

  return TRUE;
}

int
main (int argc,
      char *argv[])
{
  g_set_prgname (argv[0]);

  if (argc == 1)
    {
      print_usage (stderr);
      return EXIT_FAILURE;
    }

  if (g_str_equal (argv[1], "--help") || g_str_equal (argv[1], "-h"))
    {
      print_help ();
      return EXIT_SUCCESS;
    }

  if (g_str_equal (argv[1], "list") && argc == 2)
    {
      GError *error = NULL;

      if (!list_monitors (&error))
        {
          g_printerr ("Failed to list current configuration: %s\n",
                      error->message);
          g_error_free (error);
          return EXIT_FAILURE;
        }
      else
        {
          return EXIT_SUCCESS;
        }
    }
  else if (g_str_equal (argv[1], "set"))
    {
      GError *error = NULL;

      if (!set_monitors (argc - 1, argv + 1, &error))
        {
          g_printerr ("Failed to set configuration: %s\n",
                      error->message);
          g_error_free (error);
          return EXIT_FAILURE;
        }
      else
        {
          return EXIT_SUCCESS;
        }
    }
  else if (g_str_equal (argv[1], "show"))
    {
      GError *error = NULL;

      if (!show_monitor_labels (&error))
        {
          g_printerr ("Failed to show monitor labels: %s\n",
                      error->message);
          g_error_free (error);
          return EXIT_FAILURE;
        }
      else
        {
          return EXIT_SUCCESS;
        }
    }
  else
    {
      print_usage (stderr);
      return EXIT_FAILURE;
    }
}
