/*
 * Copyright (C) 2014 Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#ifndef _WEBOOB_SOURCE_H_
#define _WEBOOB_SOURCE_H_

#include <gio/gio.h>
#include <json-glib/json-glib.h>

gchar *weboob_node_get_string (JsonNode *node, const gchar *pattern);

void weboob_read_async (GDataInputStream *dis,
                        int io_priority,
                        GCancellable *cancellable,
                        GAsyncReadyCallback callback,
                        gpointer user_data);

GDataInputStream *weboob_run (const gchar *command,
                              const gchar *backend,
                              int count,
                              const gchar *select,
                              const gchar **argv,
                              GError **error);

GList *weboob_modules (const gchar *cap,
                       GError **error);

GIcon *weboob_module_get_icon (const gchar *backend);

#endif /* _GRL_VIDEOOB_SOURCE_H_ */
