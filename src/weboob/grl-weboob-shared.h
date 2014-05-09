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

/* A header to share declarations between weboob's plugin code */

#ifndef _GRL_WEBOOB_SHARED_H_
#define _GRL_WEBOOB_SHARED_H_

#include <grilo.h>

/* --------- Logging  -------- */

#define GRL_LOG_DOMAIN_DEFAULT weboob_log_domain
GRL_LOG_DOMAIN_EXTERN (weboob_log_domain);

/* --- Data types --- */

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

#endif /* _GRL_WEBOOB_SHARED_H_ */
