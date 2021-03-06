/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * glade-inspector.h
 *
 * Copyright (C) 2001 Ximian, Inc.
 * Copyright (C) 2007 Vincent Geddes
 *
 * Authors:
 *   Chema Celorio
 *   Tristan Van Berkom <tvb@gnome.org>
 *   Vincent Geddes <vincent.geddes@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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

#include <config.h>

/**
 * SECTION:glade-inspector
 * @Short_Description: A widget for inspecting objects in a #GladeProject.
 *
 * A #GladeInspector is a widget for inspecting the objects that make up a user interface. 
 *
 * An inspector is created by calling either glade_inspector_new() or glade_inspector_new_with_project(). 
 * The current project been inspected can be changed by calling glade_inspector_set_project().
 */

#include "glade.h"
#include "glade-widget.h"
#include "glade-project.h"
#include "glade-widget-adaptor.h"
#include "glade-inspector.h"
#include "glade-popup.h"
#include "glade-app.h"

#include <string.h>
#include <glib/gi18n-lib.h>

#define GLADE_INSPECTOR_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object),\
					    GLADE_TYPE_INSPECTOR,                 \
			 		    GladeInspectorPrivate))

enum
{
	PROP_0,
	PROP_PROJECT
};

enum
{
	SELECTION_CHANGED,
	ITEM_ACTIVATED,
	LAST_SIGNAL
};
					  
enum
{
	WIDGET_COLUMN,
	N_COLUMNS
};

struct _GladeInspectorPrivate
{
	GtkWidget    *view;
	GtkTreeStore *model;

	GladeProject *project;
};


static guint glade_inspector_signals[LAST_SIGNAL] = {0};


static void     glade_inspector_dispose     (GObject           *object);
static void     glade_inspector_finalize    (GObject           *object);
static void     add_columns                 (GtkTreeView       *inspector);
static void     item_activated_cb           (GtkTreeView       *view,
					     GtkTreePath       *path, 
					     GtkTreeViewColumn *column,
					     GladeInspector    *inspector);
static void     selection_changed_cb        (GtkTreeSelection  *selection,
					     GladeInspector    *inspector);			      
static gint     button_press_cb 	    (GtkWidget         *widget,
					     GdkEventButton    *event,
					     GladeInspector    *inspector);
static void     update_model                (GladeInspector    *inspector);
static void     disconnect_project_signals  (GladeInspector    *inspector,
			 	             GladeProject      *project);
static void     connect_project_signals     (GladeInspector    *inspector,
			 	             GladeProject      *project);

G_DEFINE_TYPE (GladeInspector, glade_inspector, GTK_TYPE_VBOX)


static void
glade_inspector_set_property (GObject        *object,
			      guint           property_id,
		              const GValue   *value,
			      GParamSpec     *pspec)
{
	GladeInspector *inspector = GLADE_INSPECTOR (object);

	switch (property_id)
	{
		case PROP_PROJECT:
			glade_inspector_set_project (inspector, g_value_get_object (value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;			
	}		
}

static void
glade_inspector_get_property (GObject        *object,
                              guint           property_id,
                              GValue         *value,
                              GParamSpec     *pspec)
{
	GladeInspector *inspector = GLADE_INSPECTOR (object);

	switch (property_id)
	{
		case PROP_PROJECT:
			g_value_set_object (value, glade_inspector_get_project (inspector));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;			
	}
}

static void
inspector_notify_cb (GObject	*object,
		     GParamSpec	*pspec)
{
	update_model (GLADE_INSPECTOR (object));
}

static void
glade_inspector_class_init (GladeInspectorClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);
	
	object_class->dispose      = glade_inspector_dispose;
	object_class->finalize     = glade_inspector_finalize;
	object_class->set_property = glade_inspector_set_property;
	object_class->get_property = glade_inspector_get_property;

	/**
	 * GladeInspector::selection-changed:
	 * @inspector: the object which received the signal
	 *
	 * Emitted when the selection changes in the GladeInspector.
	 */
	glade_inspector_signals[SELECTION_CHANGED] =
		g_signal_new ("selection-changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GladeInspectorClass, selection_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);

	/**
	 * GladeInspector::item-activated:
	 * @inspector: the object which received the signal
	 *
	 * Emitted when a item is activated in the GladeInspector.
	 */
	glade_inspector_signals[ITEM_ACTIVATED] =
		g_signal_new ("item-activated",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GladeInspectorClass, item_activated),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);
			      
	g_object_class_install_property (object_class,
					 PROP_PROJECT,
					 g_param_spec_object ("project",
							      _("Project"),
							      _("The project being inspected"),
							      GLADE_TYPE_PROJECT,
							      G_PARAM_READABLE | G_PARAM_WRITABLE));
	
	g_type_class_add_private (klass, sizeof (GladeInspectorPrivate));
}

static void
glade_inspector_init (GladeInspector *inspector)
{
	GladeInspectorPrivate *priv;
	GtkWidget             *sw;
	GtkTreeSelection      *selection;
	
	inspector->priv = priv = GLADE_INSPECTOR_GET_PRIVATE (inspector);

	gtk_widget_push_composite_child ();
	
	priv->project = NULL;
	priv->view = gtk_tree_view_new ();

	priv->model = gtk_tree_store_new (N_COLUMNS, G_TYPE_POINTER);
	gtk_tree_view_set_model (GTK_TREE_VIEW (priv->view), GTK_TREE_MODEL (priv->model));
	g_object_unref (G_OBJECT (priv->model));
 
	add_columns (GTK_TREE_VIEW (priv->view));

	g_signal_connect (G_OBJECT (priv->view), "row-activated",
			  G_CALLBACK (item_activated_cb), inspector);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->view));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);
	g_signal_connect (G_OBJECT (selection), "changed",
			  G_CALLBACK (selection_changed_cb), inspector);

	/* popup menu */
	g_signal_connect (G_OBJECT (priv->view), "button-press-event",
			  G_CALLBACK (button_press_cb), inspector);

	g_signal_connect (G_OBJECT (inspector), "notify::project",
			  G_CALLBACK (inspector_notify_cb),
			  NULL);
	
	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_IN);
	gtk_container_add (GTK_CONTAINER (sw), priv->view);
	gtk_box_pack_start (GTK_BOX (inspector), sw, TRUE, TRUE, 0);
	
	gtk_widget_show (priv->view);
	gtk_widget_show (sw);	
	
	gtk_widget_pop_composite_child ();	
}


static void
glade_inspector_dispose (GObject *object)
{
	GladeInspectorPrivate *priv = GLADE_INSPECTOR_GET_PRIVATE (object);
	
	if (priv->project)
	{
		disconnect_project_signals (GLADE_INSPECTOR (object), priv->project);
		g_object_unref (priv->project);
		priv->project = NULL;
	}
	
	G_OBJECT_CLASS (glade_inspector_parent_class)->dispose (object);
}

static void
glade_inspector_finalize (GObject *object)
{	
	G_OBJECT_CLASS (glade_inspector_parent_class)->finalize (object);
}

static void
fill_model (GtkTreeStore *model,
	    const GList  *widgets,
	    GtkTreeIter  *parent_iter)
{
	GList *children, *l;
	GtkTreeIter       iter;

	for (l = (GList *) widgets; l; l = l->next)
	{
		GladeWidget *widget;
		
		widget = glade_widget_get_from_gobject ((GObject *) l->data);

		if (widget != NULL)
		{
			gtk_tree_store_append (model, &iter, parent_iter);
			gtk_tree_store_set    (model, &iter, WIDGET_COLUMN, widget, -1);

			children = glade_widget_adaptor_get_children (widget->adaptor, widget->object);	
		
			if (children != NULL)
			{
				GtkTreeIter *copy = NULL;

				copy = gtk_tree_iter_copy (&iter);
				
				fill_model (model, children, copy);
				
				gtk_tree_iter_free (copy);

				g_list_free (children);
			}
		}
	}
}

static void
update_model (GladeInspector *inspector)
{
	GladeInspectorPrivate *priv = inspector->priv;
	GList *l, *toplevels = NULL;
	
	gtk_tree_store_clear (priv->model);

	if (!priv->project)
		return;

	/* make a list of only the toplevel widgets */
	for (l = (GList *) glade_project_get_objects (priv->project); l; l = l->next)
	{
		GObject     *object  = G_OBJECT (l->data);
		GladeWidget *gwidget = glade_widget_get_from_gobject (object);
		g_assert (gwidget);

		if (gwidget->parent == NULL)
			toplevels = g_list_prepend (toplevels, object);
	}

	/* recursively fill model */
	fill_model (priv->model, toplevels, NULL);
	g_list_free (toplevels);
}

static void
project_add_widget_cb (GladeProject   *project,
		       GladeWidget    *widget,
		       GladeInspector *inspector)
{
	GladeWidget *parent_widget;
	GtkTreeIter widget_iter, *parent_iter = NULL;
	GList *l;

	parent_widget = glade_widget_get_parent (widget);
	if (parent_widget != NULL)
		parent_iter = glade_util_find_iter_by_widget (GTK_TREE_MODEL (inspector->priv->model),
							      parent_widget,
							      WIDGET_COLUMN);
	/* we have to add parents first, then children */
	if (!parent_iter && parent_widget)
		return;
	
	gtk_tree_store_append (inspector->priv->model, &widget_iter, parent_iter);	
	gtk_tree_store_set    (inspector->priv->model, &widget_iter, WIDGET_COLUMN, widget, -1);
	
	fill_model (inspector->priv->model,
		    l = glade_widget_adaptor_get_children (widget->adaptor, widget->object),
		    &widget_iter);

	g_list_free (l);
}

static void
project_remove_widget_cb (GladeProject   *project,
			  GladeWidget    *widget,
			  GladeInspector *inspector)
{
	GtkTreeIter *iter;

	iter = glade_util_find_iter_by_widget (GTK_TREE_MODEL (inspector->priv->model),
					       widget,
					       WIDGET_COLUMN);
	if (iter)
	{
		gtk_tree_store_remove (inspector->priv->model, iter);
		gtk_tree_iter_free (iter);
	}
}

static void
project_widget_name_changed_cb (GladeProject   *project,
			        GladeWidget    *widget,
				GladeInspector *inspector)
{
	GtkTreeModel *model;
	GtkTreeIter  *iter;
	GtkTreePath  *path;

	model = GTK_TREE_MODEL (inspector->priv->model);

	iter = glade_util_find_iter_by_widget (model, widget, WIDGET_COLUMN);

	if (iter)
	{
		path = gtk_tree_model_get_path (model, iter);
		gtk_tree_model_row_changed (model, path, iter);
		gtk_tree_iter_free (iter);
	}
}

static void
project_selection_changed_cb (GladeProject     *project,
			      GladeInspector   *inspector)
{
	GladeWidget      *widget;
	GtkTreeSelection *selection;
	GtkTreeModel     *model;
	GtkTreeIter      *iter;
	GtkTreePath      *path, *ancestor_path;
	GList            *list;
	
	g_return_if_fail (GLADE_IS_INSPECTOR (inspector));
	g_return_if_fail (GLADE_IS_PROJECT (project));
	g_return_if_fail (inspector->priv->project == project);

	g_signal_handlers_block_by_func (gtk_tree_view_get_selection (GTK_TREE_VIEW (inspector->priv->view)),
					 G_CALLBACK (selection_changed_cb),
					 inspector);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (inspector->priv->view));
	g_return_if_fail (selection != NULL);
	
	model = GTK_TREE_MODEL (inspector->priv->model);

	gtk_tree_selection_unselect_all (selection);

	for (list = glade_project_selection_get (project);
	     list && list->data; list = list->next)
	{	
		if ((widget = glade_widget_get_from_gobject
		     (G_OBJECT (list->data))) != NULL)
		{
			if ((iter = glade_util_find_iter_by_widget 
			     (model, widget, WIDGET_COLUMN)) != NULL)
			{
				path = gtk_tree_model_get_path (model, iter);
				ancestor_path = gtk_tree_path_copy (path);
							
				/* expand parent node */ 
				if (gtk_tree_path_up (ancestor_path))
					gtk_tree_view_expand_to_path (GTK_TREE_VIEW (inspector->priv->view), ancestor_path);			
			
				gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (inspector->priv->view),
							      path, 
							      NULL,
							      TRUE,
							      0.5,
							      0);
			
				gtk_tree_selection_select_iter (selection, iter);				
				
				gtk_tree_iter_free (iter);
				gtk_tree_path_free (path);
				gtk_tree_path_free (ancestor_path);
			}
		}
	}
	
	g_signal_handlers_unblock_by_func (gtk_tree_view_get_selection (GTK_TREE_VIEW (inspector->priv->view)),
					   G_CALLBACK (selection_changed_cb),
					   inspector);
}

static void
selection_foreach_func (GtkTreeModel *model, 
		        GtkTreePath  *path,
		        GtkTreeIter  *iter, 
		        gpointer      data)
{
	GladeWidget *widget;
	
	gtk_tree_model_get (model, iter, WIDGET_COLUMN, &widget, -1);
	
	glade_app_selection_add (glade_widget_get_object (widget), FALSE);
}

static void
selection_changed_cb (GtkTreeSelection *selection,
		      GladeInspector   *inspector)
{
	g_signal_handlers_block_by_func (inspector->priv->project,
					 G_CALLBACK (project_selection_changed_cb),
					 inspector); 

	glade_app_selection_clear (FALSE);
	
	gtk_tree_selection_selected_foreach (selection,
					     selection_foreach_func,
					     inspector);
	glade_app_selection_changed ();
	
	g_signal_handlers_unblock_by_func (inspector->priv->project,
					   G_CALLBACK (project_selection_changed_cb),
					   inspector);

	g_signal_emit_by_name (inspector, "selection-changed");
}

static void
item_activated_cb (GtkTreeView *view,
		   GtkTreePath *path, 
		   GtkTreeViewColumn *column,
		   GladeInspector *inspector)
{	
	g_signal_emit_by_name (inspector, "item-activated");
}

static gint
button_press_cb (GtkWidget      *widget,
		 GdkEventButton *event,
		 GladeInspector *inspector)
{
	GtkTreeView      *view = GTK_TREE_VIEW (widget);
	GtkTreePath      *path      = NULL;
	gboolean          handled   = FALSE;

	if (event->window == gtk_tree_view_get_bin_window (view) &&
	    gtk_tree_view_get_path_at_pos (view, (gint) event->x, (gint) event->y,
					   &path, NULL, 
					   NULL, NULL) && path != NULL)
	{
		GtkTreeIter  iter;
		GladeWidget *widget = NULL;
		if (gtk_tree_model_get_iter (GTK_TREE_MODEL (inspector->priv->model),
					     &iter, path))
		{
			/* now we can obtain the widget from the iter.
			 */
			gtk_tree_model_get (GTK_TREE_MODEL (inspector->priv->model), &iter,
					    WIDGET_COLUMN, &widget, -1);
			if (widget != NULL && event->button == 3)
			{
				glade_popup_widget_pop (widget, event, FALSE);
				handled = TRUE;
			}
			gtk_tree_path_free (path);
		}
	}
	return handled;
}

typedef enum 
{
	CELL_ICON,
	CELL_NAME,
	CELL_MISC
} CellType;

static void
glade_inspector_cell_function (GtkTreeViewColumn *tree_column,
			       GtkCellRenderer   *cell,
			       GtkTreeModel      *tree_model,
			       GtkTreeIter       *iter,
			       gpointer           data)
{
	CellType     type = GPOINTER_TO_INT (data);
	GladeWidget *widget;
	gchar       *icon_name, *text = NULL, *child_type;

	gtk_tree_model_get (tree_model, iter, WIDGET_COLUMN, &widget, -1);

	/* The cell exists, but no widget has been associated with it */
	if (!GLADE_IS_WIDGET (widget))
		return;

	g_return_if_fail (widget->name != NULL);
	g_return_if_fail (widget->adaptor != NULL);
	g_return_if_fail (widget->adaptor->name != NULL);

	switch (type) 
	{
	case CELL_ICON:
		g_object_get (widget->adaptor, "icon-name", &icon_name, NULL);
		g_object_set (G_OBJECT (cell), "icon-name", icon_name, NULL);
		g_free (icon_name);
		break;
		
	case CELL_NAME:
		g_object_set (G_OBJECT (cell), "text", widget->name, NULL);
		break;
	case CELL_MISC:
		/* special child type / internal child */
		if (glade_widget_get_internal (widget) != NULL)
			text = g_strdup_printf (_("(internal %s)"),  
						glade_widget_get_internal (widget));
		else if ((child_type = g_object_get_data (glade_widget_get_object (widget),
							  "special-child-type")) != NULL)
			text = g_strdup_printf (_("(%s child)"), child_type);

		g_object_set (G_OBJECT (cell), "text", text ? text : " ", NULL);
		if (text) g_free (text);
		break;		
	default:
		break;
	}
}

static gboolean
glade_inspector_search_func (GtkTreeModel *model,
			     gint column,
			     const gchar *key,
			     GtkTreeIter *iter,
			     gpointer search_data)
{
	GladeWidget *widget;

	gtk_tree_model_get (model, iter, WIDGET_COLUMN, &widget, -1);
	
	if (!widget)
		return TRUE;
	
	g_return_val_if_fail (widget->name != NULL, TRUE);
	
	return ! g_str_has_prefix (widget->name, key);
}

static void
add_columns (GtkTreeView *view)
{
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;

	column = gtk_tree_view_column_new ();

	renderer = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_set_cell_data_func (column, renderer,
						 glade_inspector_cell_function,
						 GINT_TO_POINTER (CELL_ICON), NULL);

	renderer = gtk_cell_renderer_text_new ();
	g_object_set (G_OBJECT (renderer), 
		      "xpad", 6, NULL);
	gtk_tree_view_column_pack_start (column, renderer, TRUE);
	gtk_tree_view_column_set_cell_data_func (column, renderer,
						 glade_inspector_cell_function,
						 GINT_TO_POINTER (CELL_NAME), NULL);	
	
	renderer = gtk_cell_renderer_text_new ();
	g_object_set (G_OBJECT (renderer), 
		      "style", PANGO_STYLE_ITALIC,
		      "foreground", "Gray", NULL);
	gtk_tree_view_column_pack_start (column, renderer, TRUE);
	gtk_tree_view_column_set_cell_data_func (column, renderer,
						 glade_inspector_cell_function,
						 GINT_TO_POINTER (CELL_MISC), NULL);
	
	gtk_tree_view_append_column (view, column);	
	
	/* Set search column */
	gtk_tree_view_set_search_equal_func (view, glade_inspector_search_func, NULL, NULL);
	gtk_tree_view_set_enable_search (view, TRUE);
	gtk_tree_view_set_search_column (view, WIDGET_COLUMN);
	
	gtk_tree_view_set_headers_visible (view, FALSE);
}

static void
disconnect_project_signals (GladeInspector *inspector,
			    GladeProject   *project)
{
	g_signal_handlers_disconnect_by_func (G_OBJECT (project),
					      G_CALLBACK (project_add_widget_cb),
					      inspector);
	g_signal_handlers_disconnect_by_func (G_OBJECT (project),
					      G_CALLBACK (project_remove_widget_cb),
					      inspector);
	g_signal_handlers_disconnect_by_func (G_OBJECT (project),
					      G_CALLBACK (project_widget_name_changed_cb),
					      inspector);
	g_signal_handlers_disconnect_by_func (G_OBJECT (project),
					      G_CALLBACK (project_selection_changed_cb),
					      inspector);
}

static void
connect_project_signals (GladeInspector *inspector,
			 GladeProject   *project)
{
	g_signal_connect (G_OBJECT (project), "add-widget",
			  G_CALLBACK (project_add_widget_cb),
			  inspector);
	g_signal_connect (G_OBJECT (project), "remove-widget",
			  G_CALLBACK (project_remove_widget_cb),
			  inspector);
	g_signal_connect (G_OBJECT (project), "widget-name-changed",
			  G_CALLBACK (project_widget_name_changed_cb),
			  inspector);
	g_signal_connect (G_OBJECT (project), "selection-changed",
			  G_CALLBACK (project_selection_changed_cb),
			  inspector);
}

/**
 * glade_inspector_set_project:
 * @inspector: a #GladeInspector
 * @project: a #GladeProject
 *
 * Sets the current project of @inspector to @project. To unset the current
 * project, pass %NULL for @project.
 */
void
glade_inspector_set_project (GladeInspector *inspector,
			     GladeProject   *project)
{
	g_return_if_fail (GLADE_IS_INSPECTOR (inspector));
	g_return_if_fail (GLADE_IS_PROJECT (project) || project == NULL);

	if (inspector->priv->project)
	{
		disconnect_project_signals (inspector, inspector->priv->project);
		g_object_unref (inspector->priv->project);
		inspector->priv->project = NULL;
	}
	
	if (project)
	{		
		inspector->priv->project = project;
		g_object_ref (inspector->priv->project);
		connect_project_signals (inspector, inspector->priv->project);		
	}

	g_object_notify (G_OBJECT (inspector), "project");
}

/**
 * glade_inspector_get_project:
 * @inspector: a #GladeInspector
 * 
 * Note that the method does not ref the returned #GladeProject. 
 *
 * Returns: A #GladeProject
 */
GladeProject *
glade_inspector_get_project (GladeInspector *inspector)
{
	g_return_val_if_fail (GLADE_IS_INSPECTOR (inspector), NULL);

	return inspector->priv->project;
}

/**
 * glade_inspector_get_selected_items:
 * @inspector: a #GladeInspector
 * 
 * Returns the selected items in the inspector. 
 *
 * Returns: A #GList
 */
GList *
glade_inspector_get_selected_items (GladeInspector *inspector)
{
	GtkTreeSelection* selection;
	GList *items = NULL, *paths;
	
	
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (inspector->priv->view));
	
	paths = gtk_tree_selection_get_selected_rows (selection, NULL);
						      
	for (; paths; paths = paths->next)
	{
		GtkTreeIter iter;
		GtkTreePath *path = (GtkTreePath *) paths->data;
		GladeWidget *widget = NULL;
		
		gtk_tree_model_get_iter (GTK_TREE_MODEL (inspector->priv->model), &iter, path);
		gtk_tree_model_get (GTK_TREE_MODEL (inspector->priv->model), &iter, WIDGET_COLUMN, &widget, -1);
		
		items = g_list_prepend (items, widget);
	}
	
	g_list_foreach (paths, (GFunc) gtk_tree_path_free, NULL);
	g_list_free (paths);

	return items;
}

/**
 * glade_inspector_new:
 * 
 * Creates a new #GladeInspector
 * 
 * Returns: a new #GladeInspector
 */
GtkWidget *
glade_inspector_new (void)
{
	return g_object_new (GLADE_TYPE_INSPECTOR, NULL);
}

/**
 * glade_inspector_new_with_project:
 * @project: a #GladeProject
 *
 * Creates a new #GladeInspector with @project
 * 
 * Returns: a new #GladeInspector
 */
GtkWidget *
glade_inspector_new_with_project (GladeProject *project)
{
	g_return_val_if_fail (GLADE_IS_PROJECT (project), NULL);

	return g_object_new (GLADE_TYPE_INSPECTOR,
			     "project", project,
			     NULL);
}


