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

#ifndef _GRL_VIDEOOB_SOURCE_H_
#define _GRL_VIDEOOB_SOURCE_H_

#include "grl-weboob-shared.h"

void videoob_ls (const gchar *backend, int count, const gchar *dir, OperationSpec *os, GError **error);

void videoob_search (const gchar *backend, int count, const gchar *pattern, OperationSpec *os, GError **error);

void videoob_info (const gchar *backend, const gchar *uri, GCancellable *cancellable, GrlSourceResolveSpec *rs, GError **error);
              
#endif /* _GRL_VIDEOOB_SOURCE_H_ */
