BOOTPACK = ./tools/bootpack

CFLAGS = -W -Wall -Wextra -O3

BOOTPACKS :=

all : tools bootpacks

# Build tools for bootfs manipulation

$(BOOTPACK) : $(BOOTPACK).c
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) -lz $< -o $@

clean ::
	rm $(BOOTPACK)

tools :: $(BOOTPACK)

# Busybox boot pack

busybox/include/autoconf.h : busybox/.config
	$(MAKE) -C busybox oldconfig

busybox/busybox : busybox/include/autoconf.h
	uclibc $(MAKE) -C busybox

busybox.bp : busybox/busybox $(BOOTPACK)
	$(MAKE) -C busybox install
	$(BOOTPACK) busybox/_install > $@

BOOTPACKS += busybox.bp

clean ::
	$(MAKE) -C busybox clean

# Boot pack common instructions

bootpacks : $(BOOTPACKS)

clean ::
	rm -f $(BOOTPACKS)
