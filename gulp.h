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

#ifndef GULP_H
#define GULP_H

#include <X11/X.h>
#include <X11/Xlib.h>
#include <stdbool.h>
#include <sys/types.h>
#include <toml.h>

#define PROGRAM_NAME "gulp"
#ifndef VERSION
#define VERSION "unknown"
#endif

#define MODE_HELP    1 << 0
#define MODE_VERSION 1 << 1
#define MODE_VERBOSE 1 << 2
#define MODE_NET_WM  1 << 3

struct window {
  unsigned long id;
  pid_t pid, ppid;
  unsigned char *name;
  // indicates whether this window is a terminal
  bool is_terminal;
  bool no_swallow;
  struct window *swallowing;
  struct window *next;
};

void gulp(struct window *parent, struct window *child);
void vomit(struct window *parent);

void init_window(struct window *w, Window from);
void update_window(struct window *w);
struct window *parent_window(struct window *w);

void init_window_list();
void append_to_window_list(struct window *w);
void remove_from_window_list(Window w);
struct window *window_list_contains(Window w);

void print_window_list();
void free_window_list();

void handle_event(struct window *w, XEvent *ev);
void handle_unmap(struct window *w, const Window last_window);
void handle_map(struct window *w, const Window last_window);
void handle_create(struct window *w, const Window last_window);
void handle_destroy(struct window *w, const Window last_window);

pid_t get_pid_from_net_wm(const struct window *w);
pid_t get_pid(const struct window *w);
pid_t get_parent(pid_t pid);
bool is_descendant_process(pid_t parent, pid_t child);

void parse_opts(int argc, char **argv);
toml_table_t *read_config(const char *path, char **errbuf);

int is_terminal(const struct window *w);
int is_exception(const struct window *w);

void usage(void);
void die(const char *msg, ...);
void debug(const char *msg, ...);

void handle_interrupt(int sig);

#endif // GULP_H
