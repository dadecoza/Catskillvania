#include <gtk/gtk.h>
#include "catskillgfx.h"
#include "catskillgame.h"

GtkImage *image;
GdkPixbuf *pb;
GdkPixbuf *pbs;

gboolean keypress_function(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
    uint8_t k = event->keyval;
    if (k == 27)
    {
        gtk_main_quit();
        return TRUE;
    }
    setButton(k, true);
    return TRUE;
}

gboolean keyrelease_function(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
    uint8_t k = event->keyval;
    setButton(k, false);
    return TRUE;
}

gboolean main_loop(GtkWidget *widget, GdkFrameClock *clock, gpointer data)
{
    pb = gdk_pixbuf_new_from_data(playfield, GDK_COLORSPACE_RGB, 0, 8, COLS, ROWS, COLS * BYTES_PER_PIXEL, NULL, NULL);
    pbs = gdk_pixbuf_scale_simple(pb, COLS * 2, ROWS * 2, GDK_INTERP_BILINEAR);
    gtk_image_set_from_pixbuf(GTK_IMAGE(image), pbs);
    g_object_unref(pb);
    g_object_unref(pbs);
    gameLoop();
    return 1;
}

int main(int argc, char *argv[])
{
    gameSetup();
    gtk_init(&argc, &argv);
    image = GTK_IMAGE(gtk_image_new());
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Catskillvania");
    gtk_window_set_default_size(GTK_WINDOW(window), (COLS * 2) + 10, (ROWS * 2) + 10);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    GdkPixbuf *icon = gdk_pixbuf_new_from_file("UI/Catskillvania.ico", NULL);
    gtk_window_set_icon(GTK_WINDOW(window), icon);
    gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(image));
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(G_OBJECT(window), "key_press_event", G_CALLBACK(keypress_function), NULL);
    g_signal_connect(G_OBJECT(window), "key_release_event", G_CALLBACK(keyrelease_function), NULL);
    gtk_widget_add_tick_callback(window, main_loop, NULL, NULL);
    gtk_widget_show_all(window);
    gtk_main();
    return 0;
}
