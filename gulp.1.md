% GULP(1) Version VERSION | General Commands Manual

# NAME

**gulp** - A window-manager agnostic swallowing daemon

# SYNOPSIS

gulp [-h] [-v] [-verbose] [-prefer-net-wm] [-c CONFIG]

# DESCRIPTION

**gulp** is a simple utility offering "swallowing" support for X11 window
managers. It detects new windows spawned from a configurable set of parent
windows (for instance your terminal emulator) and upon doing so will hide the
parent, effectively making the child "swallow" its parent until the process
exits, at which point the parent will be re-revealed.

**gulp** operates on a similar principle to **devour**. The primary difference is
that **gulp** does not require the user to prefix their command with **devour**;
instead, it will automatically listen for new windows spawned from terminal
emulators and act accordingly. It is essentially **bspswallow** or **dwm** with a
swallow-patch, with the exception of being window manager agnostic.

# OPTIONS

**-h**
: print usage and exit

**-version**
: print version and exit

**-v**
: enable verbose output

**-prefer-net-wm**
: Prefer process information supplied via the **\_NET_WM_PID** atom (if it
exists). Note that processes can set this value arbitrarily. Off by default.

**-c** *CONFIG*
: specify a path to a configuration file. Defaults to the any of the following
files, given in order of precedence: **$XDG_CONFIG_HOME/gulp/config.toml**,
**~/.config/gulp/config.toml**. See the section
**[CONFIGURATION](#configuration)** for information on how to configure
**gulp**



# CONFIGURATION

**gulp** can be customized through a configuration file. This file must be
valid TOML.

## EXAMPLE

```toml
[gulp]
  terminals = [ "kitty", "st", "Alacritty" ]
  exceptions = [ "Zathura" ]
```

## TERMINALS

A list of strings to compare to windows' **WM_CLASS** atom. If the class name
matches any entry in this list, the window is considered a terminal emulator.
Any descendant windows of this window will swallow the parent. This option is configured by specifying the **terminal** key under the **gulp** header.

## EXCEPTIONS

A list of window classes that when matched by a new window will prevent it from
swallowing other windows, even if it descends from a terminal emulator. This
option is configured by specifying the **exceptions** key under the **gulp**
header.

# AUTHOR

Samuel Kunst <samuel at kunst.me>

# LICENSE

**gulp** is released under the terms of the GPLv3 license

# SEE ALSO

- [bspswallow](https://github.com/JopStro/bspswallow)
- [devour](https://github.com/salman-abedin/devour)
- [dwm swallow patch](https://dwm.suckless.org/patches/swallow)
