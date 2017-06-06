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

#include <glib-object.h>

#include "gmc-display-config.h"

#define CC_TYPE_DISPLAY_CONFIG_MANAGER (cc_display_config_manager_get_type ())
G_DECLARE_FINAL_TYPE (CcDisplayConfigManager, cc_display_config_manager,
                      CC, DISPLAY_CONFIG_MANAGER, GObject)

CcDisplayState * cc_display_config_manager_new_current_state (CcDisplayConfigManager *manager,
							      GError **error);

CcDisplayConfigManager * cc_display_config_manager_new (GError **error);

gboolean cc_display_config_manager_apply (CcDisplayConfigManager *manager,
					  CcDisplayState *state,
					  CcDisplayConfig *config,
					  CcDisplayConfigMethod method,
					  GError **error);
