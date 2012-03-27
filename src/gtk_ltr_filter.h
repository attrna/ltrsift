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

#ifndef GTK_LTR_FILTER_H
#define GTK_LTR_FILTER_H

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>
#include "genometools.h"

#define GTK_LTR_FILTER_TYPE\
        gtk_ltr_filter_get_type()
#define GTK_LTR_FILTER(obj)\
        G_TYPE_CHECK_INSTANCE_CAST((obj), GTK_LTR_FILTER_TYPE, GtkLTRFilter)
#define GTK_LTR_FILTER_CLASS(klass)\
        G_TYPE_CHECK_CLASS_CAST((klass), GTK_LTR_FILTER_TYPE, GtkLTRFilterClass)
#define IS_GTK_LTR_FILTER(obj)\
        G_TYPE_CHECK_INSTANCE_TYPE((obj), GTK_LTR_FILTER_TYPE)
#define IS_GTK_LTR_FILTER_CLASS(klass)\
        G_TYPE_CHECK_CLASS_TYPE((klass), GTK_LTR_FILTER_TYPE)

typedef struct _GtkLTRFilter      GtkLTRFilter;
typedef struct _GtkLTRFilterClass GtkLTRFilterClass;

struct _GtkLTRFilter
{
  GtkWindow window;
  GtkWidget *label_descr;
  GtkWidget *label_author;
  GtkWidget *label_email;
  GtkWidget *list_view_all;
  GtkWidget *list_view_sel;
  GtkWidget *edit_dialog;
  GtkWidget *filter_action;
  GtkWidget *filter_logic;
  GtkWidget *apply;
  GtkWidget *ltrfams;
  GtkTextBuffer *text_buffer;
  GtScriptFilter *script_filter;
  gchar *last_dir;
  gchar *cur_filename;
  gint range;
};

struct _GtkLTRFilterClass
{
  GtkWindowClass parent_class;
  void (* gtk_ltr_filter) (GtkLTRFilter *ltrfilt);
};

enum {
  LTR_FILTER_ACTION_DELETE = 0,
  LTR_FILTER_ACTION_NEW_FAM
};

enum {
  LTR_FILTER_LOGIC_AND = 0,
  LTR_FILTER_LOGIC_OR
};

enum {
  LTR_FILTER_RANGE_PROJECT = 0,
  LTR_FILTER_RANGE_FAMILIES,
  LTR_FILTER_RANGE_CANDIDATES
};

enum {
  LTR_FILTER_LV_FILE = 0,
  LTR_FILTER_LV_SEL_NOT,
  LTR_FILTER_LV_SEL_N_COLUMNS
};

GType      gtk_ltr_filter_get_type(void);

void       gtk_ltr_filter_set_ltrfams(GtkLTRFilter *ltrfilt,
                                      GtkWidget *ltrfams);

gint       gtk_ltr_filter_get_filter_files_from_sql(GtkLTRFilter *ltrfilt,
                                                    GtError *err);

void       gtk_ltr_filter_set_range(GtkLTRFilter *ltrfilt, gint range);

void       gtk_ltr_filter_set_apply_text(GtkLTRFilter *ltrfilt,
                                         const gchar *text);

gint       gtk_ltr_filter_save_data(GtkLTRFilter *ltrfilt, GtError *err);

GtkWidget* gtk_ltr_filter_new(GtkWidget *ltrfams);

#endif
