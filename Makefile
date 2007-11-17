BOOTPACK=./bootpack

BOOTPACKS :=

all : bootpacks

clean ::

busybox/include/autoconf.h : busybox/.config
	$(MAKE) -C busybox oldconfig

busybox/busybox : busybox/include/autoconf.h
	uclibc $(MAKE) -C busybox

busybox.bp : busybox/busybox
	$(MAKE) -C busybox install
	$(BOOTPACK) busybox/_install > $@

BOOTPACKS += busybox.bp

bootpacks : $(BOOTPACKS)

clean ::
	rm -f $(BOOTPACKS)
