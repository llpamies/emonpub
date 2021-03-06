# EmonPub

Reads data from emontx devices and exports it to EmonCMS.

This is a personal project to read data from three [EmonTx
Shield](https://wiki.openenergymonitor.org/index.php/EmonTx_Arduino_Shield)
devices, using an ESP8266 board, and export the data to
[EmonCMS](https://www.emoncms.org).

The firmware for the EmonTX Shield boards is in: [emontx](https://github.com/llpamies/emontx)

The [OpenEnergyMonitor](https://www.openenergymonitor.org/) project has existing
solutions that do exactly that. However, I didn't manage to make them work as I
intended. The problem is that the
[EmonTxV3CM](https://github.com/openenergymonitor/EmonTxV3CM) firmware exposes
data only via serial interface. You can then use the
[EmonESP](https://github.com/openenergymonitor/EmonESP) firmware to read that
serial interface and export the readings to EmonCMS.  However, for three EmonTx,
I would need three ESP8266 boards since they only have one single serial port.
Alternatively, you could also use soft-serial ports, but then you would need
more wires than with the existing I2C solution.

I also tried to integrate this `emonpub` project into the existing EmonESP, but
the size of the binary was getting to large, and the firmware was crashing for
some still unknown reason.

# The Setup

This is my setup, of three EmonTx Shields, each with an Arduino Leonardo, and a
single NodeMCU board that sequentially reads from all of them and sends data to
EmonCMS:

![energy_monitor](https://github.com/llpamies/emonpub/blob/main/energy_monitor.jpg?raw=true)

The box is custom-made and 3D-printed: [Emon
Box](https://cad.onshape.com/documents/c6e4b7e304f4f2130f204cad/w/c06564a53a9a23fffc05c28a/e/69a91d13ed4525e4a211841a?renderMode=0&uiState=61e229798163d20dec98bcd3n)
