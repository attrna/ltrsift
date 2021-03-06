/*
  Copyright (c) 2011-2012 Sascha Kastens <sascha.kastens@studium.uni-hamburg.de>
  Copyright (c) 2011-2012 Center for Bioinformatics, University of Hamburg

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "error.h"
#include "menubar.h"
#include "message_strings.h"
#include "project_wizard.h"
#include "support.h"

static gboolean project_wizard_finished_job(gpointer data)
{
  ThreadData *threaddata = (ThreadData*) data;
  GtkWidget *ltrfams = threaddata->ltrgui->ltrfams;
  (GTK_LTR_FAMILIES(ltrfams))->regions = threaddata->nodes;
  g_source_remove(GPOINTER_TO_INT(
                               g_object_get_data(G_OBJECT(threaddata->window),
                                                 "source_id")));
  gtk_widget_destroy(threaddata->window);
  reset_progressbar(threaddata->progressbar);
  if (!threaddata->had_err) {
    gtk_widget_destroy(threaddata->ltrgui->projset);
    threaddata->ltrgui->projset = gtk_project_settings_new(NULL);
    if (gtk_ltr_assistant_get_classification(
                            GTK_LTR_ASSISTANT(threaddata->ltrgui->assistant))) {
      gtk_ltr_families_determine_fl_cands(GTK_LTR_FAMILIES(ltrfams),
                                          threaddata->ltrtolerance,
                                          threaddata->lentolerance);
    }
    extract_project_settings(threaddata->ltrgui);
    first_save_and_reload(threaddata->ltrgui, threaddata->nodes,
                          threaddata->regions, threaddata->fullname);
    create_recently_used_resource(threaddata->fullname);
  } else {
    gdk_threads_enter();
    error_handle(threaddata->ltrgui->main_window, threaddata->err);
    gdk_threads_leave();
  }
  threaddata_delete(threaddata);

  return FALSE;
}

static gpointer project_wizard_start_job(gpointer data)
{
  ThreadData *threaddata = (ThreadData*) data;
  GtkWidget *ltrassi = threaddata->ltrgui->assistant;
  GtkTreeSelection *sel;
  GtkTreeView *list_view;
  GtkTreeModel *model;
  GtkTreeIter iter;
  GList *rows,
        *tmp;
  GtStr *tmpdirprefix = NULL;
  GtNodeStream *last_stream = NULL,
               *gff3_in_stream = NULL,
               *ltr_cluster_stream = NULL,
               *ltr_classify_stream = NULL,
               *array_out_stream = NULL;
  GtEncseqLoader *el = NULL;
  GtEncseq *encseq = NULL;
  GtArray *nodes;
  GtHashmap *sel_features = NULL;
  gint psmall = 0,
       plarge = 0,
       i = 0,
       num_of_files;
  char *tmp_gff3 = NULL,
       *old_gff3 = NULL;
  const char **gff3_files,
             *indexname;
  gboolean first = TRUE;

  list_view =
          gtk_ltr_assistant_get_list_view_gff3files(GTK_LTR_ASSISTANT(ltrassi));
  sel = gtk_tree_view_get_selection(list_view);
  gtk_tree_selection_select_all(sel);
  num_of_files = gtk_tree_selection_count_selected_rows(sel);
  rows = gtk_tree_selection_get_selected_rows(sel, &model);
  tmp = rows;
  gff3_files = g_malloc((size_t) num_of_files * sizeof (const char*));
  while (tmp != NULL) {
    gtk_tree_model_get_iter(model, &iter, (GtkTreePath*) tmp->data);
    gtk_tree_model_get(model, &iter,
                       0, &gff3_files[i],
                       -1);
    if (first) {
      tmp_gff3 = g_strdup(gff3_files[i]);
      first = FALSE;
    } else {
      old_gff3 = g_strdup(tmp_gff3);
      g_free(tmp_gff3);
      tmp_gff3 = g_strjoin("\n", old_gff3, gff3_files[i], NULL);
      g_free(old_gff3);
    }
    i++;
    tmp = tmp->next;
  }
  g_list_foreach(rows, (GFunc) gtk_tree_path_free, NULL);
  g_list_free(rows);

  last_stream = gff3_in_stream = gt_gff3_in_stream_new_unsorted(num_of_files,
                                                                gff3_files);

  if (gtk_ltr_assistant_get_clustering(GTK_LTR_ASSISTANT(ltrassi))) {
    gchar *match_params;

    indexname = gtk_ltr_assistant_get_indexname(GTK_LTR_ASSISTANT(ltrassi));
    match_params =
                 gtk_ltr_assistant_get_match_params(GTK_LTR_ASSISTANT(ltrassi));
    psmall = gtk_ltr_assistant_get_psmall(GTK_LTR_ASSISTANT(ltrassi));
    plarge = gtk_ltr_assistant_get_plarge(GTK_LTR_ASSISTANT(ltrassi));
    g_free(match_params);

    el = gt_encseq_loader_new();
    encseq = gt_encseq_loader_load(el, indexname, threaddata->err);
    if (!encseq)
      threaddata->had_err = -1;
    if (!threaddata->had_err) {
      last_stream = ltr_cluster_stream = gt_ltr_cluster_stream_new(last_stream,
                encseq,
                gtk_ltr_assistant_get_matchscore(GTK_LTR_ASSISTANT(ltrassi)),
                gtk_ltr_assistant_get_mismatchcost(GTK_LTR_ASSISTANT(ltrassi)),
                gtk_ltr_assistant_get_gapopen(GTK_LTR_ASSISTANT(ltrassi)),
                gtk_ltr_assistant_get_gapextend(GTK_LTR_ASSISTANT(ltrassi)),
                gtk_ltr_assistant_get_xgapped(GTK_LTR_ASSISTANT(ltrassi)),
                gtk_ltr_assistant_get_xgapless(GTK_LTR_ASSISTANT(ltrassi)),
                gtk_ltr_assistant_get_xfinal(GTK_LTR_ASSISTANT(ltrassi)),
                gtk_ltr_assistant_get_stepsize(GTK_LTR_ASSISTANT(ltrassi)),
                gtk_ltr_assistant_get_mscoregapped(GTK_LTR_ASSISTANT(ltrassi)),
                gtk_ltr_assistant_get_mscoregapless(GTK_LTR_ASSISTANT(ltrassi)),
                plarge,
                psmall,
                &threaddata->current_state,
                threaddata->err);
    }
  }
  if (!threaddata->had_err &&
      gtk_ltr_assistant_get_classification(GTK_LTR_ASSISTANT(ltrassi))) {
    GtkTreeView *list_view;
    GtkTreeModel *model;
    GtkTreeSelection *sel;
    GtkTreeIter iter;
    GList *rows;
    gchar *feature_name;
    const char *fam_prefix;

    fam_prefix = gtk_ltr_assistant_get_fam_prefix(GTK_LTR_ASSISTANT(ltrassi));
    sel_features = gt_hashmap_new(GT_HASH_STRING, free_gt_hash_elem, NULL);
    list_view =
           gtk_ltr_assistant_get_list_view_features(GTK_LTR_ASSISTANT(ltrassi));
    sel = gtk_tree_view_get_selection(list_view);
    rows = gtk_tree_selection_get_selected_rows(sel, &model);
    tmp = rows;
    while (tmp != NULL) {
      gtk_tree_model_get_iter(model, &iter, (GtkTreePath*) tmp->data);
      gtk_tree_model_get(model, &iter,
                         0, &feature_name,
                         -1);
      gt_hashmap_add(sel_features, (void*) gt_cstr_dup(feature_name),
                     (void*) 1);
      g_free(feature_name);
      tmp = tmp->next;
    }
    g_list_foreach(rows, (GFunc) gtk_tree_path_free, NULL);
    g_list_free(rows);

    threaddata->ltrtolerance = (gfloat)
                       gtk_ltr_assistant_get_ltrtol(GTK_LTR_ASSISTANT(ltrassi));
    threaddata->lentolerance = (gfloat)
                       gtk_ltr_assistant_get_lentol(GTK_LTR_ASSISTANT(ltrassi));

    last_stream = ltr_classify_stream = gt_ltr_classify_stream_new(last_stream,
                                                     sel_features,
                                                     fam_prefix,
                                                     &threaddata->current_state,
                                                     NULL,
                                                     threaddata->err);
  }
  if (!threaddata->had_err) {
    nodes = gt_array_new(sizeof(GtGenomeNode*));
    last_stream = array_out_stream = gt_array_out_stream_new(last_stream, nodes,
                                                             threaddata->err);
  }
  if (!array_out_stream)
    threaddata->had_err = -1;
  if (!threaddata->had_err)
    threaddata->had_err = gt_node_stream_pull(last_stream, threaddata->err);

  if (!threaddata->had_err) {
    threaddata->regions =
                      gtk_ltr_assistant_get_regions(GTK_LTR_ASSISTANT(ltrassi));
    threaddata->nodes = nodes;
  }
  gt_node_stream_delete(ltr_classify_stream);
  gt_node_stream_delete(ltr_cluster_stream);
  gt_node_stream_delete(gff3_in_stream);
  gt_node_stream_delete(array_out_stream);
  gt_encseq_loader_delete(el);
  gt_encseq_delete(encseq);
  gt_hashmap_delete(sel_features);

  for (i = 0; i < num_of_files; i++)
    g_free((gpointer) gff3_files[i]);
  g_free(gff3_files);
  g_free(tmp_gff3);
  gt_str_delete(tmpdirprefix);

  g_idle_add(project_wizard_finished_job, data);
  return NULL;
}

void project_wizard_apply(GtkAssistant *assistant, GUIData *ltrgui)
{
  ThreadData *threaddata;
  GtkWidget *dialog;
  const gchar *fullname;
  gchar *projectdir,
        *projectfile,
        *tmp_pfile,
        projecttmpdir[BUFSIZ];
  gint had_err = 0;

  gtk_widget_hide(GTK_WIDGET(assistant));
  fullname = gtk_ltr_assistant_get_projectfile(GTK_LTR_ASSISTANT(assistant));
  if (!g_str_has_suffix(fullname, SQLITE_PATTERN)) {
    fullname = g_strconcat(fullname, SQLITE_PATTERN, NULL);
  }
  tmp_pfile = g_path_get_basename(fullname);
  projectfile = g_strndup(tmp_pfile,
                          strlen(tmp_pfile) - strlen(SQLITE_PATTERN));
  projectdir = g_path_get_dirname(fullname);
  if (g_file_test(fullname, G_FILE_TEST_EXISTS)) {
    gchar buffer[BUFSIZ];
    g_snprintf(buffer, BUFSIZ, FILE_EXISTS_DIALOG, tmp_pfile);
    dialog = gtk_message_dialog_new(NULL,
                                    GTK_DIALOG_MODAL |
                                    GTK_DIALOG_DESTROY_WITH_PARENT,
                                    GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO,
                                    "%s",
                                    buffer);
    gtk_window_set_title(GTK_WINDOW(dialog), "Attention!");
    gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ALWAYS);
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_NO) {
      gtk_widget_destroy(dialog);
      gtk_widget_show(GTK_WIDGET(assistant));
      gtk_assistant_set_current_page(assistant, 0);
      g_free(tmp_pfile);
      g_free(projectdir);
      g_free(projectfile);
      return;
    } else
      gtk_widget_destroy(dialog);
  }
  g_free(tmp_pfile);
  g_snprintf(projecttmpdir, BUFSIZ, "%s/tmp", projectdir);
  if (!g_file_test(projecttmpdir, G_FILE_TEST_EXISTS))
    had_err = g_mkdir(projecttmpdir, 0755);
  if (had_err != 0) {
    gt_error_set(ltrgui->err, "Could not create directory %s",
                 projecttmpdir);
    error_handle(ltrgui->main_window, ltrgui->err);
    return;
  }

  threaddata = threaddata_new();
  threaddata->ltrgui = ltrgui;
  threaddata->fullname = fullname;
  threaddata->progressbar = ltrgui->progressbar;
  threaddata->projectfile = projectfile;
  threaddata->projectdir = projectdir;
  threaddata->projectw = TRUE;
  threaddata->n_features = LTRFAMS_LV_N_COLUMS;
  threaddata->current_state = gt_cstr_dup("Starting...");
  threaddata->err = gt_error_new();
  progress_dialog_init(threaddata, ltrgui->main_window);

  if (!g_thread_create(project_wizard_start_job, (gpointer) threaddata, FALSE,
                       NULL)) {
    gt_error_set(ltrgui->err, "Could not create new thread");
    error_handle(ltrgui->main_window, ltrgui->err);
  }
}
