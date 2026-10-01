# Primer arranque: Raspberry Pi 4 desde USB

[Guía de plataforma](../../README.es.md) | [English](README.md)

`kernel8.img`, `config.txt` y `SHA256SUMS` están en [el directorio del ejemplo generado](../../../../../build/standalone/raspberry_pi/examples/pi4_usb/), respecto a este documento. Desde la raíz, la ruta es `build/standalone/raspberry_pi/examples/pi4_usb/`.

La imagen se entrega para **pruebas físicas en Raspberry Pi 4 Model B**, no como afirmación de un arranque físico validado. Contiene las ROMs de Pac-Man recibidas y no utiliza Linux ni SDL/MAME/CRT.

## Procedimiento inicial del paquete recibido

1. Prepara un medio USB de arranque Pi 4 con el firmware oficial Raspberry Pi ya presente en la partición FAT de boot. El paquete original propone Raspberry Pi Imager para prepararlo; este proyecto no incluye firmware ni configura EEPROM. El arranque USB debe estar habilitado en el orden de arranque de la placa. Usa un medio dedicado a pruebas y respalda su configuración.
2. Monta la partición de arranque. Desde la raíz del proyecto instala el ejemplo preparado:

   ```bash
   bash standalone/platforms/raspberry_pi/scripts/install_pi4_example.sh /ruta/al/boot
   ```

   El instalador verifica SHA-256, respalda `config.txt` y `kernel8.img` existentes en una carpeta nueva `arcade-core-backup/install.*` y reemplaza únicamente esos dos archivos. Conserva los demás archivos de firmware. No formatea ni selecciona automáticamente un disco.
3. Desmonta el medio de forma segura, conecta HDMI y enciende la Pi. Retira una SD si tiene prioridad sobre USB. Si no intenta arrancar desde USB, utiliza las herramientas oficiales para configurar el firmware/EEPROM de la placa; este instalador no lo modifica.
4. El resultado previsto es Pac-Man directamente en attract mode, sin Linux. Para jugar se necesitan los controles GPIO siguientes; este kernel no admite teclado ni gamepad USB.

## Pulsadores

Los pulsadores activos en bajo conectan GPIO con GND y necesitan pull-ups externos a 3.3 V; la guía recibida propone 10 kΩ. Son identificadores GPIO, no números físicos de pines. Nunca conectes 5 V a GPIO.

| Función | GPIO |
| --- | ---: |
| Arriba / Abajo | 5 / 6 |
| Izquierda / Derecha | 13 / 19 |
| Moneda | 16 |
| Start 1 / Start 2 | 20 / 21 |

## Audio del backend recibido

El sonido utiliza PDM por software en GPIO18, no HDMI ni el jack analógico integrado. El paquete original propone este filtro inicial:

```text
GPIO18 -- 1 kΩ --+-- entrada amplificada
                 |
                10 nF
                 |
                GND
```

La guía recibida también recomienda acoplamiento capacitivo antes del amplificador. No conectes directamente un altavoz al GPIO. El circuito y el backend de audio síncrono siguen pendientes de validación física; USB HID, audio HDMI y DMA/PWM no están implementados.

## Recrear el ejemplo

Si eliminaste `build/` raíz, proporciona las ROMs comunes y el toolchain cruzado, y ejecuta `make pacman-rpi-example TOOLCHAIN=clang` desde la raíz. Esto recompila Pi 4 y regenera los tres archivos del ejemplo. [Validación y procedencia de la imagen](../../../../../docs/validation/raspberry-pi-integration.md).
