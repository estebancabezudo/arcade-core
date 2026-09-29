# Targets standalone

[Resumen del proyecto](../README.es.md) | [English](README.md)

- `platforms/browser/`: port JavaScript implementado. `make pacman-web` genera un runtime de un archivo con ROMs integradas bajo `build/standalone/browser/`; su HTML muestra únicamente la pantalla arcade.
- `include/platform.h`: contrato C de vídeo nativo, audio PCM, entradas, inicialización y reloj monotónico.
- `platforms/raspberry_pi/`: target nativo previsto. No están implementados el arranque, linker script, drivers ni bucle standalone nativo.

El bundle web es autocontenido, pero requiere navegador/OS. No implementa el contrato nativo, no compila el C compartido a WebAssembly y no valida un target bare-metal.
