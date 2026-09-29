# arcade-core

[English](README.md) | [Español](README.es.md)

`arcade-core` comenzó como un emulador de hardware clásico pensado para ejecutar como bare-metal en dispositivos muy pequeños. La arquitectura original es modular separando procesador de la placa del arcade, audio y video con fin de poder extenderlo fácilmente a otros arcade. La idea es no tener que levantar un OS, ni MAME e ir directamente al juego como si fuera la máquina original.  Fue pensado para una implementación híbrida CPU + FPGA para correr sobre HDMI.

El proyecto está escrito principalmente en:

- C17 para emulación de CPU, placas, máquinas y herramientas host.
- SystemVerilog para el pipeline de vídeo.
- C++17 para los testbenches de Verilator y la integración SDL del RTL.

Actualmente incluye una implementación funcional de **Pac-Man**, con:

- CPU Z80.
- placa CPU de Pac-Man;
- vídeo, tilemap, sprites y paleta;
- audio Namco WSG de tres voces;
- ejecución de la máquina sincronizada por T-states;
- simulador interactivo SDL;
- pipeline de framebuffer y vídeo 720p en RTL;
- integración Pac-Man → RTL → SDL mediante Verilator.

El objetivo del proyecto no es depender de MAME en tiempo de ejecución. Los componentes de la máquina están implementados dentro de `arcade-core`.

---

## Estado actual

La implementación actual tiene validados:

- core Z80 mediante pruebas unitarias y ZEXDOC/ZEXALL;
- decoder y mapa de memoria de la placa CPU de Pac-Man;
- interrupciones y VBlank;
- tilemap y sprites;
- raster de Pac-Man;
- ROM set de CPU, vídeo y sonido;
- Namco WSG de tres voces;
- música y efectos de Pac-Man;
- sincronización CPU → vídeo → audio;
- simulador SDL jugable;
- framebuffer RTL;
- cambio de dominio de reloj;
- administración de múltiples framebuffers;
- escalado entero;
- scanout 1280×720;
- efectos CRT;
- integración Pac-Man → `video_system` RTL.

# Estructura del proyecto

```text
arcade-core/
├── devices/
│   ├── z80/
│   └── namco_wsg/
│
├── boards/
│   ├── pacman_cpu/
│   ├── pacman_video/
│   └── pacman_audio/
│
├── machines/
│   └── pacman/
│
├── romsets/
│   └── pacman/
│
├── platform/
│   └── host/
│
├── rtl/
├── sim/
├── fpga/
│   └── tangnano20k/
│
├── tests/
│   ├── host/
│   ├── namco_wsg/
│   ├── pacman_audio/
│   ├── pacman_cpu/
│   ├── pacman/
│   └── z80/
│
├── scripts/
├── roms/
│   └── pacman/
│
├── Makefile
└── README.md
```

## Capas

### `devices/`

Componentes reutilizables que representan dispositivos concretos.

Actualmente:

- `z80`: emulador del Zilog Z80.
- `namco_wsg`: generador de sonido Namco de tres voces utilizado por Pac-Man.

### `boards/`

Implementaciones de placas o subsistemas de una máquina.

Pac-Man está dividido en:

```text
pacman_cpu
pacman_video
pacman_audio
```

La placa CPU conecta el Z80 con memoria, I/O, interrupciones y registros de hardware.

La placa de vídeo implementa raster, tilemap, sprites, color y timing.

La placa de audio conecta los registros de sonido de Pac-Man con el Namco WSG.

### `machines/`

Integra las placas que forman una máquina completa.

Actualmente:

```text
machines/pacman/
```

`PacmanMachine` coordina CPU, vídeo, audio, entradas y tiempo de emulación.

### `romsets/`

Describe la composición lógica de las ROM de una máquina independientemente de cómo se carguen desde el host.

### `platform/host/`

Código dependiente del ordenador anfitrión:

- carga de archivos;
- carga de ROMs;
- simuladores SDL;
- integración con el pipeline RTL simulado.

### `rtl/`

Pipeline de vídeo genérico escrito en SystemVerilog.

### `sim/`

Testbenches C++ para Verilator e integración RTL + SDL.

### `fpga/`

Código específico para futuras placas FPGA.

Actualmente existe una frontera lógica para Tang Nano 20K, pero no un bitstream físico terminado.

---

# Requisitos

En Debian o derivados se necesitan al menos:

```bash
sudo apt install \
    build-essential \
    make \
    pkg-config \
    libsdl2-dev \
    verilator
```

Herramientas utilizadas:

- compilador C con soporte C17;
- compilador C++;
- GNU Make;
- Bash;
- SDL2;
- Verilator.

# ROMs de Pac-Man

Las ROM comerciales **no forman parte del repositorio**.

Para ejecutar Pac-Man deben colocarse localmente en:

```text
roms/pacman/
```

Se esperan estos diez archivos:

```text
pacman.6e
pacman.6f
pacman.6h
pacman.6j
pacman.5e
pacman.5f
82s123.7f
82s126.4a
82s126.1m
82s126.3m
```

Los últimos dos corresponden al subsistema de sonido:

```text
82s126.1m    waveform PROM
82s126.3m    sound timing PROM
```

El loader exige que cada archivo tenga exactamente el tamaño esperado.

---

# Compilar Pac-Man

Para construir los targets host:

```bash
make pacman-host
```

Se generan:

```text
build/host/pacman-sim
build/host/pacman-hdmi-sim
build/host/pacman-video-system-sdl
```

También existen los targets:

```bash
make pacman-sim
make pacman-hdmi-sim
make pacman-video-system
```

Actualmente estos targets utilizan el mismo proceso de construcción host.

---

# Simuladores de Pac-Man

## `pacman-sim`

Es el simulador principal y la referencia funcional de la máquina.

Ejecutar:

```bash
./build/host/pacman-sim
```

Puede indicarse otro directorio de ROMs:

```bash
./build/host/pacman-sim /ruta/a/roms
```

Incluye:

- CPU;
- vídeo;
- audio;
- entradas;
- ventana SDL;
- ejecución en tiempo real.

Es la opción adecuada para jugar y validar el comportamiento general de Pac-Man.

---

## `pacman-hdmi-sim`

Viewer host que presenta Pac-Man dentro de una superficie 1280×720 usando la geometría final prevista para HDMI.

```bash
./build/host/pacman-hdmi-sim
```

No simula ciclo a ciclo el pipeline RTL completo.

Es útil para validar rápidamente orientación, escalado y presentación.

---

## `pacman-video-system-sdl`

Integra la máquina Pac-Man con el `video_system` real de SystemVerilog mediante Verilator.

La ruta conceptual es:

```text
PacmanMachine
      │
      ▼
RGB565 224×288
      │
      ▼
video_system RTL
      │
      ├── capture
      ├── framebuffer
      ├── memory arbitration
      ├── line fetch
      ├── integer scaling
      ├── CRT processing
      ▼
1280×720
      │
      ▼
SDL
```

Ejecutar:

```bash
./build/host/pacman-video-system-sdl
```

Este programa está pensado principalmente como prueba de integración del RTL, no como el simulador más eficiente para jugar.

---

# Controles

En los simuladores interactivos:

| Tecla   | Función                        |
| ------- | ------------------------------ |
| Flechas | Joystick de cuatro direcciones |
| `5`     | Insertar moneda                |
| `1`     | Start jugador 1                |
| `2`     | Start jugador 2                |
| `F`     | Pantalla completa              |
| `Esc`   | Salir                          |

El joystick original es de cuatro vías. El host evita enviar diagonales simultáneas a la máquina emulada.

---

# CPU Z80

El core se encuentra en:

```text
devices/z80/
```

Está diseñado como componente aislado reutilizable y no depende de ninguna implementación de juego.

Las pruebas cubren, entre otras áreas:

- cargas;
- ALU de 8 y 16 bits;
- flags;
- saltos;
- stack;
- CB;
- ED;
- DD/FD;
- DDCB/FDCB;
- IX/IY;
- block transfer;
- block I/O;
- interrupciones;
- modos de interrupción;
- `DAA`;
- `RRD` / `RLD`;
- instrucciones con prefijos ignorados.

Además del conjunto de pruebas locales, el core fue validado con **ZEXDOC** y **ZEXALL**.

---

# Ejecutar ZEXDOC / ZEXALL

`tests/z80/zex_runner.c` es un runner separado porque las imágenes `.cim` externas no forman parte del repositorio.

Compilar:

```bash
mkdir -p build/z80

cc \
    -std=c17 \
    -O2 \
    -Wall \
    -Wextra \
    -Wpedantic \
    -Werror \
    -Idevices/z80 \
    devices/z80/z80.c \
    tests/z80/zex_runner.c \
    -o build/z80/zex_runner
```

Ejecutar, por ejemplo:

```bash
./build/z80/zex_runner /ruta/a/zexdoc.cim
```

o:

```bash
./build/z80/zex_runner /ruta/a/zexall.cim
```

ZEX no se ejecuta automáticamente desde `make test` porque depende de archivos externos.

---

# Vídeo de Pac-Man

La máquina original se modela con un raster de:

```text
384 × 264
```

La zona visible utilizada por Pac-Man es:

```text
224 × 288
```

El vídeo incluye:

- RAM de tiles;
- ROM gráfica de tiles;
- ROM gráfica de sprites;
- palette PROM;
- color lookup PROM;
- flip screen;
- sprites;
- transparencia;
- VBlank.

La CPU y el raster se sincronizan mediante T-states del Z80.

---

# Audio de Pac-Man

El subsistema de audio utiliza un dispositivo Namco WSG de tres voces.

Implementación:

```text
devices/namco_wsg/
boards/pacman_audio/
```

Características principales:

- 3 voces;
- 8 waveforms;
- 32 muestras por waveform;
- PROM de waveform de 4 bits;
- acumuladores de fase;
- frecuencia independiente por voz;
- volumen de 0 a 15;
- generación interna a 192 kHz.

La relación utilizada con el Z80 es:

```text
CPU: 3.072 MHz
WSG: 192 kHz
```

por lo que se genera una muestra interna cada:

```text
16 T-states
```

La salida SDL se reduce a 48 kHz mediante decimación 4:1.

Las escrituras a los registros WSG se aplican después de generar el audio correspondiente al intervalo de la instrucción actual. Esto evita que una escritura modifique retroactivamente el tiempo ya consumido por esa instrucción.

Esta sincronización es importante para efectos rápidos como el sonido de Pac-Man comiendo.

---

# Pipeline de vídeo RTL

El pipeline final parte de:

```text
rtl/video_system.sv
```

El diseño desacopla completamente:

```text
raster fuente
    │
    ▼
captura
    │
    ▼
framebuffer
    │
    ▼
memoria
    │
    ▼
scanout HDMI
```

La fuente arcade nunca recibe backpressure.

Si la memoria no puede aceptar una captura completa, el frame puede abortarse sin detener el raster original.

---

## Framebuffers

El diseño utiliza cuatro slots RGB565.

Estados conceptuales:

```text
FREE
RESERVED
COMPLETE
READING
```

Los eventos:

```text
COMMIT
ABORT
```

actúan como barreras respecto a las escrituras de memoria anteriores.

HDMI selecciona únicamente frames completos.

La política de presentación es:

```text
latest complete frame wins
```

Por tanto, si la fuente produce ligeramente más rápido que HDMI, pueden omitirse frames completos sin alterar el reloj de la máquina original.

---

# Geometrías soportadas

El RTL es parametrizable.

## Geometría genérica

```text
320×240
SCALE=3
```

Resultado:

```text
960×720
```

centrado horizontalmente dentro de:

```text
1280×720
```

## Pac-Man

```text
224×288
SCALE=2
```

Resultado:

```text
448×576
```

centrado dentro de:

```text
1280×720
```

El reloj de Pac-Man no se modifica para convertir sus aproximadamente 60.606 Hz a 60 Hz.

---

# Procesamiento CRT

El pipeline incluye efectos CRT discretos y sintetizables:

1. modulación vertical del haz;
2. bleed horizontal;
3. máscara de fósforo RGB.

Se aplican después del escalado y antes de la salida final.

No se utiliza un framebuffer adicional para el efecto CRT.

---

# Memoria y dominios de reloj

El diseño maneja dominios independientes para:

- raster fuente;
- memoria;
- HDMI.

Se utilizan FIFOs asíncronos para transferencias entre dominios de reloj.

El pipeline mantiene:

- orden de comandos;
- integridad de `COMMIT` y `ABORT`;
- identificación de slot;
- identificación de frame;
- prioridad de lecturas para scanout;
- ausencia de stalls en el raster fuente.

---

# Verificación

La validación principal se ejecuta con:

```bash
make test
```

Actualmente este comando realiza:

1. lint completo de SystemVerilog;
2. pruebas RTL mediante Verilator;
3. pruebas C de dispositivos;
4. pruebas de placas;
5. pruebas de máquina;
6. pruebas del loader host;
7. pruebas del core Z80.

Los artefactos generados se almacenan exclusivamente bajo:

```text
build/
```

---

## Lint RTL

```bash
make lint
```

Comprueba:

- RTL genérico;
- shell lógico Tang Nano 20K.

---

## Geometría y vídeo

```bash
make geometry
```

Ejecuta las pruebas de geometría para:

- 320×240;
- Pac-Man 224×288.

También puede ejecutarse únicamente la prueba del sistema de vídeo:

```bash
make video-system
```

---

## Suite C

`make test` compila y ejecuta automáticamente los archivos:

```text
tests/**/*_test.c
```

La suite cubre actualmente:

- file loader;
- ROM loader de Pac-Man;
- Namco WSG;
- audio board;
- CPU board;
- ROM set;
- vídeo;
- sprites;
- tilemap;
- máquina Pac-Man;
- sincronización de vídeo;
- integración de audio;
- core Z80.

`zex_runner.c` no pertenece a esta suite porque requiere una imagen ZEX externa.

---

# Artefactos de build

Todo lo generado debe permanecer dentro de:

```text
build/
```

El directorio puede eliminarse por completo en cualquier momento:

```bash
rm -rf build
```

y reconstruirse ejecutando nuevamente los targets de `make`.

`build/` no lo vamos a encontrar en Git.

---

# Tang Nano 20K

El directorio:

```text
fpga/tangnano20k/
```

separa el código específico de placa del RTL genérico.

Actualmente contiene una frontera lógica para:

- entrada de raster desde un futuro RP2350/RP2354;
- conexión con `video_system`;
- interfaz abstracta de memoria;
- salida RGB/sync para una futura capa TMDS.

El proyecto **todavía no afirma funcionamiento sobre una Tang Nano 20K física**.

Antes de completar el target real deben verificarse mediante documentación oficial de la revisión exacta de la placa:

- oscilador y pin de clock;
- package y speed grade del FPGA;
- SDRAM instalada;
- pines y timing SDRAM;
- pines HDMI/TMDS;
- primitivas Gowin necesarias;
- configuración PLL;
- constraints.

No deben inventarse constraints ni parámetros eléctricos sin esa información.

El estado específico se documenta también en:

```text
fpga/tangnano20k/README.md
```

---

# Flujo recomendado de desarrollo

Después de modificar C:

```bash
make test
```

Después de modificar RTL:

```bash
make lint
make test
```

Para probar Pac-Man:

```bash
make pacman-host
./build/host/pacman-sim
```

Para validar la integración completa con RTL:

```bash
./build/host/pacman-video-system-sdl
```

---

# Principios de diseño

El proyecto intenta mantener estas reglas:

- dispositivos independientes de máquinas concretas cuando sea posible;
- placas como fronteras claras de hardware;
- máquinas como composición de placas;
- tiempo de emulación derivado del hardware y no del host;
- sin `malloc` dentro de la máquina emulada;
- raster fuente sin backpressure;
- ROM loading aislado en la plataforma host;
- RTL genérico separado de adapters específicos de placa;
- pruebas deterministas;
- artefactos generados únicamente bajo `build/`;
- ROMs comerciales fuera del repositorio.

---

# Estado del target físico

La arquitectura y la simulación están bastante avanzadas, pero la implementación FPGA física sigue siendo trabajo futuro.

En particular, todavía faltan:

- controlador SDRAM físico;
- PLL de placa;
- constraints verificados;
- TMDS/HDMI físico;
- síntesis;
- timing closure;
- bitstream;
- pruebas sobre hardware real.

Hasta completar esas etapas, los resultados RTL se consideran validados en simulación, no sobre FPGA física.
