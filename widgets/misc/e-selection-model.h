/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _E_SELECTION_MODEL_H_
#define _E_SELECTION_MODEL_H_

#include <gtk/gtkobject.h>
#include <gal/util/e-sorter.h>
#include <gdk/gdktypes.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define E_SELECTION_MODEL_TYPE        (e_selection_model_get_type ())
#define E_SELECTION_MODEL(o)          (GTK_CHECK_CAST ((o), E_SELECTION_MODEL_TYPE, ESelectionModel))
#define E_SELECTION_MODEL_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_SELECTION_MODEL_TYPE, ESelectionModelClass))
#define E_IS_SELECTION_MODEL(o)       (GTK_CHECK_TYPE ((o), E_SELECTION_MODEL_TYPE))
#define E_IS_SELECTION_MODEL_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_SELECTION_MODEL_TYPE))

typedef void (*EForeachFunc) (int model_row,
			      gpointer closure);

/* list selection modes */
typedef enum {
	E_CURSOR_LINE,
	E_CURSOR_SIMPLE,
	E_CURSOR_SPREADSHEET,
} ECursorMode;

typedef struct {
	GtkObject     base;

	ESorter *sorter;

	GtkSelectionMode mode;
	ECursorMode cursor_mode;
} ESelectionModel;

typedef struct {
	GtkObjectClass parent_class;

	/* Virtual methods */
	gboolean (*is_row_selected)     (ESelectionModel *esm, int row);
	void     (*foreach)             (ESelectionModel *esm, EForeachFunc callback, gpointer closure);
	void     (*clear)               (ESelectionModel *esm);
	gint     (*selected_count)      (ESelectionModel *esm);
	void     (*select_all)          (ESelectionModel *esm);
	void     (*invert_selection)    (ESelectionModel *esm);
	int      (*row_count)           (ESelectionModel *esm);

	/* Protected virtual methods. */
	void     (*change_one_row)      (ESelectionModel *esm, int row, gboolean on);
	void     (*change_cursor)       (ESelectionModel *esm, int row, int col);
	int      (*cursor_row)          (ESelectionModel *esm);
	int      (*cursor_col)          (ESelectionModel *esm);

	void     (*select_single_row)   (ESelectionModel *selection, int row);
	void     (*toggle_single_row)   (ESelectionModel *selection, int row);
	void     (*move_selection_end)  (ESelectionModel *selection, int row);
	void     (*set_selection_end)   (ESelectionModel *selection, int row);

	/*
	 * Signals
	 */

	void     (*cursor_changed)      (ESelectionModel *esm, int row, int col);
	void     (*cursor_activated)    (ESelectionModel *esm, int row, int col);
	void     (*selection_changed)   (ESelectionModel *esm);

} ESelectionModelClass;


GtkType   e_selection_model_get_type            (void);
void      e_selection_model_do_something        (ESelectionModel *esm,
						 guint            row,
						 guint            col,
						 GdkModifierType  state);
void      e_selection_model_maybe_do_something  (ESelectionModel *esm,
						 guint            row,
						 guint            col,
						 GdkModifierType  state);
gint      e_selection_model_key_press           (ESelectionModel *esm,
						 GdkEventKey     *key);

/* Virtual functions */
gboolean  e_selection_model_is_row_selected     (ESelectionModel *esm,
						 gint             n);
void      e_selection_model_foreach             (ESelectionModel *esm,
						 EForeachFunc     callback,
						 gpointer         closure);
void      e_selection_model_clear               (ESelectionModel *esm);
gint      e_selection_model_selected_count      (ESelectionModel *esm);
void      e_selection_model_select_all          (ESelectionModel *esm);
void      e_selection_model_invert_selection    (ESelectionModel *esm);
int       e_selection_model_row_count           (ESelectionModel *esm);


/* Private virtual Functions */
void      e_selection_model_change_one_row      (ESelectionModel *esm,
						 int              row,
						 gboolean         on);
void      e_selection_model_change_cursor       (ESelectionModel *esm,
						 int              row,
						 int              col);
int       e_selection_model_cursor_row          (ESelectionModel *esm);
int       e_selection_model_cursor_col          (ESelectionModel *esm);
void      e_selection_model_select_single_row   (ESelectionModel *selection,
						 int              row);
void      e_selection_model_toggle_single_row   (ESelectionModel *selection,
						 int              row);
void      e_selection_model_move_selection_end  (ESelectionModel *selection,
						 int              row);
void      e_selection_model_set_selection_end   (ESelectionModel *selection,
						 int              row);

/* Signals */
void      e_selection_model_cursor_changed      (ESelectionModel *selection,
						 int              row,
						 int              col);
void      e_selection_model_selection_changed   (ESelectionModel *selection);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* _E_SELECTION_MODEL_H_ */
