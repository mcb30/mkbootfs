BOOTPACK = ./tools/bootpack

CFLAGS = -W -Wall -Wextra -O3

BOOTPACKS :=

all : tools bootpacks

# Build tools for bootfs manipulation

$(BOOTPACK) : $(BOOTPACK).c
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) -lz $< -o $@

clean ::
	rm -f $(BOOTPACK)

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
	mkdir -p skeleton
	rm -rf skeleton/_install
	mkdir -p skeleton/_install/{bin,dev,etc,lib/modules,mnt,proc,sbin,sys}
	mkdir -p skeleton/_install/{usr/{bin,sbin,share}}
	mkdir -p skeleton/_install/{var/{lock,log,run}}
	$(BOOTPACK) -o $@ skeleton/_install=/ $(SKEL_DEVS)

BOOTPACKS += skeleton.bp

# Busybox boot pack

busybox/busybox : busybox/.config
	$(MAKE) -C busybox oldconfig
	uclibc $(MAKE) -C busybox

busybox.bp : busybox/busybox $(BOOTPACK)
	$(MAKE) -C busybox install
	$(BOOTPACK) -o $@ busybox/_install=/

BOOTPACKS += busybox.bp

clean ::
	$(MAKE) -C busybox clean

# Policy boot pack

POL_FILES :=
POL_FILES += policy/etc/passwd
POL_FILES += policy/etc/group
POL_FILES += policy/etc/protocols
POL_FILES += policy/etc/inittab
POL_FILES += policy/etc/init.d/rcS
POL_FILES += policy/usr/share/udhcpc/default.script
POL_FILES += policy/bin/welcome.sh

policy.bp : $(POL_FILES) $(BOOTPACK)
	rm -rf policy/_install
	mkdir -p policy/_install/{etc/init.d,bin,usr/share/udhcpc}
	for file in $(POL_FILES) ; do \
		cp -p $$file \
		  `echo $$file | sed 's/^policy\//policy\/_install\//'` ; \
	done
	$(BOOTPACK) -o $@ policy/_install=/

BOOTPACKS += policy.bp

# Boot pack common instructions

bootpacks : $(BOOTPACKS)

clean ::
	rm -f $(BOOTPACKS)
