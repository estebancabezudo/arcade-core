# Browser standalone Pac-Man

[Open the canvas-only example](index.html) | [Project documentation](../../../README.md#javascript-browser-example) | [Español](README.es.md)

`pacman.template.js` is the source, without ROM payload. Do not load it directly into a page. Run `make pacman-web` from the repository root to validate `common/roms/pacman/` and create `build/standalone/browser/pacman.js` plus a portable `index.html`. The private ZIP already includes this output.

The tracked `index.html` points to the generated bundle. The generated index uses `./pacman.js`, so the two generated files can be copied together to any static directory. Both are UTF-8.

Click/tap the screen or press Enter after boot to start. Use arrows/WASD, 5 for coin, 1/2 for start, P/Esc for pause, M for mute and F for fullscreen. Touch dragging supplies a four-way direction. The default page creates only the arcade canvas; `screenOnly: false` restores the full interface. Pause freezes the original screen without adding an overlay.

The source port is independent of C/RTL; edits to shared C do not regenerate JavaScript logic. The packager only validates/injects ROM bytes and copies the entry page. Browser tests are separate from `make test`.
