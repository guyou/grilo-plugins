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

#include "videoob.h"

#define VIDEOOB_COMMAND "videoob"

static gint
parse_duration (const gchar *sduration)
{
  guint hours, minutes, seconds;

  sscanf (sduration, "%u:%u:%u",
          &hours, &minutes, &seconds);

  return hours*60*60 + minutes*60 + seconds;
}

static GrlMedia *
build_media_box_from_entry (const char *line)
{
  GrlMedia *box;
  gchar *id = NULL;
  gchar *title = NULL;
  gchar *ptr = NULL;
  gint len = 0;
  
  box = grl_media_box_new ();
  
  /* parse line */
  id = strchr (line, '(');
  if (id) {
    /* Id */
    id = id + 1;
    ptr = strchr (id, ')');
    len = ptr - id;
    id = g_strndup (id, len);
    
    /* Title */
    title = g_strdup (ptr + 2);
  }

  grl_media_set_id (box, id);
  grl_media_set_title (box, title);

  g_free (id);
  g_free (title);
  
  return box;
}

/* Result example:
 * 
 * "id": "3qVJLOK_zao@youtube"
 * "title": "GNOME Shell 3.8 search redesign"
 * "url": null
 * "ext": null
 * "author": "Cosimo Cecchi"
 * "description": null
 * "date": null
 * "size": null
 * "rating": null
 * "rating_max": null
 * "nsfw": false
 * "thumbnail": {"id": "https://i.ytimg.com/vi/3qVJLOK_zao/0.jpg", "title": null, "url": "https://i.
ytimg.com/vi/3qVJLOK_zao/0.jpg", "ext": null, "author": null, "description": null, "date": null, "si
ze": null, "rating": null, "rating_max": null, "nsfw": false, "thumbnail": null, "data": null}
 * "data": null
 * "duration": "0:00:45"
 */

static gchar *
videoob_node_get_string (JsonNode *node, const gchar *pattern)
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
  
  match = json_array_get_element (results, 0);
  id = json_node_dup_string (match);
  
  json_node_free (matches);
  
  return id;
}

static GrlMedia *
build_media_from_node (GrlMedia *content, JsonNode *node)
{
  GrlMedia *media = NULL;
  gchar *id;
  gchar *title;
  gchar *url;
  gchar *author;
  gchar *desc;
  gchar *date;
  gchar *thumbnail;
  gchar *duration;

  if (content) {
    media = content;
  } else {
    media = grl_media_video_new ();
  }

  id = videoob_node_get_string (node, "$.id");
  title = videoob_node_get_string (node, "$.title");
  author = videoob_node_get_string (node, "$.author");
  url = videoob_node_get_string (node, "$.url");
  desc = videoob_node_get_string (node, "$.description");
  date = videoob_node_get_string (node, "$.date");
  thumbnail = videoob_node_get_string (node, "$.thumbnail.url");
  duration = videoob_node_get_string (node, "$.duration");

  grl_media_set_id (media, id);
  grl_media_set_title (media, title);
  if (url) {
    grl_media_set_url (media, url);
  }
  if (desc) {
    grl_media_set_description (media, desc);
  }
  if (author) {
    grl_media_set_author (media, author);
  }
  if (date) {
    GDateTime *date_time = grl_date_time_from_iso8601 (date);
    if (date_time) {
      grl_media_set_creation_date (media, date_time);
      g_date_time_unref (date_time);
    }
  }
  if (thumbnail) {
    grl_media_add_thumbnail (media, thumbnail);
  }
  if (duration) {
    grl_media_set_duration (media, parse_duration (duration));
  }

  g_free (id);
  g_free (title);
  g_free (author);
  g_free (url);
  g_free (desc);
  g_free (date);
  g_free (thumbnail);
  g_free (duration);
  
  return media;
}

static GList *
build_medias_from_json (const gchar *line, GError **error)
{
  GList *medias = NULL;
  JsonParser *parser = NULL;
  gboolean ret;
  int i = 0;

  parser = json_parser_new ();
  ret = json_parser_load_from_data (parser,
                              line, -1,
                              error);

  /* FIXME check ret */

  JsonNode *root = json_parser_get_root (parser);
  JsonArray *array = json_node_get_array (root);
  guint len = json_array_get_length (array);
  GRL_DEBUG ("Length: %d", len);
  for (i=0 ; i < len ; i++) {
    JsonNode *node = json_array_get_element (array, i);
    GrlMedia *media = build_media_from_node (NULL, node);
    medias = g_list_prepend (medias, media);
  }
  
  return medias;
}

static void
videoob_wait_cb (GObject      *source_object,
                 GAsyncResult *res,
                 gpointer      user_data)
{
  gboolean success = FALSE;

  success = g_subprocess_wait_finish (G_SUBPROCESS (source_object), res, NULL);

  if (success)
    g_debug ("Process videoob terminated with success!");
  else
    g_debug ("Process videoob terminated with error!");

}

static GList *
videoob_run (const gchar *backend,
             int count,
             const gchar **argv,
             GError **error)
{
  GList *medias = NULL;
  GList *news = NULL;
  GrlMedia *media;
  gchar *line = NULL;
  GSubprocess *process;
  const gchar *args[64];
  int i = 0;
  int j = 0;
  gchar scount[64];
  GInputStream *is;
  GDataInputStream *dis;

  GRL_DEBUG ("Running %s %s...", VIDEOOB_COMMAND, argv[0]);

  /* Consolidate arguments */
  args[i++] = VIDEOOB_COMMAND;

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

  /* Spawn command */
  process = g_subprocess_newv (args, G_SUBPROCESS_FLAGS_STDOUT_PIPE, error);

  if (!process)
  {
    g_error ("SPAWN FAILED");
    return NULL;
  }

  is = g_subprocess_get_stdout_pipe (process);

  dis = g_data_input_stream_new (is);
  line = g_data_input_stream_read_line (dis, NULL, NULL, error);
  while (NULL != line && NULL == *error) {
    if ('~' == line[0]) {
      media = build_media_box_from_entry (line);
      medias = g_list_prepend (medias, media);
    } else if ('[' == line[0]) {
      news = build_medias_from_json (line, error);
      medias = g_list_concat (medias, news);
      //g_free (news);
    }
    
    /* next */
    line = g_data_input_stream_read_line (dis, NULL, NULL, error);
  }

  g_object_unref (dis);
  // no need g_free (is);

  return medias;
}

GList *
videoob_ls (const gchar *backend,
            int count,
            const gchar *dir,
            GError **error)
{
  const gchar *args[64];
  int i = 0;
  
  args[i++] = "ls";
  
  if (NULL != dir)
    args[i++] = dir;

  /* End of args */
  args[i++] = NULL;

  return videoob_run (backend, count, args, error);
}

GList *
videoob_search (const gchar *backend,
                int count,
                const gchar *pattern,
                GError **error)
{
  const gchar *args[64];
  int i = 0;
  
  args[i++] = "search";
  args[i++] = pattern;

  /* End of args */
  args[i++] = NULL;

  return videoob_run (backend, count, args, error);
}

GrlMedia *
videoob_info (const gchar *backend,
              const gchar *uri,
              GError **error)
{
  const gchar *args[64];
  GList *medias;
  int i = 0;
  
  args[i++] = "info";
  
  args[i++] = uri;

  /* End of args */
  args[i++] = NULL;

  medias = videoob_run (backend, 1, args, error);
  
  if (NULL != medias && g_list_length (medias) > 0) {
    return g_list_nth_data (medias, 0);
  } else {
    return NULL;
  }
}
