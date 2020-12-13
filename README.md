# ABOUT

**DSBAutostart**
is a Qt program to manage your XDG autostart files.

# INSTALLATION

## Dependencies

**DSBAutostart**
depends on
*devel/qt5-buildtools*, *devel/qt5-core*, *devel/qt5-linguisttools*,
*devel/qt5-qmake*, *x11-toolkits/qt5-gui*,
and
*x11-toolkits/qt5-widgets*

## Getting the source code

	# git clone https://github.com/mrclksr/DSBAutostart.git

## Building and installation

	# cd DSBAutostart && qmake && make
	# make install

# SETUP

## Openbox
*Openbox* supports XDG autostart through its *openbox-xdg-autostart* script.
Just install *devel/py-xdg*.

## Other window managers and desktop environments
If your window manager (WM) or desktop environment (DE) does not support XDG
autostart, add the line `dsbautostart -a` to your WM/DE's autostart script.
Alternatively, if you use `~/.xinitrc` to start your WM/DE, add the line
`dsbautostart -a` to `~/.xinitrc`.

# Usage

**dsbautostart** \[**-h**\]

**dsbautostart** <**-a**|**-c**>
## Options
**-a**
> Autostart commands, and exit.

**-c**
> Create desktop files in the user's autostart directory from the
command list read from stdin.

# Upgrading from previous versions < 2.0

In order to upgrade from a previous version < 2.0, convert
the commands from `~/.config/DSB/autostart.sh` to desktop files in
`$XDG_CONFIG_HOME/autostart`:

	% dsbautostart -c < ~/.config/DSB/autostart.sh

Make sure to remove calls to `~/.config/DSB/autostart.sh` from your
window manager's autostart file, or your `~/.xinitrc`. Finally follow
the steps in the *SETUP* section above.
