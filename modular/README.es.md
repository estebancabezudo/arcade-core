# Integraciones modulares por plataforma

[Resumen del proyecto](../README.es.md) | [English](README.md)

`platforms/host/` compone la máquina compartida con lectura nativa de archivos, presentación SDL y el adaptador Verilator/SDL. `platforms/fpga/tangnano20k/` contiene la frontera lógica de la placa física, no un bitstream verificado. Los modelos C compartidos y RTL genérico permanecen en `../common/`.

Los comandos existentes `make pacman-host`, `make lint`, `make geometry` y `make test` se ejecutan desde la raíz. Se conservan los ejecutables en `build/host/`. El directorio de ROMs predeterminado pasa a `common/roms/pacman/`, relativo a la raíz; se mantiene el argumento para indicar otro directorio.
