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
#include <glib/gi18n-lib.h>
#include <glib/gprintf.h>

#include <gio/gio.h>

#include <json-glib/json-glib.h>

#include <grilo.h>

#include "grl-weboob-shared.h"

#include "weboob.h"


gchar *
weboob_node_get_string (JsonNode *node, const gchar *pattern)
{
  gchar * id = NULL;
  JsonNode *matches;
  JsonNode *match;
  JsonArray *results;
  gint len;
  GError *error = NULL;
  
  matches = json_path_query (pattern, node, &error);

  /* FIXME checks on matches */
  results = json_node_get_array (matches);
  len = json_array_get_length (results); 
  
  if (len > 0) {
    match = json_array_get_element (results, 0);
    id = json_node_dup_string (match);
  }
  
  json_node_free (matches);
  
  return id;
}

void
weboob_read_async (GDataInputStream *dis,
                    int io_priority,
                    GCancellable *cancellable,
                    GAsyncReadyCallback callback,
                    gpointer user_data)
{
  g_data_input_stream_read_line_async (dis, G_PRIORITY_DEFAULT,
                                       cancellable,
                                       callback,
                                       user_data);
}


GDataInputStream *
weboob_run (const gchar *command,
            const gchar *backend,
            int count,
            const gchar **argv,
            GError **error)
{
  GSubprocess *process;
  const gchar *args[64];
  int i = 0;
  int j = 0;
  gchar scount[64];
  GInputStream *is;
  GDataInputStream *dis;

  GRL_DEBUG ("Running %s %s...", command, argv[0]);

  /* Consolidate arguments */
  args[i++] = command;

  /* Backend */
  if (NULL != backend) {
    args[i++] = "-b";
    args[i++] = backend;
  }
  
  /* Result count */
  if (-1 != count) {
    args[i++] = "-n";
    /* We use a raw char buffer in order to avoid allocating and
     * deallocating memory for this conversion.
     */
    g_snprintf (scount, 64-1, "%d", count);
    args[i++] = scount;
  }

  /* JSON format */
  args[i++] = "-f";
  args[i++] = "json";

  /* Run quiet */
  args[i++] = "-q";
  
  /* Copy remaining (and specific) args */
  for (j=0 ; NULL != argv && NULL != argv[j] ; j++) {
    args[i++] = argv[j];
  }
  
  /* End of args */
  args[i++] = NULL;

  gchar *arguments = g_strjoinv (" ", args);
  GRL_DEBUG ("Running %s...", arguments);
  g_free (arguments);

  /* Spawn command */
  process = g_subprocess_newv (args, G_SUBPROCESS_FLAGS_STDOUT_PIPE, error);

  if (!process)
  {
    g_error ("SPAWN FAILED");
    return NULL;
  }

  is = g_subprocess_get_stdout_pipe (process);
  dis = g_data_input_stream_new (is);

  g_object_unref (process);
  
  return dis;
}

static gchar **
build_module_from_node (JsonNode *node)
{
  gchar *name;
  gchar *description;

  name = weboob_node_get_string (node, "$.Name");
  description = weboob_node_get_string (node, "$.Description");

  GRL_DEBUG ("%s found %s: %s", __FUNCTION__, name, description);

  gchar **module = g_new (gchar*, 3);
  module[0] = name;
  module[1] = description;
  module[2] = NULL;

  // Tranfered
  //g_free (name);
  //g_free (description);
  
  return module;
}

static GList *
build_modules_from_json (const gchar *line, GError **error)
{
  GList *medias = NULL;
  JsonParser *parser = NULL;
  gboolean ret;
  int i = 0;

  parser = json_parser_new ();
  ret = json_parser_load_from_data (parser,
                              line, -1,
                              error);

  if (!ret) {
    /* Error occured and 'error' already set */
    return NULL;
  }

  JsonNode *root = json_parser_get_root (parser);
  JsonArray *array = json_node_get_array (root);
  guint len = json_array_get_length (array);
  GRL_DEBUG ("Length: %d", len);
  for (i=0 ; i < len ; i++) {
    JsonNode *node = json_array_get_element (array, i);
    gchar *media = build_module_from_node (node);
    medias = g_list_prepend (medias, media);
  }
  
  g_object_unref (parser);
  
  return medias;
}

GList *
weboob_modules (const gchar* cap,
                GError **error)
{
  GList *backends = NULL;
  GDataInputStream *dis;
  const gchar *args[64];
  int i = 0;
  gchar *line;
  
  GRL_DEBUG ("%s ", __FUNCTION__);
  
  args[i++] = "modules";
  
  if (NULL != cap)
    args[i++] = cap;

  /* End of args */
  args[i++] = NULL;

  dis = weboob_run ("weboob-config", NULL, -1, args, error);

  if (dis != NULL) {
    while ((line = g_data_input_stream_read_line (dis, NULL, NULL, error)) != NULL
           && *error == NULL) {
      if (line[0] == '[') {
        backends = build_modules_from_json (line, error);
      }
      g_free (line);
    }
  }
  
  return backends;

}
