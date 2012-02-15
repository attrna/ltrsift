/*
  Copyright (c) 2011-2012 Sascha Kastens <sascha.kastens@studium.uni-hamburg.de>
  Copyright (c) 2011-2012 Center for Bioinformatics, University of Hamburg

  Permission to use, copy, modify, and distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include "error.h"
#include "gtk_ltr_filter.h"
#include "support.h"

static void remove_list_view_row(GtkTreeRowReference *rowref,
                                 GtkTreeModel *model)
{
  GtkTreeIter iter;
  GtkTreePath *path;

  path = gtk_tree_row_reference_get_path(rowref);
  gtk_tree_model_get_iter(model, &iter, path);

  gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
  gtk_tree_path_free(path);
}

void gtk_ltr_filter_set_ltrfams(GtkLTRFilter *ltrfilt, GtkWidget *ltrfams)
{
  ltrfilt->ltrfams = ltrfams;
}

static void
gtk_ltr_filter_edit_dialog_delete_event(GtkWidget *widget,
                                        GT_UNUSED GdkEvent *event,
                                        GtkLTRFilter *ltrfilt)
{
  gtk_widget_destroy(widget);
  ltrfilt->edit_dialog = NULL;
}

static void
gtk_ltr_filter_edit_dialog_close_clicked(GT_UNUSED GtkWidget *button,
                                          GtkLTRFilter *ltrfilt)
{
  GtkWidget *dialog;

  if (ltrfilt->cur_filename)
    g_free(ltrfilt->cur_filename);
  ltrfilt->cur_filename = NULL;
  if (gtk_text_buffer_get_modified(ltrfilt->text_buffer)) {
    dialog = gtk_message_dialog_new(GTK_WINDOW(ltrfilt),
                                    GTK_DIALOG_MODAL |
                                    GTK_DIALOG_DESTROY_WITH_PARENT,
                                    GTK_MESSAGE_INFO, GTK_BUTTONS_YES_NO,
                                    "%s",
                                    LTR_FILTER_UNSAVED_CHANGES);
    gtk_window_set_title(GTK_WINDOW(dialog), "Attention!");

    if (gtk_dialog_run(GTK_DIALOG(dialog)) != GTK_RESPONSE_YES) {
      gtk_widget_destroy(dialog);
      return;
    } else
      gtk_widget_destroy(dialog);
  }
  gtk_widget_destroy(ltrfilt->edit_dialog);
  ltrfilt->edit_dialog = NULL;
}

static gboolean save_filter_file(GtkLTRFilter *ltrfilt)
{
  GtkWidget *dialog;
  GtkTextIter start, end;
  GtScriptFilter *script_filter = NULL;
  GtError *err = gt_error_new();
  gboolean valid, result = FALSE;
  gchar *text;

  gtk_text_buffer_get_bounds(ltrfilt->text_buffer, &start, &end);
  text = gtk_text_buffer_get_text(ltrfilt->text_buffer, &start, &end, FALSE);
  script_filter = gt_script_filter_new_from_string(text, err);
  if (script_filter != NULL) {
    valid = gt_script_filter_validate(script_filter, err);
    if (valid) {
      result = g_file_set_contents(ltrfilt->cur_filename, text, -1,
                                   &ltrfilt->gerr);
      if (!result)
        error_handle(GTK_WIDGET(ltrfilt), ltrfilt->gerr);
      else
        gtk_text_buffer_set_modified(ltrfilt->text_buffer, FALSE);
    } else {
      dialog = gtk_message_dialog_new(GTK_WINDOW(ltrfilt),
                                      GTK_DIALOG_MODAL |
                                      GTK_DIALOG_DESTROY_WITH_PARENT,
                                      GTK_MESSAGE_INFO, GTK_BUTTONS_OK,
                                      "%s",
                                      LTR_FILTER_NOT_SAVED_FILE);
      gtk_window_set_title(GTK_WINDOW(dialog), "Attention!");
      gtk_dialog_run(GTK_DIALOG(dialog));
      gtk_widget_destroy(dialog);
    }
  } else {
    dialog = gtk_message_dialog_new(GTK_WINDOW(ltrfilt),
                                    GTK_DIALOG_MODAL |
                                    GTK_DIALOG_DESTROY_WITH_PARENT,
                                    GTK_MESSAGE_INFO, GTK_BUTTONS_OK,
                                    "%s",
                                    LTR_FILTER_NOT_SAVED_FILE);
    gtk_window_set_title(GTK_WINDOW(dialog), "Attention!");
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
  }
  gt_script_filter_delete(script_filter);

  return result;
}

static void
gtk_ltr_filter_edit_dialog_save_as_clicked(GT_UNUSED GtkWidget *button,
                                           GtkLTRFilter *ltrfilt)
{
  GtkWidget *fc;
  GtkTreeModel *model;
  GtkTreeIter iter;
  GtkFileFilter *lua_file_filter;
  gboolean saved;
  gchar *filename;

  fc = gtk_file_chooser_dialog_new(GUI_DIALOG_SAVE_AS,
                                   GTK_WINDOW(ltrfilt->edit_dialog),
                                   GTK_FILE_CHOOSER_ACTION_SAVE,
                                   GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                   GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT, NULL);
  gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(fc), TRUE);
  lua_file_filter = gtk_file_filter_new();
  gtk_file_filter_set_name(lua_file_filter, LUA_FILTER_PATTERN);
  gtk_file_filter_add_pattern(lua_file_filter, LUA_FILTER_PATTERN);
  gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(fc), lua_file_filter);
  if (ltrfilt->cur_filename) {
    gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(fc), ltrfilt->cur_filename);
    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(fc),
                                    g_path_get_basename(ltrfilt->cur_filename));
  } else if (ltrfilt->last_dir)
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(fc),
                                        ltrfilt->last_dir);
  else
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(fc), g_get_home_dir());

  if (gtk_dialog_run(GTK_DIALOG(fc)) == GTK_RESPONSE_ACCEPT) {
    filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(fc));
    if (!g_str_has_suffix(filename, LUA_PATTERN)) {
      ltrfilt->cur_filename = g_strconcat(filename, LUA_PATTERN, NULL);
      g_free(filename);
    } else
      ltrfilt->cur_filename = filename;
  } else {
    gtk_widget_destroy(fc);
    return;
  }
  gtk_widget_destroy(fc);
  saved = save_filter_file(ltrfilt);
  if (saved) {
    model = gtk_tree_view_get_model(GTK_TREE_VIEW(ltrfilt->list_view_all));
    if (!entry_in_list_view(model, ltrfilt->cur_filename, LTR_FILTER_LV_FILE)) {
      gtk_list_store_append(GTK_LIST_STORE(model), &iter);
      gtk_list_store_set(GTK_LIST_STORE(model), &iter,
                         LTR_FILTER_LV_FILE, ltrfilt->cur_filename, -1);
    }
  }
}

static void
gtk_ltr_filter_edit_dialog_save_clicked(GT_UNUSED GtkWidget *button,
                                        GtkLTRFilter *ltrfilt)
{
  if (ltrfilt->cur_filename == NULL)
    gtk_ltr_filter_edit_dialog_save_as_clicked(NULL, ltrfilt);
  else {
    gboolean saved;
    saved = save_filter_file(ltrfilt);
  }
}

static void create_edit_dialog(GtkLTRFilter *ltrfilt)
{
  GtkWidget *sw, *tv, *vbox, *hbox, *button;
  PangoFontDescription *font_desc;

  font_desc = pango_font_description_from_string("Courier");

  ltrfilt->edit_dialog = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(ltrfilt->edit_dialog),
                       "Edit/New filter script");
  gtk_container_set_border_width(GTK_CONTAINER(ltrfilt->edit_dialog), 5);
  tv = gtk_text_view_new();
  gtk_widget_modify_font(tv, font_desc);
  ltrfilt->text_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(tv));

  if (ltrfilt->cur_filename == NULL) {
    gtk_text_buffer_set_text(ltrfilt->text_buffer, LTR_FILTER_TEMPLATE, -1);
    gtk_text_buffer_set_modified(ltrfilt->text_buffer, TRUE);
  } else {
    gboolean result;
    gchar *text;
    result = g_file_get_contents(ltrfilt->cur_filename, &text, NULL, NULL);
    gtk_text_buffer_set_text(ltrfilt->text_buffer, text, -1);
    gtk_text_buffer_set_modified(ltrfilt->text_buffer, FALSE);
    g_free(text);
  }
  sw = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
                                 GTK_POLICY_AUTOMATIC,
                                 GTK_POLICY_AUTOMATIC);
  gtk_container_add(GTK_CONTAINER(sw), tv);
  vbox = gtk_vbox_new(FALSE, 1);
  gtk_box_pack_start(GTK_BOX(vbox), sw, TRUE, TRUE, 1);
  hbox = gtk_hbox_new(FALSE, 1);
  button = gtk_button_new_from_stock(GTK_STOCK_SAVE);
  g_signal_connect(G_OBJECT(button), "clicked",
                   G_CALLBACK(gtk_ltr_filter_edit_dialog_save_clicked),
                   ltrfilt);
  gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 1);
  button = gtk_button_new_from_stock(GTK_STOCK_SAVE_AS);
  g_signal_connect(G_OBJECT(button), "clicked",
                   G_CALLBACK(gtk_ltr_filter_edit_dialog_save_as_clicked),
                   ltrfilt);
  gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 1);
  button = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
  g_signal_connect(G_OBJECT(button), "clicked",
                   G_CALLBACK(gtk_ltr_filter_edit_dialog_close_clicked),
                   ltrfilt);
  gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 1);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 1);

  gtk_container_add(GTK_CONTAINER(ltrfilt->edit_dialog), vbox);
  gtk_window_set_transient_for(GTK_WINDOW(ltrfilt->edit_dialog),
                               GTK_WINDOW(ltrfilt));
  gtk_window_set_position(GTK_WINDOW(ltrfilt->edit_dialog),
                          GTK_WIN_POS_CENTER_ON_PARENT);
  gtk_window_set_modal(GTK_WINDOW(ltrfilt->edit_dialog), TRUE);
  gtk_window_resize(GTK_WINDOW(ltrfilt->edit_dialog), 512, 384);
  g_signal_connect(G_OBJECT(ltrfilt->edit_dialog), "delete_event",
                   G_CALLBACK(gtk_ltr_filter_edit_dialog_delete_event),
                   ltrfilt);
  gtk_widget_show_all(ltrfilt->edit_dialog);
  pango_font_description_free(font_desc);
}

static void gtk_ltr_filter_apply_clicked(GT_UNUSED GtkButton *button,
                                         GtkLTRFilter *ltrfilt)
{
  CandidateData *cdata;
  GtkTreeModel *model;
  GtkTreeIter iter;
  GtStrArray *filter_files = NULL;
  GtError *err = gt_error_new();
  GtNodeStream *array_in_stream,
               *script_filter_stream,
               *array_out_stream;
  GtArray *nodes, *filtered_nodes;
  GtGenomeNode *gn;
  gchar *filter_file;
  gint action, had_err = 0;
  unsigned long i;

  model = gtk_tree_view_get_model(GTK_TREE_VIEW(ltrfilt->list_view_sel));

  if (!gtk_tree_model_get_iter_first(model, &iter)) {
    return;
  } else {
    filter_files = gt_str_array_new();
    gtk_tree_model_get(model, &iter,
                       LTR_FILTER_LV_FILE, &filter_file, -1);
    gt_str_array_add_cstr(filter_files, filter_file);
    g_free(filter_file);
    while (gtk_tree_model_iter_next(model, &iter)) {
      gtk_tree_model_get(model, &iter,
                         LTR_FILTER_LV_FILE, &filter_file, -1);
      gt_str_array_add_cstr(filter_files, filter_file);
      g_free(filter_file);
    }
  }
  filtered_nodes = gt_array_new(sizeof (GtGenomeNode*));
  nodes = gtk_ltr_families_get_nodes(GTK_LTR_FAMILIES(ltrfilt->ltrfams));
  array_in_stream = gt_array_in_stream_new(nodes, NULL, err);
  script_filter_stream = ltrgui_script_filter_stream_new(array_in_stream,
                                                         filter_files,
                                                         NULL, err);
  array_out_stream = gt_array_out_stream_new(script_filter_stream,
                                             filtered_nodes, err);
  had_err = gt_node_stream_pull(array_out_stream, err);

  gt_node_stream_delete(script_filter_stream);
  gt_node_stream_delete(array_in_stream);
  gt_node_stream_delete(array_out_stream);

  action = gtk_combo_box_get_active(GTK_COMBO_BOX(ltrfilt->filter_action));

  switch (action) {
    case LTR_FILTER_ACTION_DELETE:
      for (i = 0; i < gt_array_size(filtered_nodes); i++) {
        gn = (GtGenomeNode*) gt_array_get(filtered_nodes, i);
        cdata = (CandidateData*) gt_genome_node_get_user_data(gn, "cdata");
        if (!cdata)
          g_warning("%s", "Programming error!");
        else {
          GtkTreePath *path;
          GtkTreeModel *model;
          GtkTreeIter iter;
          GtArray *tmp_nodes;
          gboolean valid;
          gchar *old_name, cur_name[BUFSIZ];
          if (cdata->fam_ref) {
            path = gtk_tree_row_reference_get_path(cdata->fam_ref);
            model = gtk_tree_row_reference_get_model(cdata->fam_ref);
            valid = gtk_tree_model_get_iter(model, &iter, path);
            if (!valid)
              g_warning("%s", "Programming error!");
            else {
              gtk_tree_model_get(model, &iter,
                                 LTRFAMS_FAM_LV_NODE_ARRAY, &tmp_nodes,
                                 LTRFAMS_FAM_LV_OLDNAME, &old_name,
                                 -1);
              remove_node_from_array(tmp_nodes, gn);
              g_snprintf(cur_name, BUFSIZ, "%s (%lu)", old_name,
                         gt_array_size(tmp_nodes));
              gtk_list_store_set(GTK_LIST_STORE(model), &iter,
                                 LTRFAMS_FAM_LV_CURNAME, cur_name,
                                 -1);
              g_free(old_name);
            }
          }
        }
      }
      break;
    case LTR_FILTER_ACTION_NEW_FAM:
      break;
    default:
      break;
  }

  gt_array_delete(filtered_nodes);
}

static void gtk_ltr_filter_cancel_clicked(GT_UNUSED GtkButton *button,
                                          gpointer user_data)
{
  gtk_widget_hide(GTK_WIDGET(user_data));
}

/*
static void gtk_ltr_filter_change_filter_dir(GtkFileChooser *chooser,
                                             gpointer user_data)
{
  GtkTreeView *list_view = GTK_TREE_VIEW(user_data);
  GtkTreeModel *model;
  GtkTreeIter iter;
  GtkTreeSelection *selection;
  GtError *err;
  GtScriptFilter *script_filter;
  GDir *dir;
  gboolean valid;
  gchar *path;
  const char *file;

  err = gt_error_new();
  model = gtk_tree_view_get_model(list_view);
  gtk_list_store_clear(GTK_LIST_STORE(model));

  path = gtk_file_chooser_get_filename(chooser);
  dir = g_dir_open(path, 0, NULL);

  while ((file = g_dir_read_name(dir))) {
    if (g_str_has_suffix(file, ".lua")) {
      script_filter = gt_script_filter_new(file, err);
      if (script_filter != NULL) {
        valid = gt_script_filter_validate(script_filter, err);
        if (valid) {
          gtk_list_store_append(GTK_LIST_STORE(model), &iter);
          gtk_list_store_set(GTK_LIST_STORE(model), &iter, 0, file, -1);
        }
      }
      gt_script_filter_delete(script_filter);
      gt_error_unset(err);
    }
  }
  valid = gtk_tree_model_get_iter_first(model, &iter);
  selection = gtk_tree_view_get_selection(list_view);
  if (!valid) {
    gtk_tree_selection_set_mode(selection, GTK_SELECTION_NONE);
    gtk_list_store_append(GTK_LIST_STORE(model), &iter);
    gtk_list_store_set(GTK_LIST_STORE(model), &iter,
                       0, "No valid filter scripts found!", -1);
  } else
    gtk_tree_selection_set_mode(selection, GTK_SELECTION_MULTIPLE);

  g_dir_close(dir);
  g_free(path);
  gt_error_delete(err);
}
*/

static void gtk_ltr_filter_delete_event(GtkWidget *widget,
                                        GT_UNUSED GdkEvent *event,
                                        GT_UNUSED gboolean user_data)
{
  gtk_widget_hide(widget);
}

static void gtk_ltr_filter_lv_all_changed(GtkTreeView *list_view,
                                          GtkLTRFilter *ltrfilt)
{
  GtkTreeSelection *sel;
  GtkTreeModel *model;
  GtkTreeIter iter;
  GtkTreePath *path;
  GtError *err;
  GtScriptFilter *script_filter;
  GList *rows;
  gchar *file;
  const char *email, *author, *descr;

  err = gt_error_new();
  sel = gtk_tree_view_get_selection(list_view);
  if (gtk_tree_selection_count_selected_rows(sel) != 1)
    return;
  model = gtk_tree_view_get_model(list_view);
  rows = gtk_tree_selection_get_selected_rows(sel, &model);
  path = (GtkTreePath*) g_list_first(rows)->data;
  gtk_tree_model_get_iter(model, &iter, path);
  gtk_tree_model_get(model, &iter, 0, &file, -1);

  script_filter = gt_script_filter_new(file, err);
  if (!script_filter) {
    ltrfilt->gerr = g_error_new(G_FILE_ERROR, 0, "Script error: %s",
                                gt_error_get(err));
    error_handle(GTK_WIDGET(ltrfilt), ltrfilt->gerr);
    gt_error_unset(err);
  } else {
    author = gt_script_filter_get_author(script_filter, err);
    if (gt_error_is_set(err)) {
      ltrfilt->gerr = g_error_new(G_FILE_ERROR, 0, "Script error: %s",
                                  gt_error_get(err));
      error_handle(GTK_WIDGET(ltrfilt), ltrfilt->gerr);
      gt_error_unset(err);
    }
    email = gt_script_filter_get_email(script_filter, err);
    if (gt_error_is_set(err)) {
      ltrfilt->gerr = g_error_new(G_FILE_ERROR, 0, "Script error: %s",
                                  gt_error_get(err));
      error_handle(GTK_WIDGET(ltrfilt), ltrfilt->gerr);
      gt_error_unset(err);
    }
    descr = gt_script_filter_get_description(script_filter, err);
    if (gt_error_is_set(err)) {
      ltrfilt->gerr = g_error_new(G_FILE_ERROR, 0, "Script error: %s",
                                  gt_error_get(err));
      error_handle(GTK_WIDGET(ltrfilt), ltrfilt->gerr);
      gt_error_unset(err);
    }

    gtk_label_set_text(GTK_LABEL(ltrfilt->label_email), email);
    gtk_label_set_text(GTK_LABEL(ltrfilt->label_author), author);
    gtk_label_set_text(GTK_LABEL(ltrfilt->label_descr), descr);
  }

  g_list_foreach(rows, (GFunc) gtk_tree_path_free, NULL);
  g_list_free(rows);
  gt_script_filter_delete(script_filter);
  gt_error_delete(err);
  g_free(file);
}

static void gtk_ltr_filter_add_clicked(GT_UNUSED GtkWidget *button,
                                       GtkLTRFilter *ltrfilt)
{
  GtkWidget *filechooser,
            *dialog;
  GtkFileFilter *lua_file_filter;
  GtkTreeIter iter;
  GtkTreeModel *model;
  GtError *err;
  GtScriptFilter *script_filter;
  GSList *filenames;
  gchar *file;
  gboolean valid,
           skipped = FALSE;

  filechooser = gtk_file_chooser_dialog_new("Select lua filter files",
                                            GTK_WINDOW(ltrfilt),
                                            GTK_FILE_CHOOSER_ACTION_OPEN,
                                            GTK_STOCK_CANCEL,
                                            GTK_RESPONSE_CANCEL,
                                            GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
                                            NULL);

  gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(filechooser), TRUE);
  if (ltrfilt->last_dir != NULL)
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(filechooser),
                                        ltrfilt->last_dir);
  else
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(filechooser),
                                        g_get_home_dir());
  lua_file_filter = gtk_file_filter_new();
  gtk_file_filter_set_name(lua_file_filter, LUA_FILTER_PATTERN);
  gtk_file_filter_add_pattern(lua_file_filter, LUA_FILTER_PATTERN);
  gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(filechooser), lua_file_filter);
  gint result = gtk_dialog_run(GTK_DIALOG(filechooser));

  if (result == GTK_RESPONSE_ACCEPT) {
    err = gt_error_new();
    model = gtk_tree_view_get_model(GTK_TREE_VIEW(ltrfilt->list_view_all));
    filenames = gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(filechooser));
    while (filenames != NULL) {
      file = (gchar*) filenames->data;
      if (!entry_in_list_view(model, file, LTR_FILTER_LV_FILE)) {
        script_filter = gt_script_filter_new(file, err);
        if (script_filter != NULL) {
          valid = gt_script_filter_validate(script_filter, err);
          if (valid) {
            gtk_list_store_append(GTK_LIST_STORE(model), &iter);
            gtk_list_store_set(GTK_LIST_STORE(model), &iter, 0, file, -1);
          } else
            skipped = TRUE;
        } else
          skipped = TRUE;
        gt_script_filter_delete(script_filter);
        gt_error_unset(err);
      }
      filenames = filenames->next;
    }
    if (ltrfilt->last_dir)
      g_free(ltrfilt->last_dir);
    ltrfilt->last_dir = g_path_get_dirname(file);
    g_slist_foreach(filenames, (GFunc) g_free, NULL);
    g_slist_free(filenames);
    gt_error_delete(err);
  }
  gtk_widget_destroy(filechooser);
  if (skipped) {
    dialog = gtk_message_dialog_new(GTK_WINDOW(ltrfilt),
                                    GTK_DIALOG_MODAL |
                                    GTK_DIALOG_DESTROY_WITH_PARENT,
                                    GTK_MESSAGE_INFO, GTK_BUTTONS_OK,
                                    "%s",
                                    LTR_FILTER_NOT_ADDED_FILES);
    gtk_window_set_title(GTK_WINDOW(dialog), "Attention!");
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
  }
}

static void gtk_ltr_filter_back_clicked(GT_UNUSED GtkWidget *button,
                                        GtkLTRFilter *ltrfilt)
{
  GtkTreeModel *model;
  GtkTreeSelection *sel;
  GtkTreeRowReference *rowref;
  GList *rows, *tmp, *references = NULL;

  sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(ltrfilt->list_view_sel));

  if (!gtk_tree_selection_count_selected_rows(sel))
    return;

  model = gtk_tree_view_get_model(GTK_TREE_VIEW(ltrfilt->list_view_sel));
  rows = gtk_tree_selection_get_selected_rows(sel, &model);
  tmp = rows;

  while (tmp != NULL) {
    rowref = gtk_tree_row_reference_new(model, (GtkTreePath*) tmp->data);
    references =
                g_list_prepend(references, gtk_tree_row_reference_copy(rowref));
    gtk_tree_row_reference_free(rowref);
    tmp = tmp->next;
  }

  g_list_foreach(references, (GFunc) remove_list_view_row, model);
  g_list_foreach(references, (GFunc) gtk_tree_row_reference_free, NULL);
  g_list_foreach(rows, (GFunc) gtk_tree_path_free, NULL);
  g_list_free(references);
  g_list_free(rows);
}

static void gtk_ltr_filter_forward_clicked(GT_UNUSED GtkWidget *button,
                                           GtkLTRFilter *ltrfilt)
{
  GtkTreeModel *model_all, *model_sel;
  GtkTreeSelection *sel;
  GtkTreeIter iter_all, iter_sel;
  GtkTreePath *path;
  GList *rows, *tmp;
  gchar *file;

  sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(ltrfilt->list_view_all));
  if (gtk_tree_selection_count_selected_rows(sel) < 1)
    return;
  model_all = gtk_tree_view_get_model(GTK_TREE_VIEW(ltrfilt->list_view_all));
  model_sel = gtk_tree_view_get_model(GTK_TREE_VIEW(ltrfilt->list_view_sel));
  rows = gtk_tree_selection_get_selected_rows(sel, &model_all);
  tmp = rows;
  while (tmp != NULL) {
    path = (GtkTreePath*) tmp->data;
    gtk_tree_model_get_iter(model_all, &iter_all, path);
    gtk_tree_model_get(model_all, &iter_all, LTR_FILTER_LV_FILE, &file, -1);
    gtk_list_store_append(GTK_LIST_STORE(model_sel), &iter_sel);
    gtk_list_store_set(GTK_LIST_STORE(model_sel), &iter_sel,
                       LTR_FILTER_LV_FILE, file,
                       LTR_FILTER_LV_SEL_NOT, FALSE,
                       -1);
    tmp = tmp->next;
  }
  g_list_foreach(rows, (GFunc) gtk_tree_path_free, NULL);
  g_list_free(rows);
  g_free(file);
  /* g_free(file_name); */
}

/*
static void gtk_ltr_filter_lv_sel_move(GtkLTRFilter *ltrfilt, gint direction)
{
  GList *rows, *tmp;
  GtkTreeModel *model;
  GtkTreeSelection *sel;

  sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(ltrfilt->list_view_sel));

  if (gtk_tree_selection_count_selected_rows(sel) < 1)
    return;

  rows = gtk_tree_selection_get_selected_rows(sel, &model);
  for (tmp = rows; tmp; tmp = g_list_next(tmp)) {
    GtkTreePath *path1, *path2;
    GtkTreeIter  iter1, iter2;

    path1 = (GtkTreePath*) tmp->data;
    path2 = gtk_tree_path_copy(path1);

    if (direction ==  LTR_FILT_MOVE_UP)
      gtk_tree_path_prev(path2);
    else if (direction ==  LTR_FILT_MOVE_DOWN)
      gtk_tree_path_next(path2);

    if (!gtk_tree_path_compare(path1, path2)) {
      gtk_tree_path_free( path2 );
      continue;
    }

    gtk_tree_model_get_iter( model, &iter1, path1 );
    if (!gtk_tree_model_get_iter(model, &iter2, path2)) {
      gtk_tree_path_free( path2 );
      continue;
    }
    gtk_list_store_swap(GTK_LIST_STORE(model), &iter1, &iter2);
    gtk_tree_path_free(path2);
  }

  g_list_foreach(rows, (GFunc) gtk_tree_path_free, NULL);
  g_list_free(rows);
}

static void gtk_ltr_filter_move_up_clicked(GT_UNUSED GtkWidget *button,
                                           GtkLTRFilter *ltrfilt)
{
  gtk_ltr_filter_lv_sel_move(ltrfilt, LTR_FILT_MOVE_UP);
}

static void gtk_ltr_filter_move_down_clicked(GT_UNUSED GtkWidget *button,
                                             GtkLTRFilter *ltrfilt)
{
  gtk_ltr_filter_lv_sel_move(ltrfilt, LTR_FILT_MOVE_DOWN);
}
*/

static void gtk_ltr_filter_remove_clicked(GT_UNUSED GtkWidget *button,
                                          GtkLTRFilter *ltrfilt)
{
  GtkTreeModel *model;
  GtkTreeSelection *sel;
  GtkTreeRowReference *rowref;
  GList *rows, *tmp, *references = NULL;

  sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(ltrfilt->list_view_all));

  if (!gtk_tree_selection_count_selected_rows(sel))
    return;

  model = gtk_tree_view_get_model(GTK_TREE_VIEW(ltrfilt->list_view_all));
  rows = gtk_tree_selection_get_selected_rows(sel, &model);
  tmp = rows;

  while (tmp != NULL) {
    rowref = gtk_tree_row_reference_new(model, (GtkTreePath*) tmp->data);
    references =
                g_list_prepend(references, gtk_tree_row_reference_copy(rowref));
    gtk_tree_row_reference_free(rowref);
    tmp = tmp->next;
  }

  g_list_foreach(references, (GFunc) remove_list_view_row, model);
  g_list_foreach(references, (GFunc) gtk_tree_row_reference_free, NULL);
  g_list_foreach(rows, (GFunc) gtk_tree_path_free, NULL);
  g_list_free(references);
  g_list_free(rows);
}

static void gtk_ltr_filter_edit_clicked(GT_UNUSED GtkWidget *button,
                                             GtkLTRFilter *ltrfilt)
{
  GtkTreeSelection *sel;
  GtkTreeModel *model;
  GtkTreeIter iter;
  GtkTreePath *path;
  GList *rows;
  gchar *file;

  sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(ltrfilt->list_view_all));
  if (gtk_tree_selection_count_selected_rows(sel) < 1)
    return;
  model = gtk_tree_view_get_model(GTK_TREE_VIEW(ltrfilt->list_view_all));
  rows = gtk_tree_selection_get_selected_rows(sel, &model);
  path = (GtkTreePath*) rows->data;
  gtk_tree_model_get_iter(model, &iter, path);
  gtk_tree_model_get(model, &iter, 0, &file, -1);

  ltrfilt->cur_filename = file;
  create_edit_dialog(ltrfilt);

  g_list_foreach(rows, (GFunc) gtk_tree_path_free, NULL);
  g_list_free(rows);
}

static void gtk_ltr_filter_new_clicked(GT_UNUSED GtkWidget *button,
                                       GtkLTRFilter *ltrfilt)
{
  ltrfilt->cur_filename = NULL;
  create_edit_dialog(ltrfilt);
}

static void
gtk_ltr_filter_lv_sel_not_toggled(GT_UNUSED GtkCellRendererToggle *rend,
                                  gchar *path,
                                  GtkTreeView *treeview)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  gboolean value;

  /* Toggle the cell renderer's current state to the logical not. */
  model = gtk_tree_view_get_model(treeview);
  if (gtk_tree_model_get_iter_from_string (model, &iter, path)) {
    gtk_tree_model_get (model, &iter, LTR_FILTER_LV_SEL_NOT, &value, -1);
    gtk_list_store_set (GTK_LIST_STORE(model), &iter,
                        LTR_FILTER_LV_SEL_NOT, !value, -1);
  }
}

static void lv_all_cell_data_func(GT_UNUSED GtkTreeViewColumn *tree_column,
                                  GtkCellRenderer *renderer,
                                  GtkTreeModel *model,
                                  GtkTreeIter *iter,
                                  GT_UNUSED gpointer data)
{
  GtError *err;
  GtScriptFilter *script_filter;
  gchar *file, text[BUFSIZ];

  gtk_tree_model_get(model, iter, LTR_FILTER_LV_FILE, &file, -1);

  err = gt_error_new();
  script_filter = gt_script_filter_new(file, err);
  g_snprintf(text, BUFSIZ, "<b>Name:</b> %s\t<b>Version:</b> %s\n%s",
             gt_script_filter_get_name(script_filter, err),
             gt_script_filter_get_version(script_filter, err),
             gt_script_filter_get_short_description(script_filter, err));
  g_object_set(renderer, "markup", text, "family", "Courier", NULL);

  g_free(file);
  gt_error_delete(err);
}

static void gtk_ltr_filter_init(GtkLTRFilter *ltrfilt)
{
  GtkWidget *apply,
            *cancel,
            *vbox,
            *vbox2,
            *hbox,
            *image,
            *sw1,
            *label,
            *hsep2,
            *button;
  GtkListStore *store;
  GtkTreeViewColumn *column;
  GtkTreeSelection *sel;
  GtkCellRenderer *renderer;
  PangoAttrList *pattrl;
  PangoAttribute *pattr;
  GType *types;

  pattr = pango_attr_weight_new(PANGO_WEIGHT_BOLD);
  pattrl = pango_attr_list_new();
  pango_attr_list_insert(pattrl, pattr);

  vbox = gtk_vbox_new(FALSE, 1);
  /*
  hbox = gtk_hbox_new(FALSE, 1);
  label1 = gtk_label_new("Directory containing filter scripts (*.lua):");
  gtk_label_set_attributes(GTK_LABEL(label1), pattrl);
  gtk_misc_set_alignment(GTK_MISC(label1), 0.0, 0.5);
  gtk_box_pack_start(GTK_BOX(hbox), label1, FALSE, FALSE, 1);
  ltrfilt->dir_chooser =
             gtk_file_chooser_button_new("Choose a Folder",
                                         GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
  gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(ltrfilt->dir_chooser),
                                      g_get_home_dir());
  gtk_box_pack_start(GTK_BOX(hbox), ltrfilt->dir_chooser, FALSE, FALSE, 5);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 1);
  */

  /* add/remove/edit/new filter buttons */
  hbox = gtk_hbox_new(FALSE, 1);
  vbox2 = gtk_vbox_new(FALSE, 1);
  button = gtk_button_new_from_stock(GTK_STOCK_ADD);
  g_signal_connect(G_OBJECT(button), "clicked",
                   G_CALLBACK(gtk_ltr_filter_add_clicked), ltrfilt);
  gtk_box_pack_start(GTK_BOX(vbox2), button, FALSE, FALSE, 1);
  button = gtk_button_new_from_stock(GTK_STOCK_REMOVE);
  g_signal_connect(G_OBJECT(button), "clicked",
                   G_CALLBACK(gtk_ltr_filter_remove_clicked), ltrfilt);
  gtk_box_pack_start(GTK_BOX(vbox2), button, FALSE, FALSE, 1);
  button = gtk_button_new_from_stock(GTK_STOCK_NEW);
  g_signal_connect(G_OBJECT(button), "clicked",
                   G_CALLBACK(gtk_ltr_filter_new_clicked), ltrfilt);
  gtk_box_pack_start(GTK_BOX(vbox2), button, FALSE, FALSE, 1);
  button = gtk_button_new_from_stock(GTK_STOCK_EDIT);
  g_signal_connect(G_OBJECT(button), "clicked",
                   G_CALLBACK(gtk_ltr_filter_edit_clicked), ltrfilt);
  gtk_box_pack_start(GTK_BOX(vbox2), button, FALSE, FALSE, 1);
  gtk_box_pack_start(GTK_BOX(hbox), vbox2, FALSE, FALSE, 1);

  /* filter pool */
  sw1 = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw1),
                                 GTK_POLICY_AUTOMATIC,
                                 GTK_POLICY_AUTOMATIC);
  ltrfilt->list_view_all = gtk_tree_view_new();
  renderer = gtk_cell_renderer_text_new();
  column = gtk_tree_view_column_new_with_attributes("lua filter files",
                                                    renderer,
                                                    "markup",
                                                    LTR_FILTER_LV_FILE,
                                                    NULL);
  gtk_tree_view_append_column(GTK_TREE_VIEW(ltrfilt->list_view_all), column);
  gtk_tree_view_column_set_cell_data_func(column, renderer,
                                          lv_all_cell_data_func, NULL, NULL);
  store = gtk_list_store_new(1, G_TYPE_STRING);
  sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(ltrfilt->list_view_all));
  gtk_tree_selection_set_mode(sel, GTK_SELECTION_MULTIPLE);
  gtk_tree_view_set_model(GTK_TREE_VIEW(ltrfilt->list_view_all),
                          GTK_TREE_MODEL(store));
  g_object_unref(store);
  gtk_container_add(GTK_CONTAINER(sw1), ltrfilt->list_view_all);
  gtk_box_pack_start(GTK_BOX(hbox), sw1, TRUE, TRUE, 1);

  /* assign/unasign filter buttons */
  vbox2 = gtk_vbox_new(FALSE, 1);
  button = gtk_button_new();
  image = gtk_image_new_from_stock(GTK_STOCK_GO_FORWARD,
                                   GTK_ICON_SIZE_SMALL_TOOLBAR);
  gtk_button_set_image(GTK_BUTTON(button), image);

  g_signal_connect(G_OBJECT(button), "clicked",
                   G_CALLBACK(gtk_ltr_filter_forward_clicked), ltrfilt);
  gtk_box_pack_start(GTK_BOX(vbox2), button, FALSE, FALSE, 1);
  button = gtk_button_new();
  image = gtk_image_new_from_stock(GTK_STOCK_GO_BACK,
                                   GTK_ICON_SIZE_SMALL_TOOLBAR);
  gtk_button_set_image(GTK_BUTTON(button), image);
  g_signal_connect(G_OBJECT(button), "clicked",
                   G_CALLBACK(gtk_ltr_filter_back_clicked), ltrfilt);
  gtk_box_pack_start(GTK_BOX(vbox2), button, FALSE, FALSE, 1);
  gtk_box_pack_start(GTK_BOX(hbox), vbox2, FALSE, FALSE, 1);

  /* selected filter */
  sw1 = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw1),
                                 GTK_POLICY_AUTOMATIC,
                                 GTK_POLICY_AUTOMATIC);
  ltrfilt->list_view_sel = gtk_tree_view_new();
  renderer = gtk_cell_renderer_text_new();
  column = gtk_tree_view_column_new_with_attributes("Selected lua filter files",
                                                    renderer,
                                                    "markup",
                                                    LTR_FILTER_LV_FILE,
                                                    NULL);
  gtk_tree_view_append_column(GTK_TREE_VIEW(ltrfilt->list_view_sel), column);
  gtk_tree_view_column_set_cell_data_func(column, renderer,
                                          lv_all_cell_data_func, NULL, NULL);
  renderer = gtk_cell_renderer_toggle_new();
  g_object_set((gpointer) renderer, "activatable", TRUE, NULL);
  g_signal_connect(G_OBJECT(renderer), "toggled",
                   G_CALLBACK(gtk_ltr_filter_lv_sel_not_toggled),
                   ltrfilt->list_view_sel);
  column = gtk_tree_view_column_new_with_attributes("Negate?",
                                                    renderer,
                                                    "active",
                                                    LTR_FILTER_LV_SEL_NOT,
                                                    NULL);
  gtk_tree_view_append_column(GTK_TREE_VIEW(ltrfilt->list_view_sel), column);
  column = gtk_tree_view_column_new();
  gtk_tree_view_append_column(GTK_TREE_VIEW(ltrfilt->list_view_sel), column);

  types = g_new0(GType, LTR_FILTER_LV_SEL_N_COLUMNS);
  types[0] = G_TYPE_STRING;
  types[1] = G_TYPE_BOOLEAN;
  store = gtk_list_store_newv(LTR_FILTER_LV_SEL_N_COLUMNS, types);
  gtk_tree_view_set_model(GTK_TREE_VIEW(ltrfilt->list_view_sel),
                          GTK_TREE_MODEL(store));
  g_object_unref(store);
  g_free(types);

  sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(ltrfilt->list_view_sel));
  gtk_tree_selection_set_mode(sel, GTK_SELECTION_MULTIPLE);
  gtk_container_add(GTK_CONTAINER(sw1), ltrfilt->list_view_sel);
  gtk_box_pack_start(GTK_BOX(hbox), sw1, TRUE, TRUE, 1);

  /* move filter up/down buttons */
  /* vbox2 = gtk_vbox_new(FALSE, 1);
  button = gtk_button_new_from_stock(GTK_STOCK_GO_UP);
  g_signal_connect(G_OBJECT(button), "clicked",
                   G_CALLBACK(gtk_ltr_filter_move_up_clicked), ltrfilt);
  gtk_box_pack_start(GTK_BOX(vbox2), button, FALSE, FALSE, 1);
  button = gtk_button_new_from_stock(GTK_STOCK_GO_DOWN);
  g_signal_connect(G_OBJECT(button), "clicked",
                   G_CALLBACK(gtk_ltr_filter_move_down_clicked), ltrfilt);
  gtk_box_pack_start(GTK_BOX(vbox2), button, FALSE, FALSE, 1);
  gtk_box_pack_start(GTK_BOX(hbox), vbox2, FALSE, FALSE, 1); */

  gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 1);

  hbox = gtk_hbox_new(FALSE, 1);
  label =
     gtk_label_new("Action to perform with filtered candidates:");
  gtk_label_set_attributes(GTK_LABEL(label), pattrl);
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
  ltrfilt->filter_action = gtk_combo_box_new_text();
  gtk_combo_box_append_text(GTK_COMBO_BOX(ltrfilt->filter_action),
                            LTR_FILTER_ACTION_DELETE_TEXT);
  gtk_combo_box_set_active(GTK_COMBO_BOX(ltrfilt->filter_action),
                            LTR_FILTER_ACTION_DELETE);
  gtk_combo_box_append_text(GTK_COMBO_BOX(ltrfilt->filter_action),
                            LTR_FILTER_ACTION_NEW_FAM_TEXT);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 1);
  gtk_box_pack_start(GTK_BOX(hbox), ltrfilt->filter_action, FALSE, FALSE, 1);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 1);

  hsep2 = gtk_hseparator_new();
  gtk_box_pack_start(GTK_BOX(vbox), hsep2, FALSE, FALSE, 1);

  label = gtk_label_new("Further script information");
  gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 1);
  ltrfilt->label_descr = gtk_label_new("");
  /* gtk_label_set_line_wrap(GTK_LABEL(ltrfilt->label_descr), TRUE);
  gtk_label_set_line_wrap_mode(GTK_LABEL(ltrfilt->label_descr),
                               PANGO_WRAP_WORD_CHAR); */
  gtk_misc_set_alignment(GTK_MISC(ltrfilt->label_descr), 0.0, 0.5);
  ltrfilt->label_author = gtk_label_new("");
  gtk_misc_set_alignment(GTK_MISC(ltrfilt->label_author), 0.0, 0.5);
  ltrfilt->label_email = gtk_label_new("");
  gtk_misc_set_alignment(GTK_MISC(ltrfilt->label_email), 0.0, 0.5);

  label = gtk_label_new("Author:");
  gtk_label_set_attributes(GTK_LABEL(label), pattrl);
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
  hsep2 = gtk_hseparator_new();
  gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 1);
  gtk_box_pack_start(GTK_BOX(vbox), ltrfilt->label_author, FALSE, FALSE, 1);
  gtk_box_pack_start(GTK_BOX(vbox), hsep2, FALSE, FALSE, 1);

  label = gtk_label_new("Author email:");
  gtk_label_set_attributes(GTK_LABEL(label), pattrl);
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
  hsep2 = gtk_hseparator_new();
  gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 1);
  gtk_box_pack_start(GTK_BOX(vbox), ltrfilt->label_email, FALSE, FALSE, 1);
  gtk_box_pack_start(GTK_BOX(vbox), hsep2, FALSE, FALSE, 1);

  label = gtk_label_new("Description:");
  gtk_label_set_attributes(GTK_LABEL(label), pattrl);
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
  hsep2 = gtk_hseparator_new();
  gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 1);
  gtk_box_pack_start(GTK_BOX(vbox), ltrfilt->label_descr, FALSE, FALSE, 1);
  gtk_box_pack_start(GTK_BOX(vbox), hsep2, FALSE, FALSE, 1);

  hbox = gtk_hbox_new(FALSE, 0);
  apply = gtk_button_new_with_mnemonic("_Apply");
  g_signal_connect(G_OBJECT(apply), "clicked",
                   G_CALLBACK(gtk_ltr_filter_apply_clicked), ltrfilt);
  cancel = gtk_button_new_with_mnemonic("_Cancel");
  g_signal_connect(G_OBJECT(cancel), "clicked",
                   G_CALLBACK(gtk_ltr_filter_cancel_clicked), ltrfilt);
  gtk_box_pack_start(GTK_BOX(hbox), apply, FALSE, FALSE, 1);
  gtk_box_pack_start(GTK_BOX(hbox), cancel, FALSE, FALSE, 1);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 1);

  gtk_widget_show_all(vbox);
  gtk_container_add(GTK_CONTAINER(ltrfilt), vbox);

  /* connect signals */
  /*g_signal_connect(G_OBJECT(ltrfilt->dir_chooser), "selection-changed",
                   G_CALLBACK(gtk_ltr_filter_change_filter_dir),
                   ltrfilt->list_view_all);*/
  g_signal_connect(G_OBJECT(ltrfilt->list_view_all), "cursor-changed",
                   G_CALLBACK(gtk_ltr_filter_lv_all_changed), ltrfilt);
  gtk_window_resize(GTK_WINDOW(ltrfilt), 800, 600);

  pango_attr_list_unref(pattrl);
}

static gboolean gtk_ltr_filter_destroy(GtkWidget *widget,
                                       GT_UNUSED GdkEvent *event,
                                       GT_UNUSED gpointer user_data)
{
  GtkLTRFilter *ltrfilt;
  ltrfilt = GTK_LTR_FILTER(widget);
  if (ltrfilt->last_dir)
    g_free(ltrfilt->last_dir);

  return FALSE;
}

GType gtk_ltr_filter_get_type(void)
{
  static GType ltr_filter_type = 0;

  if (!ltr_filter_type) {
    const GTypeInfo ltr_filter_info =
    {
      sizeof (GtkLTRFilterClass),
      NULL, /* base_init */
      NULL, /* base_finalize */
      NULL, /*(GClassInitFunc) */
      NULL, /* class_finalize */
      NULL, /* class_data */
      sizeof (GtkLTRFilter),
      0, /* n_preallocs */
      (GInstanceInitFunc) gtk_ltr_filter_init,
    };
    ltr_filter_type = g_type_register_static(GTK_TYPE_WINDOW, "GtkLTRFilter",
                                             &ltr_filter_info, 0);
  }
  return ltr_filter_type;
}

GtkWidget* gtk_ltr_filter_new(GtkWidget *ltrfams)
{
  GtkLTRFilter *ltrfilt;
  ltrfilt = gtk_type_new(GTK_LTR_FILTER_TYPE);
  ltrfilt->last_dir = NULL;
  ltrfilt->ltrfams = ltrfams;
  g_signal_connect(G_OBJECT(ltrfilt), "delete_event",
                   G_CALLBACK(gtk_ltr_filter_delete_event), NULL);
  g_signal_connect(G_OBJECT(ltrfilt), "destroy",
                   G_CALLBACK(gtk_ltr_filter_destroy), NULL);
  gtk_window_set_position(GTK_WINDOW(ltrfilt), GTK_WIN_POS_CENTER);
  gtk_window_set_modal(GTK_WINDOW(ltrfilt), TRUE);
  gtk_window_set_title(GTK_WINDOW(ltrfilt), "LTRGui - Filter");
  gtk_container_set_border_width(GTK_CONTAINER(ltrfilt), 5);

  return GTK_WIDGET(ltrfilt);
}
