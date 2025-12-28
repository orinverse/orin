
Debian
====================
This directory contains files used to package orind/orin-qt
for Debian-based Linux systems. If you compile orind/orin-qt yourself, there are some useful files here.

## orin: URI support ##


orin-qt.desktop  (Gnome / Open Desktop)
To install:

	sudo desktop-file-install orin-qt.desktop
	sudo update-desktop-database

If you build yourself, you will either need to modify the paths in
the .desktop file or copy or symlink your orin-qt binary to `/usr/bin`
and the `../../share/pixmaps/orin128.png` to `/usr/share/pixmaps`

orin-qt.protocol (KDE)

