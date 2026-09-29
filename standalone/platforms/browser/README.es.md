# Pac-Man standalone para navegador

[Abrir el ejemplo con solo la pantalla](index.html) | [Documentación del proyecto](../../../README.es.md#ejemplo-javascript-para-navegador) | [English](README.md)

`pacman.template.js` es el fuente sin bytes de ROMs. No lo cargues directamente en una página. Ejecuta `make pacman-web` desde la raíz para validar `common/roms/pacman/` y generar `build/standalone/browser/pacman.js` junto con un `index.html` portable. El ZIP privado ya incluye ese resultado.

El `index.html` versionado referencia el bundle generado. El índice generado usa `./pacman.js`, por lo que puedes copiar ambos archivos generados juntos a cualquier directorio estático. Ambos están en UTF-8.

Tras el arranque, haz clic/toca la pantalla o pulsa Enter para empezar. Flechas/WASD mueven; 5 introduce moneda; 1/2 inician; P/Esc pausan; M silencia y F activa pantalla completa. En táctil, arrastrar envía una dirección de cuatro vías. La página predeterminada solo crea el canvas arcade; `screenOnly: false` restaura la interfaz completa. La pausa congela la pantalla original sin superponer paneles.

El port es independiente de C/RTL; cambiar C no regenera la lógica JavaScript. El empaquetador solo valida/inserta ROMs y copia la página de inicio. Las pruebas web están separadas de `make test`.
