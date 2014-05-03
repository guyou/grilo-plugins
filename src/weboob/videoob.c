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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>
#include <glib/gprintf.h>

#include <json-glib/json-glib.h>

#include "videoob.h"

#define VIDEOOB_COMMAND "videoob"

static void
videoob_run (gchar **argv,
             GError **error)
{
  gchar *output;
  gboolean ret;
  JsonParser *parser;
  gint exit_status;

  ret = g_spawn_sync (NULL, argv, NULL,
                      G_SPAWN_SEARCH_PATH,
                      NULL, NULL,
                      &output, NULL,
                      &exit_status,
                      error);

  g_debug ("Exit status: %d", exit_status);
  if( ! ret )
  {
    g_error( "SPAWN FAILED" );
    return;
  }

  parser = json_parser_new ();
  ret = json_parser_load_from_data (parser,
                              output, -1,
                              error);

  JsonNode *root = json_parser_get_root (parser);
  JsonArray *array = json_node_get_array (root);
  guint len = json_array_get_length (array);
  g_debug ("Length: %d", len);
}

void
videoob_ls (gchar *backend,
            gchar *dir,
            GError **error)
{
  gchar *args[64];
  int i = 0;

  args[i++] = VIDEOOB_COMMAND;

  /* Backend */
  if (NULL != backend) {
    args[i++] = "-b";
    args[i++] = backend;
  }
  
  /* JSON format */
  args[i++] = "-f";
  args[i++] = "json";
  
  args[i++] = "ls";
  args[i++] = dir;

  videoob_run (args, error);
}
