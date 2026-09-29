# Raspberry Pi — target nativo previsto

Este target **no está implementado**. El snapshot original solo contiene los directorios vacíos `boot/`, `include/`, `src/`, `linker/` y `scripts/`. Sus nombres reservan áreas de trabajo: no implican una imagen arrancable, modelo de placa, mapa de periféricos, layout de linker ni validación física.

La interfaz nativa genérica está en [standalone/include/platform.h](../../include/platform.h). Una futura implementación debe elegir un modelo exacto de Raspberry Pi y verificar el hardware antes de definir relojes, mapas de memoria o archivos de arranque. El juego para navegador es independiente de este target.
