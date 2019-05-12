Latency Measurement of Arduino USB Interface
============================================

When using the Arduino to control something from a host PC, the question
arises how long it takes for sending control information from the host to
the Arduino. In this small project, this topic is investigated using simple
test setups with an Arduino and a Raspberry Pi 3B+ resp. an Odroid XU4.


Communication with the Arduino
------------------------------

The Arduino is usually connected to the host via USB. Actually, the
Arduino itself consists of an ATmega328 microcontroller and an USB to serial
converter like an CH340G, FTDI FT232RL, or - like the Uno - an ATmega16U2. Such
a chip is necessary, since most modern PCs don't have a serial port anymore but
rather USB ports.

The USB protocol is rather complex compared to a simple serial line and the
question arises how long it actually takes for sending and receiving data from
the Arduino over USB.


Hardware Setup
--------------

A naive approach is to send one byte from the host to the Arduino, let the
Arduino write one byte back as an answer and measure the time until the answer
arrives at the host. The time between sending the request and receiving the
answer can then divided by two to get a rough estimate of the time in one
direction. We will call this method _roundtrip_ and will see that
the times measured here are far from the actual truth.

Since we are interested in the time until some action is performed by the
Arduino, we use a digital output pin of the Arduino and connect it to an
input pin of the host. Of course, this will only work if we have such a host,
so we use an Raspberry Pi 3B+ or an Odroid XU4. They have GPIO pins that can be
used to measure and control stuff similar to an Arduino.

In addition, we are interested how long it actually takes for an Raspberry Pi
resp. an Odroid XU4 to detect a signal change on one of its GPIO pins. So we
connect two GPIO pins on the host and use one as an output pin and one as an
input pin to determine the latency for the signal change detection.

Since the Arduino and the Raspberry Pi resp. the Odroid XU4 uses different
voltage levels, we need to apply a voltage divider when connecting the
Arduino to the host:


### Voltage Divider Arduino - Raspberry Pi

The Arduino uses 5V levels whereas the Raspberry Pi uses 3.3V. By using a
2.2 kOhm and a 3.3 kOhm resistor, we get the matching voltage for the
Raspberry Pi:

```
D2  ---------+
             |
            +-+
            | | 2200 Ohm (red, red, black, brown, (brown))
            +-+
             |
             +-------------------- GPIO 7 (Pin 7, BCM Pin 4)
             |
            +-+
            | | 3300 Ohm (orange, orange, black, brown, (brown))
            +-+
             |
GND ---------+-------------------- GND (Pin 6)
```

For the interrupt latency test a direct connection between the Raspberry Pi GPIO
Pins 35 and 37 is used:
```
Pin 35 (Output) (BCM Pin 19) ----+
                                 |
Pin 37 (Input)  (BCM Pin 26) ----+
```


### Voltage Divider Arduino - Odroid XU4

The OdroidXU4 uses only 1.8V levels, so we use a 6.8kOhm and a 3.3 kOhm resistor:

```
D2  ---------+
             |
            +-+
            | | 6800 Ohm (blue, grey, black, brown, (brown))
            +-+
             |
             +-------------------- GPIO 37 (Pin 27)
             |
            +-+
            | | 3300 Ohm (orange, orange, black, brown, (brown))
            +-+
             |
GND ---------+-------------------- GND (Pin 30)
```

For the interrupt latency test a direct connection between the OdroidXU4 GPIO
Pins 13 and 17 is used:
```
Pin 13 (Output) (GPIO 21) ----+
                              |
Pin 17 (Input)  (GPIO 22) ----+
```


Tests and Results
-----------------

### Roundtrip

The first test was the previously mentioned _roundtrip_ test that measures how
long it takes for one byte sent to the Arduino to be received again by the host.
This test was first performed on a laptop and later on the Raspberry Pi 3B+ and
the OdroidXU4. The results are as follows:

| Arduino | Serial2USB Chip | Time on Laptop         | Time on Raspberry Pi 3B+ | Time on OdroidXU4   |
| ------- |:---------------:| ---------------------- | ------------------------ | ------------------- |
| Nano    | FTDI FT232RL    | 0.98 [0.80,1.13] ms    | 2.17 [2.14,2.24] ms      | 0.92 [0.87,0.97] ms |
| Nano    | CH340G          | 2.46 [2.03,2.77] ms    |                          | 2.18 [2.14,2.22] ms |
| Uno     | ATmega16U2      | 3.16 [2.87,3.42] ms    |                          | 3.18 [3.13,3.22] ms |

Please note that these measurements were taken with setting the FTDI latency
register to 1ms. As you can see, the OdroidXU4 has more or less the same timings
as the laptop. However, the Raspberry Pi 3B+ showed a large difference on the
FTDI chip. Interestingly enough, it seemed not possible to go below the 2ms with
the Raspberry Pi. It might be because the USB port on the Raspberry Pi 3B+ is
actually on the LAN-chip which itself uses USB and can't be deactivated.

Nevertheless, the results vary quite a lot between the individual chips. So the
next idea was to use an interrupt and measure the time between sending a byte
and the reaction of the Arduino (putting a pin to high or low). But before we do
that, we should know how the interrupt latency on the Raspberry Pi 3B+ resp. the
OdroidXU4 is.


### Interrupt latency (WiringPi interrupt handler)

The WiringPi library provides "interrupt" handlers in the user-space, so that was
the first method I tried out. Let's look at the results:

| Platform         | Start Digital Write to Interrupt | End Digital Write to Interrupt |
| ---------------- | -------------------------------- | ------------------------------ |
| Raspberry Pi 3B+ | 21.3 [21.04,22.03] mus           | 15.11 [14.90,15.68] mus        |
| OdroidXU4        | 57.8 [57.12,63.42] mus           | 55.38 [54.71,60.71] mus        |

Here "Start Digital Write" means the time was measured right before the command to
change the digital pin was called, and "End Digital Write" means the time was
measured right after the command.

The first number gives the median, and the range is the 80% range (removing the 10% lowest
and highest values).

As we can see, it took the OdroidXU4 much longer to react on the interrupt and it has
a much higher variance.

But why is the interrupt latency so high? It should be more or less 0. Well when
looking at the code of the WiringPi library, it is quite obvious: There is no actual
interrupt handler, but rather a high priority thread that polls the sysfs interface.

So the next step was to use a real interrupt handler by writing a small kernel module.


### Interrupt latency (Kernel Module)

The results with the kernel module are:

| Platform         | Start Digital Write to Interrupt | End Digital Write to Interrupt |
| ---------------- | -------------------------------- | ------------------------------ |
| Raspberry Pi 3B+ | 2.03 [1.98,2.14] mus             | -1.04 [-1.15,-0.99] mus        |
| OdroidXU4        | 6.58 [6.50,7.33] mus             | 4.12 [4.04,4.88] mus           |

Again, the Raspberry Pi 3B+ is much quicker on reacting to the interrupt than
the OdroidXU4. This is also the first time we see a negative number, how can
this happen? Well the explanation is quite simple when looking at the following
timing diagram:

```
 | measure first time
 | set digital pin to high
t|                                     interrupt handler: measure time
 | measure second time
 v
```
So it takes about 2 microseconds from the first measure to the interrupt handler
and about one microsecond from the interrupt handler to the second time
measurement.

For the OdroidXU4 the timing is different. The interrupt is handled after the
second time measurement and therefore both values are positive. In a timing diagram:

```
 | measure first time
 | set digital pin to high
t| measure second time
 |                                    interrupt handler: measure time
 v
```

So with these timings, we can assume that we have a interrupt latency of
2 microseconds on the Raspberry Pi 3B+ and 6.6 microseconds on the OdroidXU4
wrt the start of the interrupt causing action.


### Latency of Arduino Nano with FTDI

Since there are differences between the Raspberry Pi 3B+ and the OdroidXU4, we
want to investigate whether they are too severe to rule out the OdroidXU4 or
whether the OdroidXU4 can be used for the time measurements. Since the OdroidXU4
has much better USB performance, it would be benefitial to use it. So let's
compare the actual timings found when using the Arduino Nano with an FTDI:

| Platform         | Start Serial Write to Interrupt  | Minus Interrupt Latency        |
| ---------------- | -------------------------------- | ------------------------------ |
| Raspberry Pi 3B+ | 166.46 [141.98,194.12] mus       | 164.46 [139.98,192.12] mus     |
| OdroidXU4        | 173.79 [153.75,188.38] mus       | 167.19 [147.15,181,78] mus     |

So we see a slight difference between the two. However, the interpretation of the
Raspberry Pi results are somewhat difficult because of multiple modes.


### Latency of Different Arduinos

So using the OdroidXU4, we can now compare the various Arduinos:

| Arduino | Serial2USB Chip | Start Serial Write to Interrupt  | Minus Interrupt Latency        | Roundtrip                     |
| ------- |:---------------:| -------------------------------- | ------------------------------ | ----------------------------- |
| Nano    | FTDI FT232RL    | 173.79 [153.75,188.38] mus       | 167.19 [147.15,181,78] mus     | 924.46 [874.67,973.83] mus    |
| Nano    | CH340G          | 136.50 [131.33,148.29] mus       | 129.9 [124.73,141.69] mus      | 2178.58 [2137.75,2219.17] mus |
| Uno     | ATmega16U2      | 139.08 [134.42,154.25] mus       | 132.48 [127.82,147.65] mus     | 3180.17 [3134.46,3218.96] mus |

It is interesting that the FTDI provides the fastest feedback from the Arduino
back to the host but requires about 167 microseconds for activating the output
pin. The CH340G requires only 130 microseconds for the activation but the round
trip time is with 2.2 ms much higher than the one of the FTDI (0.9ms).
