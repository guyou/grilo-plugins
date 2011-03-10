/*
 * Copyright (C) 2011 Igalia S.L.
 * Copyright (C) 2011 Intel Corporation.
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

#include <grilo.h>
#include <string.h>
#include <tracker-sparql.h>

#include "grl-tracker-media.h"
#include "grl-tracker-media-priv.h"
#include "grl-tracker-media-api.h"
#include "grl-tracker-media-cache.h"
#include "grl-tracker-media-notif.h"
#include "grl-tracker-utils.h"

/* --------- Logging  -------- */

#define GRL_LOG_DOMAIN_DEFAULT tracker_general_log_domain
GRL_LOG_DOMAIN_STATIC(tracker_general_log_domain);

/* ------- Definitions ------- */

#define MEDIA_TYPE "grilo-media-type"

#define TRACKER_ITEM_CACHE_SIZE (10000)

/* --- Other --- */

enum {
  PROP_0,
  PROP_TRACKER_CONNECTION,
};

static void grl_tracker_media_set_property (GObject      *object,
                                             guint         propid,
                                             const GValue *value,
                                             GParamSpec   *pspec);

static void grl_tracker_media_constructed (GObject *object);

static void grl_tracker_media_finalize (GObject *object);

/* ===================== Globals  ================= */

/* shared data across  */
GrlTrackerCache *grl_tracker_item_cache;
GHashTable *grl_tracker_modified_sources;

/* ================== TrackerMedia GObject ================ */

G_DEFINE_TYPE (GrlTrackerMedia, grl_tracker_media, GRL_TYPE_MEDIA_SOURCE);

static GrlTrackerMedia *
grl_tracker_media_new (TrackerSparqlConnection *connection)
{
  GRL_DEBUG ("%s", __FUNCTION__);

  return g_object_new (GRL_TRACKER_MEDIA_TYPE,
                       "source-id", GRL_TRACKER_MEDIA_ID,
                       "source-name", GRL_TRACKER_MEDIA_NAME,
                       "source-desc", GRL_TRACKER_MEDIA_DESC,
                       "tracker-connection", connection,
                       NULL);
}

static void
grl_tracker_media_class_init (GrlTrackerMediaClass * klass)
{
  GrlMediaSourceClass    *source_class   = GRL_MEDIA_SOURCE_CLASS (klass);
  GrlMetadataSourceClass *metadata_class = GRL_METADATA_SOURCE_CLASS (klass);
  GObjectClass           *g_class        = G_OBJECT_CLASS (klass);

  source_class->query               = grl_tracker_media_query;
  source_class->metadata            = grl_tracker_media_metadata;
  source_class->search              = grl_tracker_media_search;
  source_class->browse              = grl_tracker_media_browse;
  source_class->cancel              = grl_tracker_media_cancel;
  source_class->notify_change_start = grl_tracker_media_change_start;
  source_class->notify_change_stop  = grl_tracker_media_change_stop;

  metadata_class->supported_keys = grl_tracker_supported_keys;

  g_class->finalize     = grl_tracker_media_finalize;
  g_class->set_property = grl_tracker_media_set_property;
  g_class->constructed  = grl_tracker_media_constructed;

  g_object_class_install_property (g_class,
                                   PROP_TRACKER_CONNECTION,
                                   g_param_spec_object ("tracker-connection",
                                                        "tracker connection",
                                                        "A Tracker connection",
                                                        TRACKER_SPARQL_TYPE_CONNECTION,
                                                        G_PARAM_WRITABLE
                                                        | G_PARAM_CONSTRUCT_ONLY
                                                        | G_PARAM_STATIC_NAME));

  g_type_class_add_private (klass, sizeof (GrlTrackerMediaPriv));

  grl_tracker_setup_key_mappings ();
}

static void
grl_tracker_media_init (GrlTrackerMedia *source)
{
  GrlTrackerMediaPriv *priv = GRL_TRACKER_MEDIA_GET_PRIVATE (source);

  source->priv = priv;

  priv->operations = g_hash_table_new (g_direct_hash, g_direct_equal);
}

static void
grl_tracker_media_constructed (GObject *object)
{
  GrlTrackerMediaPriv *priv = GRL_TRACKER_MEDIA_GET_PRIVATE (object);

  if (grl_tracker_per_device_source)
    g_object_get (object, "source-id", &priv->tracker_datasource, NULL);
}

static void
grl_tracker_media_finalize (GObject *object)
{
  GrlTrackerMedia *self;

  self = GRL_TRACKER_MEDIA (object);
  if (self->priv->tracker_connection)
    g_object_unref (self->priv->tracker_connection);

  G_OBJECT_CLASS (grl_tracker_media_parent_class)->finalize (object);
}

static void
grl_tracker_media_set_property (GObject      *object,
                                 guint         propid,
                                 const GValue *value,
                                 GParamSpec   *pspec)

{
  GrlTrackerMediaPriv *priv = GRL_TRACKER_MEDIA_GET_PRIVATE (object);

  switch (propid) {
    case PROP_TRACKER_CONNECTION:
      if (priv->tracker_connection != NULL)
        g_object_unref (G_OBJECT (priv->tracker_connection));
      priv->tracker_connection = g_object_ref (g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, propid, pspec);
  }
}

/* =================== TrackerMedia Plugin  =============== */

void
grl_tracker_add_source (GrlTrackerMedia *source)
{
  GrlTrackerMediaPriv *priv = GRL_TRACKER_MEDIA_GET_PRIVATE (source);

  GRL_DEBUG ("====================>add source '%s' count=%u",
             grl_metadata_source_get_name (GRL_METADATA_SOURCE (source)),
             priv->notification_ref);

  if (priv->notification_ref > 0) {
    priv->notification_ref--;
  }
  if (priv->notification_ref == 0) {
    g_hash_table_remove (grl_tracker_modified_sources,
                         grl_metadata_source_get_id (GRL_METADATA_SOURCE (source)));
    priv->state = GRL_TRACKER_MEDIA_STATE_RUNNING;
    grl_plugin_registry_register_source (grl_plugin_registry_get_default (),
                                         grl_tracker_plugin,
                                         GRL_MEDIA_PLUGIN (source),
                                         NULL);
  }
}

void
grl_tracker_del_source (GrlTrackerMedia *source)
{
  GrlTrackerMediaPriv *priv = GRL_TRACKER_MEDIA_GET_PRIVATE (source);

  GRL_DEBUG ("==================>del source '%s' count=%u",
             grl_metadata_source_get_name (GRL_METADATA_SOURCE (source)),
             priv->notification_ref);
  if (priv->notification_ref > 0) {
    priv->notification_ref--;
  }
  if (priv->notification_ref == 0) {
    g_hash_table_remove (grl_tracker_modified_sources,
                         grl_metadata_source_get_id (GRL_METADATA_SOURCE (source)));
    grl_tracker_media_cache_del_source (grl_tracker_item_cache, source);
    priv->state = GRL_TRACKER_MEDIA_STATE_DELETED;
    grl_plugin_registry_unregister_source (grl_plugin_registry_get_default (),
                                           GRL_MEDIA_PLUGIN (source),
                                           NULL);
  }
}

gboolean
grl_tracker_media_can_notify (GrlTrackerMedia *source)
{
  GrlTrackerMediaPriv *priv = GRL_TRACKER_MEDIA_GET_PRIVATE (source);

  if (priv->state == GRL_TRACKER_MEDIA_STATE_RUNNING)
    return priv->notify_changes;

  return FALSE;
}

GrlTrackerMedia *
grl_tracker_media_find (const gchar *id)
{
  GrlMediaPlugin *source;

  source = grl_plugin_registry_lookup_source (grl_plugin_registry_get_default (),
					      id);

  if (source && GRL_IS_TRACKER_MEDIA (source)) {
    return (GrlTrackerMedia *) source;
  }

  return
    (GrlTrackerMedia *) g_hash_table_lookup (grl_tracker_modified_sources,
                                             id);
}

static void
tracker_get_datasource_cb (GObject             *object,
                           GAsyncResult        *result,
                           TrackerSparqlCursor *cursor)
{
  const gchar *type, *datasource, *datasource_name, *uri;
  gboolean source_available;
  GError *error = NULL;
  GrlTrackerMedia *source;

  GRL_DEBUG ("%s", __FUNCTION__);

  if (!tracker_sparql_cursor_next_finish (cursor, result, &error)) {
    if (error == NULL) {
      GRL_DEBUG ("\tEnd of parsing of devices");
    } else {
      GRL_WARNING ("\tError while parsing devices: %s", error->message);
      g_error_free (error);
    }
    g_object_unref (G_OBJECT (cursor));
    return;
  }

  type = tracker_sparql_cursor_get_string (cursor, 0, NULL);
  datasource = tracker_sparql_cursor_get_string (cursor, 1, NULL);
  datasource_name = tracker_sparql_cursor_get_string (cursor, 2, NULL);
  uri = tracker_sparql_cursor_get_string (cursor, 3, NULL);
  source_available = tracker_sparql_cursor_get_boolean (cursor, 4);

  source = grl_tracker_media_find (datasource);

  if ((source == NULL) && source_available) {
    gchar *source_name = grl_tracker_get_media_name (type, uri, datasource,
                                                     datasource_name);
    GRL_DEBUG ("\tnew datasource: urn=%s name=%s uri=%s\n",
	       datasource, datasource_name, uri);
    source = g_object_new (GRL_TRACKER_MEDIA_TYPE,
                           "source-id", datasource,
                           "source-name", source_name,
                           "source-desc", GRL_TRACKER_MEDIA_DESC,
                           "tracker-connection", grl_tracker_connection,
                           NULL);
    grl_tracker_add_source (source);
    g_free (source_name);
  }

  tracker_sparql_cursor_next_async (cursor, NULL,
                                    (GAsyncReadyCallback) tracker_get_datasource_cb,
                                    cursor);
}

static void
tracker_get_datasources_cb (GObject      *object,
                            GAsyncResult *result,
                            gpointer      data)
{
  GError *error = NULL;
  TrackerSparqlCursor *cursor;

  GRL_DEBUG ("%s", __FUNCTION__);

  cursor = tracker_sparql_connection_query_finish (grl_tracker_connection,
                                                   result, &error);

  if (error) {
    GRL_WARNING ("Cannot handle datasource request : %s", error->message);
    g_error_free (error);
    return;
  }

  tracker_sparql_cursor_next_async (cursor, NULL,
                                    (GAsyncReadyCallback) tracker_get_datasource_cb,
                                    cursor);
}

void
grl_tracker_media_sources_init (void)
{
  GRL_DEBUG ("%s", __FUNCTION__);

  grl_tracker_item_cache =
    grl_tracker_media_cache_new (TRACKER_ITEM_CACHE_SIZE);
  grl_tracker_modified_sources = g_hash_table_new (g_str_hash, g_str_equal);


  if (grl_tracker_connection != NULL) {
    grl_tracker_media_dbus_start_watch ();

    if (grl_tracker_per_device_source == TRUE) {
      /* Let's discover available data sources. */
      GRL_DEBUG ("\tper device source mode request: '"
                 TRACKER_DATASOURCES_REQUEST "'");

      tracker_sparql_connection_query_async (grl_tracker_connection,
                                             TRACKER_DATASOURCES_REQUEST,
                                             NULL,
                                             (GAsyncReadyCallback) tracker_get_datasources_cb,
                                             NULL);
    } else {
      /* One source to rule them all. */
      grl_tracker_add_source (grl_tracker_media_new (grl_tracker_connection));
    }
  }
}
