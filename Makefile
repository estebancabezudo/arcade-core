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
