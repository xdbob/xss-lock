#include "x_event_source.h"

// TODO:
// - dispatch/callback return value
// - handle xcb errors?
// - for glib-2.36: use new unixfd

typedef struct XEventSource {
    GSource source;
    xcb_connection_t *connection;
    GPollFD poll_fd;
    GQueue *queue;
} XEventSource;

static gboolean x_event_prepare(GSource *source, gint *timeout);
static gboolean x_event_check(GSource *source);
static gboolean x_event_dispatch(GSource *source, GSourceFunc callback, gpointer user_data);
static gboolean x_event_finalize(GSource *source);

static GSourceFuncs x_event_funcs = {
    x_event_prepare,
    x_event_check,
    x_event_dispatch,
    x_event_finalize
};

static void
enqueue_events(XEventSource *x_event_source,
               xcb_generic_event_t *(*poll)(xcb_connection_t *))
{
    xcb_generic_event_t *event;

    while (event = (*poll)(x_event_source->connection))
        g_queue_push_tail(x_event_source->queue, event);
}

static gboolean
x_event_prepare(GSource *source, gint *timeout)
{
    XEventSource *x_event_source = (XEventSource *)source;
    
    enqueue_events(x_event_source, xcb_poll_for_queued_event);
    xcb_flush(x_event_source->connection);

    if (g_queue_is_empty(x_event_source->queue)) {
        *timeout = -1;
        return FALSE;
    } else {
        *timeout = 0;
        return TRUE;
    }
}

static gboolean
x_event_check(GSource *source)
{
    XEventSource *x_event_source = (XEventSource *)source;

    enqueue_events(x_event_source, xcb_poll_for_event);

    return !g_queue_is_empty(x_event_source->queue);
}

static gboolean
x_event_dispatch(GSource *source, GSourceFunc callback, gpointer user_data)
{
    xcb_generic_event_t *event;
    XEventFunc x_event_callback = (XEventFunc)callback;

    if (!callback) {
        g_warning("XEvent source dispatched without a callback");
        return FALSE;
    }

    while (event = q_queue_pop_head(x_event_source->queue)) {
        x_event_callback(event, user_data);
        free(event);
    }
    return; // FIXME
}

static void
x_event_finalize(GSource *source)
{
    XEventSource *x_event_source = (XEventSource *)source;

    q_queue_free_full(x_event_source->queue, free);
}

GSource *
x_event_source_new(xcb_connection_t *connection)
{
    GSource *source = g_source_new(&x_event_funcs, sizeof(XEventSource));
    XEventSource *x_event_source = (XEventSource *)source;

    x_event_source->connection = connection;
    x_event_source->queue = g_queue_new();
    x_event_source->poll_fd = {xcb_get_file_descriptor(connection),
                               G_IO_IN | G_IO_HUP | G_IO_ERR,
                               0};
    g_source_add_poll(source, x_event_source->poll_fd);
    
    return source;
}

guint
x_event_add(xcb_connection_t *connection, XEventFunc function, gpointer data)
{
    guint id;
    GSource *source;

    g_return_val_if_fail(function != NULL, 0);
 
    source = x_event_source_new(connection);
    g_source_set_callback(source, (GSourceFunc)function, data, NULL);
    id = g_source_attach(source, NULL);
    g_source_unref(source);

    return id;
}
