# Pac-Man bare-metal para Raspberry Pi

[Resumen del proyecto](../../../README.es.md) | [English](README.md) | [Primer arranque](examples/pi4_usb/README.es.md)

Esta es la implementación importada del paquete standalone de Raspberry Pi, no una plataforma vacía. Ejecuta el core C nativo generado como kernel AArch64 sin Linux, SDL, MAME ni pipeline RTL/CRT. El objetivo principal es **Raspberry Pi 4 Model B**; `MODEL=3` conserva la configuración de compilación Pi 3/Zero 2 recibida. Pi 5 no está implementada.

**La validación física sigue pendiente.** Esta integración compila las configuraciones Pi 3 y Pi 4 con Clang 17/LLD y conserva la imagen Pi 4 recibida. La compilación y las pruebas de emulación en host no verifican firmware, arranque, GPIO, vídeo ni sonido en una placa real.

## Estructura

```text
standalone/platforms/raspberry_pi/
├── Makefile
├── boot/config.txt
├── boot/start.S
├── include/raspberry_pi.h
├── linker/kernel8.ld
├── src/main.c
├── src/platform.c
├── scripts/
│   ├── pack_roms.py
│   ├── install_boot.sh
│   ├── install_pi4_example.sh
│   └── install_image.sh
└── examples/pi4_usb/             # documentación; la imagen está en build/ raíz
```

Los headers nativos públicos están en `standalone/include/`; el generador es `standalone/tools/generate_pacman.py`. Las ROMs se centralizan en `common/roms/pacman/`. Todos los generados se guardan bajo `build/` de la **raíz del repositorio**, nunca dentro de los fuentes de la plataforma.

## Ejemplo Pi 4 preparado

La distribución privada incluye, respecto a la raíz:

```text
build/standalone/raspberry_pi/examples/pi4_usb/
├── kernel8.img
├── config.txt
└── SHA256SUMS
```

Son la imagen y configuración originales recibidas. No necesitas recompilar para la primera prueba física. Sigue la [guía de primer arranque](examples/pi4_usb/README.es.md); debes proporcionar por separado el firmware y un medio de arranque adecuado.

## Compilación desde la raíz

Con el toolchain cruzado AArch64 GCC/binutils, Python 3 y GNU Make instalados:

```bash
make pacman-rpi MODEL=4 TOOLCHAIN=gcc
```

Como alternativa, con Clang/LLVM, LLD y `llvm-objcopy` instalados:

```bash
make pacman-rpi MODEL=4 TOOLCHAIN=clang
```

El paquete recibido contempla Clang 17 o posterior; esta integración se compiló con Clang 17. Se conserva la configuración GCC, pero aquí no se ejecutó compilación cruzada GCC. Puedes indicar otro prefijo GCC mediante `CROSS_COMPILE=...`.

Las salidas distinguen placa y toolchain para no reutilizar objetos Pi 3 en Pi 4:

```text
build/standalone/native/pacman.c
build/standalone/raspberry_pi/generated/roms.c
build/standalone/raspberry_pi/model4/gcc/kernel8.img
build/standalone/raspberry_pi/model4/clang/kernel8.img
```

Solo se construye el directorio del toolchain elegido. `MODEL=3` selecciona Cortex-A53 y su base de periféricos; `MODEL=4`, Cortex-A72 y su base. Se conservan `-O3`, LTO, flags freestanding y warnings tratados como errores. La compilación no descarga firmware ni arranca hardware.

La invocación equivalente desde la raíz es `make -C standalone/platforms/raspberry_pi MODEL=4 TOOLCHAIN=clang`.

## Generadores y limpieza

`make pacman-standalone` regenera el core. `make pacman-rpi-roms` valida tamaños y CRC32 de las diez ROMs comunes y genera el C con sus bytes. `make pacman-rpi` invoca ambos cuando corresponde; los generadores conservan los timestamps si los bytes no cambian. Puedes pasar otra carpeta local mediante `ROM_DIR=/ruta/absoluta` a `make pacman-rpi`.

El target `clean` de la plataforma elimina únicamente el directorio del modelo/toolchain seleccionado. Eliminar `build/` raíz también elimina el ejemplo preparado y el C generado de ROMs/core. Reconstruye el kernel con `make pacman-rpi`; para recrear expresamente el ejemplo Pi 4 listo para copiar:

```bash
make pacman-rpi-example TOOLCHAIN=clang
```

Este último comando reemplaza el ejemplo por el kernel Pi 4 recién compilado y regenera `SHA256SUMS`. La compilación normal **no** reemplaza el ejemplo suministrado.

## Instalación

Desde la raíz, después de montar una partición de arranque Raspberry Pi existente:

```bash
bash standalone/platforms/raspberry_pi/scripts/install_pi4_example.sh /ruta/al/boot
```

Para un kernel compilado localmente con la configuración Pi 4 Clang:

```bash
bash standalone/platforms/raspberry_pi/scripts/install_boot.sh /ruta/al/boot 4 clang
```

Los dos últimos argumentos toman por defecto `4 gcc`; elige los del build real. Ambos instaladores sustituyen **solo** `kernel8.img` y `config.txt`, guardan las versiones anteriores en `arcade-core-backup/install.*` y dejan intactos los demás archivos de firmware. El instalador del ejemplo también comprueba su manifiesto SHA-256. No formatean, montan ni preparan dispositivos, no instalan firmware ni cambian el orden de arranque EEPROM. Usa un medio dedicado a pruebas y conserva las personalizaciones de arranque necesarias.

## Backend de hardware implementado

| Área | Implementación recibida |
| --- | --- |
| Arranque | Entrada AArch64 con `kernel8.img`; cores secundarios detenidos y limpieza de `.bss` |
| Vídeo | Solicitud de framebuffer al firmware por mailbox, 1280×720×32; contenido RGB565 de 224×288 escalado 2× a 448×576 y centrado |
| Entradas | Pulsadores GPIO activos en bajo con pull-up externo a 3.3 V |
| Audio | PCM16 mono a 48 kHz convertido a PDM síncrono por software en GPIO18 |
| Tiempo | Contador genérico AArch64; ejecución por polling |
| ROMs | Integradas durante la compilación; sin carga desde filesystem en runtime |

Los números siguientes son los identificadores GPIO utilizados en `include/raspberry_pi.h`, **no los números físicos del conector**:

| Función | GPIO |
| --- | ---: |
| Arriba | 5 |
| Abajo | 6 |
| Izquierda | 13 |
| Derecha | 19 |
| Moneda | 16 |
| Start 1 | 20 |
| Start 2 | 21 |

Cada botón conecta GPIO y GND y requiere un pull-up externo a 3.3 V. No conectes 5 V a GPIO. GPIO18 necesita un filtro pasa-bajos apropiado y una entrada amplificada; nunca conectes un altavoz directamente. La guía importada de primer arranque describe su circuito inicial de prueba, que aquí sigue sin validación física.

## Limitaciones

USB puede ser el medio de arranque, pero el kernel **no tiene controlador de teclado/joystick USB, filesystem FAT/USB, carga de ROMs en runtime, audio HDMI, audio por jack analógico integrado, backend DMA/PWM ni soporte Pi 5**. La escritura PDM por software es síncrona y puede afectar latencia y velocidad efectiva: es un backend de puesta en marcha, no una solución de audio de baja latencia validada.

El core común y el wrapper nativo son independientes de esta implementación de hardware. La demo del navegador también es independiente y no valida Raspberry Pi. Consulta las [comprobaciones de integración](../../../docs/validation/raspberry-pi-integration.md).
