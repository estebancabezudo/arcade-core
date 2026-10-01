.PHONY: lint test geometry video-system pacman-host pacman-sim pacman-hdmi-sim pacman-video-system

lint:
	bash scripts/verify.sh lint

test:
	bash scripts/verify.sh test

geometry:
	bash scripts/verify.sh geometry

video-system:
	bash scripts/verify.sh video-system

pacman-host:
	bash scripts/build-pacman-host.sh

pacman-sim: pacman-host

pacman-hdmi-sim: pacman-host

pacman-video-system: pacman-host

# Browser builds only need Python 3 and the local ROM set.
.PHONY: pacman-web test-c test-web

pacman-web:
	python3 scripts/build-pacman-web.py

test-c:
	bash scripts/verify.sh c

test-web: pacman-web
	node --test tests/standalone/browser/pacman.test.cjs
	python3 -B -m unittest discover -s tests/standalone/browser -p 'test_*.py'

.PHONY: readme-pages
readme-pages: pacman-web
	python3 scripts/render-readmes.py

# Native standalone; C models and ROM input are shared with the modular project.
MODEL ?= 4
TOOLCHAIN ?= gcc
RPI := standalone/platforms/raspberry_pi
.PHONY: pacman-standalone pacman-rpi pacman-rpi-roms pacman-rpi-example test-standalone

pacman-standalone:
	python3 standalone/tools/generate_pacman.py

pacman-rpi:
	$(MAKE) -C $(RPI) MODEL=$(MODEL) TOOLCHAIN=$(TOOLCHAIN)

pacman-rpi-roms:
	python3 $(RPI)/scripts/pack_roms.py

pacman-rpi-example:
	$(MAKE) -C $(RPI) MODEL=4 TOOLCHAIN=$(TOOLCHAIN) example-pi4

test-standalone: pacman-standalone pacman-rpi-roms
	bash scripts/test-pacman-standalone.sh
	python3 -B -m unittest discover -s tests/standalone/native -p 'test_*.py'

# Optional documentation checks; uses docs/requirements.txt, not runtime dependencies.
.PHONY: test-docs
test-docs: readme-pages
	python3 -B -m unittest discover -s tests/docs -p 'test_*.py'
