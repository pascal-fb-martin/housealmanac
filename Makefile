# housealmanac - A simple Almanac service based on a static config.
#
# Copyright 2025, Pascal Martin
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor,
# Boston, MA  02110-1301, USA.
#
# WARNING
#
# This Makefile depends on echttp and houseportal (dev) being installed.

prefix=/usr/local
SHARE=$(prefix)/share/house

INSTALL=/usr/bin/install

HAPP=housealmanac

# Application build. --------------------------------------------

OBJS=housealmanac.o
LIBOJS=

all: housealmanac

main: housealmanac.o

clean:
	rm -f *.o *.a housealmanac

rebuild: clean all

%.o: %.c
	gcc -c -Wall -g -Os -o $@ $<

housealmanac: $(OBJS)
	gcc -Os -o housealmanac $(OBJS) -lhouseportal -lechttp -lssl -lcrypto -lmagic -lrt

# Application files installation --------------------------------

install-ui: install-preamble
	$(INSTALL) -m 0755 -d $(DESTDIR)$(SHARE)/public/almanac
	$(INSTALL) -m 0644 public/* $(DESTDIR)$(SHARE)/public/almanac

install-app: install-ui
	$(INSTALL) -m 0755 -s housealmanac $(prefix)/bin
	touch $(DESTDIR)/etc/default/housealmanac

uninstall-app:
	rm -rf $(DESTDIR)$(SHARE)/public/almanac
	rm -f $(DESTDIR)$(prefix)/bin/housealmanac

purge-app:

purge-config:
	rm -rf $(DESTDIR)/etc/default/housealmanac

# System installation. ------------------------------------------

include $(SHARE)/install.mak

# Docker installation -------------------------------------------

docker: all
	rm -rf build
	mkdir -p build
	cp Dockerfile build
	mkdir -p build$(prefix)/bin
	cp housealmanac build$(prefix)/bin
	chmod 755 build$(prefix)/bin/housealmanac
	cd build ; docker build -t housealmanac .
	rm -rf build
