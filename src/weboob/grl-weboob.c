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

#include <grilo.h>
#include <glib/gi18n-lib.h>
#include <string.h>

#include "videoob.h"
#include "weboob.h"

#include "grl-weboob-shared.h"
#include "grl-weboob.h"

enum {
  PROP_0,
  PROP_BACKEND,
};

#define GRL_WEBOOB_SOURCE_GET_PRIVATE(object)          \
  (G_TYPE_INSTANCE_GET_PRIVATE((object),               \
                               GRL_WEBOOB_SOURCE_TYPE, \
                               GrlWeboobSourcePriv))

/* --------- Logging  -------- */

GRL_LOG_DOMAIN (weboob_log_domain);

/* ----- Root categories ---- */

#define WEBOOB_ROOT_NAME       "Weboob"

#define ROOT_DIR_FEEDS_INDEX      0
#define ROOT_DIR_CATEGORIES_INDEX 1

/* ----- Feeds categories ---- */

/* --- Other --- */

/* --- Plugin information --- */

#define PLUGIN_ID   WEBOOB_PLUGIN_ID

#define SOURCE_ID   "grl-weboob"
#define SOURCE_NAME "Weboob"
#define SOURCE_DESC _("A source for browsing and searching videos with weboob")
#define SOURCE_NAME_FMT "Weboob for %s"

/* --- Data types --- */

typedef void (*BuildMediaFromEntryCbFunc) (GrlMedia *media, gpointer user_data);

typedef struct {
  GSourceFunc callback;
  gpointer user_data;
} BuildCategorySpec;

typedef struct {
  gchar *id;
  gchar *name;
  guint count;
} CategoryInfo;

typedef struct {
  GrlSource *source;
  GCancellable *cancellable;
  guint operation_id;
  const gchar *container_id;
  GList *keys;
  GrlResolutionFlags flags;
  guint skip;
  guint count;
  GrlSourceResultCb callback;
  gpointer user_data;
  guint error_code;
  CategoryInfo *category_info;
  guint emitted;
  guint matches;
  guint ref_count;
} OperationSpec;

struct _GrlWeboobSourcePriv {
  gchar *backend;
};

#define WEBOOB_CLIENT_ID "grilo"

static GrlWeboobSource *grl_weboob_source_new_all (void);

static GrlWeboobSource *grl_weboob_source_new_backend (const gchar *backend, const gchar *desc);

static void grl_weboob_source_set_property (GObject *object,
                                            guint propid,
                                            const GValue *value,
                                            GParamSpec *pspec);
static void grl_weboob_source_finalize (GObject *object);

gboolean grl_weboob_plugin_init (GrlRegistry *registry,
                                 GrlPlugin *plugin,
                                 GList *configs);

static GrlSupportedOps grl_weboob_source_supported_operations (GrlSource *source);

static const GList *grl_weboob_source_supported_keys (GrlSource *source);

static const GList *grl_weboob_source_slow_keys (GrlSource *source);

static void grl_weboob_source_search (GrlSource *source,
                                      GrlSourceSearchSpec *ss);

static void grl_weboob_source_browse (GrlSource *source,
                                      GrlSourceBrowseSpec *bs);

static void grl_weboob_source_resolve (GrlSource *source,
                                       GrlSourceResolveSpec *rs);

static gboolean grl_weboob_test_media_from_uri (GrlSource *source,
                                                const gchar *uri);

static void grl_weboob_get_media_from_uri (GrlSource *source,
                                           GrlSourceMediaFromUriSpec *mfus);

static void grl_weboob_source_cancel (GrlSource *source,
                                       guint operation_id);

/* ==================== Global Data  ================= */

CategoryInfo *categories_dir = NULL;

static GrlWeboobSource *ytsrc = NULL;

/* =================== Weboob Plugin  =============== */

static void
add_backend (gpointer data,
             gpointer user_data)
{
  gchar *backend = (gchar*) data;
  GrlRegistry *registry = (GrlRegistry*) user_data;
  GrlPlugin *plugin = NULL;
  GrlWeboobSource *source;

  plugin = grl_registry_lookup_plugin (registry, WEBOOB_PLUGIN_ID);

  source = grl_weboob_source_new_backend (backend, NULL);

  grl_registry_register_source (registry,
                                plugin,
                                GRL_SOURCE (source),
                                NULL);
}

gboolean
grl_weboob_plugin_init (GrlRegistry *registry,
                        GrlPlugin *plugin,
                        GList *configs)
{
  gchar *type = NULL;
  GrlConfig *config;
  gint config_count;
  GrlWeboobSource *source;
  gchar **backends;
  GError *error = NULL;

  GRL_LOG_DOMAIN_INIT (weboob_log_domain, "weboob");

  GRL_DEBUG (__FUNCTION__);

  /* Initialize i18n */
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

  if (!configs) {
    GRL_INFO ("Configuration not provided! Will use default settings");
  } else {
    config_count = g_list_length (configs);
    if (config_count > 1) {
      GRL_INFO ("Provided %d configs, but will only use one", config_count);
    }

    config = GRL_CONFIG (configs->data);
    type = grl_config_get_string (config, "backends");
  }
  
  if (NULL == type) {
    type = g_strdup ("both");
  }

  backends = videoob_backends(&error);
  if (backends != NULL) {

    if (strcmp (type, "both") == 0 ||
        strcmp (type, "all") == 0) {
      /* A source for all */
      source = grl_weboob_source_new_all ();

      grl_registry_register_source (registry,
                                    plugin,
                                    GRL_SOURCE (source),
                                    NULL);
    }

    if (strcmp (type, "both") == 0 ||
        strcmp (type, "backends") == 0) {
      /* A source for each backend */
      gchar **backends_iter = backends;
      while (*backends_iter != NULL) {
        add_backend (*backends_iter, registry);
        
        /* Next */
        backends_iter++;
      }
    }
    
    g_strfreev (backends);
  }
  /* else videoob not present */

  g_free (type);

  return TRUE;
}

GRL_PLUGIN_REGISTER (grl_weboob_plugin_init,
                     NULL,
                     PLUGIN_ID);

/* ================== YouTube GObject ================ */

G_DEFINE_TYPE (GrlWeboobSource, grl_weboob_source, GRL_TYPE_SOURCE);

static GrlWeboobSource *
grl_weboob_source_new (const gchar *id,
                       const gchar *name,
                       const gchar *desc,
                       const gchar *backend)
{
  GrlWeboobSource *source;
  GIcon *icon;

  GRL_DEBUG ("%s ( %s, %s, %s, %s )",
             __FUNCTION__, id, name, desc, backend);

  icon = weboob_module_get_icon (backend);

  source = GRL_WEBOOB_SOURCE (g_object_new (GRL_WEBOOB_SOURCE_TYPE,
                                            "source-id", id,
                                            "source-name", name,
                                            "source-desc", desc,
                                            "weboob-backend", backend,
                                            "auto-split-threshold", 0,
                                            "supported-media", GRL_MEDIA_TYPE_VIDEO,
                                            "source-icon", icon,
                                            NULL));

  g_object_unref (icon);

  return source;
}

static GrlWeboobSource *
grl_weboob_source_new_all (void)
{
  GrlWeboobSource *source;

  GRL_DEBUG (__FUNCTION__);

  source = grl_weboob_source_new (SOURCE_ID, SOURCE_NAME, SOURCE_DESC, NULL);
  
  ytsrc = source;
  g_object_add_weak_pointer (G_OBJECT (source), (gpointer *) &ytsrc);

  return source;
}

static GrlWeboobSource *
grl_weboob_source_new_backend (const gchar *backend, const gchar *desc)
{
  GrlWeboobSource *source;
  gchar *id;
  gchar *name;

  GRL_DEBUG (__FUNCTION__);

  id = g_strdup_printf ("%s-%s", SOURCE_ID, backend);
  name = g_strdup_printf (SOURCE_NAME_FMT, backend),
  source = grl_weboob_source_new (id, name, desc, backend);
  g_free (id);
  g_free (name);
  
  ytsrc = source;
  g_object_add_weak_pointer (G_OBJECT (source), (gpointer *) &ytsrc);

  return source;
}

static void
grl_weboob_source_class_init (GrlWeboobSourceClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GrlSourceClass *source_class = GRL_SOURCE_CLASS (klass);

  gobject_class->set_property = grl_weboob_source_set_property;
  gobject_class->finalize = grl_weboob_source_finalize;

  source_class->supported_operations = grl_weboob_source_supported_operations;
  source_class->supported_keys = grl_weboob_source_supported_keys;
  source_class->slow_keys = grl_weboob_source_slow_keys;
  source_class->cancel = grl_weboob_source_cancel;

  source_class->search = grl_weboob_source_search;
  source_class->browse = grl_weboob_source_browse;
  source_class->resolve = grl_weboob_source_resolve;
  source_class->test_media_from_uri = grl_weboob_test_media_from_uri;
  source_class->media_from_uri = grl_weboob_get_media_from_uri;

  g_object_class_install_property (gobject_class,
                                   PROP_BACKEND,
                                   g_param_spec_string ("weboob-backend",
                                                        "weboob-backend",
                                                        "Weboob backend identifier",
                                                        NULL,
                                                        G_PARAM_WRITABLE
                                                        | G_PARAM_CONSTRUCT_ONLY
                                                        | G_PARAM_STATIC_NAME));

  g_type_class_add_private (klass, sizeof (GrlWeboobSourcePriv));
}

static void
grl_weboob_source_init (GrlWeboobSource *source)
{
  source->priv = GRL_WEBOOB_SOURCE_GET_PRIVATE (source);
}

static void
grl_weboob_source_set_property (GObject *object,
                                 guint propid,
                                 const GValue *value,
                                 GParamSpec *pspec)

{
  GrlWeboobSourcePriv *priv = GRL_WEBOOB_SOURCE_GET_PRIVATE (object);

  switch (propid) {
    case PROP_BACKEND:
      g_clear_pointer (&priv->backend, g_free);
      priv->backend = g_strdup (g_value_get_string (value));
      break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, propid, pspec);
  }
}

static void
grl_weboob_source_finalize (GObject *object)
{
  GrlWeboobSource *self;

  self = GRL_WEBOOB_SOURCE (object);

  g_free (GRL_WEBOOB_SOURCE_GET_PRIVATE (self)->backend);

  G_OBJECT_CLASS (grl_weboob_source_parent_class)->finalize (object);
}

/* ======================= Utilities ==================== */

static OperationSpec *
operation_spec_new (void)
{
  OperationSpec *os;

  GRL_DEBUG ("Allocating new spec");

  os = g_slice_new0 (OperationSpec);
  os->ref_count = 1;

  return os;
}

static void
operation_spec_unref (OperationSpec *os)
{
  os->ref_count--;
  if (os->ref_count == 0) {
    g_clear_object (&os->cancellable);
    g_slice_free (OperationSpec, os);
    GRL_DEBUG ("freeing spec");
  }
}

static void
operation_spec_ref (OperationSpec *os)
{
  GRL_DEBUG ("Reffing spec");
  os->ref_count++;
}

static void
operation_spec_set_medias (OperationSpec *os, GList *medias)
{
  GrlMedia *media;
  GList *iter;
  gint count = 0;
  
  if (NULL != medias)
    count = g_list_length (medias); 

  if (count > 0) {
    medias = g_list_reverse (medias);
    iter = medias;
    while (iter) {
      media = GRL_MEDIA (iter->data);
      os->callback (os->source,
                    os->operation_id,
                    media,
                    GRL_SOURCE_REMAINING_UNKNOWN,
                    os->user_data,
                    NULL);
      iter = g_list_next (iter);
    }
  }
}

/* ================== API Implementation ================ */

static GrlSupportedOps
grl_weboob_source_supported_operations (GrlSource *source)
{
  GrlSupportedOps caps;

  caps = GRL_OP_BROWSE | GRL_OP_RESOLVE | GRL_OP_SEARCH;

  return caps;
}

static const GList *
grl_weboob_source_supported_keys (GrlSource *source)
{
  static GList *keys = NULL;
  if (!keys) {
    keys = grl_metadata_key_list_new (GRL_METADATA_KEY_ID,
                                      GRL_METADATA_KEY_TITLE,
                                      GRL_METADATA_KEY_URL,
                                      GRL_METADATA_KEY_DESCRIPTION,
                                      GRL_METADATA_KEY_DURATION,
                                      GRL_METADATA_KEY_PUBLICATION_DATE,
                                      GRL_METADATA_KEY_THUMBNAIL,
                                      GRL_METADATA_KEY_SITE,
                                      GRL_METADATA_KEY_RATING,
                                      GRL_METADATA_KEY_INVALID);
  }
  return keys;
}

static const GList *
grl_weboob_source_slow_keys (GrlSource *source)
{
  static GList *keys = NULL;
  if (!keys) {
    keys = grl_metadata_key_list_new (GRL_METADATA_KEY_URL,
                                      GRL_METADATA_KEY_INVALID);
  }
  return keys;
}

static void
result_cb (GObject      *source_object,
           GAsyncResult *res,
           gpointer      user_data)
{
  GDataInputStream *dis = G_DATA_INPUT_STREAM (source_object);
  OperationSpec *os = (OperationSpec*) user_data;
  GList *medias = NULL;
  GCancellable *cancellable;
  GError *error = NULL;
  
  GRL_DEBUG ("%s", __FUNCTION__);

  cancellable = os->cancellable;

  if (g_cancellable_is_cancelled (cancellable)) {
    GRL_DEBUG ("%s: cancelled", __FUNCTION__);
    /* Keep line as NULL to interrupt reading */
  } else {
    medias = videoob_read_finish (dis, res, &error);
  }
  if (NULL != medias && NULL == error) {
    operation_spec_set_medias (os, medias);
    g_list_free (medias);
    
    /* next */
    weboob_read_async (dis, G_PRIORITY_DEFAULT,
                       os->cancellable,
                       result_cb,
                       os);
  } else {
    /* Nothing more */
    g_input_stream_close (G_INPUT_STREAM (dis), NULL, NULL);
    os->callback (os->source, os->operation_id, NULL, 0, os->user_data, error);
    
    operation_spec_unref (os);
  }
}

static void
grl_weboob_source_search (GrlSource *source,
                          GrlSourceSearchSpec *ss)
{
  OperationSpec *os;
  GDataInputStream *dis;
  gchar *backend;
  GError *error = NULL;
  
  GRL_DEBUG ("%s (%u, %d)",
             __FUNCTION__,
             grl_operation_options_get_skip (ss->options),
             grl_operation_options_get_count (ss->options));

  if (!ss->text) {
    /* Vimeo does not support searching all */
    error = g_error_new (GRL_CORE_ERROR,
                         GRL_CORE_ERROR_SEARCH_NULL_UNSUPPORTED,
                         _("Failed to search: %s"),
                         _("non-NULL search text is required"));
    ss->callback (ss->source, ss->operation_id, NULL, 0, ss->user_data, error);
    g_error_free (error);
    return;
  }

  if (0 != grl_operation_options_get_skip (ss->options)) {
    GRL_INFO ("Skip operation unsupported");
    ss->callback (ss->source, ss->operation_id, NULL, 0, ss->user_data, error);
    return;
  }

  os = operation_spec_new ();
  os->source = source;
  os->cancellable = g_cancellable_new ();
  os->operation_id = ss->operation_id;
  os->keys = ss->keys;
  os->skip = grl_operation_options_get_skip (ss->options);
  os->count = grl_operation_options_get_count (ss->options);
  os->callback = ss->callback;
  os->user_data = ss->user_data;
  os->error_code = GRL_CORE_ERROR_SEARCH_FAILED;

  /* Look for OPERATION_SPEC_REF_RATIONALE for details */
  operation_spec_ref (os);

  grl_operation_set_data (ss->operation_id, os->cancellable);

  backend = GRL_WEBOOB_SOURCE_GET_PRIVATE (source)->backend;
  dis = videoob_search (backend, os->count, ss->text, &error);
  weboob_read_async (dis, G_PRIORITY_DEFAULT,
                     os->cancellable,
                     result_cb,
                     os);
  g_object_unref (dis);

  operation_spec_unref (os);
}

static void
grl_weboob_source_browse (GrlSource *source,
                          GrlSourceBrowseSpec *bs)
{
  OperationSpec *os;
  GDataInputStream *dis;
  const gchar *container_id;
  const gchar *url;
  gchar *backend;
  GError *error = NULL;

  GRL_DEBUG ("%s: %s (%u, %d)",
             __FUNCTION__,
             grl_media_get_id (bs->container),
             grl_operation_options_get_skip (bs->options),
             grl_operation_options_get_count (bs->options));

  container_id = grl_media_get_id (bs->container);
  url = grl_media_get_url (bs->container);

  os = operation_spec_new ();
  os->source = bs->source;
  os->cancellable = g_cancellable_new ();
  os->operation_id = bs->operation_id;
  os->container_id = container_id;
  os->keys = bs->keys;
  os->flags = grl_operation_options_get_flags (bs->options);
  os->skip = grl_operation_options_get_skip (bs->options);
  os->count = grl_operation_options_get_count (bs->options);
  os->callback = bs->callback;
  os->user_data = bs->user_data;
  os->error_code = GRL_CORE_ERROR_BROWSE_FAILED;

  /* Look for OPERATION_SPEC_REF_RATIONALE for details */
  operation_spec_ref (os);

  grl_operation_set_data (bs->operation_id, os->cancellable);

  backend = GRL_WEBOOB_SOURCE_GET_PRIVATE (source)->backend;
  dis = videoob_ls (backend, os->count, url, &error);
  weboob_read_async (dis, G_PRIORITY_DEFAULT,
                     os->cancellable,
                     result_cb,
                     os);
  g_object_unref (dis);

  operation_spec_unref (os);
}

static void
resolve_cb (GObject      *source_object,
            GAsyncResult *res,
            gpointer      user_data)
{
  GDataInputStream *dis = G_DATA_INPUT_STREAM (source_object);
  GrlSourceResolveSpec *rs = (GrlSourceResolveSpec*) user_data;
  GList *medias = NULL;
  GrlMedia *media;
  GCancellable *cancellable;
  GError *error = NULL;
  
  GRL_DEBUG ("%s", __FUNCTION__);
  
  /* FIXME operation_id can be invalid as operation can be cancelled */
  cancellable = grl_operation_get_data (rs->operation_id);
  
  if (g_cancellable_is_cancelled (cancellable)) {
    GRL_DEBUG ("%s: cancelled", __FUNCTION__);
    /* Keep line as NULL to interrupt reading */
  } else {
    medias = videoob_read_finish (dis, res, &error);
  }
  if (NULL != medias && NULL == error && g_list_length (medias) > 0) {
    media = g_list_nth_data (medias, 0);
    /* Resolve Media */
    GRL_DEBUG ("%s: media url: %s", __FUNCTION__, grl_media_get_url (media));
    grl_media_set_url (rs->media, grl_media_get_url (media));
    g_object_ref (media);
  }
  g_list_free_full (medias, g_object_unref);

  g_input_stream_close (G_INPUT_STREAM (dis), NULL, NULL);
  rs->callback (rs->source, rs->operation_id, rs->media, rs->user_data, error);
}

static void
grl_weboob_source_resolve (GrlSource *source,
                           GrlSourceResolveSpec *rs)
{
  const gchar *id;
  GDataInputStream *dis;
  GCancellable *cancellable;
  gchar *backend;
  GError *error = NULL;
  
  GRL_DEBUG (__FUNCTION__);

  if (!rs->media || (id = grl_media_get_id (rs->media)) == NULL) {
    goto send_unchanged;
  }

  /* As all the keys are added always, except URL, the only case is missing URL */
  if (g_list_find (rs->keys, GRLKEYID_TO_POINTER (GRL_METADATA_KEY_URL)) != NULL &&
      grl_media_get_url (rs->media) == NULL) {

    cancellable = g_cancellable_new ();

    grl_operation_set_data (rs->operation_id, cancellable);

    backend = GRL_WEBOOB_SOURCE_GET_PRIVATE (source)->backend;
    dis = videoob_info (backend, id, &error);
    weboob_read_async (dis, G_PRIORITY_DEFAULT,
                       cancellable,
                       resolve_cb,
                       rs);
    g_object_unref (dis);

    return;
  }

 send_unchanged:
  rs->callback (rs->source, rs->operation_id, rs->media, rs->user_data, NULL);
}

static gboolean
grl_weboob_test_media_from_uri (GrlSource *source, const gchar *uri)
{
  // videoob info http://url
  gboolean ok = TRUE;

  GRL_DEBUG (__FUNCTION__);

  /* TODO */

  return ok;
}

static void
grl_weboob_get_media_from_uri (GrlSource *source,
                               GrlSourceMediaFromUriSpec *mfus)
{
  gchar *video_id = NULL;
  GError *error;
  GCancellable *cancellable;

  GRL_DEBUG (__FUNCTION__);

  if (video_id == NULL) {
    error = g_error_new (GRL_CORE_ERROR,
                         GRL_CORE_ERROR_MEDIA_FROM_URI_FAILED,
                         _("Cannot get media from %s"),
                         mfus->uri);
    mfus->callback (source, mfus->operation_id, NULL, mfus->user_data, error);
    g_error_free (error);
    return;
  }

  cancellable = g_cancellable_new ();
  grl_operation_set_data (mfus->operation_id, cancellable);

  /* TODO */
}

static void
grl_weboob_source_cancel (GrlSource *source,
                          guint operation_id)
{
  GCancellable *cancellable;

  GRL_DEBUG (__FUNCTION__);

  cancellable = G_CANCELLABLE (grl_operation_get_data (operation_id));

  if (cancellable) {
    g_cancellable_cancel (cancellable);
  }
}

// https://developer.gnome.org/gio/unstable/GUnixInputStream.html#g-unix-input-stream-new
