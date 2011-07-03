/*
 * Copyright (C) 2010-2011 Igalia S.L.
 *
 * Contact: Guillaume Emont <gemont@igalia.com>
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

#include <string.h>
#include <stdlib.h>

#include <grilo.h>
#include <gio/gio.h>

#include "grl-local-metadata.h"

#define GRL_LOG_DOMAIN_DEFAULT local_metadata_log_domain
GRL_LOG_DOMAIN_STATIC(local_metadata_log_domain);

#define PLUGIN_ID   LOCALMETADATA_PLUGIN_ID

#define SOURCE_ID   "grl-local-metadata"
#define SOURCE_NAME "Local Metadata Provider"
#define SOURCE_DESC "A source providing locally available metadata"

/**/

#define TV_REGEX                                \
  "(?<showname>.*)\\."                          \
  "(?<season>(?:\\d{1,2})|(?:[sS]\\K\\d{1,2}))" \
  "(?<episode>(?:\\d{2})|(?:[eE]\\K\\d{1,2}))"  \
  "\\.?(?<name>.*)?"
#define MOVIE_REGEX                             \
  "(?<name>.*)"                                 \
  "\\.?[\\(\\[](?<year>[12][90]\\d{2})[\\)\\]]"

/**/

#define GRL_LOCAL_METADATA_SOURCE_GET_PRIVATE(object)           \
  (G_TYPE_INSTANCE_GET_PRIVATE((object),                        \
                               GRL_LOCAL_METADATA_SOURCE_TYPE,	\
                               GrlLocalMetadataSourcePriv))

enum {
  PROP_0,
  PROP_GUESS_VIDEO,
};

struct _GrlLocalMetadataSourcePriv {
  gboolean guess_video;
};

/**/

typedef enum {
  FLAG_VIDEO_TITLE    = 0x1,
  FLAG_VIDEO_SHOWNAME = 0x2,
  FLAG_VIDEO_DATE     = 0x4,
  FLAG_VIDEO_SEASON   = 0x8,
  FLAG_VIDEO_EPISODE  = 0x10,
  FLAG_THUMBNAIL      = 0x20,
} resolution_flags_t;

const gchar *video_blacklisted_prefix[] = {
  "tpz-", NULL
};

const char *video_blacklisted_words[] = {
  "720p", "1080p",
  "ws", "WS", "proper", "PROPER",
  "repack", "real.repack",
  "hdtv", "HDTV", "pdtv", "PDTV", "notv", "NOTV",
  "dsr", "DSR", "DVDRip", "divx", "DIVX", "xvid", "Xvid",
  NULL
};

/**/

static void grl_local_metadata_source_set_property (GObject      *object,
                                                    guint         propid,
                                                    const GValue *value,
                                                    GParamSpec   *pspec);

static GrlLocalMetadataSource *grl_local_metadata_source_new (gboolean guess_video);

static void grl_local_metadata_source_resolve (GrlMetadataSource *source,
                                              GrlMetadataSourceResolveSpec *rs);

static const GList *grl_local_metadata_source_supported_keys (GrlMetadataSource *source);

static gboolean grl_local_metadata_source_may_resolve (GrlMetadataSource *source,
                                                       GrlMedia *media,
                                                       GrlKeyID key_id,
                                                       GList **missing_keys);

static void grl_local_metadata_source_cancel (GrlMetadataSource *source,
                                              guint operation_id);

gboolean grl_local_metadata_source_plugin_init (GrlPluginRegistry *registry,
                                               const GrlPluginInfo *plugin,
                                               GList *configs);

/**/

/* =================== GrlLocalMetadata Plugin  =============== */

gboolean
grl_local_metadata_source_plugin_init (GrlPluginRegistry *registry,
                                      const GrlPluginInfo *plugin,
                                      GList *configs)
{
  guint config_count;
  gboolean guess_video = TRUE;
  GrlConfig *config;

  GRL_LOG_DOMAIN_INIT (local_metadata_log_domain, "local-metadata");

  GRL_DEBUG ("grl_local_metadata_source_plugin_init");

  if (!configs) {
    GRL_INFO ("\tConfiguration not provided! Using default configuration.");
  } else {
    config_count = g_list_length (configs);
    if (config_count > 1) {
      GRL_INFO ("\tProvided %i configs, but will only use one", config_count);
    }

    config = GRL_CONFIG (configs->data);

    guess_video = grl_config_get_boolean (config, "guess-video");
  }

  GrlLocalMetadataSource *source = grl_local_metadata_source_new (guess_video);
  grl_plugin_registry_register_source (registry,
                                       plugin,
                                       GRL_MEDIA_PLUGIN (source),
                                       NULL);
  return TRUE;
}

GRL_PLUGIN_REGISTER (grl_local_metadata_source_plugin_init,
                     NULL,
                     PLUGIN_ID);

/* ================== GrlLocalMetadata GObject ================ */

static GrlLocalMetadataSource *
grl_local_metadata_source_new (gboolean guess_video)
{
  GRL_DEBUG ("grl_local_metadata_source_new");
  return g_object_new (GRL_LOCAL_METADATA_SOURCE_TYPE,
		       "source-id", SOURCE_ID,
		       "source-name", SOURCE_NAME,
		       "source-desc", SOURCE_DESC,
                       "guess-video", guess_video,
		       NULL);
}

static void
grl_local_metadata_source_class_init (GrlLocalMetadataSourceClass * klass)
{
  GObjectClass           *g_class        = G_OBJECT_CLASS (klass);
  GrlMetadataSourceClass *metadata_class = GRL_METADATA_SOURCE_CLASS (klass);

  metadata_class->supported_keys = grl_local_metadata_source_supported_keys;
  metadata_class->may_resolve = grl_local_metadata_source_may_resolve;
  metadata_class->resolve = grl_local_metadata_source_resolve;
  metadata_class->cancel = grl_local_metadata_source_cancel;

  g_class->set_property = grl_local_metadata_source_set_property;

  g_object_class_install_property (g_class,
                                   PROP_GUESS_VIDEO,
                                   g_param_spec_boolean ("guess-video",
                                                         "Guess video",
                                                         "Guess video metadata "
                                                         "from filename",
                                                         TRUE,
                                                         G_PARAM_WRITABLE |
                                                         G_PARAM_CONSTRUCT_ONLY));

  g_type_class_add_private (klass, sizeof (GrlLocalMetadataSourcePriv));
}

static void
grl_local_metadata_source_init (GrlLocalMetadataSource *source)
{
}

G_DEFINE_TYPE (GrlLocalMetadataSource,
               grl_local_metadata_source,
               GRL_TYPE_METADATA_SOURCE);

static void
grl_local_metadata_source_set_property (GObject      *object,
                                        guint         propid,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  GrlLocalMetadataSourcePriv *priv =
    GRL_LOCAL_METADATA_SOURCE_GET_PRIVATE (object);

  switch (propid) {
  case PROP_GUESS_VIDEO:
    priv->guess_video = g_value_get_boolean (value);
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, propid, pspec);
  }
}

/* ======================= Utilities ==================== */

static gchar *
video_sanitise_string (const gchar *str)
{
  int    i;
  gchar *line;

  line = (gchar *) str;
  for (i = 0; video_blacklisted_prefix[i]; i++) {
    if (g_str_has_prefix (str, video_blacklisted_prefix[i])) {
      int len = strlen (video_blacklisted_prefix[i]);

      line = (gchar *) str + len;
    }
  }

  for (i = 0; video_blacklisted_words[i]; i++) {
    gchar *end;

    end = strstr (line, video_blacklisted_words[i]);
    if (end) {
      return g_strndup (line, end - line);
    }
  }

  return g_strdup (line);
}

/* tidies strings before we run them through the regexes */
static gchar *
video_uri_to_metadata (const gchar *uri)
{
  gchar *ext, *basename, *name, *whitelisted;

  basename = g_path_get_basename (uri);
  ext = strrchr (basename, '.');
  if (ext) {
    name = g_strndup (basename, ext - basename);
    g_free (basename);
  } else {
    name = basename;
  }

  /* Replace _ <space> with . */
  g_strdelimit (name, "_ ", '.');
  whitelisted = video_sanitise_string (name);
  g_free (name);

  return whitelisted;
}

static void
video_guess_values_from_uri (const gchar *uri,
                             gchar      **title,
                             gchar      **showname,
                             GDateTime  **date,
                             gint        *season,
                             gint        *episode)
{
  gchar      *metadata;
  GRegex     *regex;
  GMatchInfo *info;

  metadata = video_uri_to_metadata (uri);

  regex = g_regex_new (MOVIE_REGEX, 0, 0, NULL);
  g_regex_match (regex, metadata, 0, &info);

  if (g_match_info_matches (info)) {
    if (title) {
      *title = g_match_info_fetch_named (info, "name");
      /* Replace "." with <space> */
      g_strdelimit (*title, ".", ' ');
    }

    if (date) {
      gchar *year = g_match_info_fetch_named (info, "year");

      *date = g_date_time_new_utc (atoi (year), 1, 1, 0, 0, 0.0);
      g_free (year);
    }

    if (showname) {
      *showname = NULL;
    }

    if (season) {
      *season = 0;
    }

    if (episode) {
      *episode = 0;
    }

    g_regex_unref (regex);
    g_match_info_free (info);
    g_free (metadata);

    return;
  }

  g_regex_unref (regex);
  g_match_info_free (info);

  regex = g_regex_new (TV_REGEX, 0, 0, NULL);
  g_regex_match (regex, metadata, 0, &info);

  if (g_match_info_matches (info)) {
    if (title) {
      *title = g_match_info_fetch_named (info, "name");
      g_strdelimit (*title, ".", ' ');
    }

    if (showname) {
      *showname = g_match_info_fetch_named (info, "showname");
      g_strdelimit (*showname, ".", ' ');
    }

    if (season) {
      gchar *s = g_match_info_fetch_named (info, "season");
      if (s) {
        if (*s == 's' || *s == 'S') {
          *season = atoi (s + 1);
        } else {
          *season = atoi (s);
        }
      } else {
        *season = 0;
      }

      g_free (s);
    }

    if (episode) {
      gchar *e = g_match_info_fetch_named (info, "episode");
      if (e) {
        if (*e == 'e' || *e == 'E') {
          *episode = atoi (e + 1);
        } else {
          *episode = atoi (e);
        }
      } else {
        *episode = 0;
      }

      g_free (e);
    }

    if (date) {
      *date = NULL;
    }

    g_regex_unref (regex);
    g_match_info_free (info);
    g_free (metadata);

    return;
  }

  g_regex_unref (regex);
  g_match_info_free (info);

  /* The filename doesn't look like a movie or a TV show, just use the
     filename without extension as the title */
  if (title) {
    *title = g_strdelimit (metadata, ".", ' ');
  }

  if (showname) {
    *showname = NULL;
  }

  if (date) {
    *date = NULL;
  }

  if (season) {
    *season = 0;
  }

  if (episode) {
    *episode = 0;
  }
}

static void
got_file_info (GFile *file, GAsyncResult *result,
               GrlMetadataSourceResolveSpec *rs)
{
  GCancellable *cancellable;
  GFileInfo *info;
  GError *error = NULL;
  const gchar *thumbnail_path;

  GRL_DEBUG ("got_file_info");

  /* Free stored operation data */
  cancellable = grl_operation_get_data (rs->resolve_id);

  if (cancellable) {
    g_object_unref (cancellable);
  }

  info = g_file_query_info_finish (file, result, &error);
  if (error)
    goto error;

  thumbnail_path =
      g_file_info_get_attribute_byte_string (info, G_FILE_ATTRIBUTE_THUMBNAIL_PATH);


  if (thumbnail_path) {
    gchar *thumbnail_uri = g_filename_to_uri (thumbnail_path, NULL, &error);
    if (error)
      goto error;

    GRL_INFO ("Got thumbnail %s for media: %s", thumbnail_uri,
              grl_media_get_url (rs->media));
    grl_media_set_thumbnail (rs->media, thumbnail_uri);
    g_free (thumbnail_uri);

    rs->callback (rs->source, rs->resolve_id, rs->media, rs->user_data, NULL);
  } else {
    GRL_INFO ("Could not find thumbnail for media: %s",
              grl_media_get_url (rs->media));
    rs->callback (rs->source, rs->resolve_id, rs->media, rs->user_data, NULL);
  }

  goto exit;

error:
    {
      GError *new_error = g_error_new (GRL_CORE_ERROR, GRL_CORE_ERROR_RESOLVE_FAILED,
                                       "Got error: %s", error->message);
      rs->callback (rs->source, rs->resolve_id, rs->media, rs->user_data, new_error);

      g_error_free (error);
      g_error_free (new_error);
    }

exit:
  if (info)
    g_object_unref (info);
}

static void
resolve_video (GrlMetadataSource *source,
               GrlMetadataSourceResolveSpec *rs,
               resolution_flags_t flags)
{
  gchar *title, *showname;
  GDateTime *date;
  gint season, episode;
  GrlData *data = GRL_DATA (rs->media);
  resolution_flags_t miss_flags = 0, fill_flags;

  GRL_DEBUG ("%s",__FUNCTION__);

  if (!(flags & (FLAG_VIDEO_TITLE |
                 FLAG_VIDEO_SHOWNAME |
                 FLAG_VIDEO_DATE |
                 FLAG_VIDEO_SEASON |
                 FLAG_VIDEO_EPISODE)))
    return;

  miss_flags |= grl_data_has_key (data, GRL_METADATA_KEY_TITLE) ?
    0 : FLAG_VIDEO_TITLE;
  miss_flags |= grl_data_has_key (data, GRL_METADATA_KEY_SHOW) ?
    0 : FLAG_VIDEO_SHOWNAME;
  miss_flags |= grl_data_has_key (data, GRL_METADATA_KEY_DATE) ?
    0 : FLAG_VIDEO_DATE;
  miss_flags |= grl_data_has_key (data, GRL_METADATA_KEY_SEASON) ?
    0 : FLAG_VIDEO_SEASON;
  miss_flags |= grl_data_has_key (data, GRL_METADATA_KEY_EPISODE) ?
    0 : FLAG_VIDEO_EPISODE;

  fill_flags = flags & miss_flags;

  if (!fill_flags)
    return;

  video_guess_values_from_uri (grl_media_get_url (rs->media),
                               &title, &showname, &date,
                               &season, &episode);

  GRL_DEBUG ("\tfound title=%s/showname=%s/year=%i/season=%i/episode=%i",
             title, showname,
             date != NULL ? g_date_time_get_year (date) : 0,
             season, episode);

  /* As this is just a guess, don't erase already provided values. */
  if (title) {
    if (fill_flags & FLAG_VIDEO_TITLE) {
      grl_data_set_string (data, GRL_METADATA_KEY_TITLE, title);
    }
    g_free (title);
  }

  if (showname) {
    if (fill_flags & FLAG_VIDEO_SHOWNAME) {
      grl_data_set_string (data, GRL_METADATA_KEY_SHOW, showname);
    }
    g_free (showname);
  }

  if (date) {
    if (fill_flags & FLAG_VIDEO_DATE) {
      gchar *str_date = g_date_time_format (date, "%F");
      grl_data_set_string (data, GRL_METADATA_KEY_DATE, str_date);
      g_free (str_date);
    }
    g_date_time_unref (date);
  }

  if (season && (fill_flags & FLAG_VIDEO_SEASON)) {
    grl_data_set_int (data, GRL_METADATA_KEY_SEASON, season);
  }

  if (episode && (fill_flags & FLAG_VIDEO_EPISODE)) {
    grl_data_set_int (data, GRL_METADATA_KEY_EPISODE, episode);
  }
}

static void
resolve_image (GrlMetadataSource *source,
               GrlMetadataSourceResolveSpec *rs,
               resolution_flags_t flags)
{
  GFile *file;
  GCancellable *cancellable;

  GRL_DEBUG ("resolve_image");

  if (flags & FLAG_THUMBNAIL) {
    file = g_file_new_for_uri (grl_media_get_url (rs->media));

    cancellable = g_cancellable_new ();
    grl_operation_set_data (rs->resolve_id, cancellable);
    g_file_query_info_async (file, G_FILE_ATTRIBUTE_THUMBNAIL_PATH,
                             G_FILE_QUERY_INFO_NONE, G_PRIORITY_DEFAULT, cancellable,
                             (GAsyncReadyCallback)got_file_info, rs);
    g_object_unref (file);
  }
}

static void
resolve_album_art (GrlMetadataSource *source,
                   GrlMetadataSourceResolveSpec *rs,
                   resolution_flags_t flags)
{
  /* FIXME: implement this, according to
   * http://live.gnome.org/MediaArtStorageSpec
   *
   * When this is implemented, _may_resolve() should be modified to accept
   * GrlMediaAudio.
   */
  GError *error;
  error = g_error_new (GRL_CORE_ERROR, GRL_CORE_ERROR_RESOLVE_FAILED,
    "Thumbnail resolution for GrlMediaAudio not implemented in local-metadata");
  rs->callback (rs->source, rs->resolve_id, rs->media, rs->user_data, error);
  g_error_free (error);
}

static gboolean
has_compatible_media_url (GrlMedia *media)
{
  gboolean ret = FALSE;
  const gchar *url;
  gchar *scheme;

  url = grl_media_get_url (media);
  scheme = g_uri_parse_scheme (url);

  ret = 0 == g_strcmp0 (scheme, "file");

  if (scheme)
    g_free (scheme);

  return ret;
}

static resolution_flags_t
get_resolution_flags (GList *keys)
{
  GList *iter = keys;
  resolution_flags_t flags = 0;

  while (iter != NULL) {
    GrlKeyID key = GRLPOINTER_TO_KEYID (iter->data);
    if (key == GRL_METADATA_KEY_TITLE)
      flags |= FLAG_VIDEO_TITLE;
    else if (key == GRL_METADATA_KEY_SHOW)
      flags |= FLAG_VIDEO_SHOWNAME;
    else if (key == GRL_METADATA_KEY_DATE)
      flags |= FLAG_VIDEO_DATE;
    else if (key == GRL_METADATA_KEY_SEASON)
      flags |= FLAG_VIDEO_SEASON;
    else if (key == GRL_METADATA_KEY_EPISODE)
      flags |= FLAG_VIDEO_EPISODE;
    else if (key == GRL_METADATA_KEY_THUMBNAIL)
      flags |= FLAG_THUMBNAIL;

    iter = iter->next;
  }

  return flags;
}

/* ================== API Implementation ================ */

static const GList *
grl_local_metadata_source_supported_keys (GrlMetadataSource *source)
{
  static GList *keys = NULL;
  if (!keys) {
    keys = grl_metadata_key_list_new (GRL_METADATA_KEY_THUMBNAIL,
                                      GRL_METADATA_KEY_TITLE,
                                      GRL_METADATA_KEY_SHOW,
                                      GRL_METADATA_KEY_DATE,
                                      GRL_METADATA_KEY_SEASON,
                                      GRL_METADATA_KEY_EPISODE,
                                      NULL);
  }
  return keys;
}

static gboolean
grl_local_metadata_source_may_resolve (GrlMetadataSource *source,
                                       GrlMedia *media,
                                       GrlKeyID key_id,
                                       GList **missing_keys)
{
  GrlLocalMetadataSourcePriv *priv =
    GRL_LOCAL_METADATA_SOURCE_GET_PRIVATE (source);

  if (media && grl_data_has_key (GRL_DATA (media),
                                 GRL_METADATA_KEY_URL)) {
    if (GRL_IS_MEDIA_IMAGE (media)) {
      if (has_compatible_media_url (media) &&
          (key_id == GRL_METADATA_KEY_THUMBNAIL))
        return TRUE;
    }

    if (GRL_IS_MEDIA_VIDEO (media)) {
      if (priv->guess_video &&
          (key_id == GRL_METADATA_KEY_TITLE ||
           key_id == GRL_METADATA_KEY_SHOW ||
           key_id == GRL_METADATA_KEY_DATE ||
           key_id == GRL_METADATA_KEY_SEASON ||
           key_id == GRL_METADATA_KEY_EPISODE))
        return TRUE;
      if (has_compatible_media_url (media)) {
        if (key_id == GRL_METADATA_KEY_THUMBNAIL)
          return TRUE;
      }
    }
  }

  if (missing_keys)
    *missing_keys = grl_metadata_key_list_new (GRL_METADATA_KEY_URL, NULL);

  return FALSE;
}

static void
grl_local_metadata_source_resolve (GrlMetadataSource *source,
                                  GrlMetadataSourceResolveSpec *rs)
{
  GError *error = NULL;
  resolution_flags_t flags;
  GrlLocalMetadataSourcePriv *priv =
    GRL_LOCAL_METADATA_SOURCE_GET_PRIVATE (source);

  GRL_DEBUG ("grl_local_metadata_source_resolve");

  flags = get_resolution_flags (rs->keys);

   if (!flags)
     error = g_error_new (GRL_CORE_ERROR, GRL_CORE_ERROR_RESOLVE_FAILED,
                          "local-metadata cannot resolve any of the given keys");
   else if (!has_compatible_media_url (rs->media))
     error = g_error_new (GRL_CORE_ERROR, GRL_CORE_ERROR_RESOLVE_FAILED,
                          "local-metadata needs a url in the file:// scheme");

  if (error) {
    /* No can do! */
    rs->callback (source, rs->resolve_id, rs->media, rs->user_data, error);
    g_error_free (error);
    return;
  }

  GRL_DEBUG ("\ttrying to resolve for: %s", grl_media_get_url (rs->media));

  if (GRL_IS_MEDIA_VIDEO (rs->media)) {
    if (priv->guess_video)
      resolve_video (source, rs, flags);
    resolve_image (source, rs, flags);
  } else if (GRL_IS_MEDIA_IMAGE (rs->media)) {
    resolve_image (source, rs, flags);
  } else if (GRL_IS_MEDIA_AUDIO (rs->media)) {
    resolve_album_art (source, rs, flags);
  } else {
    /* What's that media type? */
    rs->callback (source, rs->resolve_id, rs->media, rs->user_data, NULL);
  }
}

static void
grl_local_metadata_source_cancel (GrlMetadataSource *source,
                                  guint operation_id)
{
  GCancellable *cancellable =
          (GCancellable *) grl_operation_get_data (operation_id);

  if (cancellable) {
    g_cancellable_cancel (cancellable);
  }
}
