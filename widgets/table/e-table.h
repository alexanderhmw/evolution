/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _E_TABLE_H_
#define _E_TABLE_H_

#include <libgnomeui/gnome-canvas.h>
#include <gtk/gtktable.h>
#include <gnome-xml/tree.h>
#include <gal/e-table/e-table-model.h>
#include <gal/e-table/e-table-header.h>
#include <gal/e-table/e-table-group.h>
#include <gal/e-table/e-table-sort-info.h>
#include <gal/e-table/e-table-item.h>
#include <gal/e-table/e-table-selection-model.h>
#include <gal/e-table/e-table-extras.h>
#include <gal/e-table/e-table-specification.h>
#include <gal/widgets/e-printable.h>
#include <gal/e-table/e-table-state.h>

BEGIN_GNOME_DECLS

#define E_TABLE_TYPE        (e_table_get_type ())
#define E_TABLE(o)          (GTK_CHECK_CAST ((o), E_TABLE_TYPE, ETable))
#define E_TABLE_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_TABLE_TYPE, ETableClass))
#define E_IS_TABLE(o)       (GTK_CHECK_TYPE ((o), E_TABLE_TYPE))
#define E_IS_TABLE_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_TABLE_TYPE))

typedef struct _ETableDragSourceSite ETableDragSourceSite;

typedef enum {
	E_TABLE_CURSOR_LOC_NONE = 0,
	E_TABLE_CURSOR_LOC_ETCTA = 1 << 0,
	E_TABLE_CURSOR_LOC_TABLE = 1 << 1,
} ETableCursorLoc;

typedef struct {
	GtkTable parent;

	ETableModel *model;

	ETableHeader *full_header, *header;

	GnomeCanvasItem *canvas_vbox;
	ETableGroup  *group;

	ETableSortInfo *sort_info;
	ETableSorter   *sorter;

	ETableSelectionModel *selection;
	ETableCursorLoc cursor_loc;
	ETableSpecification *spec;

	int table_model_change_id;
	int table_row_change_id;
	int table_cell_change_id;
	int table_rows_inserted_id;
	int table_rows_deleted_id;

	int group_info_change_id;

	int reflow_idle_id;

	GnomeCanvas *header_canvas, *table_canvas;

	GnomeCanvasItem *header_item, *root;

	GnomeCanvasItem *white_item;

	gint length_threshold;

	gint rebuild_idle_id;
	guint need_rebuild:1;

	/*
	 * Configuration settings
	 */
	guint alternating_row_colors : 1;
	guint horizontal_draw_grid : 1;
	guint vertical_draw_grid : 1;
	guint draw_focus : 1;
	guint row_selection_active : 1;

	guint horizontal_scrolling : 1;

	guint is_grouped : 1;
	
	char *click_to_add_message;
	GnomeCanvasItem *click_to_add;
	gboolean use_click_to_add;

	ECursorMode cursor_mode;

	int drag_get_data_row;
	int drag_get_data_col;

	int drop_row;
	int drop_col;
	GnomeCanvasItem *drop_highlight;

	int drag_row;
	int drag_col;
	ETableDragSourceSite *site;
	
	int drag_source_button_press_event_id;
	int drag_source_motion_notify_event_id;
} ETable;

typedef struct {
	GtkTableClass parent_class;

	void        (*cursor_change)      (ETable *et, int row);
	void        (*cursor_activated)   (ETable *et, int row);
	void        (*selection_change)   (ETable *et);
	void        (*double_click)       (ETable *et, int row, int col, GdkEvent *event);
	gint        (*right_click)        (ETable *et, int row, int col, GdkEvent *event);
	gint        (*click)              (ETable *et, int row, int col, GdkEvent *event);
	gint        (*key_press)          (ETable *et, int row, int col, GdkEvent *event);

	void  (*set_scroll_adjustments)   (ETable	 *table,
					   GtkAdjustment *hadjustment,
					   GtkAdjustment *vadjustment);

	/* Source side drag signals */
	void (* table_drag_begin)	           (ETable	       *table,
						    int                 row,
						    int                 col,
						    GdkDragContext     *context);
	void (* table_drag_end)	           (ETable	       *table,
					    int                 row,
					    int                 col,
					    GdkDragContext     *context);
	void (* table_drag_data_get)             (ETable             *table,
						  int                 row,
						  int                 col,
						  GdkDragContext     *context,
						  GtkSelectionData   *selection_data,
						  guint               info,
						  guint               time);
	void (* table_drag_data_delete)          (ETable	       *table,
						  int                 row,
						  int                 col,
						  GdkDragContext     *context);
	
	/* Target side drag signals */	   
	void (* table_drag_leave)	           (ETable	       *table,
						    int                 row,
						    int                 col,
						    GdkDragContext     *context,
						    guint               time);
	gboolean (* table_drag_motion)           (ETable	       *table,
						  int                 row,
						  int                 col,
						  GdkDragContext     *context,
						  gint                x,
						  gint                y,
						  guint               time);
	gboolean (* table_drag_drop)             (ETable	       *table,
						  int                 row,
						  int                 col,
						  GdkDragContext     *context,
						  gint                x,
						  gint                y,
						  guint               time);
	void (* table_drag_data_received)        (ETable             *table,
						  int                 row,
						  int                 col,
						  GdkDragContext     *context,
						  gint                x,
						  gint                y,
						  GtkSelectionData   *selection_data,
						  guint               info,
						  guint               time);
} ETableClass;

GtkType         e_table_get_type                  (void);

ETable         *e_table_construct                 (ETable               *e_table,
						   ETableModel          *etm,
						   ETableExtras         *ete,
						   const char           *spec,
						   const char           *state);
GtkWidget      *e_table_new                       (ETableModel          *etm,
						   ETableExtras         *ete,
						   const char           *spec,
						   const char           *state);

/* Create an ETable using files. */
ETable         *e_table_construct_from_spec_file  (ETable               *e_table,
						   ETableModel          *etm,
						   ETableExtras         *ete,
						   const char           *spec_fn,
						   const char           *state_fn);
GtkWidget      *e_table_new_from_spec_file        (ETableModel          *etm,
						   ETableExtras         *ete,
						   const char           *spec_fn,
						   const char           *state_fn);

/* To save the state */
gchar          *e_table_get_state                 (ETable               *e_table);
void            e_table_save_state                (ETable               *e_table,
						   const gchar          *filename);
ETableState    *e_table_get_state_object          (ETable               *e_table);

/* note that it is more efficient to provide the state at creation time */
void            e_table_set_state                 (ETable               *e_table,
						   const gchar          *state);
void            e_table_set_state_object          (ETable               *e_table,
						   ETableState          *state);
void            e_table_load_state                (ETable               *e_table,
						   const gchar          *filename);

void            e_table_set_cursor_row            (ETable               *e_table,
						   int                   row);

/* -1 means we don't have the cursor. */
int             e_table_get_cursor_row            (ETable               *e_table);
void            e_table_selected_row_foreach      (ETable               *e_table,
						   EForeachFunc          callback,
						   gpointer              closure);
gint            e_table_selected_count            (ETable               *e_table);
EPrintable     *e_table_get_printable             (ETable               *e_table);

gint            e_table_get_next_row              (ETable               *e_table,
						   gint                  model_row);
gint            e_table_get_prev_row              (ETable               *e_table,
						   gint                  model_row);

gint            e_table_model_to_view_row         (ETable               *e_table,
						   gint                  model_row);
gint            e_table_view_to_model_row         (ETable               *e_table,
						   gint                  view_row);
void            e_table_get_cell_at               (ETable *table,
						   int x, int y,
						   int *row_return, int *col_return);

void            e_table_get_cell_geometry         (ETable *table,
						   int row, int col,
						   int *x_return, int *y_return,
						   int *width_return, int *height_return);

/* Drag & drop stuff. */
/* Target */
void            e_table_drag_get_data             (ETable               *table,
						   int                   row,
						   int                   col,
						   GdkDragContext       *context,
						   GdkAtom               target,
						   guint32               time);
void            e_table_drag_highlight            (ETable               *table,
						   int                   row,
						   int                   col); /* col == -1 to highlight entire row. */
void            e_table_drag_unhighlight          (ETable               *table);
void            e_table_drag_dest_set             (ETable               *table,
						   GtkDestDefaults       flags,
						   const GtkTargetEntry *targets,
						   gint                  n_targets,
						   GdkDragAction         actions);
void            e_table_drag_dest_set_proxy       (ETable               *table,
						   GdkWindow            *proxy_window,
						   GdkDragProtocol       protocol,
						   gboolean              use_coordinates);

/* There probably should be functions for setting the targets
 * as a GtkTargetList
 */
void            e_table_drag_dest_unset           (GtkWidget            *widget);

/* Source side */
void            e_table_drag_source_set           (ETable               *table,
						   GdkModifierType       start_button_mask,
						   const GtkTargetEntry *targets,
						   gint                  n_targets,
						   GdkDragAction         actions);
void            e_table_drag_source_unset         (ETable               *table);

/* There probably should be functions for setting the targets
 * as a GtkTargetList
 */
GdkDragContext *e_table_drag_begin                (ETable               *table,
						   int                   row,
						   int                   col,
						   GtkTargetList        *targets,
						   GdkDragAction         actions,
						   gint                  button,
						   GdkEvent             *event);

/* selection stuff */
void            e_table_select_all                (ETable               *table);
void            e_table_invert_selection          (ETable               *table);

END_GNOME_DECLS

#endif /* _E_TABLE_H_ */

