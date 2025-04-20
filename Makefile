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

HAPP=housealmanac
HROOT=/usr/local
SHARE=$(HROOT)/share/house

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
	gcc -Os -o housealmanac $(OBJS) -lhouseportal -lechttp -lssl -lcrypto -lrt

# Application files installation --------------------------------

install-ui:
	mkdir -p $(SHARE)/public/almanac
	chmod 755 $(SHARE) $(SHARE)/public $(SHARE)/public/almanac
	cp public/* $(SHARE)/public/almanac
	chown root:root $(SHARE)/public/almanac/*
	chmod 644 $(SHARE)/public/almanac/*

install-app: install-ui
	mkdir -p $(HROOT)/bin
	mkdir -p /var/lib/house
	mkdir -p /etc/house
	rm -f $(HROOT)/bin/housealmanac
	cp housealmanac $(HROOT)/bin
	chown root:root $(HROOT)/bin/housealmanac
	chmod 755 $(HROOT)/bin/housealmanac
	touch /etc/default/housealmanac

uninstall-app:
	rm -rf $(SHARE)/public/housealmanac
	rm -f $(HROOT)/bin/housealmanac

purge-app:

purge-config:
	rm -rf /etc/default/housealmanac

# System installation. ------------------------------------------

include $(SHARE)/install.mak

# Docker installation -------------------------------------------

docker: all
	rm -rf build
	mkdir -p build
	cp Dockerfile build
	mkdir -p build$(HROOT)/bin
	cp housealmanac build$(HROOT)/bin
	chmod 755 build$(HROOT)/bin/housealmanac
	cd build ; docker build -t housealmanac .
	rm -rf build
