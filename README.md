yakuake-octopus
===============

This project is fork of git://anongit.kde.org/yakuake

Yakuake is a drop-down terminal emulator based on KDE Konsole technology.
The Yakuake website is located at: http://yakuake.kde.org/

install from packages (Debian/stretch)
--------------------------------------

```bash
# echo "deb https://furmur.org/debian stretch main" > /etc/apt/sources.list.d/furmur.list
# apt install apt-transport-https
# wget -O - https://furmur.org/debian/key.gpg | apt-key add -
# apt update
# apt install yakuake-octopus
```

build pkg from the sources (Debian/stretch)
-------------------------------------------

```bash
$ git clone https://github.com/furmur/yakuake.git
$ cd yakuake
$ debuild -us -uc -b
```
