#include <X11/Xlib.h>
#include <glib.h>
#include "x_event_source.h"

typedef struct XEventSource {
    GSource source;
    Display *display;
    GPollFD poll_fd;
} XEventSource;

static gboolean x_event_prepare(GSource *source, gint *timeout);
static gboolean x_event_check(GSource *source);
static gboolean x_event_dispatch(GSource *source, GSourceFunc callback, gpointer user_data);

static GSourceFuncs x_event_funcs = {
    x_event_prepare,
    x_event_check,
    x_event_dispatch,
    NULL
};

static gboolean
x_event_prepare(GSource *source, gint *timeout)
{
    XEventSource *x_event_source = (XEventSource *)source;
    gboolean ready;
    
    if (XEventsQueued(x_event_source->display, QueuedAlready)) {
        *timeout = 0;
        ready = TRUE;
    } else {
        *timeout = -1;
        ready = FALSE;
    }
    XFlush(x_event_source->display);
    return ready;
}

static gboolean
x_event_check(GSource *source)
{
    XEventSource *x_event_source = (XEventSource *)source;

    if (x_event_source->poll_fd.revents & GIO_IN)
        return XEventsQueued(x_event_source->display, QueuedAfterReading);

    return FALSE;
}

static gboolean
x_event_dispatch(GSource *source, GSourceFunc callback, gpointer user_data)
{
    if (!callback) {
        g_warning("XEvent source dispatched without a callback");
        return FALSE;
    }
    return callback(user_data);
}

GSource *
x_event_source_new(Display *display)
{
    GSource *source = g_source_new(&x_event_funcs, sizeof(XEventSource));
    XEventSource *x_event_source = (XEventSource *)source;

    x_event_source->display = display;
    x_event_source->poll_fd = {ConnectionNumber(display), G_IO_IN, 0};
    g_source_add_poll(source, x_event_source->poll_fd);
    
    return source;
}

guint
x_event_add(Display *display, GSourceFunc function, gpointer data)
{
    guint id;
    GSource *source;

    g_return_val_if_fail(function != NULL, 0);
 
    source = x_event_source_new(display);
    g_source_set_callback(source, function, data, NULL);
    id = g_source_attach(source, NULL);
    g_source_unref(source);

    return id;
}
