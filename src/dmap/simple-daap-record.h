/*
 *  Database record class for DAAP sharing
 *
 *  Copyright (C) 2008 W. Michael Petullo <mike@flyn.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef __SIMPLE_DAAP_RECORD
#define __SIMPLE_DAAP_RECORD

#include <libdmapsharing/dmap.h>

G_BEGIN_DECLS

#define TYPE_SIMPLE_DAAP_RECORD                 \
  (simple_daap_record_get_type ())

#define SIMPLE_DAAP_RECORD(o)                            \
  (G_TYPE_CHECK_INSTANCE_CAST((o),                       \
                              TYPE_SIMPLE_DAAP_RECORD,   \
                              SimpleDAAPRecord))

#define SIMPLE_DAAP_RECORD_CLASS(k)                   \
  (G_TYPE_CHECK_CLASS_CAST((k),                       \
                           TYPE_SIMPLE_DAAP_RECORD,   \
                           SimpleDAAPRecordClass))

#define IS_SIMPLE_DAAP_RECORD(o)                         \
  (G_TYPE_CHECK_INSTANCE_TYPE((o),                       \
                              TYPE_SIMPLE_DAAP_RECORD))

#define IS_SIMPLE_DAAP_RECORD_CLASS(k)                      \
  (G_TYPE_CHECK_CLASS_TYPE((k),                             \
                           TYPE_SIMPLE_DAAP_RECORD_CLASS))

#define SIMPLE_DAAP_RECORD_GET_CLASS(o)               \
  (G_TYPE_INSTANCE_GET_CLASS((o),                     \
                             TYPE_SIMPLE_DAAP_RECORD, \
                             SimpleDAAPRecordClass))

#define SIMPLE_DAAP_RECORD_GET_PRIVATE(o)                \
  (G_TYPE_INSTANCE_GET_PRIVATE((o),                      \
                               TYPE_SIMPLE_DAAP_RECORD,  \
                               SimpleDAAPRecordPrivate))

typedef struct SimpleDAAPRecordPrivate SimpleDAAPRecordPrivate;

typedef struct {
  GObject parent;
  SimpleDAAPRecordPrivate *priv;
} SimpleDAAPRecord;

typedef struct {
  GObjectClass parent;
} SimpleDAAPRecordClass;

GType simple_daap_record_get_type (void);

SimpleDAAPRecord *simple_daap_record_new    (void);
GInputStream     *simple_daap_record_read   (DAAPRecord *record, GError **error);
gint              simple_daap_record_get_id (DAAPRecord *record);

#endif /* __SIMPLE_DAAP_RECORD */

G_END_DECLS
