#include "xcb_utils.h"
#include <stdlib.h>

// TODO:
// - handle xcb_connection_has_error?

typedef struct XcbEventSource {
    GSource source;
    xcb_connection_t *connection;
#if !GLIB_CHECK_VERSION(2, 36, 0)
    GPollFD poll;
#endif
    GQueue *queue;
} XcbEventSource;

static void xcb_enqueue_events(XcbEventSource *xcb_event_source, xcb_generic_event_t *(*poll)(xcb_connection_t *));
static gboolean xcb_event_prepare(GSource *source, gint *timeout);
static gboolean xcb_event_check(GSource *source);
static gboolean xcb_event_dispatch(GSource *source, GSourceFunc callback, gpointer user_data);
static void xcb_event_finalize(GSource *source);

static GSourceFuncs xcb_event_funcs = {
    xcb_event_prepare,
    xcb_event_check,
    xcb_event_dispatch,
    xcb_event_finalize
};

GQuark
xcb_error_quark(void)
{
    return g_quark_from_static_string("xcb-error-quark");
}

xcb_screen_t *
xcb_get_screen(xcb_connection_t *connection, int screen_number)
{
    const xcb_setup_t *setup = xcb_get_setup(connection);
    xcb_screen_iterator_t iter = xcb_setup_roots_iterator(setup);

    g_return_val_if_fail(screen_number < xcb_setup_roots_length(setup), NULL);

    while (screen_number--)
        xcb_screen_next(&iter);

    return iter.data;
}

static void
xcb_enqueue_events(XcbEventSource *xcb_event_source,
                   xcb_generic_event_t *(*poll)(xcb_connection_t *))
{
    xcb_generic_event_t *event;

    while (event = (*poll)(xcb_event_source->connection))
        g_queue_push_tail(xcb_event_source->queue, event);
}

static gboolean
xcb_event_prepare(GSource *source, gint *timeout)
{
    XcbEventSource *xcb_event_source = (XcbEventSource *)source;
    
    xcb_flush(xcb_event_source->connection);
#if XCB_POLL_FOR_QUEUED_EVENT
    xcb_enqueue_events(xcb_event_source, xcb_poll_for_queued_event);
#endif

    if (g_queue_is_empty(xcb_event_source->queue)) {
        *timeout = -1;
        return FALSE;
    } else {
        *timeout = 0;
        return TRUE;
    }
}

static gboolean
xcb_event_check(GSource *source)
{
    XcbEventSource *xcb_event_source = (XcbEventSource *)source;

    xcb_enqueue_events(xcb_event_source, xcb_poll_for_event);
    return !g_queue_is_empty(xcb_event_source->queue);
}

static gboolean
xcb_event_dispatch(GSource *source, GSourceFunc callback, gpointer user_data)
{
    XcbEventSource *xcb_event_source = (XcbEventSource *)source;
    XcbEventFunc xcb_event_callback = (XcbEventFunc)callback;
    xcb_generic_event_t *event;
    gboolean again = TRUE;

    if (!callback) {
        g_warning("XcbEvent source dispatched without a callback");
        return FALSE;
    }

    while (again && (event = g_queue_pop_head(xcb_event_source->queue))) {
        again = xcb_event_callback(xcb_event_source->connection, event, user_data);
        free(event);
    }
    return again;
}

static void
xcb_event_finalize(GSource *source)
{
    XcbEventSource *xcb_event_source = (XcbEventSource *)source;

    g_queue_free_full(xcb_event_source->queue, free);
}

GSource *
xcb_event_source_new(xcb_connection_t *connection)
{
    GSource *source = g_source_new(&xcb_event_funcs, sizeof(XcbEventSource));
    XcbEventSource *xcb_event_source = (XcbEventSource *)source;
    gint xcb_fd = xcb_get_file_descriptor(connection);
    GIOCondition fd_event_mask = G_IO_IN | G_IO_HUP | G_IO_ERR;

    xcb_event_source->connection = connection;
    xcb_event_source->queue = g_queue_new();

#if GLIB_CHECK_VERSION(2, 36, 0)
    g_source_add_unix_fd(source, xcb_fd, fd_event_mask);
#else
    xcb_event_source->poll.fd = xcb_fd;
    xcb_event_source->poll.events = fd_event_mask;
    g_source_add_poll(source, &xcb_event_source->poll);
#endif
    
    return source;
}

guint
xcb_event_add(xcb_connection_t *connection, XcbEventFunc function, gpointer data)
{
    guint id;
    GSource *source;

    g_return_val_if_fail(function != NULL, 0);
 
    source = xcb_event_source_new(connection);
    g_source_set_callback(source, (GSourceFunc)function, data, NULL);
    id = g_source_attach(source, NULL);
    g_source_unref(source);

    return id;
}
