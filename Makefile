PATH := ./tools:$(PATH)
BOOTPACK = ./tools/bootpack

BOOTPACKS :=

all : tools bootpacks

# Build tools for bootfs manipulation

tools :
	$(MAKE) -C tools

clean ::
	$(MAKE) -C tools clean

.PHONY : tools

# Busybox boot pack

busybox/include/autoconf.h : busybox/.config
	$(MAKE) -C busybox oldconfig

busybox/busybox : busybox/include/autoconf.h
	uclibc $(MAKE) -C busybox

busybox.bp : busybox/busybox $(BOOTPACK)
	$(MAKE) -C busybox install
	$(BOOTPACK)k busybox/_install > $@

BOOTPACKS += busybox.bp

clean ::
	$(MAKE) -C busybox clean

# Boot pack common instructions

bootpacks : $(BOOTPACKS)

clean ::
	rm -f $(BOOTPACKS)
