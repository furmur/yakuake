yakuake-octopus
===============

This project is fork of git://anongit.kde.org/yakuake

Yakuake is a drop-down terminal emulator based on KDE Konsole technology.
The Yakuake website is located at: http://yakuake.kde.org/

major differences from the original project
-------------------------------------------

* tabs are grouped (to make life easier for those who really need a lot of tabs)
* predefined layout ([example](examples/.yakuake_layout))

install from packages (Debian/buster)
--------------------------------------

```bash
# echo "deb https://furmur.org/debian buster main" > /etc/apt/sources.list.d/furmur.list
# apt install apt-transport-https
# wget -O - https://furmur.org/debian/key.gpg | apt-key add -
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
