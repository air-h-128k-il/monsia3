/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2001 Ximian, Inc.
 * Copyright (C) 2004 Imendio AB
 * Copyright (C) 2007 The GNOME Foundation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef __GLADE_CATALOG_H__
#define __GLADE_CATALOG_H__

#include <glib.h>

G_BEGIN_DECLS

#define GLADE_CATALOG(c)    ((GladeCatalog *) c)
#define GLADE_IS_CATALOG(c) (c != NULL)

typedef struct _GladeCatalog GladeCatalog;

#define GLADE_WIDGET_GROUP(g)    ((GladeWidgetGroup *) g)
#define GLADE_IS_WIDGET_GROUP(g) (g != NULL)

typedef struct _GladeWidgetGroup GladeWidgetGroup;


const GList  *glade_catalog_load_all                (void);

const gchar  *glade_catalog_get_name                (GladeCatalog     *catalog);

GList        *glade_catalog_get_widget_groups       (GladeCatalog     *catalog);

GList        *glade_catalog_get_adaptors            (GladeCatalog     *catalog);

gboolean      glade_catalog_is_loaded               (const gchar      *name);

void          glade_catalog_destroy_all             (void);


const gchar  *glade_widget_group_get_name           (GladeWidgetGroup *group);

const gchar  *glade_widget_group_get_title          (GladeWidgetGroup *group);

gboolean      glade_widget_group_get_expanded       (GladeWidgetGroup *group);

const GList  *glade_widget_group_get_adaptors       (GladeWidgetGroup *group);

G_END_DECLS

#endif /* __GLADE_CATALOG_H__ */
