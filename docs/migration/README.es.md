# Migración de directorios

[English](README.md) | [README del proyecto](../../README.es.md)

Este paquete combina la reorganización previa de `arcade-core(2).zip` con `arcade-core-standalone-pacman-rpi4(2).zip`. No tiene una carpeta envolvente adicional: `Makefile`, `common/`, `modular/` y `standalone/` deben copiarse directamente en la raíz del repositorio existente.

Copia primero los archivos nuevos. Conserva tu `.git/` y cualquier archivo local que no figure en el snapshot original. El paquete no sustituye metadatos Git, no crea commits ni publica ROMs.

| Ruta anterior | Ruta nueva |
| --- | --- |
| `devices/` | `common/devices/` |
| `boards/` | `common/boards/` |
| `machines/` | `common/machines/` |
| `romsets/` | `common/romsets/` |
| `rtl/` | `common/rtl/` |
| `roms/pacman/` | `common/roms/pacman/` |
| `platform/host/` | `modular/platforms/host/` |
| `sim/pacman_video_system_sdl.cpp` | `modular/platforms/host/pacman_video_system_sdl.cpp` |
| Resto de `sim/*_tb.cpp` | `tests/rtl/` |
| `fpga/tangnano20k/` | `modular/platforms/fpga/tangnano20k/` |
| `tests/host/` | `tests/modular/host/` |
| Resto de directorios de pruebas C | `tests/common/` |
| `standalone/platform.h` | `standalone/include/platform.h` |
| `standalone/browser/index.html` | `standalone/platforms/browser/index.html` |
| `standalone/browser/pacman.js` | `standalone/platforms/browser/pacman.template.js` + generado `build/standalone/browser/pacman.js` |
| `standalone/platforms/raspberry_pi/` | Conserva los fuentes de hardware importados; se reubican ROMs y generados |

## Archivos obsoletos que debes eliminar

[remove-old-paths.txt](remove-old-paths.txt) enumera todos los archivos antiguos que debes retirar, con una ruta relativa por línea. [path-map.tsv](path-map.tsv) indica el destino correspondiente. Elimina las versiones antiguas solo después de confirmar las nuevas copias; los bytes de ROMs del JavaScript ahora están en el bundle generado, no en la plantilla sin ROMs.

Las raíces antiguas son `devices/`, `boards/`, `machines/`, `romsets/`, `rtl/`, `roms/pacman/`, `platform/host/`, `sim/`, `fpga/tangnano20k/`, `tests/host/`, `tests/namco_wsg/`, `tests/pacman/`, `tests/pacman_audio/`, `tests/pacman_cpu/`, `tests/z80/` y `standalone/browser/`, además del archivo `standalone/platform.h`. Retira los directorios padre que queden vacíos solo después de comprobar que no contienen otros archivos locales. No borres de forma recursiva todo `tests/`, `standalone/` ni `.git/`.

La lista exacta cubre únicamente archivos del snapshot recibido. Conserva variantes locales de ROMs, imágenes ZEX externas y cambios sin confirmar antes de eliminar rutas antiguas. `scripts/`, `Makefile`, `.gitignore`, `README.md` y `README.es.md` se actualizan en su ubicación, no se eliminan.

## Resultados nativos y web

Se conservan los nombres de ejecutables en `build/host/`. Los programas nativos ahora buscan ROMs en `common/roms/pacman/` al ejecutarse desde la raíz. Se mantiene el argumento para indicar otro directorio.

Este paquete privado incluye bajo `build/` raíz la demo web y los README HTML, el C generado de core/ROMs y el ejemplo Pi 4 recibido. No incluye ejecutables de pruebas nativas, objetos ni metadatos Git. Reconstruye navegador con `make pacman-web`, README HTML con `make readme-pages` y Raspberry Pi con `make pacman-rpi MODEL=4 TOOLCHAIN=clang`. `make pacman-rpi-example TOOLCHAIN=clang` recrea el ejemplo Pi 4 después de eliminar `build/` raíz.

ROMs, C generado con ROMs, kernels y bundles web siguen ignorados por Git. Versiona únicamente fuentes, headers, scripts, configuración, pruebas y documentación.

## Integración del paquete Raspberry Pi

El paquete nuevo es **completo**, no un ZIP que contenga solo el agregado Raspberry Pi. Los fuentes de hardware permanecen en `standalone/platforms/raspberry_pi/`. El generador conserva `standalone/tools/generate_pacman.py`, pero ahora lee el C común de `common/` y escribe en `build/` raíz. El runtime nativo recibido no cambia, salvo los comentarios de rutas fuente en el C generado.

| Ruta del paquete recibido | Ruta integrada |
| --- | --- |
| `standalone/pacman.c` | `build/standalone/native/pacman.c` (generado, no mantenido a mano) |
| `standalone/pacman.h` | `standalone/include/pacman.h` |
| `standalone/platform.h` | `standalone/include/platform.h` (API equivalente existente) |
| Raspberry Pi `roms/pacman/` | `common/roms/pacman/` (bytes idénticos; una copia de entrada) |
| Raspberry Pi `roms/CHECKSUMS.txt` | `common/roms/CHECKSUMS.txt` |
| Raspberry Pi `generated/roms.c` | `build/standalone/raspberry_pi/generated/roms.c` |
| Raspberry Pi `examples/pi4_usb/{kernel8.img,config.txt,SHA256SUMS}` | `build/standalone/raspberry_pi/examples/pi4_usb/` |
| `standalone/FIRST_RUN_PI4.md` | Integrado en el README bilingüe del ejemplo |
| `standalone/VALIDATION.txt` | `docs/validation/raspberry-pi-imported.txt` (histórico sin modificar) |
| `standalone/MANIFEST.txt` | `docs/migration/raspberry-pi-input-manifest.txt` (inventario del ZIP original) |
| `.gitignore` internos de standalone/Raspberry Pi | Integrados en `.gitignore` raíz |

El manifiesto SHA-256 se ajusta al nuevo directorio del ejemplo; kernel y configuración conservan sus bytes. El build normal no reemplaza el ejemplo suministrado. Las instrucciones de `generated/README.md` y `roms/README.md` antiguos están integradas en la documentación de plataforma/ROMs comunes.

### Qué lista de eliminación corresponde

- **Ya utilizas el ZIP reorganizado anterior y no copiaste encima el ZIP Raspberry:** no hay archivos existentes que eliminar; copia este paquete sobre esa raíz.
- **Ya reorganizaste y además copiaste la estructura antigua del ZIP Raspberry:** utiliza [remove-raspberry-pi-old-paths.txt](remove-raspberry-pi-old-paths.txt) y [su mapa exacto](raspberry-pi-path-map.tsv).
- **Todavía utilizas la estructura original:** utiliza [remove-old-paths.txt](remove-old-paths.txt), que ahora cubre ambos paquetes. Las rutas antiguas que no existan se omiten.

Son listas de archivos individuales, no scripts de borrado. Copia primero el paquete, verifica destinos y retira únicamente archivos obsoletos listados que realmente existan. Conserva `.git/` y cualquier archivo exclusivamente local. El `build/` antiguo dentro de la plataforma no estaba en el ZIP recibido y no se elimina automáticamente: revísalo por separado. Retira directorios vacíos solo después de comprobar su contenido. Ninguna lista incluye archivos vigentes del proyecto nuevo.
