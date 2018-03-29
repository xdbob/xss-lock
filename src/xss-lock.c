/* Copyright (c) 2013-2014 Raymond Wagenmaker
 *
 * See LICENSE for the MIT license.
 */
#include <locale.h>
#include <fcntl.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <glib-unix.h>
#include <gio/gio.h>
#include <gio/gunixfdlist.h>
#include <xcb/xcb.h>
#include <xcb/xcb_aux.h>
#include <xcb/xcb_event.h>
#include <xcb/screensaver.h>

#include "config.h"
#include "xcb_utils.h"

#define LOGIND_SERVICE "org.freedesktop.login1"
#define LOGIND_PATH    "/org/freedesktop/login1"
#define LOGIND_MANAGER_INTERFACE "org.freedesktop.login1.Manager"
#define LOGIND_SESSION_INTERFACE "org.freedesktop.login1.Session"

typedef struct Child {
    gchar        *name;
    gchar       **cmd;
    GPid          pid;
    gboolean      transfer_sleep_lock_fd;
    struct Child *kill_first;
} Child;

static gboolean register_screensaver(xcb_connection_t *connection, xcb_screen_t *screen, xcb_atom_t *atom, GError **error);
static void unregister_screensaver(xcb_connection_t *connection, xcb_screen_t *screen, xcb_atom_t atom);
static gboolean screensaver_event_cb(xcb_connection_t *connection, xcb_generic_event_t *event, const int *xcb_screensaver_notify);

static void keep_sleep_lock_fd_open(gpointer user_data);
static void start_child(Child *child);
static void kill_child(Child *child);
static void child_watch_cb(GPid pid, gint status, Child *child);

static void logind_manager_proxy_new_cb(GObject *source_object, GAsyncResult *res, gpointer user_data);
static void logind_manager_take_sleep_delay_lock(void);
static void logind_manager_call_inhibit_cb(GObject *source_object, GAsyncResult *res, gpointer user_data);
static void logind_manager_on_signal_prepare_for_sleep(GDBusProxy *proxy, gchar *sender_name, gchar *signal_name, GVariant *parameters, gpointer user_data);
static void logind_manager_call_get_session_cb(GObject *source_object, GAsyncResult *res, gpointer user_data);
static void logind_session_proxy_new_cb(GObject *source_object, GAsyncResult *res, gpointer user_data);
static void logind_session_on_signal_lock(GDBusProxy *proxy, gchar *sender_name, gchar *signal_name, GVariant *parameters, gpointer user_data);
static void logind_session_set_idle_hint(gboolean idle);

static gboolean parse_options(int argc, char *argv[], GError **error);
static gboolean parse_notifier_cmd(const gchar *option_name, const gchar *value, gpointer data, GError **error);
static gboolean reset_screensaver(xcb_connection_t *connection);
static gboolean exit_service(GMainLoop *loop);
static void log_handler(const gchar *log_domain, GLogLevelFlags log_level, const gchar *message, gpointer user_data);

static Child notifier = {"notifier", NULL, 0, FALSE, NULL};
static Child locker = {"locker", NULL, 0, FALSE, &notifier};
static gboolean opt_quiet = FALSE;
static gboolean opt_verbose = FALSE;
static gboolean opt_ignore_sleep = FALSE;
static gboolean opt_print_version = FALSE;
static gchar *opt_session = NULL;

static GOptionEntry opt_entries[] = {
    {G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &locker.cmd, NULL, "LOCK_CMD [ARG...]"},
    {"notifier", 'n', G_OPTION_FLAG_FILENAME, G_OPTION_ARG_CALLBACK, parse_notifier_cmd, "Send notification using CMD", "CMD"},
    {"transfer-sleep-lock", 'l', 0, G_OPTION_ARG_NONE, &locker.transfer_sleep_lock_fd, "Pass sleep delay lock file descriptor to locker", NULL},
    {"ignore-sleep", 0, 0, G_OPTION_ARG_NONE, &opt_ignore_sleep, "Do not lock on suspend/hibernate", NULL},
    {"quiet", 'q', 0, G_OPTION_ARG_NONE, &opt_quiet, "Output only fatal errors", NULL},
    {"verbose", 'v', 0, G_OPTION_ARG_NONE, &opt_verbose, "Output more messages", NULL},
    {"version", 0, 0, G_OPTION_ARG_NONE, &opt_print_version, "Print version number and exit", NULL},
    {"session", 's', 0, G_OPTION_ARG_STRING, &opt_session, "Use ID instead of the current session", "ID"},
    {NULL}
};

static GDBusProxy *logind_manager = NULL;
static GDBusProxy *logind_session = NULL;
static gint sleep_lock_fd = -1;
static gboolean preparing_for_sleep = FALSE;

static gboolean
register_screensaver(xcb_connection_t *connection, xcb_screen_t *screen,
                     xcb_atom_t *atom, GError **error)
{
    uint32_t xid;
    const xcb_query_extension_reply_t *extension_reply;
    xcb_screensaver_query_version_cookie_t version_cookie;
    xcb_screensaver_query_version_reply_t *version_reply = NULL;
    xcb_intern_atom_cookie_t atom_cookie;
    xcb_intern_atom_reply_t *atom_reply = NULL;
    xcb_void_cookie_t set_attributes_cookie;
    xcb_generic_error_t *xcb_error = NULL;

    xcb_prefetch_extension_data(connection, &xcb_screensaver_id);
    xid = xcb_generate_id(connection);
    xcb_create_pixmap(connection, screen->root_depth, xid, screen->root, 1, 1);
    atom_cookie = xcb_intern_atom(connection, FALSE,
                                  strlen(XCB_SCREENSAVER_PROPERTY_NAME),
                                  XCB_SCREENSAVER_PROPERTY_NAME);
    extension_reply = xcb_get_extension_data(connection, &xcb_screensaver_id);
    if (!extension_reply || !extension_reply->present) {
        g_set_error(error, XCB_ERROR, 0, "Screensaver extension unavailable");
        goto out;
    }

    version_cookie = xcb_screensaver_query_version(connection, 1, 0);
    set_attributes_cookie =
        xcb_screensaver_set_attributes_checked(connection, screen->root,
                                               -1, -1, 1, 1, 0,
                                               XCB_COPY_FROM_PARENT,
                                               XCB_COPY_FROM_PARENT,
                                               XCB_COPY_FROM_PARENT,
                                               0, NULL);

    xcb_screensaver_select_input(connection, screen->root,
                                 XCB_SCREENSAVER_EVENT_NOTIFY_MASK |
                                 XCB_SCREENSAVER_EVENT_CYCLE_MASK);

    version_reply = xcb_screensaver_query_version_reply(connection,
                                                        version_cookie,
                                                        &xcb_error);
    if (xcb_error = xcb_request_check(connection, set_attributes_cookie)) {
        g_set_error(error, XCB_ERROR, 0, "Failed to set screensaver attributes; "
                                         "is another one running?");
        goto out;
    }

    atom_reply = xcb_intern_atom_reply(connection, atom_cookie, &xcb_error);
    *atom = atom_reply->atom;
    xcb_change_property(connection, XCB_PROP_MODE_REPLACE, screen->root,
                        *atom, XCB_ATOM_PIXMAP, 32, 1, &xid);

    xcb_event_add(connection, (XcbEventFunc)screensaver_event_cb,
                  (void *)&extension_reply->first_event);

out:
    if (version_reply) free(version_reply);
    if (atom_reply) free(atom_reply);
    if (xcb_error) {
        free(xcb_error);
        return FALSE;
    }
    return TRUE;
}

static void
unregister_screensaver(xcb_connection_t *connection, xcb_screen_t *screen,
                       xcb_atom_t atom)
{
    xcb_screensaver_unset_attributes(connection, screen->root);
    xcb_delete_property(connection, screen->root, atom);
}

static gboolean
screensaver_event_cb(xcb_connection_t *connection, xcb_generic_event_t *event,
                     const int *const xcb_screensaver_notify)
{
    uint8_t event_type;
    
    if (!event)
        g_critical("X connection lost; exiting.");
    
    event_type = XCB_EVENT_RESPONSE_TYPE(event);
    if (event_type == 0) {
        xcb_generic_error_t *error = (xcb_generic_error_t *)event;

        g_warning("X error: %s", xcb_event_get_error_label(error->error_code));
    } else if (event_type == *xcb_screensaver_notify) {
        xcb_screensaver_notify_event_t *xss_event =
            (xcb_screensaver_notify_event_t *)event;

        switch (xss_event->state) {
        case XCB_SCREENSAVER_STATE_ON:
            if (xss_event->kind == XCB_SCREENSAVER_KIND_INTERNAL)
                /* According to the original protocol, this forces the external
                 * saver (i.e., me) to be started after deactivating the
                 * internal saver, which may be started if the server is
                 * grabbed when the saver activates, but Xorg does not seem to
                 * work that way; I'm leaving this in anyway.
                 */
                xcb_force_screen_saver(connection, XCB_SCREEN_SAVER_ACTIVE);
            else if (!notifier.cmd || xss_event->forced) {
                start_child(&locker);
                logind_session_set_idle_hint(TRUE);
            } else if (!locker.pid)
                start_child(&notifier);
            else
                logind_session_set_idle_hint(TRUE);
            break;
        case XCB_SCREENSAVER_STATE_OFF:
            kill_child(&notifier);
            logind_session_set_idle_hint(FALSE);
            break;
        case XCB_SCREENSAVER_STATE_CYCLE:
            if (!locker.pid) {
                logind_session_set_idle_hint(TRUE);
                start_child(&locker);
            }
            break;
        }
    }
    return TRUE;
}

static void
keep_sleep_lock_fd_open(gpointer user_data)
{
    fcntl(sleep_lock_fd, F_SETFD, ~FD_CLOEXEC & fcntl(sleep_lock_fd, F_GETFD));
}

static void
start_child(Child *child)
{
    GSpawnFlags flags = G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD;
    GSpawnChildSetupFunc setup = NULL;
    gchar **env = NULL;
    GError *error = NULL;

    if (child->pid)
        return;

    if (child->kill_first)
        kill_child(child->kill_first);

    if (preparing_for_sleep && child->transfer_sleep_lock_fd) {
        gchar *fd = g_strdup_printf("%d", sleep_lock_fd);
        env = g_environ_setenv(g_get_environ(), "XSS_SLEEP_LOCK_FD", fd, TRUE);
        g_free(fd);

        flags |= G_SPAWN_LEAVE_DESCRIPTORS_OPEN;
        setup = keep_sleep_lock_fd_open;
    }

    if (!g_spawn_async(NULL, child->cmd, env, flags, setup, NULL, &child->pid, &error)) {
        g_warning("Error spawning %s: %s", child->name, error->message);
        g_error_free(error);
        goto out;
    }
    g_child_watch_add(child->pid, (GChildWatchFunc)child_watch_cb, child);

out:
    g_strfreev(env);
}

static void
kill_child(Child *child)
{
    if (child->pid && kill(child->pid, SIGTERM))
        g_warning("Error sending SIGTERM to %s: %s", child->name, g_strerror(errno));
}

static void
child_watch_cb(GPid pid, gint status, Child *child)
{
#if GLIB_CHECK_VERSION(2, 34, 0)
    GError *error = NULL;

    if (!g_spawn_check_exit_status(status, &error)) {
        g_message("%s exited abnormally: %s", child->name, error->message);
        g_error_free(error);
    }
#endif
    child->pid = 0;
    g_spawn_close_pid(pid);
}

static void
logind_manager_proxy_new_cb(GObject *source_object, GAsyncResult *res,
                            gpointer user_data)
{
    GError *error = NULL;

    logind_manager = g_dbus_proxy_new_for_bus_finish(res, &error);

    if (!logind_manager) {
        g_warning("Error connecting to systemd login manager: %s", error->message);
        g_error_free(error);
        return;
    }
    if (!opt_ignore_sleep) {
        g_signal_connect(logind_manager, "g-signal",
                         G_CALLBACK(logind_manager_on_signal_prepare_for_sleep), NULL);
        logind_manager_take_sleep_delay_lock();
    }

    GVariant *data;
    gchar *name;
    if (!user_data) {
        data = g_variant_new("(u)", getpid());
        name = "GetSessionByPID";
    } else {
        data = g_variant_new("(s)", user_data);
        name = "GetSession";
    }

    g_dbus_proxy_call(logind_manager, name, data, G_DBUS_CALL_FLAGS_NONE,
                      -1, NULL, logind_manager_call_get_session_cb, NULL);
}

static void
logind_manager_take_sleep_delay_lock(void)
{
    if (sleep_lock_fd >= 0)
        return;

    g_dbus_proxy_call_with_unix_fd_list(logind_manager, "Inhibit",
                                        g_variant_new("(ssss)", "sleep", APP_NAME,
                                                      "Lock screen first", "delay"),
                                        G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL,
                                        logind_manager_call_inhibit_cb, NULL);
}

static void
logind_manager_call_inhibit_cb(GObject *source_object, GAsyncResult *res,
                               gpointer user_data)
{
    GVariant *result = NULL;
    GError *error = NULL;
    GUnixFDList *fd_list;
    gint32 fd_index = 0;
    
    result = g_dbus_proxy_call_with_unix_fd_list_finish(logind_manager,
                                                        &fd_list, res, &error);
    if (!result) {
        g_warning("Error taking sleep inhibitor lock: %s", error->message);
        g_error_free(error);
        return;
    }

    g_variant_get(result, "(h)", &fd_index);
    sleep_lock_fd = g_unix_fd_list_get(fd_list, fd_index, &error);
    if (sleep_lock_fd == -1) {
        g_warning("Error getting file descriptor for sleep inhibitor lock: %s",
                   error->message);
        g_error_free(error);
    }
    g_variant_unref(result);
    g_object_unref(fd_list);
}

static void
logind_manager_on_signal_prepare_for_sleep(GDBusProxy *proxy,
                                           gchar      *sender_name,
                                           gchar      *signal_name,
                                           GVariant   *parameters,
                                           gpointer    user_data)
{
    gboolean active;

    if (g_strcmp0(signal_name, "PrepareForSleep"))
        return;

    g_variant_get(parameters, "(b)", &active);
    if (active) {
        preparing_for_sleep = TRUE;

        start_child(&locker);

        if (sleep_lock_fd >= 0) {
            close(sleep_lock_fd);
            sleep_lock_fd = -1;
        }
        preparing_for_sleep = FALSE;
    } else
        logind_manager_take_sleep_delay_lock();
}

static void
logind_manager_call_get_session_cb(GObject *source_object, GAsyncResult *res,
                                   gpointer user_data)
{
    GVariant *result;
    GError *error = NULL;
    gchar *session_object_path = NULL;

    result = g_dbus_proxy_call_finish(logind_manager, res, &error);
    if (!result) {
        g_warning("Error getting session: %s", error->message);
        g_error_free(error);
        return;
    }
    g_variant_get(result, "(o)", &session_object_path);
    g_dbus_proxy_new_for_bus(G_BUS_TYPE_SYSTEM,
                             G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES, NULL,
                             LOGIND_SERVICE, session_object_path,
                             LOGIND_SESSION_INTERFACE, NULL,
                             logind_session_proxy_new_cb, NULL);
    g_variant_unref(result);
    g_free(session_object_path);
}

static void
logind_session_proxy_new_cb(GObject *source_object, GAsyncResult *res,
                            gpointer user_data)
{
    GError *error = NULL;

    logind_session = g_dbus_proxy_new_for_bus_finish(res, &error);

    if (!logind_session) {
        g_warning("Error connecting to session: %s", error->message);
        g_error_free(error);
        return;
    }
    g_signal_connect(logind_session, "g-signal",
                     G_CALLBACK(logind_session_on_signal_lock), NULL);
}

static void
logind_session_on_signal_lock(GDBusProxy *proxy,
                              gchar      *sender_name,
                              gchar      *signal_name,
                              GVariant   *parameters,
                              gpointer    user_data)
{
    if (!g_strcmp0(signal_name, "Lock"))
        start_child(&locker);
    else if (!g_strcmp0(signal_name, "Unlock"))
        kill_child(&locker);
}

static void
logind_session_set_idle_hint(gboolean idle)
{
    if (logind_session)
        g_dbus_proxy_call(logind_session, "SetIdleHint", g_variant_new("(b)", idle),
                          G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
}

static gboolean
parse_options(int argc, char *argv[], GError **error)
{
    GOptionContext *opt_context;
    gboolean success;

    opt_context = g_option_context_new("- use external locker as X screen saver");
    g_option_context_add_main_entries(opt_context, opt_entries, NULL);
    success = g_option_context_parse(opt_context, &argc, &argv, error);
    g_option_context_free(opt_context);

    if (success && !locker.cmd) {
        g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_FAILED,
                    "No %s specified", locker.name);
        success = FALSE;
    }
    return success;
}

static gboolean
parse_notifier_cmd(const gchar *option_name, const gchar *value,
                   gpointer data, GError **error)
{
    GError *parse_error = NULL;

    if (!g_shell_parse_argv(value, NULL, &notifier.cmd, &parse_error)) {
        g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_FAILED,
                    "Error parsing argument for %s: %s",
                    option_name, parse_error->message);
        g_error_free(parse_error);
        return FALSE;
    }
    return TRUE;
}

static gboolean
reset_screensaver(xcb_connection_t *connection)
{
    if (!locker.pid)
        xcb_force_screen_saver(connection, XCB_SCREEN_SAVER_RESET);
    return TRUE;
}

static gboolean
exit_service(GMainLoop *loop)
{
    kill_child(&notifier);
    kill_child(&locker);
    g_main_loop_quit(loop);
    return TRUE;
}

static void
log_handler(const gchar *log_domain, GLogLevelFlags log_level,
            const gchar *message, gpointer user_data)
{
    if (opt_verbose || log_level & G_LOG_FLAG_FATAL
        || !(opt_quiet || log_level & G_LOG_LEVEL_MESSAGE))
        g_log_default_handler(log_domain, log_level, message, user_data);
}

int
main(int argc, char *argv[])
{
    GMainLoop *loop;
    GError *error = NULL;
    xcb_connection_t *connection = NULL;
    int default_screen_number;
    xcb_screen_t *default_screen;
    xcb_atom_t atom;

    setlocale(LC_ALL, "");
    
    if (!parse_options(argc, argv, &error) || opt_print_version) {
        if (opt_print_version) {
            g_print(VERSION "\n");
            g_clear_error(&error);
        }
        goto init_error;
    }
    g_log_set_default_handler(log_handler, NULL);
    g_log_set_fatal_mask(NULL, G_LOG_LEVEL_CRITICAL);

    connection = xcb_connect(NULL, &default_screen_number);
    if (xcb_connection_has_error(connection)) {
        g_set_error(&error, XCB_ERROR, 0, "Connecting to X server failed");
        goto init_error;
    }

#if !GLIB_CHECK_VERSION(2, 36, 0)
    g_type_init();
#endif
 
    g_dbus_proxy_new_for_bus(G_BUS_TYPE_SYSTEM,
                             G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES, NULL,
                             LOGIND_SERVICE, LOGIND_PATH, LOGIND_MANAGER_INTERFACE,
                             NULL, logind_manager_proxy_new_cb, opt_session);

    loop = g_main_loop_new(NULL, FALSE);
    g_unix_signal_add(SIGTERM, (GSourceFunc)exit_service, loop);
    g_unix_signal_add(SIGINT,  (GSourceFunc)exit_service, loop);
    g_unix_signal_add(SIGHUP,  (GSourceFunc)exit_service, loop);

    default_screen = xcb_aux_get_screen(connection, default_screen_number);
    if (!register_screensaver(connection, default_screen, &atom, &error))
        goto init_error;

    g_main_loop_run(loop);

    unregister_screensaver(connection, default_screen, atom);
    g_main_loop_unref(loop);
    if (sleep_lock_fd >= 0) close(sleep_lock_fd);
    if (logind_manager) g_object_unref(logind_manager);
    if (logind_session) g_object_unref(logind_session);

init_error:
    g_strfreev(notifier.cmd);
    g_strfreev(locker.cmd);
    if (connection) xcb_disconnect(connection);

    if (error) {
        g_printerr("%s\n", error->message);
        if (error->domain == G_OPTION_ERROR)
            g_printerr("Use --help or -h to see usage information.\n");
        g_error_free(error);
        exit(EXIT_FAILURE);
    }
    exit(EXIT_SUCCESS);
}
