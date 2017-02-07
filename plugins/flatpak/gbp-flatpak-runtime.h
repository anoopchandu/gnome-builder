/* gbp-flatpak-runtime.h
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef GBP_FLATPAK_RUNTIME_H
#define GBP_FLATPAK_RUNTIME_H

#include <ide.h>

G_BEGIN_DECLS

#define GBP_TYPE_FLATPAK_RUNTIME (gbp_flatpak_runtime_get_type())

G_DECLARE_FINAL_TYPE (GbpFlatpakRuntime, gbp_flatpak_runtime, GBP, FLATPAK_RUNTIME, IdeRuntime)

#define FLATPAK_REPO_NAME "gnome-builder-builds"

const gchar         *gbp_flatpak_runtime_get_branch   (GbpFlatpakRuntime *self);
void                 gbp_flatpak_runtime_set_branch   (GbpFlatpakRuntime *self,
                                                       const gchar *branch);
const gchar         *gbp_flatpak_runtime_get_platform (GbpFlatpakRuntime *self);
void                 gbp_flatpak_runtime_set_platform (GbpFlatpakRuntime *self,
                                                       const gchar *platform);
const gchar         *gbp_flatpak_runtime_get_sdk      (GbpFlatpakRuntime *self);
void                 gbp_flatpak_runtime_set_sdk      (GbpFlatpakRuntime *self,
                                                       const gchar *sdk);

G_END_DECLS

#endif /* GBP_FLATPAK_RUNTIME_H */
