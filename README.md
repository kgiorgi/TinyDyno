# TinyDyno
esp32-s3 based board for measuring time and energy of edge AI. Its main use is to compare my kernels to ESP-DL and TensorFlow Lite Micro on identical inputs.
![TinyDyno](docs/images/TinyDyno.png)
so basically USB-C provides 5V. Two separate 3.3V regulators turn it into two rails: measured and peripheral.
Current flowing through the shunt creates a small voltage across it.The INA226(aka U3) power monitor measures it to get the current, and measures the 3.3 V rail itself to get the voltage. By multiplying it gets power. Its ALERT pin alerts the ESP32 when a new reading is ready.The shunt is also measured by a pair of thin sense traces that run straight from the shunt's pads to the INA226.
Energy = power × time. During an inference, the ESP32 reads the power about once a millisecond and adds up power × time.
Time comes from the ESP32's internal timer. Test point TP1 is set high when an inference starts and low when it ends, so a logic analyzer or oscilloscope can check the timing independently.
Ordered from JLCPCB, Standard assembly, top side only. JLC adds 5 mm edge rails on two sides; snap them off along the V-cut.
U3 change: the plan was the INA228, but it was out of stock at JLC. It was replaced with the INA226, which has the same footprint and pin order, so the layout didn't change.
MK1 rotation: the position file says 180°, but the microphone was corrected by hand in JLC's placement preview. Check it again on any re-order.
