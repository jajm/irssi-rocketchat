all: core fe-common

core:
	$(MAKE) -C src/core

fe-common:
	$(MAKE) -C src/fe-common

install:
	$(MAKE) -C src/core install
	$(MAKE) -C src/fe-common install

clean:
	$(MAKE) -C src/core clean
	$(MAKE) -C src/fe-common clean
