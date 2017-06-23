#include <glib-object.h>
#include <gio/gio.h>
#include <stdio.h>
#include <dazzle.h>

#include "ide-ast-indexer.h"
#define BUFF_SIZE 10000

/* This will index a sequence of set of files. Each set of files will be indexed into
 * some destination using IdeAstIndexer. cflags for each file should also be provided.
 * 
 * Input Format: input is sequence of set of files and is terminated with 0.
 * Each set of files contain n + 2 lines in input, where n is number of files
 * First Line : number of files
 * Second Line : destination
 * Each of next n lines : source + space/tab separated cflags
 *
 * Sample Input:
 * 2
 * cache/index/test/
 * test/a.c -I/usr/include/glib-2.0
 * test/b.c -I/usr/include/glib-2.0
 * 0
 */

static void
indexer (GApplication *app,
         gpointer      data)
{
  GQueue *indexing_queue;
  g_autoptr(IdeAstIndexer) ast_indexer;
  gchar buffer[BUFF_SIZE];
  gint num_files;

  indexing_queue = g_queue_new ();
  ast_indexer = ide_ast_indexer_new ();

  while (scanf ("%d", &num_files) && num_files!=0)
    {
      IndexingData *idata;

      scanf ("%s\n", buffer);

      idata = g_slice_new (IndexingData);
      idata->num_files = num_files;
      idata->num_flags = g_malloc (sizeof(gint)*num_files);
      idata->flags = g_ptr_array_new_full (num_files*2, g_free);
      idata->destination = g_strdup (buffer);

      for (int i = 0; i < num_files; i++)
        {
            gchar **flags;
            gint j;

            fgets (buffer, BUFF_SIZE, stdin);

            flags = g_strsplit (buffer, " ", -1);

            for (j = 0; flags[j] != NULL; j++)
              g_ptr_array_add (idata->flags, flags[j]);

            idata->num_flags[i] = j;            
        }
      
      g_queue_push_tail (indexing_queue, idata);
    }

  while (!g_queue_is_empty (indexing_queue))
    ide_ast_indexer_index (ast_indexer, g_queue_pop_head (indexing_queue));

  g_queue_free (indexing_queue);

  g_application_quit (app);
}

int
main (int     argc,
      char  **argv)
{
  GApplication *app;

  app = g_application_new (NULL, G_APPLICATION_FLAGS_NONE);

  g_signal_connect (app, "activate", G_CALLBACK (indexer), NULL);

  return g_application_run (app, argc, argv);
}
