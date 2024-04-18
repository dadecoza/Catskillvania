#include <gtk/gtk.h>
#include <pthread.h>
#include "catskillgfx.h"
#include "catskillgame.h"
#include <stdio.h>
#include <time.h>

// higher the number the faster the framerate
#define SPEED 120

static pthread_t drawing_thread;
static pthread_mutex_t mutex;
static cairo_surface_t *surface = NULL;
static int currently_drawing = 0;
static gboolean drawing_area_configure_cb(GtkWidget *, GdkEventConfigure *);
static void drawing_area_draw_cb(GtkWidget *, cairo_t *, void *);
static void *thread_draw(void *);
static int speed = SPEED;

gboolean keypress_function(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
    uint16_t k = event->keyval;
    setButton(k, true);
    return TRUE;
}

gboolean keyrelease_function(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
    uint16_t k = event->keyval;
    setButton(k, false);
    return TRUE;
}

void close_game(GtkWidget *window, gpointer data)
{
    quitGame();
    gtk_main_quit();
}

gboolean timer_exe(GtkWidget *window)
{
    static gboolean first_execution = TRUE;
    int drawing_status = g_atomic_int_get(&currently_drawing);
    if (drawing_status == 0)
    {
        gameLoop();
        static pthread_t thread_info;
        if (first_execution != TRUE)
        {
            pthread_join(thread_info, NULL);
        }
        pthread_create(&thread_info, NULL, thread_draw, NULL);
        if (GTK_IS_WIDGET(window))
        {
            gtk_widget_queue_draw(GTK_WIDGET(window));
        }
    }
    first_execution = FALSE;
    return TRUE;
}

void set_speed(char **spc)
{
    int sp = atoi(spc[1]);
    if ((sp > 60) && (sp < 500))
    {
        speed = sp;
        printf("speed set to %d\n", speed);
    }
}

int main(int argc, char **argv)
{
    if (argc > 1)
    {
        set_speed(argv);
    }
    gameSetup();
    gtk_init(&argc, &argv);
    GtkWidget *main_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(main_window), "Catskillvania");
    GdkPixbuf *icon = gdk_pixbuf_new_from_file("UI/Catskillvania.ico", NULL);
    gtk_window_set_icon(GTK_WINDOW(main_window), icon);
    gtk_window_set_default_size(GTK_WINDOW(main_window), (COLS * 2) + 10, (ROWS * 2) + 10);
    gtk_window_set_resizable(GTK_WINDOW(main_window), FALSE);
    GtkWidget *drawing_area = gtk_drawing_area_new();
    g_signal_connect(drawing_area, "configure-event", G_CALLBACK(drawing_area_configure_cb), NULL);
    gtk_container_add(GTK_CONTAINER(main_window), drawing_area);
    gtk_widget_show_all(main_window);
    pthread_mutex_init(&mutex, NULL);
    pthread_create(&drawing_thread, NULL, thread_draw, NULL);
    g_signal_connect(drawing_area, "draw", G_CALLBACK(drawing_area_draw_cb), NULL);
    g_signal_connect(main_window, "delete-event", G_CALLBACK(close_game), NULL);
    g_signal_connect(main_window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(G_OBJECT(main_window), "key_press_event", G_CALLBACK(keypress_function), NULL);
    g_signal_connect(G_OBJECT(main_window), "key_release_event", G_CALLBACK(keyrelease_function), NULL);
    g_timeout_add(1000 / speed, (GSourceFunc)timer_exe, drawing_area);
    gtk_main();
}

static gboolean
drawing_area_configure_cb(GtkWidget *widget, GdkEventConfigure *event)
{
    if (event->type == GDK_CONFIGURE)
    {
        pthread_mutex_lock(&mutex);
        if (surface != (cairo_surface_t *)NULL)
        {
            cairo_surface_destroy(surface);
        }
        surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24, COLS, ROWS);
        pthread_mutex_unlock(&mutex);
    }

    return TRUE;
}

static void
drawing_area_draw_cb(GtkWidget *widget, cairo_t *context, void *ptr)
{
    pthread_mutex_lock(&mutex);
    if (surface != (cairo_surface_t *)NULL)
    {
        cairo_scale(context, 2, 2);
        cairo_set_source_surface(context, surface, 2, 2);
        cairo_paint(context);
    }
    pthread_mutex_unlock(&mutex);
}

static void *
thread_draw(void *ptr)
{
    if (surface == (cairo_surface_t *)NULL)
    {
        return NULL;
    }
    currently_drawing = 1;
    pthread_mutex_lock(&mutex);
    cairo_t *context = cairo_create(surface);
    unsigned char *current_row;
    current_row = cairo_image_surface_get_data(surface);
    int stride = cairo_image_surface_get_stride(surface);
    uint8_t *pf;
    pf = &playfield[0];
    for (int y = 0; y < ROWS; y++)
    {
        uint32_t *row = (void *)current_row;
        for (int x = 0; x < COLS; x++)
        {
            uint32_t r = *pf++;
            uint32_t g = *pf++;
            uint32_t b = *pf++;
            row[x] = (r << 16) | (g << 8) | b;
        }
        current_row += stride;
    }
    cairo_stroke(context);
    cairo_destroy(context);
    pthread_mutex_unlock(&mutex);
    currently_drawing = 0;
    return NULL;
}
