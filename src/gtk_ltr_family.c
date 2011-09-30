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

#include "gtk_ltr_family.h"

static void draw_image(GtkLTRFamily *ltrfam, GtGenomeNode **gn)
{
  GtkWidget *a = GTK_WIDGET(ltrfam->image_area);
  GtFeatureIndex *features;
  GtRange range;
  GtError *err;
  const char *seqid;

  err = gt_error_new();
  ltrfam->style = gt_style_new(err);
  gt_style_load_file(ltrfam->style,
   "/local/skastens/masterarbeit/ltrgui/tmp/ltr_colors_pdoms_uncollapsed.style",
                     err);
  features = gt_feature_index_memory_new();
  gt_feature_index_add_feature_node(features, (GtFeatureNode*) gn);
  seqid = gt_feature_index_get_first_seqid(features);
  gt_feature_index_get_range_for_seqid(features, &range, seqid);
  gt_diagram_delete(ltrfam->diagram);
  ltrfam->diagram = gt_diagram_new(features, seqid, &range, ltrfam->style, err);
  gtk_widget_queue_draw_area(a, 0, 0, a->allocation.width,
                             a->allocation.height);
}

static gboolean image_area_expose_event(GtkWidget *widget,
                                        GdkEventExpose *event,
                                        GtkLTRFamily *ltrfam)
{
  cairo_t *cr;
  GtCanvas *canvas = NULL;
  GtLayout *l;
  GtError *err;
  err = gt_error_new();
  unsigned long height;
  int rval;
  if (!ltrfam->diagram || widget->allocation.width <= 30) return FALSE;

  /* render image */
  l = gt_layout_new(ltrfam->diagram, widget->allocation.width, ltrfam->style,
                    err);
  if (!l) return FALSE;
  rval = gt_layout_get_height(l, &height, err);
  gt_assert(rval == 0);
  gtk_layout_set_size(GTK_LAYOUT(widget),
                      widget->allocation.width,
                      height);
  cr = gdk_cairo_create(GTK_LAYOUT(widget)->bin_window);
  cairo_rectangle(cr, event->area.x, event->area.y, event->area.width,
                  event->area.height);
  cairo_clip(cr);
  canvas = gt_canvas_cairo_context_new(ltrfam->style, cr, 0,
                                       widget->allocation.width, height, NULL,
                                       err);
  gt_assert(canvas);
  gt_layout_sketch(l, canvas, err);
  gt_layout_delete(l);
  gt_canvas_delete(canvas);
  return FALSE;
}

static char* double_underscores(const char *str)
{
  char **arr;
  char *ret;

  arr = g_strsplit(str, "_", 0);
  ret = g_strjoinv("__", arr);
  g_strfreev(arr);

  return ret;
}

static int add_feature_columns(void *key, void *value, void *ltrfam,
                               GT_UNUSED GtError *err)
{
  GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
  GtkTreeViewColumn *column;
  const char *caption = double_underscores((const char*) key);
  unsigned long num = (unsigned long) value;
  GtkLTRFamily *fam = (GtkLTRFamily*) ltrfam;

  g_object_set(renderer,
               "cell-background", "Grey",
               "cell-background-set", TRUE,
               "xalign", 1.0,
               NULL);

  column = gtk_tree_view_column_new_with_attributes(caption, renderer, "text",
                                                    num, NULL);
  gtk_tree_view_column_set_resizable(column, true);
  gtk_tree_view_column_set_sort_column_id(column, num);
  gtk_tree_view_append_column(GTK_TREE_VIEW(fam->list_view), column);

  return 0;
}

void gtk_ltr_family_clear_tree_view(GtkLTRFamily *ltrfam)
{
  GtkTreeModel *model;
  model = gtk_tree_view_get_model(GTK_TREE_VIEW(ltrfam->tree_view));
  if (model)
    gtk_tree_store_clear(GTK_TREE_STORE(model));
}

void gtk_ltr_family_clear_detail_on_equal_nodes(GtkLTRFamily *ltrfam,
                                                GtGenomeNode **gn)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  GtGenomeNode **gn_tmp;

  model = gtk_tree_view_get_model(GTK_TREE_VIEW(ltrfam->tree_view));
  if (model) {
    gboolean valid = gtk_tree_model_get_iter_first(model, &iter);
    if (!valid)
      return;
    gtk_tree_model_get(model, &iter,
                       LTRFAM_TV_NODE, &gn_tmp,
                       -1);
    if (gn_tmp == gn) {
      gtk_tree_store_clear(GTK_TREE_STORE(model));
    }
  }
}

GtkWidget* gtk_ltr_family_get_list_view(GtkLTRFamily *ltrfam)
{
  return ltrfam->list_view;
}

static void free_iter_hash(void *elem)
{
  gtk_tree_iter_free((GtkTreeIter*) elem);
}

static void list_view_row_activated(GtkTreeView *list_view,
                                    GtkLTRFamily *ltrfam)
{
  GtkTreeModel *list_model, *tree_model;
  GtkTreeIter iter, tmp;
  GtkTreeStore *store;
  GtkTreeSelection *selection;
  GtGenomeNode **gn;
  GtFeatureNodeIterator *fni;
  GtFeatureNode *curnode;
  GtRange range;
  GtHashmap *iter_hash;
  const char *fnt;

  selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(list_view));
  if (gtk_tree_selection_count_selected_rows(selection) < 1)
    return;
  list_model = gtk_tree_view_get_model(list_view);
  gtk_tree_selection_get_selected(selection, &list_model, &tmp);
  /* get GtGenomeNode */
  gtk_tree_model_get(list_model, &tmp,
                     LTRFAM_LV_NODE, &gn,
                     -1);
  tree_model = gtk_tree_view_get_model(GTK_TREE_VIEW(ltrfam->tree_view));
  if (!tree_model) {
    GType *types = g_new0(GType, LTRFAM_TV_N_COLUMS);

    types[0] = G_TYPE_POINTER;
    types[1] = G_TYPE_STRING;
    types[2] = G_TYPE_ULONG;
    types[3] = G_TYPE_ULONG;
    types[4] = G_TYPE_STRING;

    store = gtk_tree_store_newv(LTRFAM_TV_N_COLUMS, types);

    gtk_tree_view_set_model(GTK_TREE_VIEW(ltrfam->tree_view),
                            GTK_TREE_MODEL(store));
    g_object_unref(store);
    g_free(types);
  } else {
    store = GTK_TREE_STORE(tree_model);
    gtk_tree_store_clear(store);
  }

  iter_hash = gt_hashmap_new(GT_HASH_STRING, NULL, free_iter_hash);
  fni = gt_feature_node_iterator_new((GtFeatureNode*) gn); 
  while ((curnode = gt_feature_node_iterator_next(fni))) {    
    fnt = gt_feature_node_get_type(curnode);
    if (g_strcmp0(fnt, FNT_REPEATR) == 0) {  
      range = gt_genome_node_get_range((GtGenomeNode*) curnode);    
      gtk_tree_store_append(store, &iter, NULL);
      gtk_tree_store_set(store, &iter,
                         LTRFAM_TV_NODE, gn,
                         LTRFAM_TV_TYPE, fnt,
                         LTRFAM_TV_START, range.start,
                         LTRFAM_TV_END, range.end,
                         -1);
      gt_hashmap_add(iter_hash,
                     (void*) gt_feature_node_get_attribute(curnode, "ID"),
                     (void*) gtk_tree_iter_copy(&iter));
    } else {
      GtkTreeIter *tmp_iter;
      tmp_iter = (GtkTreeIter*) gt_hashmap_get(iter_hash,
                      (void*) gt_feature_node_get_attribute(curnode, "Parent"));
      if (tmp_iter) {
        GtkTreeIter child;
        const char *id;
        range = gt_genome_node_get_range((GtGenomeNode*) curnode);
        gtk_tree_store_append(store, &child, tmp_iter);
        gtk_tree_store_set(store, &child,
                           LTRFAM_TV_TYPE, fnt,
                           LTRFAM_TV_START, range.start,
                           LTRFAM_TV_END, range.end,
                           -1);
        if ((id = gt_feature_node_get_attribute(curnode, "ID")))
          gt_hashmap_add(iter_hash, (void*) id,
                         (void*) gtk_tree_iter_copy(&child));
      }
    }
  }
  gtk_tree_view_expand_all(GTK_TREE_VIEW(ltrfam->tree_view));
  draw_image(ltrfam, gn);
  gt_feature_node_iterator_delete(fni);
  gt_hashmap_delete(iter_hash);
}

void gtk_ltr_family_list_view_remove(GtkLTRFamily *ltrfam,
                                     GtkTreeRowReference *rowref)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  GtkTreePath *path;

  model = gtk_tree_view_get_model(GTK_TREE_VIEW(ltrfam->list_view));
  path = gtk_tree_row_reference_get_path(rowref);
  gtk_tree_model_get_iter(model, &iter, path);
  gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
}

void gtk_ltr_family_list_view_append(GtkLTRFamily *ltrfam,
                                     GtGenomeNode **gn,
                                     GtHashmap *features,
                                     GtkTreeRowReference *rowref,
                                     GtkListStore *tmp)
{
  GtFeatureNodeIterator *fni;
  GtFeatureNode *curnode;
  GtkTreeIter iter;
  GtkTreeModel *model;
  GtkListStore *store;
  const char *fnt;
  gboolean first_ltr = TRUE;

  model = gtk_tree_view_get_model(GTK_TREE_VIEW(ltrfam->list_view));

  if (!tmp)
    store =
      GTK_LIST_STORE(model);
  else
    store = tmp;

  fni = gt_feature_node_iterator_new((GtFeatureNode*) gn);

  while ((curnode = gt_feature_node_iterator_next(fni))) {
    fnt = gt_feature_node_get_type(curnode);
    if ((g_strcmp0(fnt, FNT_REPEATR) == 0)) {
      GtRange range;
      char *seqid;
      const char *tmp;
      unsigned long rid;
      seqid = gt_str_get(gt_genome_node_get_seqid((GtGenomeNode*) curnode));
      range = gt_genome_node_get_range((GtGenomeNode*) curnode);
      tmp = gt_feature_node_get_attribute(curnode, ATTR_RID);
      sscanf(tmp, "repeat_region%lu", &rid);
      gtk_list_store_append(store, &iter);
      gtk_list_store_set(store, &iter,
                         LTRFAM_LV_NODE, gn,
                         LTRFAM_LV_FEAT, features,
                         LTRFAM_LV_ROWREF, rowref,
                         LTRFAM_LV_SEQID, seqid,
                         LTRFAM_LV_TYPE, rid,
                         LTRFAM_LV_START, range.start,
                         LTRFAM_LV_END, range.end,
                         -1);
    } else if (g_strcmp0(fnt, FNT_PROTEINM) == 0) {
      const char *attr = gt_feature_node_get_attribute(curnode, ATTR_PFAMN);
      if (gt_hashmap_get(features, attr) != NULL) {
        const char *clid;
        unsigned long cno;
        clid = gt_feature_node_get_attribute(curnode, ATTR_CLUSTID);
        cno = (unsigned long) gt_hashmap_get(features, attr);
        gtk_list_store_set(store, &iter,
                           cno, clid,
                           -1);
      }
    } else if (g_strcmp0(fnt, FNT_LTR) == 0) {
      const char *tmp;
      if (first_ltr) {
        tmp = FNT_LLTR;
        first_ltr = FALSE;
      } else
        tmp = FNT_RLTR;
      if (gt_hashmap_get(features, tmp) != NULL) {
        const char *clid;
        unsigned long cno;
        clid = gt_feature_node_get_attribute(curnode, ATTR_CLUSTID);
        cno = (unsigned long) gt_hashmap_get(features, tmp);
        gtk_list_store_set(store, &iter,
                           cno, clid,
                           -1);
      }
    } else if ((gt_hashmap_get(features, fnt)) != NULL) {
      const char *clid;
      unsigned long cno;
      clid = gt_feature_node_get_attribute(curnode, ATTR_CLUSTID);
      cno = (unsigned long) gt_hashmap_get(features, fnt);
      gtk_list_store_set(store, &iter,
                         cno, clid,
                         -1);
    }
  }
  gt_feature_node_iterator_delete(fni);

  return;
}

static void gtk_ltr_family_list_view_new(GtkLTRFamily *ltrfam,
                                         GtArray *nodes,
                                         GtHashmap *features,
                                         unsigned long noc)
{
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;
  GtkListStore *store;
  GType *types;
  GtError *err = NULL;
  unsigned long i;

  types = g_new0(GType, noc);

  renderer = gtk_cell_renderer_text_new();
  column = gtk_tree_view_column_new_with_attributes(LTRFAM_LV_CAPTION_SEQID,
                                                    renderer, "text",
                                                    LTRFAM_LV_SEQID, NULL);
  gtk_tree_view_column_set_sort_column_id(column, LTRFAM_LV_SEQID);
  gtk_tree_view_append_column(GTK_TREE_VIEW(ltrfam->list_view), column);

  renderer = gtk_cell_renderer_text_new();
  g_object_set(renderer, "xalign", 1.0, NULL);
  column = gtk_tree_view_column_new_with_attributes(LTRFAM_LV_CAPTION_TYPE,
                                                    renderer, "text",
                                                    LTRFAM_LV_TYPE, NULL);
  gtk_tree_view_column_set_sort_column_id(column, LTRFAM_LV_TYPE);
  gtk_tree_view_append_column(GTK_TREE_VIEW(ltrfam->list_view), column);

  renderer = gtk_cell_renderer_text_new();
  g_object_set(renderer, "xalign", 1.0, NULL);
  column = gtk_tree_view_column_new_with_attributes(LTRFAM_LV_CAPTION_START,
                                                    renderer, "text",
                                                    LTRFAM_LV_START, NULL);
  gtk_tree_view_column_set_sort_column_id(column, LTRFAM_LV_START);
  gtk_tree_view_append_column(GTK_TREE_VIEW(ltrfam->list_view), column);

  renderer = gtk_cell_renderer_text_new();
  g_object_set(renderer, "xalign", 1.0, NULL);
  column = gtk_tree_view_column_new_with_attributes(LTRFAM_LV_CAPTION_END,
                                                    renderer, "text",
                                                    LTRFAM_LV_END, NULL);
  gtk_tree_view_column_set_sort_column_id(column, LTRFAM_LV_END);
  gtk_tree_view_append_column(GTK_TREE_VIEW(ltrfam->list_view), column);

  gt_hashmap_foreach(features, add_feature_columns, (void*) ltrfam, err);

  column = gtk_tree_view_column_new();
  gtk_tree_view_append_column(GTK_TREE_VIEW(ltrfam->list_view), column);

  types[0] = G_TYPE_POINTER;
  types[1] = G_TYPE_POINTER;
  types[2] = G_TYPE_POINTER;
  types[3] = G_TYPE_STRING;
  types[4] = G_TYPE_ULONG;
  types[5] = G_TYPE_ULONG;
  types[6] = G_TYPE_ULONG;

  for (i = LTRFAM_LV_N_COLUMS; i < noc; i++) {
    types[i] = G_TYPE_STRING;
  }

  store = gtk_list_store_newv(noc, types);

  if (nodes) {
    for (i = 0; i < gt_array_size(nodes); i++) {
      GtGenomeNode **gn;
      gn = *(GtGenomeNode***) gt_array_get(nodes, i);
      gtk_ltr_family_list_view_append(ltrfam, gn, features, NULL, store);
    }
  }

  gtk_tree_view_set_model(GTK_TREE_VIEW(ltrfam->list_view),
                          GTK_TREE_MODEL(store));
  g_object_unref(store);
  g_signal_connect(G_OBJECT(ltrfam->list_view), "cursor-changed",
                   G_CALLBACK(list_view_row_activated), ltrfam);
  g_free(types);
}

static void gtk_ltr_family_tree_view_new(GtkLTRFamily *ltrfam)
{
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;

  renderer = gtk_cell_renderer_text_new();
  column = gtk_tree_view_column_new_with_attributes(LTRFAM_LV_CAPTION_TYPE,
                                                    renderer, "text",
                                                    LTRFAM_TV_TYPE, NULL);
  gtk_tree_view_append_column(GTK_TREE_VIEW(ltrfam->tree_view), column);

  renderer = gtk_cell_renderer_text_new();
  g_object_set(renderer, "xalign", 1.0, NULL);
  column = gtk_tree_view_column_new_with_attributes(LTRFAM_LV_CAPTION_START,
                                                    renderer, "text",
                                                    LTRFAM_TV_START, NULL);
  gtk_tree_view_append_column(GTK_TREE_VIEW(ltrfam->tree_view), column);

  renderer = gtk_cell_renderer_text_new();
  g_object_set(renderer, "xalign", 1.0, NULL);
  column = gtk_tree_view_column_new_with_attributes(LTRFAM_LV_CAPTION_END,
                                                    renderer, "text",
                                                    LTRFAM_TV_END, NULL);
  gtk_tree_view_append_column(GTK_TREE_VIEW(ltrfam->tree_view), column);

  renderer = gtk_cell_renderer_text_new();
  column = gtk_tree_view_column_new_with_attributes(LTRFAM_TV_CAPTION_INFO,
                                                    renderer, "text",
                                                    LTRFAM_TV_INFO, NULL);
  gtk_tree_view_append_column(GTK_TREE_VIEW(ltrfam->tree_view), column);
}

/* maybe add some signal-stuff here */
/* static void gtk_ltr_family_class_init(GtkLTRFamilyClass *class)
{
  GtkObjectClass *object_class;

  object_class = (GtkObjectClass*) class;

  class->gtk_ltr_family = NULL;
}*/

gboolean pane_moved(GtkPaned *pane, GT_UNUSED GtkScrollType scrolltype,
                    GT_UNUSED gpointer user_data)
{
  g_warning("pixel pos: %d", gtk_paned_get_position(pane));
  return TRUE;
}

gboolean pane_move_accept(GtkPaned *pane, GT_UNUSED gpointer user_data)
{
  g_warning("pixel pos acc: %d", gtk_paned_get_position(pane));
  return TRUE;
}

static void gtk_ltr_family_init(GtkLTRFamily *ltrfam)
{
  GtkWidget *sw1,
            *sw2,
            *sw3,
            *hpane,
            *label1,
            *label2,
            *vbox1,
            *vbox2;
  GtkAdjustment *vadj = NULL;

  hpane = gtk_hpaned_new();
  /*g_signal_connect(G_OBJECT(hpane),
                   "move-handle",
                   G_CALLBACK(pane_moved), NULL);*/
  gtk_paned_set_position(GTK_PANED(hpane), 400);
  gtk_paned_set_position(GTK_PANED(ltrfam), 400);

  label1 = gtk_label_new(LTRFAM_DETINFO);
  label2 = gtk_label_new(LTRFAM_IMAGE);

  vbox1 = gtk_vbox_new(FALSE, 0);
  vbox2 = gtk_vbox_new(FALSE, 0);

  sw1 = gtk_scrolled_window_new(NULL, NULL);
  sw2 = gtk_scrolled_window_new(NULL, NULL);
  sw3 = gtk_scrolled_window_new(NULL, vadj);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw1),
                                 GTK_POLICY_AUTOMATIC,
                                 GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw2),
                                 GTK_POLICY_AUTOMATIC,
                                 GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw3),
                                 GTK_POLICY_NEVER,
                                 GTK_POLICY_AUTOMATIC);

  ltrfam->list_view = gtk_tree_view_new();
  gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(ltrfam->list_view), TRUE);
  gtk_container_add(GTK_CONTAINER(sw1), ltrfam->list_view);
  gtk_paned_add1(GTK_PANED(ltrfam), sw1);
  gtk_paned_add2(GTK_PANED(ltrfam), hpane);

  gtk_box_pack_start(GTK_BOX(vbox1), label1, FALSE, TRUE, 0);
  ltrfam->tree_view = gtk_tree_view_new();
  gtk_container_add(GTK_CONTAINER(sw2), ltrfam->tree_view);
  gtk_box_pack_start(GTK_BOX(vbox1), sw2, TRUE, TRUE, 0);

  ltrfam->image_area = gtk_layout_new(NULL, vadj);

  gtk_layout_set_size(GTK_LAYOUT(ltrfam->image_area), 100, 100);
  gtk_container_add(GTK_CONTAINER(sw3), ltrfam->image_area);
  gtk_box_pack_start(GTK_BOX(vbox2), label2, FALSE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(vbox2), sw3, TRUE, TRUE, 0);

  gtk_paned_add1(GTK_PANED(hpane), vbox1);
  gtk_paned_add2(GTK_PANED(hpane), vbox2);
  gtk_widget_show_all(GTK_WIDGET(ltrfam));
}

GType gtk_ltr_family_get_type(void)
{
  static GType ltrfam_type = 0;

  if (!ltrfam_type) {
    const GTypeInfo ltrfam_info =
    {
      sizeof (GtkLTRFamilyClass),
      NULL, /* base_init */
      NULL, /* base_finalize */
      NULL, /* (GClassInitFunc) gtk_ltr_family_class_init, */
      NULL, /* class_finalize */
      NULL, /* class_data */
      sizeof (GtkLTRFamily),
      0,    /* n_preallocs */
      (GInstanceInitFunc) gtk_ltr_family_init,
    };
    ltrfam_type = g_type_register_static(GTK_TYPE_VPANED, "GtkLTRFamily",
                                         &ltrfam_info, 0);
  }
  return ltrfam_type;
}

gboolean            user_function  (GtkWidget *widget,
                                     GT_UNUSED GdkEvent  *event,
                                      GT_UNUSED gpointer   user_data)
{
  g_warning("pixel pos: %d", gtk_paned_get_position(GTK_PANED(widget)));
  return FALSE;
}

GtkWidget* gtk_ltr_family_new(GtHashmap *features,
                              GtArray *nodes,
                              unsigned long noc)
{
  GtkLTRFamily *ltrfam;
  gt_assert(features);
  /* no gt_assert(nodes); due to tests */
  ltrfam = gtk_type_new(GTKLTRFAMILY_TYPE);

  g_signal_connect(G_OBJECT(ltrfam),
                   "move-handle",
                   G_CALLBACK(pane_moved), NULL);
  g_signal_connect(ltrfam->image_area, "expose-event",
      G_CALLBACK (image_area_expose_event), ltrfam);
 /* g_signal_connect(G_OBJECT(ltrfam),
                   "button-release-event",
                   G_CALLBACK(user_function), NULL);*/
  gtk_ltr_family_list_view_new(ltrfam, nodes, features, noc);
  gtk_ltr_family_tree_view_new(ltrfam);

  return GTK_WIDGET(ltrfam);
}
