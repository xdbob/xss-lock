#ifndef XCB_UTIL_H
#define XCB_UTIL_H

#include <glib.h>
#include <xcb/xcb.h>

#define XCB_ERROR xcb_error_quark()

G_BEGIN_DECLS

GQuark xcb_error_quark(void) G_GNUC_CONST;

xcb_screen_t *xcb_get_screen(xcb_connection_t *connection, int screen_number);

typedef gboolean (*XcbEventFunc)(xcb_connection_t *connection, xcb_generic_event_t *event, gpointer user_data);

GSource *xcb_event_source_new(xcb_connection_t *connection);

guint xcb_event_add(xcb_connection_t *connection, XcbEventFunc function, gpointer data);

G_END_DECLS

#endif /* XCB_UTIL_H */
