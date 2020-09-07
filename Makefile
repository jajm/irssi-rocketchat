all: core

core:
	$(MAKE) -C src/core

install:
	$(MAKE) -C src/core install

clean:
	$(MAKE) -C src/core clean
