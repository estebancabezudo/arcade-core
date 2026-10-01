# Targets standalone

[Resumen del proyecto](../README.es.md) | [English](README.md)

Los targets standalone empaquetan un runtime completo de Pac-Man sin SDL, MAME ni el pipeline FPGA/CRT. El navegador sigue necesitando un navegador y su sistema operativo; Raspberry Pi ejecuta un kernel AArch64 nativo sin Linux.

## Capa nativa compartida

`include/pacman.h` define las regiones de ROM y `pacman_standalone_init()`, `pacman_standalone_reset()`, `pacman_standalone_frame()` y `pacman_standalone_run()`. `include/platform.h` establece la frontera con el hardware:

```c
bool platform_init(void);
void platform_video_present(const uint16_t *pixels, size_t pixel_count);
void platform_audio_write(const int16_t *samples, size_t sample_count);
PacmanStandaloneInput platform_input_read(void);
uint64_t platform_time_us(void);
void platform_idle(void);
```

El core entrega RGB565 de 224×288 en orientación de pantalla y PCM16 mono a 48 kHz. Recibe joystick de cuatro vías, moneda e inicio de jugadores. No utiliza filesystem, `malloc`, threads ni sistema operativo.

Desde la raíz del repositorio:

```bash
make pacman-standalone
```

`tools/generate_pacman.py` lee los fuentes canónicos de `common/devices/`, `common/boards/` y `common/machines/`, y escribe `build/standalone/native/pacman.c`. Su wrapper conserva la implementación standalone importada: selección de cuatro vías, pulso de moneda de dos cuadros, rotación RGB565, decimación 4:1 del audio y plazo de 16.500 µs por cuadro. El reset de hardware conserva SRAM según el contrato de la placa CPU común; no equivale a una inicialización en frío.

Los fuentes C comunes siguen siendo la referencia. No edites `pacman.c` generado: modifica los componentes comunes o el wrapper del generador y regenera. Una única unidad de compilación permite optimizaciones entre componentes. Raspberry Pi utiliza `-O3`, LTO, compilación freestanding y flags de CPU por modelo; esto no constituye una garantía universal de rendimiento.

## Plataformas

| Plataforma | Fuentes | Runtime y estado |
| --- | --- | --- |
| Navegador | [platforms/browser/](platforms/browser/README.es.md) | Port JavaScript implementado; un JS generado y una página HTML que solo muestra el arcade |
| Raspberry Pi | [platforms/raspberry_pi/](platforms/raspberry_pi/README.es.md) | Backend bare-metal importado, imagen Pi 4 preparada y opción de compilación Pi 3/Zero 2; validación física pendiente |

El navegador es un port JavaScript separado, no una compilación del core C a WebAssembly. Su empaquetador de ROMs y los targets nativos consumen los mismos archivos locales de `common/roms/pacman/`.

## Salidas generadas

```text
build/standalone/
├── native/pacman.c
├── browser/index.html
├── browser/pacman.js
└── raspberry_pi/
    ├── generated/roms.c
    ├── model4/clang/kernel8.img     # ejemplo de selección de build
    └── examples/pi4_usb/           # imagen Pi 4, configuración y checksums
```

`make pacman-web` construye el navegador. `make pacman-rpi MODEL=4 TOOLCHAIN=clang` construye el kernel nativo. La imagen Pi 4 suministrada puede probarse sin recompilar; consulta la [guía de primer arranque](platforms/raspberry_pi/examples/pi4_usb/README.es.md).

El ZIP privado conserva las ROMs y los artefactos preparados. Las ROMs, el C con ROMs embebidas, los kernels y los bundles web permanecen fuera del control de versiones en sus rutas ignoradas. No hay una segunda copia de ROMs en el directorio de fuentes de Raspberry Pi.

## Verificación

`make test-standalone` ejecuta la traza nativa en host y las pruebas de generador, empaquetado de ROMs e instaladores. `make test-web` comprueba la ruta JavaScript independiente. Son targets separados de `make test` para C/RTL modular. Consulta las [pruebas nativas](../tests/standalone/native/README.md) y la [validación de integración](../docs/validation/raspberry-pi-integration.md). Compilar un kernel AArch64 no demuestra funcionamiento de vídeo, GPIO ni audio en hardware físico.
