# Building

The following steps can be used to build `gnome-monitor-config`

```shell
$ meson build
$ cd build
$ meson compile
```

The output binary can be found in `build/src/gnome-monitor-config`.

# Usage

For usage details, run

```shell
$ ./gnome-monitor-config --help

Usage: ./src/gnome-monitor-config [OPTIONS...] COMMAND [COMMAND OPTIONS...]
Options:
 -h, --help                  Print help text

Commands:
  list                       List current monitors and current configuration
  set                        Set new configuration
  show                       Show monitor labels

Options for 'set':
 -L, --logical-monitor       Add logical monitor
 -x, --x=X                   Set x position of newly added logical monitor
 -y, --y=Y                   Set y position of newly added logical monitor
 -s, --scale=SCALE           Set scale of newly added logical monitor
 -t, --transform=TRANSFORM   Set transform (normal, left, right, flip)
 -p, --primary               Mark the newly added logical monitor as primary
 -m, --mode                  Set the display resolution and refresh rate. ex: 1920x1080@60
 -M, --monitor=CONNECTOR     Add a monitor (given its connector) to newly added
                             logical monitor
 -p, --primary               Mark the newly added logical monitor as primary
 --logical-layout-mode       Set logical layout mode
 --physical-layout-mode      Set physical layout mode
```

## Single-Monitor configuration

Start with `gnome-monitor-config list` to get a list of available monitors and configurations.

```shell
$ gnome-monitor-config list
...
  800x600@59.8614 [id: '800x600@59.861'] [preferred scale = 1 (1)]
  720x576@50 [id: '720x576@50.000'] [preferred scale = 1 (1)]
Logical monitor [ 2560x1440+0+0 ], scale = 1, transform = left
  DP-3
Logical monitor [ 3840x1600+1440+270 ], PRIMARY, scale = 1, transform = normal
  DP-1
Max screen size: unlimited
```
For example to set a single display to a particular configuration:

```shell
$ gnome-monitor-config set -LpM DP-1 -t normal -m 3840x1600@143.998
```

This sets the display attached to `DP-1` as the primary display
(specified with `-p`) at `3840x1600@143.998` configuration with
a `normal` orientation.

## Multi-Monitor config

Similarly, to setup multple monitors. First use the `list` command to get a
list of what is available, then simply:

```shell
$ gnome-monitor-config set -LM DP-3 -m 2560x1440@143.912 -t left -LpM DP-1 -m 3840x1600@143.998" -x 1440 -y 270 -t normal 
```

This command sets the monitor attached to DP-3 at the starting point [0, 0],
configured at 2560x1440@143.912 and left transformed (portrait). Then the
next monitor on DP-1 is configured as the primary (`-p`) with an x,y offset of
[1440, 270] relative to the first one with normal orientation. For getting the
correct y offset with multiple monitors, you can simple experiment with the value
to get an appropriate alignment.
