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
	rm -rf skeleton
	mkdir -p skeleton/{bin,dev,etc,lib/modules,mnt,proc,sbin,sys}
	mkdir -p skeleton/{usr/{bin,sbin,share}}
	mkdir -p skeleton/{var/{lock,log,run}}
	$(BOOTPACK) -o $@ skeleton=/ $(SKEL_DEVS)

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
	$(BOOTPACK) -o $@ \
		`echo $(POL_FILES) | sed 's/policy\(\S*\)/policy\1=\1/g'`

BOOTPACKS += policy.bp

# Boot pack common instructions

bootpacks : $(BOOTPACKS)

clean ::
	rm -f $(BOOTPACKS)
