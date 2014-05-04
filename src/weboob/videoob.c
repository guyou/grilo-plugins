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

#include <json-glib/json-glib.h>

#include <grilo.h>

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
  GRL_DEBUG ("Found %d result", len);
  
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
  gchar *desc;
  gchar *date;
  gchar *thumbnail;
  gchar *duration;

  GRL_DEBUG ("Parsing node %s", json_node_type_name (node));

  if (content) {
    media = content;
  } else {
    media = grl_media_video_new ();
  }

  id = videoob_node_get_string (node, "$.id");
  GRL_DEBUG ("Id: %s", id);
  title = videoob_node_get_string (node, "$.title");
  GRL_DEBUG ("Title: %s", title);
  url = videoob_node_get_string (node, "$.url");
  GRL_DEBUG ("Url: %s", url);
  desc = videoob_node_get_string (node, "$.description");
  GRL_DEBUG ("Desc: %s", desc);
  date = videoob_node_get_string (node, "$.date");
  GRL_DEBUG ("Date: %s", date);
  thumbnail = videoob_node_get_string (node, "$.thumbnail.url");
  GRL_DEBUG ("Thumbnail URL: %s", thumbnail);
  duration = videoob_node_get_string (node, "$.duration");
  GRL_DEBUG ("Duration: %s", duration);

  grl_media_set_id (media, id);
  grl_media_set_title (media, title);
  if (url) {
    grl_media_set_url (media, url);
  }
  if (desc) {
    grl_media_set_description (media, desc);
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
  g_free (url);
  g_free (desc);
  g_free (date);
  g_free (thumbnail);
  g_free (duration);
  
  return media;
}

static GList *
videoob_run (gchar *backend,
             int count,
             gchar **argv,
             GError **error)
{
  GList *medias = NULL;
  gchar *output = NULL;
  gboolean ret;
  JsonParser *parser = NULL;
  gint exit_status;
  gchar *args[64];
  int i = 0;
  int j = 0;
  gchar scount[64];

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
  ret = g_spawn_sync (NULL, args, NULL,
                      G_SPAWN_SEARCH_PATH,
                      NULL, NULL,
                      &output, NULL,
                      &exit_status,
                      error);

  GRL_DEBUG ("Exit status: %d", exit_status);
  if (!ret)
  {
    g_error ("SPAWN FAILED");
    return NULL;
  }

  if (0 != exit_status) {
    GRL_DEBUG ("Subprocess failure: %d", exit_status);
    /* FIXME error code */
    g_set_error (error,
                 GRL_CORE_ERROR,
                 GRL_CORE_ERROR_BROWSE_FAILED,
                 _("Subprocess failure"));
    return NULL;
  }

  parser = json_parser_new ();
  ret = json_parser_load_from_data (parser,
                              output, -1,
                              error);

  JsonNode *root = json_parser_get_root (parser);
  JsonArray *array = json_node_get_array (root);
  guint len = json_array_get_length (array);
  g_debug ("Length: %d", len);
  for (i=0 ; i < len ; i++) {
    JsonNode *node = json_array_get_element (array, i);
    GrlMedia *media = build_media_from_node (NULL, node);
    medias = g_list_prepend (medias, media);
  }
  
  return medias;
}

GList *
videoob_ls (const gchar *backend,
            int count,
            const gchar *dir,
            GError **error)
{
  gchar *args[64];
  int i = 0;
  
  args[i++] = "ls";
  args[i++] = dir;

  /* End of args */
  args[i++] = NULL;

  return videoob_run (backend, count, args, error);
}

static GrlMediaBox *
build_media_box_from_entry (const gchar *line)
{
  GrlMediaBox *box;
  
  box = grl_media_box_new ();
  
  /* TODO parse line */
  
  return box;
}

GList *
videoob_search (const gchar *backend,
                int count,
                const gchar *pattern,
                GError **error)
{
  gchar *args[64];
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
  /* TODO */
  return NULL;
}
