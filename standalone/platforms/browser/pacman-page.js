// Architecture and design
//
// The standalone example configures the runtime before its deferred bundle executes
// and owns the bundle-load error state. It deliberately contains no emulation or CSS.

"use strict";

globalThis.PacmanOptions = Object.freeze({
  container: "#game",
  screenOnly: true,
  volume: 0.65,
  stylesheetHref: "./pacman-screen.css",
});

document.getElementById("pacman-bundle")?.addEventListener("error", () => {
  const error = document.getElementById("load-error");
  if (error) error.hidden = false;
});
