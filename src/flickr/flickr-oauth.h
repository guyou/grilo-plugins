/*
 * Authors: Marek Chalupa <mchalupa@redhat.com>
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

#include <glib.h>

#ifndef FLICKR_OAUTH_H
#define FLICKR_OAUTH_H

#define FLICKR_API_URL "http://api.flickr.com/services/rest"
#define FLICKR_OAUTH_AUTHPOINT "http://www.flickr.com/services/oauth/authorize"

/* OAuth definitions */
#define FLICKR_OAUTH_SIGNATURE_METHOD "HMAC-SHA1"
#define FLICKR_OAUTH_VERSION "1.0"
#define FLICKR_OAUTH_CALLBACK "oob"
#define FLICKR_OAUTH_HTTP_METHOD "GET"


/* public API */

gchar *
flickroauth_get_signature (const gchar *consumer_secret,
                           const gchar *token_secret,
                           const gchar *url,
                           gchar **params,
                           gint params_no);

gchar *
flickroauth_create_api_url (const gchar *consumer_key,
                            const gchar *consumer_secret,
                            const gchar *oauth_token,
                            const gchar *oauth_token_secret,
                            gchar **params,
                            const guint params_no);

gchar *
flickroauth_authorization_url (const gchar *oauth_token, const gchar *perms);

#endif /* FLICKR_OAUTH_H */
