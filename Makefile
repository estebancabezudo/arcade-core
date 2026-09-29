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
