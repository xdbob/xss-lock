/* Copyright (c) 2013 Raymond Wagenmaker
 *
 * See LICENSE for the MIT license.
 */
#ifndef XCB_UTILS_H
#define XCB_UTILS_H

#include <glib.h>
#include <xcb/xcb.h>

G_BEGIN_DECLS

#define XCB_ERROR xcb_error_quark()

#define XCB_SCREENSAVER_PROPERTY_NAME "_MIT_SCREEN_SAVER_ID"

#define xcb_screensaver_notify_event_t xcb_screensaver_notify_event_t_fixed
typedef struct xcb_screensaver_notify_event_t_fixed {
    uint8_t         response_type; /**<  */
    uint8_t         state; /**<  */
    uint16_t        sequence; /**<  */
    xcb_timestamp_t time; /**<  */
    xcb_window_t    root; /**<  */
    xcb_window_t    window; /**<  */
    uint8_t         kind; /**<  */
    uint8_t         forced; /**<  */
    uint8_t         pad1[14]; /**<  */
} xcb_screensaver_notify_event_t_fixed;

GQuark xcb_error_quark(void) G_GNUC_CONST;

xcb_screen_t *xcb_get_screen(xcb_connection_t *connection, int screen_number);

typedef gboolean (*XcbEventFunc)(xcb_connection_t *connection, xcb_generic_event_t *event, gpointer user_data);

GSource *xcb_event_source_new(xcb_connection_t *connection);

guint xcb_event_add(xcb_connection_t *connection, XcbEventFunc function, gpointer data);

G_END_DECLS

#endif /* XCB_UTILS_H */
