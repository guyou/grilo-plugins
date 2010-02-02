/*
 * Copyright (C) 2010 Igalia S.L.
 *
 * Contact: Iago Toral Quiroga <itoral@igalia.com>
 *
 * Authors: Juan A. Suarez Romero <jasuarez@igalia.com>
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

#include <media-store.h>
#include <flickcurl.h>
#include <string.h>

#include "ms-flickr.h"

#define MS_FLICKR_SOURCE_GET_PRIVATE(object)    \
  (G_TYPE_INSTANCE_GET_PRIVATE((object), MS_FLICKR_SOURCE_TYPE, MsFlickrSourcePrivate))

typedef struct {
  MsMediaSourceSearchSpec *ss;
  MsContentMedia *media;
  gint remaining;
} SearchData;

/* --------- Logging  -------- */

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "ms-flickr"

/* ----- Security tokens ---- */

#define FLICKR_KEY    "fa037bee8120a921b34f8209d715a2fa"
#define FLICKR_SECRET "9f6523b9c52e3317"
#define FLICKR_FROB   "416-357-743"
#define FLICKR_TOKEN  "72157623286932154-c90318d470e96a29"

/* --- Plugin information --- */

#define PLUGIN_ID   "ms-flickr"
#define PLUGIN_NAME "Flickr"
#define PLUGIN_DESC "A plugin for browsing and searching Flickr photos"

#define SOURCE_ID   "ms-flickr"
#define SOURCE_NAME "Flickr"
#define SOURCE_DESC "A source for browsing and searching Flickr photos"

#define AUTHOR      "Igalia S.L."
#define LICENSE     "LGPL"
#define SITE        "http://www.igalia.com"

struct _MsFlickrSourcePrivate {
  flickcurl *fc;
};

static MsFlickrSource *ms_flickr_source_new (void);

gboolean ms_flickr_plugin_init (MsPluginRegistry *registry,
				 const MsPluginInfo *plugin);

static const GList *ms_flickr_source_supported_keys (MsMetadataSource *source);

static void ms_flickr_source_metadata (MsMediaSource *source,
                                       MsMediaSourceMetadataSpec *ss);

static void ms_flickr_source_search (MsMediaSource *source,
                                     MsMediaSourceSearchSpec *ss);

/* =================== Flickr Plugin  =============== */

gboolean
ms_flickr_plugin_init (MsPluginRegistry *registry, const MsPluginInfo *plugin)
{
  g_debug ("flickr_plugin_init\n");

  MsFlickrSource *source = ms_flickr_source_new ();
  if (source) {
    ms_plugin_registry_register_source (registry, plugin, MS_MEDIA_PLUGIN (source));
    return TRUE;
  } else {
    return FALSE;
  }
}

MS_PLUGIN_REGISTER (ms_flickr_plugin_init,
                    NULL,
                    PLUGIN_ID,
                    PLUGIN_NAME,
                    PLUGIN_DESC,
                    PACKAGE_VERSION,
                    AUTHOR,
                    LICENSE,
                    SITE);

/* ================== Flickr GObject ================ */

static MsFlickrSource *
ms_flickr_source_new (void)
{
  MsFlickrSource *fs;
  static flickcurl *fc = NULL;

  g_debug ("ms_flickr_source_new");

  if (!fc) {
    if (flickcurl_init ()) {
      g_warning ("Unable to initialize Flickcurl");
      return NULL;
    }

    fc = flickcurl_new ();
    if (!fc) {
      g_warning ("Unable to get a Flickcurl session");
      return NULL;
    }

    flickcurl_set_api_key (fc, FLICKR_KEY);
    flickcurl_set_auth_token (fc, FLICKR_TOKEN);
    flickcurl_set_shared_secret (fc, FLICKR_SECRET);
  }

  fs = g_object_new (MS_FLICKR_SOURCE_TYPE,
                     "source-id", SOURCE_ID,
                     "source-name", SOURCE_NAME,
                     "source-desc", SOURCE_DESC,
                     NULL);
  fs->priv->fc = fc;

  return fs;
}

static void
ms_flickr_source_class_init (MsFlickrSourceClass * klass)
{
  MsMediaSourceClass *source_class = MS_MEDIA_SOURCE_CLASS (klass);
  MsMetadataSourceClass *metadata_class = MS_METADATA_SOURCE_CLASS (klass);

  source_class->metadata = ms_flickr_source_metadata;
  source_class->search = ms_flickr_source_search;
  metadata_class->supported_keys = ms_flickr_source_supported_keys;

  g_type_class_add_private (klass, sizeof (MsFlickrSourcePrivate));
}

static void
ms_flickr_source_init (MsFlickrSource *source)
{
  source->priv = MS_FLICKR_SOURCE_GET_PRIVATE (source);

  if (!g_thread_supported ()) {
    g_thread_init (NULL);
  }
}

G_DEFINE_TYPE (MsFlickrSource, ms_flickr_source, MS_TYPE_MEDIA_SOURCE);

/* ======================= Utilities ==================== */

static void
update_media (MsContentMedia *media, flickcurl_photo *fc_photo)
{
  if (fc_photo->uri) {
    ms_content_media_set_url (media, fc_photo->uri);
  }
  if (fc_photo->fields[PHOTO_FIELD_owner_realname].string) {
    ms_content_media_set_author (media, fc_photo->fields[PHOTO_FIELD_owner_realname].string);
  }
  if (fc_photo->fields[PHOTO_FIELD_title].string) {
    ms_content_media_set_title (media, fc_photo->fields[PHOTO_FIELD_title].string);
  }
  if (fc_photo->fields[PHOTO_FIELD_description].string) {
    ms_content_media_set_description (media, fc_photo->fields[PHOTO_FIELD_description].string);
  }
  if (fc_photo->fields[PHOTO_FIELD_dates_taken].string) {
    ms_content_media_set_date (media, fc_photo->fields[PHOTO_FIELD_dates_taken].string);
  }
}

static MsContentMedia *
get_content_image (flickcurl_photo *fc_photo)
{
  MsContentMedia *media;

  if (strcmp (fc_photo->media_type, "photo") == 0) {
    media = ms_content_image_new ();
  } else {
    media = ms_content_video_new ();
  }

  ms_content_media_set_id (media, fc_photo->id);
  update_media (media, fc_photo);

  return media;
}

static gboolean
metadata_cb (gpointer data)
{
  MsMediaSourceMetadataSpec *ms = (MsMediaSourceMetadataSpec *) data;

  ms->callback(ms->source,
               ms->media,
               ms->user_data,
               NULL);

  return FALSE;
}

static gboolean
search_cb (gpointer data)
{
  SearchData *search_data = (SearchData *) data;

  search_data->ss->callback(search_data->ss->source,
                            search_data->ss->search_id,
                            search_data->media,
                            search_data->remaining,
                            search_data->ss->user_data,
                            NULL);

  g_free (data);

  return FALSE;
}

/* Make get_url TRUE if url has been requested.
 * Make get_others TRUE if other (supported) keys has been requested
 */
static void
check_keys (GList *keys, gboolean *get_url, gboolean *get_others)
{
  GList *iter;
  MsKeyID key_id;
  gboolean others = get_others? FALSE: TRUE;
  gboolean url = get_url? FALSE: TRUE;

  iter = keys;
  while (iter && (!url || !others)) {
    key_id = POINTER_TO_MSKEYID (iter->data);
    if (key_id == MS_METADATA_KEY_AUTHOR ||
        key_id == MS_METADATA_KEY_DESCRIPTION ||
        key_id == MS_METADATA_KEY_DATE) {
      others = TRUE;
    } else if (key_id == MS_METADATA_KEY_URL) {
      url = TRUE;
    }
    iter = g_list_next (iter);
  }

  if (get_url) {
    *get_url = url;
  }

  if (get_others) {
    *get_others = others;
  }
}

static gpointer
ms_flickr_source_metadata_main (gpointer data)
{
  MsMediaSourceMetadataSpec *ms = (MsMediaSourceMetadataSpec *) data;
  const gchar *id;
  flickcurl_photo *photo;
  flickcurl_size **photo_sizes;
  gboolean get_other_keys;
  gboolean get_slow_url;
  gchar *url;
  gint s;
  gint width, height;

  if (!ms->media || (id = ms_content_media_get_id (ms->media)) == NULL) {
    g_idle_add (metadata_cb, ms);
    return NULL;
  }

  check_keys (ms->keys, &get_slow_url, &get_other_keys);

  if (ms->flags & MS_RESOLVE_FAST_ONLY && get_other_keys) {
      get_slow_url = FALSE;
  }

  if (get_slow_url) {
    photo_sizes = flickcurl_photos_getSizes (MS_FLICKR_SOURCE (ms->source)->priv->fc,
                                             id);
    if (photo_sizes) {
      url = photo_sizes[0]->source;
      width = photo_sizes[0]->width;
      height = photo_sizes[0]->height;

      /* Look for "Original" size */
      s = 0;
      while (photo_sizes[s]) {
            if (strcmp (photo_sizes[s]->label, "Original") == 0) {
              url = photo_sizes[s]->source;
              width = photo_sizes[s]->width;
              height = photo_sizes[s]->height;
              break;
            }
            s++;
          }

      /* Update media */
      ms_content_media_set_url (ms->media, url);
      if (MS_IS_CONTENT_IMAGE (ms->media)) {
        ms_content_image_set_size (MS_CONTENT_IMAGE (ms->media), width, height);
      } else if (MS_IS_CONTENT_VIDEO (ms->media)) {
        ms_content_video_set_size (MS_CONTENT_VIDEO (ms->media), width, height);
      }
      flickcurl_free_sizes (photo_sizes);
    }
  }

  photo = flickcurl_photos_getInfo (MS_FLICKR_SOURCE (ms->source)->priv->fc,
                                    id);
  if (photo) {
    update_media (ms->media, photo);
  }

  flickcurl_free_photo (photo);

  g_idle_add (metadata_cb, ms);

  return NULL;
}

static gpointer
ms_flickr_source_search_main (gpointer data)
{
  MsContentMedia *media;
  MsMediaSourceSearchSpec *ss = (MsMediaSourceSearchSpec *) data;
  SearchData *search_data;
  char *url;
  flickcurl_photo *photo;
  flickcurl_photos_list *result;
  flickcurl_photos_list_params lparams;
  flickcurl_search_params sparams;
  flickcurl_size **photo_sizes;
  gboolean get_slow_keys = FALSE;
  gboolean get_slow_url = FALSE;
  int height;
  int i, s;
  int offset_in_page;
  int per_page;
  int width;

  flickcurl_search_params_init (&sparams);
  flickcurl_photos_list_params_init (&lparams);
  sparams.text = ss->text;

  /* Compute page offset */
  per_page = 1 + ss->skip + ss->count;
  lparams.per_page = per_page > 100? 100: per_page;
  lparams.page = 1 + (ss->skip/lparams.per_page);
  offset_in_page = 1 + (ss->skip%lparams.per_page);

  /* Check if we need need to ask for complete information for each photo */
  if (!(ss->flags & MS_RESOLVE_FAST_ONLY)) {
    /* Check if some "slow" key is requested */
    check_keys (ss->keys, &get_slow_url, &get_slow_keys);
  }

  for (;;) {
    result = flickcurl_photos_search_params (MS_FLICKR_SOURCE (ss->source)->priv->fc,
                                             &sparams,
                                             &lparams);
    /* No (more) results */
    if (!result || result->photos_count == 0) {
      g_debug ("No (more) results");
      search_data = g_new (SearchData, 1);
      search_data->ss = ss;
      search_data->media = NULL;
      search_data->remaining = 0;
      g_idle_add (search_cb, search_data);
      break;
    }
    for (i = offset_in_page; i < result->photos_count && ss->count > 0; i++) {
      /* As we are not computing whether there are enough photos to satisfy user
         requirement, use -1 in remaining elements (i.e., "unknown"), and use 0
         no more elements are/can be sent */
      media = get_content_image (result->photos[i]);
      if (get_slow_url) {
        photo_sizes = flickcurl_photos_getSizes (MS_FLICKR_SOURCE (ss->source)->priv->fc,
                                                 result->photos[i]->id);
        if (photo_sizes) {
          url = photo_sizes[0]->source;
          width = photo_sizes[0]->width;
          height = photo_sizes[0]->height;

        /* Look for "Original" size */
          s = 0;
          while (photo_sizes[s]) {
            if (strcmp (photo_sizes[s]->label, "Original") == 0) {
              url = photo_sizes[s]->source;
              width = photo_sizes[s]->width;
              height = photo_sizes[s]->height;
              break;
            }
            s++;
          }

          /* Update media */
          ms_content_media_set_url (media, url);
          if (MS_IS_CONTENT_IMAGE (media)) {
            ms_content_image_set_size (MS_CONTENT_IMAGE (media), width, height);
          } else if (MS_IS_CONTENT_VIDEO (media)) {
            ms_content_video_set_size (MS_CONTENT_VIDEO (media), width, height);
          }
          flickcurl_free_sizes (photo_sizes);
        }
      }

      if (get_slow_keys) {
        photo = flickcurl_photos_getInfo (MS_FLICKR_SOURCE (ss->source)->priv->fc,
                                          result->photos[i]->id);
        if (photo) {
          update_media (media, photo);
        }

        flickcurl_free_photo (photo);
      }

      search_data = g_new (SearchData, 1);
      search_data->ss = ss;
      search_data->media = media;
      search_data->remaining = ss->count == 1? 0: -1;
      g_idle_add (search_cb, search_data);

      ss->count--;
    }
    /* Sent all requested photos */
    if (ss->count == 0) {
      g_debug ("All results sent");
      break;
    }
    flickcurl_free_photos_list (result);
    offset_in_page = 0;
    lparams.page++;
  }
  /* Free last results */
 flickcurl_free_photos_list (result);

 return NULL;
}

/* ================== API Implementation ================ */

static const GList *
ms_flickr_source_supported_keys (MsMetadataSource *source)
{
  static GList *keys = NULL;
  if (!keys) {
    keys = ms_metadata_key_list_new (MS_METADATA_KEY_ID,
                                     MS_METADATA_KEY_URL,
                                     MS_METADATA_KEY_AUTHOR,
				     MS_METADATA_KEY_TITLE,
                                     MS_METADATA_KEY_DESCRIPTION,
                                     MS_METADATA_KEY_DATE,
                                     NULL);
  }
  return keys;
}

static void
ms_flickr_source_metadata (MsMediaSource *source,
                           MsMediaSourceMetadataSpec *ms)
{
  if (!g_thread_create (ms_flickr_source_metadata_main,
                        ms,
                        FALSE,
                        NULL)) {
    g_critical ("Unable to create thread");
  }
}

static void
ms_flickr_source_search (MsMediaSource *source,
                         MsMediaSourceSearchSpec *ss)
{
  if (!g_thread_create (ms_flickr_source_search_main,
                        ss,
                        FALSE,
                        NULL)) {
    g_critical ("Unable to create thread");
  }
}