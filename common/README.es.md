# Componentes comunes

[Resumen del proyecto](../README.es.md) | [English](README.md)

Este directorio contiene modelos C reutilizables (`devices`, `boards`, `machines`, `romsets`), el pipeline genérico SystemVerilog (`rtl`) y las ROMs proporcionadas localmente (`roms`). No mezclar SDL, DOM, constraints físicos ni lectura del sistema de archivos con los modelos de hardware.

Los ejecutables modulares nativos utilizan los modelos C. El navegador es un port JavaScript separado: comparte ROMs de entrada y comportamiento modelado, no código C compilado. La interfaz standalone nativa es un punto de integración futuro, no un port Raspberry Pi existente.
