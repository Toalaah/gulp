# Gulp

**gulp** is a small daemon program offering "swallowing" support for X11 window
managers. It detects new windows spawned from a configurable set of parent
windows (for instance your terminal emulator) and upon doing so will hide the
parent, effectively making the child "swallow" its parent until the process
exits, at which point the parent will be re-revealed.

**gulp** operates on a similar principle to **devour**. The primary difference
is that **gulp** does not require the user to prefix their command with
**devour**; instead, it will automatically listen for new windows spawned from
terminal emulators and act accordingly. It is essentially **bspswallow** or
**dwm** with a swallow-patch, with the exception of being window manager
agnostic.

## Installation

Dependencies:

- `libX11`
- `libXres`
- `procps`
- `tomlc99`
- `pandoc` (optional, if you want to build and install the manpage)

To build and install run:

```bash
make
sudo make install
```

You may also want to optionally install the Systemd unit:

```bash
make
sudo make SYSTEMD=1 install
```

## Usage

For more detailed documentation, refer to `gulp(1)`.

```
gulp [-h] [-v] [-verbose] [-prefer-net-wm] [-c <CONFIG>]
```

`-h`: Print usage and exit

`-v`: Print version and exit

`-verbose`: Enable verbose output

`-prefer-net-wm`: Prefer PID info supplied via the `_NET_WM_PID` atom (if it
exists). Note that processes can set this value arbitrarily. False by default.

`-c <CONFIG>`: Specify a path to a configuration file

## Configuration

**gulp** is configured in TOML:

```toml
[gulp]
  # Any window whose WM_CLASS atom matches one of these values is considered a
  # terminal emulator.
  terminals = [ "kitty", "st", "Alacritty" ]
  # Any window whose WM_CLASS atom matches one of these values will *not* be swalloed,
  # even when execd from a terminal.
  exceptions = [ "Zathura" ]
```

Unless specified via the `-c` flag, **gulp** will look in the following places
for configuration files, in order of precedence:

- `$XDG_CONFIG_HOME/gulp/config.toml` (if `XDG_CONFIG_HOME` is defined)
- `~/.config/gulp/config.toml`

## Acknowledgments & Related Projects

- [bspswallow](https://github.com/JopStro/bspswallow)
- [devour](https://github.com/salman-abedin/devour)
- [dwm swallow patch](https://dwm.suckless.org/patches/swallow)

## Roadmap

- [ ] Sometimes X window errors will cause the program to crash. This is not
  too much of an issue when using `Restart=OnFailure`, but should be caught and
  dealt with gracefully.
- [ ] Possibly add Wayland support?

## License

This project is released under the terms of the GPLv3 license
