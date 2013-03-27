#ifndef X_EVENT_SOURCE_H
#define X_EVENT_SOURCE_H

#include <xcb/xcb.h>
#include <glib.h>

G_BEGIN_DECLS

typedef gboolean (*XEventFunc)(xcb_generic_event_t *event, gpointer user_data);

GSource *x_event_source_new(xcb_connection_t *connection);

guint x_event_add(xcb_connection_t *connection, XEventFunc function, gpointer data);

G_END_DECLS

#endif /* X_EVENT_SOURCE_H */
