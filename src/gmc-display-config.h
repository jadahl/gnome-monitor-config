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

#ifndef _CC_DISPLAY_CONFIG_H
#define _CC_DISPLAY_CONFIG_H

#include <cairo.h>
#include <glib.h>
#include <stdbool.h>

#include "gmc-dbus-display-config.h"

typedef enum _CcDisplayLayoutMode
{
  CC_DISPLAY_LAYOUT_MODE_LOGICAL = 1,
  CC_DISPLAY_LAYOUT_MODE_PHYSICAL = 2
} CcDisplayLayoutMode;

typedef enum _CcDisplayConfigMethod
{
  CC_DISPLAY_METHOD_VERIFY = 0,
  CC_DISPLAY_METHOD_TEMPORARY = 1,
  CC_DISPLAY_METHOD_PERSISTENT = 2
} CcDisplayConfigMethod;

typedef enum
{
  CC_DISPLAY_TRANSFORM_NORMAL,
  CC_DISPLAY_TRANSFORM_90,
  CC_DISPLAY_TRANSFORM_180,
  CC_DISPLAY_TRANSFORM_270,
  CC_DISPLAY_TRANSFORM_FLIPPED,
  CC_DISPLAY_TRANSFORM_FLIPPED_90,
  CC_DISPLAY_TRANSFORM_FLIPPED_180,
  CC_DISPLAY_TRANSFORM_FLIPPED_270,
} CcDisplayTransform;

typedef struct _CcDisplayState CcDisplayState;
typedef struct _CcDisplayMonitor CcDisplayMonitor;
typedef struct _CcDisplayLogicalMonitor CcDisplayLogicalMonitor;

typedef struct _CcDisplayMonitorConfig CcDisplayMonitorConfig;
typedef struct _CcDisplayLogicalMonitorConfig CcDisplayLogicalMonitorConfig;
typedef struct _CcDisplayConfig CcDisplayConfig;

typedef struct _CcDisplayMode CcDisplayMode;

CcDisplayState *cc_display_state_new_current (CcDbusDisplayConfig *proxy,
                                              GError **error);
void cc_display_state_free (CcDisplayState *state);

unsigned int cc_display_state_get_serial (CcDisplayState *state);
GList *cc_display_state_get_monitors (CcDisplayState *state);
GList *cc_display_state_get_logical_monitors (CcDisplayState *state);
gboolean cc_display_state_get_max_screen_size (CcDisplayState *state,
                                               int *max_width,
                                               int *max_height);

double *
cc_display_mode_get_supported_scales (CcDisplayMode *mode,
				      int *n_supported_scales);
const char *
cc_display_mode_get_id (CcDisplayMode *mode);

GList * cc_display_logical_monitor_get_monitors (CcDisplayLogicalMonitor *logical_monitor);
bool cc_display_logical_monitor_is_primary (CcDisplayLogicalMonitor *logical_monitor);
void cc_display_logical_monitor_calculate_layout (CcDisplayLogicalMonitor *logical_monitor,
                                                  cairo_rectangle_int_t *layout);
double cc_display_logical_monitor_get_scale (CcDisplayLogicalMonitor *logical_monitor);
CcDisplayTransform cc_display_logical_monitor_get_transform (CcDisplayLogicalMonitor *logical_monitor);

bool cc_display_monitor_is_active (CcDisplayMonitor *monitor);
const char * cc_display_monitor_get_connector (CcDisplayMonitor *monitor);
bool cc_display_monitor_is_builtin_display (CcDisplayMonitor *monitor);
const char * cc_display_monitor_get_display_name (CcDisplayMonitor *monitor);

bool cc_display_monitor_supports_underscanning (CcDisplayMonitor *monitor);
bool cc_display_monitor_is_underscanning (CcDisplayMonitor *monitor);

CcDisplayMode * cc_display_monitor_lookup_mode (CcDisplayMonitor *monitor,
						const char *mode_id);
GList * cc_display_monitor_get_modes (CcDisplayMonitor *monitor);
CcDisplayMode * cc_display_monitor_get_current_mode (CcDisplayMonitor *monitor);
CcDisplayMode * cc_display_monitor_get_preferred_mode (CcDisplayMonitor *monitor);

void cc_display_mode_get_resolution (CcDisplayMode *mode,
				     int *width,
				     int *height);
double cc_display_mode_get_refresh_rate (CcDisplayMode *mode);
double cc_display_mode_get_preferred_scale (CcDisplayMode *mode);

CcDisplayMonitor * cc_display_monitor_config_get_monitor (CcDisplayMonitorConfig *monitor_config);
CcDisplayMode * cc_display_monitor_config_get_mode (CcDisplayMonitorConfig *monitor_config);
void cc_display_monitor_config_set_mode (CcDisplayMonitorConfig *monitor_config,
                                         CcDisplayMode *mode);

CcDisplayLogicalMonitorConfig * cc_display_logical_monitor_config_new (void);

void cc_display_logical_monitor_config_set_position (CcDisplayLogicalMonitorConfig *logical_monitor_config,
                                                     int x,
                                                     int y);
void cc_display_logical_monitor_config_set_scale (CcDisplayLogicalMonitorConfig *logical_monitor_config,
                                                  double scale);

void cc_display_logical_monitor_config_set_transform (CcDisplayLogicalMonitorConfig *logical_monitor_config,
                                                      CcDisplayTransform             transform);

void cc_display_logical_monitor_config_set_is_primary (CcDisplayLogicalMonitorConfig *logical_monitor_config,
                                                       bool is_primary);

void cc_display_logical_monitor_config_add_monitor (CcDisplayLogicalMonitorConfig *logical_monitor_config,
                                                    CcDisplayMonitor *monitor,
                                                    CcDisplayMode *mode);

bool cc_display_logical_monitor_config_is_primary (CcDisplayLogicalMonitorConfig *logical_monitor_config);

double cc_display_logical_monitor_config_get_scale (CcDisplayLogicalMonitorConfig *logical_monitor_config);

CcDisplayTransform cc_display_logical_monitor_config_get_transform (CcDisplayLogicalMonitorConfig *logical_monitor_config);

void cc_display_logical_monitor_config_get_position (CcDisplayLogicalMonitorConfig *logical_monitor_config,
                                                     int *x,
                                                     int *y);

void cc_display_logical_monitor_config_calculate_layout (CcDisplayLogicalMonitorConfig *logical_monitor_config,
                                                         cairo_rectangle_int_t *layout);

GList * cc_display_logical_monitor_config_get_monitor_configs (CcDisplayLogicalMonitorConfig *logical_monitor_config);

CcDisplayConfig * cc_display_config_new (void);

void cc_display_config_set_layout_mode (CcDisplayConfig *config,
                                        CcDisplayLayoutMode layout_mode);
gboolean cc_display_config_get_layout_mode (CcDisplayConfig *config,
                                            CcDisplayLayoutMode *layout_mode);
void cc_display_config_add_logical_monitor (CcDisplayConfig *config,
                                            CcDisplayLogicalMonitorConfig *logical_monitor_config);
GList * cc_display_config_get_logical_logical_monitor_configs (CcDisplayConfig *config);

#endif /* _CC_DISPLAY_CONFIG_H */
