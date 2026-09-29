# Entrada local de ROMs

Coloca las diez ROMs/PROMs de Pac-Man indicadas en el [README raíz](../../README.es.md#roms-de-pac-man) en `common/roms/pacman/`. Esta distribución privada conserva los archivos entregados por el usuario; Git los ignora. El loader nativo y el empaquetador JavaScript utilizan este mismo directorio. La plantilla web sin ROMs puede versionarse; el JavaScript generado sí contiene esos bytes y permanece bajo `build/`, ignorado.
