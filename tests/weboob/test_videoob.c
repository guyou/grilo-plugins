/*
 * Copyright (C) 2013 Igalia S.L.
 *
 * Author: Juan A. Suarez Romero <jasuarez@igalia.com>
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

/* To see debug messages: G_MESSAGES_DEBUG=all ./test_videoob */

#include <stdio.h>

#include <grilo.h>

#include <../../src/weboob/videoob.h>

void
test_ls ()
{
  GList *medias;
  GError *error;
  error = NULL;

  medias = videoob_ls (NULL, -1, "arte-latest", &error);
  printf ("ls: %d\n", g_list_length (medias));

  medias = videoob_ls (NULL, 5, "latest", &error);
  printf ("ls: %d\n", g_list_length (medias));
  
  medias = videoob_ls (NULL, 5, NULL, &error);
  printf ("ls: %d\n", g_list_length (medias));
  
  if (error != NULL)
  {
    g_error("ls failed: %s", error->message);
  }
}

int
main (int argc, char **argv)
{
  test_ls ();
}
