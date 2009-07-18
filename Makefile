CFLAGS =
LDFLAGS =

OBJECTS=rpcd.o
TARGETS=rpcd

include rules.mk

rpcd: $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o rpcd -lasn

install: install-std
