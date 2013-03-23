#ifndef X_EVENT_H  // more underscores?
#define X_EVENT_H

G_BEGIN_DECLS

GSource *x_event_source_new(Display *display);

guint x_event_add(Display *display, GSourceFunc function, gpointer data);

G_END_DECLS

#endif /* X_EVENT_H */
