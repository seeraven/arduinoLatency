Flashing the Software on an Arduino
===================================

Open the arduino IDE and load the sketch provided in the directory `arduino`. In
the `Tools` menu, select your Arduino board and - if available - the Arduino
`Processor` as `ATmega328P (Old Bootloader)`. Then use `Sketch` -> `Upload` from
the menu or the arrow button.

Test the software by opening the `Tools` -> `Serial Monitor` and select the
baud rate `115200 baud`. Then enter some chars and ensure you get the same chars
back.

If you have problems accessing the serial port, ensure you are in the group
`dialout` or add a udev-rule. To add yourself to the `dialout` group, call

    sudo gpasswd -a <username> dialout

and re-login or reboot.

If you want to use a udev-rule, create a new file
`/etc/udev/rules.d/50-serialusb.rules` with the following content:

    KERNEL=="ttyUSB[0-9]*",MODE="0666"
    KERNEL=="ttyACM[0-9]*",MODE="0666"

Then (re)plug-in the device.
