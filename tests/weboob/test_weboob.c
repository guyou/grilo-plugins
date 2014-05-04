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

#include <grilo.h>

#define WEBOOB_ID "grl-weboob"

static GMainLoop *main_loop = NULL;

static void
test_setup (void)
{
  GError *error = NULL;
  GrlConfig *config;
  GrlRegistry *registry;

  registry = grl_registry_get_default ();

  config = grl_config_new (WEBOOB_ID, NULL);
  grl_config_set_string (config, "format", "HD");
  grl_registry_add_config (registry, config, NULL);

  grl_registry_load_all_plugins (registry, &error);
  g_assert_no_error (error);
}

static void
test_search_normal (void)
{
  GError *error = NULL;
  GList *medias;
  GrlMedia *media;
  GrlOperationOptions *options;
  GrlRegistry *registry;
  GrlSource *source;

  registry = grl_registry_get_default ();
  source = grl_registry_lookup_source (registry, WEBOOB_ID);
  g_assert (source);
  options = grl_operation_options_new (NULL);
  grl_operation_options_set_count (options, 2);
  grl_operation_options_set_flags (options, GRL_RESOLVE_FAST_ONLY);
  g_assert (options);

  medias = grl_source_search_sync (source,
                                   "gnome",
                                   grl_source_supported_keys (source),
                                   options,
                                   &error);

  g_assert_cmpint (g_list_length(medias), ==, 2);
  g_assert_no_error (error);

  media = g_list_nth_data (medias, 0);

  g_assert (GRL_IS_MEDIA_VIDEO (media));

  media = g_list_nth_data (medias, 1);

  g_assert (GRL_IS_MEDIA_VIDEO (media));

  g_list_free_full (medias, g_object_unref);
  g_object_unref (options);
}

static void
test_search_null (void)
{
  GError *error = NULL;
  GList *medias;
  GrlOperationOptions *options;
  GrlRegistry *registry;
  GrlSource *source;

  registry = grl_registry_get_default ();
  source = grl_registry_lookup_source (registry, WEBOOB_ID);
  g_assert (source);
  options = grl_operation_options_new (NULL);
  grl_operation_options_set_count (options, 2);
  grl_operation_options_set_flags (options, GRL_RESOLVE_FAST_ONLY);
  g_assert (options);

  medias = grl_source_search_sync (source,
                                   NULL,
                                   grl_source_supported_keys (source),
                                   options,
                                   &error);

  g_assert_cmpint (g_list_length(medias), ==, 0);
  g_assert_error (error,
                  GRL_CORE_ERROR,
                  GRL_CORE_ERROR_SEARCH_NULL_UNSUPPORTED);

  g_object_unref (options);
  g_error_free (error);
}

static void
test_search_empty (void)
{
  GError *error = NULL;
  GList *medias;
  GrlOperationOptions *options;
  GrlRegistry *registry;
  GrlSource *source;

  registry = grl_registry_get_default ();
  source = grl_registry_lookup_source (registry, WEBOOB_ID);
  g_assert (source);
  options = grl_operation_options_new (NULL);
  grl_operation_options_set_count (options, 2);
  grl_operation_options_set_flags (options, GRL_RESOLVE_FAST_ONLY);
  g_assert (options);

  medias = grl_source_search_sync (source,
                                   "invalidfoo",
                                   grl_source_supported_keys (source),
                                   options,
                                   &error);

  g_assert_cmpint (g_list_length(medias), ==, 0);
  g_assert_no_error (error);

  g_object_unref (options);
}

static void
search_cb (GrlSource *source,
           guint operation_id,
           GrlMedia *media,
           guint remaining,
           gpointer user_data,
           const GError *error)
{
  static gboolean is_cancelled = FALSE;

  if (!is_cancelled) {
    g_assert (media);
    g_object_unref (media);
    g_assert_cmpint (remaining, >, 0);
    g_assert_no_error (error);
    grl_operation_cancel (operation_id);
    is_cancelled = TRUE;
  } else {
    g_assert (!media);
    g_assert_cmpint (remaining, ==, 0);
    g_assert_error (error,
                    GRL_CORE_ERROR,
                    GRL_CORE_ERROR_OPERATION_CANCELLED);
    g_main_loop_quit (main_loop);
  }
}

static void
test_cancel (void)
{
  GrlOperationOptions *options;
  GrlRegistry *registry;
  GrlSource *source;

  registry = grl_registry_get_default ();
  source = grl_registry_lookup_source (registry, WEBOOB_ID);
  g_assert (source);
  options = grl_operation_options_new (NULL);
  grl_operation_options_set_count (options, 2);
  grl_operation_options_set_flags (options, GRL_RESOLVE_FAST_ONLY);
  g_assert (options);

  grl_source_search (source,
                     "gnome",
                     grl_source_supported_keys (source),
                     options,
                     search_cb,
                     NULL);

  if (!main_loop) {
    main_loop = g_main_loop_new (NULL, FALSE);
  }

  g_main_loop_run (main_loop);
  g_object_unref (options);
}

int
main (int argc, char **argv)
{
  g_setenv ("GRL_PLUGIN_PATH", WEBOOB_PLUGIN_PATH, TRUE);
  g_setenv ("GRL_PLUGIN_LIST", WEBOOB_ID, TRUE);

  grl_init (&argc, &argv);
  g_test_init (&argc, &argv, NULL);

#if !GLIB_CHECK_VERSION(2,32,0)
  g_thread_init (NULL);
#endif

  test_setup ();

  g_test_add_func ("/weboob/search/normal", test_search_normal);
  //g_test_add_func ("/weboob/search/null", test_search_null);
  //g_test_add_func ("/weboob/search/empty", test_search_empty);
  g_test_add_func ("/weboob/cancel", test_cancel);

  gint result = g_test_run ();

  grl_deinit ();

  return result;
}
