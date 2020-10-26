yakuake-octopus
===============

This project is fork of git://anongit.kde.org/yakuake

Yakuake is a drop-down terminal emulator based on KDE Konsole technology.
The Yakuake website is located at: http://yakuake.kde.org/

major differences from the original project
-------------------------------------------

* tabs are grouped (to make life easier for those who really need a lot of tabs)
* predefined layout ([example](examples/.yakuake_layout))

install from packages (Debian bullseye/buster/stretch)
--------------------------------------

* add repo using hints from here: https://furmur.org/debian/readme.txt
* install package:
```bash
# apt update
# apt install yakuake-octopus
```

build pkg from the sources (Debian)
-------------------------------------------

```bash
$ git clone https://github.com/furmur/yakuake.git
$ cd yakuake
$ debuild -us -uc -b
```
