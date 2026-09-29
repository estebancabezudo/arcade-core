# Shared components

[Project overview](../README.md) | [Español](README.es.md)

This directory owns reusable C hardware models (`devices`, `boards`, `machines`, `romsets`), the generic SystemVerilog video pipeline (`rtl`), and locally supplied ROM input (`roms`). Do not add SDL, browser DOM code, board pin constraints or filesystem loading to these hardware models.

Native modular executables consume the C models. The browser is a separate JavaScript port and shares only ROM input and modeled behavior, not compiled C code. The standalone native interface is a future integration point, not an existing Raspberry Pi port.
