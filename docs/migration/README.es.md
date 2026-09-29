# Migración de directorios

[English](README.md) | [README del proyecto](../../README.es.md)

Este paquete se basa en `arcade-core(2).zip`. No tiene una carpeta envolvente adicional: `Makefile`, `common/`, `modular/` y `standalone/` deben copiarse directamente en la raíz del repositorio existente.

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
| `standalone/platforms/raspberry_pi/` | Sin cambio; documentado explícitamente como previsto |

## Archivos obsoletos que debes eliminar

[remove-old-paths.txt](remove-old-paths.txt) enumera todos los archivos antiguos que debes retirar, con una ruta relativa por línea. [path-map.tsv](path-map.tsv) indica el destino correspondiente. Elimina las versiones antiguas solo después de confirmar las nuevas copias; los bytes de ROMs del JavaScript ahora están en el bundle generado, no en la plantilla sin ROMs.

Las raíces antiguas son `devices/`, `boards/`, `machines/`, `romsets/`, `rtl/`, `roms/pacman/`, `platform/host/`, `sim/`, `fpga/tangnano20k/`, `tests/host/`, `tests/namco_wsg/`, `tests/pacman/`, `tests/pacman_audio/`, `tests/pacman_cpu/`, `tests/z80/` y `standalone/browser/`, además del archivo `standalone/platform.h`. Retira los directorios padre que queden vacíos solo después de comprobar que no contienen otros archivos locales. No borres de forma recursiva todo `tests/`, `standalone/` ni `.git/`.

La lista exacta cubre únicamente archivos del snapshot recibido. Conserva variantes locales de ROMs, imágenes ZEX externas y cambios sin confirmar antes de eliminar rutas antiguas. `scripts/`, `Makefile`, `.gitignore`, `README.md` y `README.es.md` se actualizan en su ubicación, no se eliminan.

## Resultados nativos y web

Se conservan los nombres de ejecutables en `build/host/`. Los programas nativos ahora buscan ROMs en `common/roms/pacman/` al ejecutarse desde la raíz. Se mantiene el argumento para indicar otro directorio.

Este paquete privado incluye bajo `build/` únicamente la demo web lista y los README HTML; no incluye ejecutables de pruebas nativas ni objetos antiguos. Los artefactos nativos que ya tengas pueden reconstruirse con los targets existentes. Si eliminas todo `build/`, regenera el navegador con `make pacman-web` y los README HTML con `make readme-pages`.

Las ROMs originales y bundles generados permanecen ignorados por Git. Solo deben versionarse la plantilla web y el fuente de la página de inicio. El ZIP privado conserva intencionadamente las ROMs del usuario para que el ejemplo incluido se pueda ejecutar inmediatamente.
