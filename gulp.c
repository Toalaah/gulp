/**
 * Copyright © 2023 Samuel Kunst <samuel at kunst.me>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <X11/Xlib.h>
#include <X11/extensions/XRes.h>

#include <errno.h>
#include <poll.h>
#include <proc/readproc.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/types.h>
#include <unistd.h>
#include <wchar.h>

#include <X11/X.h>
#include <X11/Xutil.h>

#include "gulp.h"

static toml_table_t *conf;
static toml_array_t *terminals, *exceptions;

static unsigned char opts;

#define set(mode)    opts |= mode
#define is_set(mode) (opts & mode)

static Display *display;
static Window root;
static struct window *windows;

static int running = 1;
int poll_ret;
int x11_fd;

void
event_handler(struct window *w, XEvent *ev) {
  switch (ev->type) {
  case MapNotify:
    handle_map(w, ev->xmap.window);
    break;
  case UnmapNotify:
    handle_unmap(w, ev->xunmap.window);
    break;
  case CreateNotify:
    handle_create(w, ev->xcreatewindow.window);
    break;
  case DestroyNotify:
    handle_destroy(w, ev->xdestroywindow.window);
    break;
  }
}

int
main(int argc, char *argv[]) {
  parse_opts(argc, argv);

  if (!(display = XOpenDisplay(NULL))) {
    die("cannot open display");
  }

  root = DefaultRootWindow(display);
  XSelectInput(display, root, SubstructureNotifyMask | StructureNotifyMask);

  init_window_list();

  signal(SIGINT, handle_interrupt);
  signal(SIGTERM, handle_interrupt);

  // https://stackoverflow.com/questions/61196629
  x11_fd = XConnectionNumber(display);
  XEvent ev;
  struct pollfd polls;
  polls.fd = x11_fd;
  polls.events = POLLIN;
  while (running) {
    poll_ret = poll(&polls, 1, 10);
    if (poll_ret < 0) {
      running = 0;
      continue;
    } else if (poll_ret > 0) {
      while (XPending(display) > 0) {
        XNextEvent(display, &ev);
        struct window *w = NULL;
        event_handler(w, &ev);
        print_window_list();
      }
    }
  }

  // TODO: catch sigint and cleanup
  // TODO: check and fix any possible memory leaks
  if (display) {
    XCloseDisplay(display);
  }
  free(conf);
  exit(EXIT_SUCCESS);
}

// Swallowing logic is largely "adapted" from https://dwm.suckless.org/patches/swallow/. Credit to original authors,
// see link.
void
gulp(struct window *parent, struct window *child) {
  if (child->no_swallow || !child || child->is_terminal || parent->swallowing || !parent->is_terminal) {
    return;
  }
  parent->swallowing = child;
  XUnmapWindow(display, parent->id);
  Window tmp = parent->id;
  parent->id = child->id;
  child->id = tmp;
}

void
vomit(struct window *w) {
  Window tmp = w->id;
  debug("vomiting: [%lu '%s']", w->pid, w->name);
  w->id = w->swallowing->id;
  w->swallowing->id = tmp;
  w->swallowing = NULL;
  XMapWindow(display, w->id);
}

pid_t
get_parent(pid_t pid) {
  proc_t proc_info;
  memset(&proc_info, 0, sizeof(proc_info));
  PROCTAB *pt_ptr = openproc(PROC_FILLSTATUS | PROC_PID, &pid);
  if (readproc(pt_ptr, &proc_info) != 0) {
    return proc_info.ppid;
  }
  return 0;
}

void
usage(void) {
  const char *usage = PROGRAM_NAME " [-h] [-v] [-verbose] [-prefer-net-wm] [-c CONFIG]\n"
                                   "    -h               print this help and exit\n"
                                   "    -version         print version and exit\n"
                                   "    -v               enable verbose output\n"
                                   "    -prefer-net-wm   try to read PIDs from the _NET_WM_PID atom\n"
                                   "    -c CONFIG        path to config\n";
  printf("%s", usage);
}

bool
is_descendant_process(pid_t parent, pid_t child) {
  while (child != 0 && child != parent) {
    child = get_parent(child);
  }
  return child == parent;
}

void
update_window(struct window *w) {
  XTextProperty text_prop;
  if (XGetTextProperty(display, w->id, &text_prop, XInternAtom(display, "WM_CLASS", false))) {
    w->name = text_prop.value;
  }
  w->is_terminal = is_terminal(w);
  w->no_swallow = is_exception(w);
  w->pid = get_pid(w);
  w->ppid = get_parent(w->pid);
}

pid_t
get_pid_from_net_wm(const struct window *w) {
  XTextProperty prop;
  if (XGetTextProperty(display, w->id, &prop, XInternAtom(display, "_NET_WM_PID", false))) {
    return *(pid_t *)prop.value;
  }
  return (pid_t)(-1);
}

pid_t
get_pid(const struct window *w) {
  pid_t pid;
  XResClientIdSpec spec;
  long num_ids;
  XResClientIdValue *client_ids;
  long i;

  pid = -1;

  if (is_set(MODE_NET_WM)) {
    if ((pid = get_pid_from_net_wm(w)) != -1) {
      return pid;
    }
  }

  spec.client = w->id;
  spec.mask = XRES_CLIENT_ID_PID_MASK;

  XResQueryClientIds(display, 1, &spec, &num_ids, &client_ids);

  for (i = 0; i < num_ids; i++) {
    if (client_ids[i].spec.mask == XRES_CLIENT_ID_PID_MASK) {
      pid = XResGetClientPid(&client_ids[i]);
      break;
    }
  }

  XResClientIdsDestroy(num_ids, client_ids);

  return pid;
}

void
init_window(struct window *w, Window from) {
  w->id = from;
  w->next = NULL;
  w->swallowing = NULL;
  w->is_terminal = false;
  w->name = (unsigned char *)"";
  w->pid = 0;
  w->ppid = 0;

  XTextProperty text_prop;
  if (XGetTextProperty(display, from, &text_prop, XInternAtom(display, "WM_CLASS", false))) {
    w->name = text_prop.value;
  }

  w->is_terminal = is_terminal(w);
  w->no_swallow = is_exception(w);

  // if (!w->is_terminal) {
  //   return;
  // }

  w->pid = get_pid(w);
  w->ppid = get_parent(w->pid);
}

void
append_to_window_list(struct window *w) {
  if (!w) {
    return;
  }
  if (!windows) {
    windows = w;
    return;
  }

  struct window *tmp = windows;
  while (tmp->next) {
    tmp = tmp->next;
  }
  tmp->next = w;
}

struct window *
parent_window(struct window *w) {
  if (w->pid == 0) {
    return NULL;
  }
  struct window *tmp = windows;
  while (tmp) {
    if (tmp->pid == 0 || tmp->pid == w->pid) {
      tmp = tmp->next;
      continue;
    }
    if (is_descendant_process(tmp->pid, w->pid)) {
      return tmp;
    }
    tmp = tmp->next;
  }
  return NULL;
}

struct window *
window_list_contains(Window w) {
  struct window *tmp = windows;
  while (tmp) {
    if (tmp->id == w) {
      return tmp;
    }
    tmp = tmp->next;
  }
  return NULL;
}

void
remove_from_window_list(Window w) {
  if (!windows) {
    return;
  }
  struct window *tmp = windows;
  while (tmp->next) {
    if (tmp->next->id == w) {
      struct window *to_remove = tmp->next;
      tmp->next = to_remove->next;
      free(to_remove);
      return;
    }
    tmp = tmp->next;
  }
}

void
init_window_list() {
  Atom type;
  int format;
  unsigned long len, bytes;
  unsigned char *prop_data;
  debug("initiziale window list");
  if (XGetWindowProperty(display, root, XInternAtom(display, "_NET_CLIENT_LIST", false), 0, 0, false, AnyPropertyType,
                         &type, &format, &len, &bytes, &prop_data) != Success) {
    die("cannot get client list property for root window");
  }
  XGetWindowProperty(display, root, XInternAtom(display, "_NET_CLIENT_LIST", false), 0, bytes, false, AnyPropertyType,
                     &type, &format, &len, &bytes, &prop_data);

  if (len == 0) {
    return;
  }

  windows = NULL;
  unsigned long *raw_data = (unsigned long *)prop_data;
  for (size_t i = 0; i < len; i++) {
    struct window *tmp = malloc(sizeof(struct window));
    init_window(tmp, raw_data[i]);
    append_to_window_list(tmp);
  }
}

void
free_window_list() {
  struct window *tmp = windows;
  while (windows != NULL) {
    tmp = windows;
    windows = windows->next;
    free(tmp);
  }
}

void
print_window_list() {
  if (!(is_set(MODE_VERBOSE))) {
    return;
  }
  struct window *tmp = windows;
  while (tmp) {
    debug("{ id: %lu; is_terminal: %d; no_swallow: %d; name: %s; pid: %d; ppid: %d }", tmp->id, tmp->is_terminal,
          tmp->no_swallow, tmp->name, tmp->pid, tmp->ppid);
    tmp = tmp->next;
  }
  printf("\n");
}

void
handle_unmap(struct window *w, const Window last_window) {
  w = window_list_contains(last_window);
  if (!w) {
    return;
  }
  debug("got unmap event: [%lu '%s']", w->id, w->name);
  // if (w->swallowing) {
  //   debug("revealing parent window [%lu '%s'] of unmapped window [%lu '%s']", w->swallowing->id,
  //   w->swallowing->name,
  //         w->id, w->name);
  //   vomit(w);
  // }
}

void
handle_map(struct window *w, const Window last_window) {
  w = window_list_contains(last_window);
  if (!w) {
    return;
  }
  update_window(w);
  debug("got map event: [%lu '%s']", w->id, w->name);
  struct window *p;
  // check if the newly mapped window was spawned by a parent window that we track
  if ((p = parent_window(w))) {
    debug("found parent: [%lu '%s'], child: [%lu '%s']", p->id, p->name, w->id, w->name);
    gulp(p, w);
  }
}

void
handle_create(struct window *w, const Window last_window) {
  w = malloc(sizeof(struct window));
  init_window(w, last_window);
  debug("got create event: [%lu '%s']", last_window, w->name);
  append_to_window_list(w);
}

void
handle_destroy(struct window *w, const Window last_window) {
  w = window_list_contains(last_window);
  if (!w) {
    return;
  }
  debug("got destroy event: [%lu '%s']", w->id, w->name);
  if (w->swallowing) {
    debug("revealing parent window [%lu '%s'] of unmapped window [%lu '%s']", w->swallowing->id, w->swallowing->name,
          w->id, w->name);
    vomit(w);
  }
  remove_from_window_list(last_window);
}

void
parse_opts(int argc, char **argv) {
  char *config_path = NULL;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-h") == 0) {
      set(MODE_HELP);
    } else if (strcmp(argv[i], "-version") == 0) {
      set(MODE_VERSION);
    } else if (strcmp(argv[i], "-v") == 0) {
      set(MODE_VERBOSE);
    } else if (strcmp(argv[i], "-prefer-net-wm") == 0) {
      set(MODE_NET_WM);
    } else if (strcmp(argv[i], "-c") == 0) {
      if (i + 1 >= argc) {
        die("missing path for config");
      }
      config_path = strdup(argv[++i]);
    }
  }

  if (is_set(MODE_HELP)) {
    usage();
    free(config_path);
    exit(EXIT_SUCCESS);
  }

  if (is_set(MODE_VERSION)) {
    printf("%s\n", VERSION);
    free(config_path);
    exit(EXIT_SUCCESS);
  }

  if (!config_path) {
    size_t alloc_sz;
    const char *fmt_str, *tmp;

    tmp = getenv("XDG_CONFIG_HOME");
    if (!tmp) {
      tmp = getenv("HOME");
      fmt_str = "%s/.config/gulp/config.toml";
    } else {
      fmt_str = "%s/gulp/config.toml";
    }

    alloc_sz = snprintf(NULL, 0, fmt_str, tmp);
    config_path = malloc(alloc_sz + 1);
    sprintf(config_path, fmt_str, tmp);
  }

  char *errbuf = malloc(200);
  conf = read_config(config_path, &errbuf);
  free(config_path);

  if (!conf) {
    fprintf(stderr, "%s: error reading config: %s\n", PROGRAM_NAME, errbuf);
    free(errbuf);
    exit(EXIT_FAILURE);
  }

  // the [gulp]  table in the conf file
  toml_table_t *gulp_opts = toml_table_in(conf, "gulp");
  if (!gulp_opts) {
    fprintf(stderr, "%s: no options found for toml table [gulp]\n", PROGRAM_NAME);
    return;
  }

  if (!(terminals = toml_array_in(gulp_opts, "terminals"))) {
    fprintf(stderr, "%s: no option 'terminals' found\n", PROGRAM_NAME);
  }

  if (!(exceptions = toml_array_in(gulp_opts, "exceptions"))) {
    fprintf(stderr, "%s: no option 'exceptions' found\n", PROGRAM_NAME);
  }
}

toml_table_t *
read_config(const char *path, char **errbuf) {
  FILE *fp;

  debug("read config from %s", path);

  fp = fopen(path, "r");
  if (!fp) {
    free(*errbuf);
    *errbuf = strdup(strerror(errno));
    return NULL;
  }

  toml_table_t *conf = toml_parse_file(fp, *errbuf, sizeof(*errbuf));
  fclose(fp);

  return conf;
}

void
die(const char *msg, ...) {
  va_list args;
  fflush(stderr);
  fprintf(stderr, "%s: ", PROGRAM_NAME);
  va_start(args, msg);
  vfprintf(stderr, msg, args);
  va_end(args);
  fprintf(stderr, "\n");
  exit(EXIT_FAILURE);
}

void
debug(const char *msg, ...) {
  if (!(is_set(MODE_VERBOSE))) {
    return;
  }

  va_list args;
  fflush(stderr);
  fprintf(stderr, "%s: ", PROGRAM_NAME);
  va_start(args, msg);
  vfprintf(stderr, msg, args);
  va_end(args);
  fprintf(stderr, "\n");
}

int
is_terminal(const struct window *w) {
  for (int i = 0;; i++) {
    toml_datum_t s = toml_string_at(terminals, i);
    if (!s.ok) {
      return 0;
    }
    if (!strcmp((const char *)w->name, s.u.s)) {
      return 1;
    }
  }
  return 0;
}

int
is_exception(const struct window *w) {
  if (w->is_terminal)
    return 0;
  for (int i = 0;; i++) {
    toml_datum_t s = toml_string_at(exceptions, i);
    if (!s.ok)
      return 0;
    if (!strcmp((const char *)w->name, s.u.s)) {
      return 1;
    }
  }
  return 0;
}

void
handle_interrupt(int sig __attribute__((unused))) {
  fprintf(stderr, "%s: caught interrupt\n", PROGRAM_NAME);
  running = false;
}
