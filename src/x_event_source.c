#include "x_event_source.h"
#include <stdlib.h>

// TODO:
// - handle xcb_connection_has_error?

typedef struct XEventSource {
    GSource source;
    xcb_connection_t *connection;
#if !GLIB_CHECK_VERSION(2, 36, 0)
    GPollFD poll;
#endif
    GQueue *queue;
} XEventSource;

static gboolean x_event_prepare(GSource *source, gint *timeout);
static gboolean x_event_check(GSource *source);
static gboolean x_event_dispatch(GSource *source, GSourceFunc callback, gpointer user_data);
static void x_event_finalize(GSource *source);

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
    
    xcb_flush(x_event_source->connection);
#if XCB_POLL_FOR_QUEUED_EVENT
    enqueue_events(x_event_source, xcb_poll_for_queued_event);
#endif

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
    XEventSource *x_event_source = (XEventSource *)source;
    XEventFunc x_event_callback = (XEventFunc)callback;
    xcb_generic_event_t *event;
    gboolean again = TRUE;

    if (!callback) {
        g_warning("XEvent source dispatched without a callback");
        return FALSE;
    }

    while (again && (event = g_queue_pop_head(x_event_source->queue))) {
        again = x_event_callback(x_event_source->connection, event, user_data);
        free(event);
    }
    return again;
}

static void
x_event_finalize(GSource *source)
{
    XEventSource *x_event_source = (XEventSource *)source;

    g_queue_free_full(x_event_source->queue, free);
}

GSource *
x_event_source_new(xcb_connection_t *connection)
{
    GSource *source = g_source_new(&x_event_funcs, sizeof(XEventSource));
    XEventSource *x_event_source = (XEventSource *)source;
    gint xcb_fd = xcb_get_file_descriptor(connection);
    GIOCondition fd_event_mask = G_IO_IN | G_IO_HUP | G_IO_ERR;

    x_event_source->connection = connection;
    x_event_source->queue = g_queue_new();

#if GLIB_CHECK_VERSION(2, 36, 0)
    g_source_add_unix_fd(source, xcb_fd, fd_event_mask);
#else
    x_event_source->poll.fd = xcb_fd;
    x_event_source->poll.events = fd_event_mask;
    g_source_add_poll(source, &x_event_source->poll);
#endif
    
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
