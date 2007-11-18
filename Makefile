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

# Skeleton boot pack

SKEL_DEVS :=
SKEL_DEVS += /dev/null
SKEL_DEVS += /dev/console
SKEL_DEVS += /dev/tty
SKEL_DEVS += /dev/tty1
SKEL_DEVS += /dev/tty2
SKEL_DEVS += /dev/tty3
SKEL_DEVS += /dev/tty4
SKEL_DEVS += /dev/tty5
SKEL_DEVS += /dev/tty6
SKEL_DEVS += /dev/mem
SKEL_DEVS += /dev/urandom

skeleton.bp : $(BOOTPACK)
	$(BOOTPACK) -o $@ skeleton=/ $(SKEL_DEVS)

BOOTPACKS += skeleton.bp

# Busybox boot pack

busybox/include/autoconf.h : busybox/.config
	$(MAKE) -C busybox oldconfig

busybox/busybox : busybox/include/autoconf.h
	uclibc $(MAKE) -C busybox

busybox.bp : busybox/busybox $(BOOTPACK)
	$(MAKE) -C busybox install
	$(BOOTPACK) -o $@ busybox/_install=/

BOOTPACKS += busybox.bp

clean ::
	$(MAKE) -C busybox clean

# Boot pack common instructions

bootpacks : $(BOOTPACKS)

clean ::
	rm -f $(BOOTPACKS)
