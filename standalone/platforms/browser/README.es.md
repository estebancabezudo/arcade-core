# Pac-Man standalone para navegador

[Fuente HTML de entrada](index.html) | [Documentación del proyecto](../../../README.es.md#ejemplo-javascript-para-navegador) | [English](README.md)

`pacman.template.js` es el fuente sin bytes de ROMs. No lo cargues directamente en una página. Ejecuta `make pacman-web` desde la raíz para validar `common/roms/pacman/` y generar `build/standalone/browser/pacman.js` junto con un `index.html` portable. El ZIP privado ya incluye ese resultado.

El `index.html` versionado referencia el bundle generado. El índice generado usa `./pacman.js`, por lo que puedes copiar ambos archivos generados juntos a cualquier directorio estático. Ambos están en UTF-8.

Tras el arranque, haz clic/toca la pantalla o pulsa Enter para empezar. Flechas/WASD mueven; 5 introduce moneda; 1/2 inician; P/Esc pausan; M silencia y F activa pantalla completa. En táctil, arrastrar envía una dirección de cuatro vías. La página predeterminada solo crea el canvas arcade; `screenOnly: false` restaura la interfaz completa. La pausa congela la pantalla original sin superponer paneles.

El port es independiente de C/RTL; cambiar C no regenera la lógica JavaScript. El empaquetador solo valida/inserta ROMs y copia la página de inicio. Las pruebas web están separadas de `make test`.

## GitHub frente a una página web

La vista del README de GitHub no ejecuta JavaScript ni incrusta iframes del juego. El enlace de arriba abre el fuente HTML cuando se ve desde GitHub. Para jugar localmente abre `build/standalone/browser/index.html`; los README HTML de `build/docs/` sí incrustan el arcade.

Los dos archivos generados también pueden alojarse como un sitio estático, por ejemplo en GitHub Pages, y enlazarse desde una captura en el README. Esa demo se abrirá como página independiente. No se ha publicado el bundle ni configurado una URL pública; consulta la [explicación del README raíz](../../../README.es.md#demo-publicada-y-github-pages).
