# TinyDyno

![TinyDyno](docs/images/TinyDyno.png)

An ESP32-S3 board that measures the speed and energy efficiency of one neural network inference.

**What is it exactly?**
TinyDyno is a custom 72 by 72 mm, 2 layered PCB based on ESP32-S3.It is basically a test bench for machine learning on tiny devices (AKA edgeAI). It runs a neural network based on the data from sensors and measures every inference for time and energy. Time is how long the inference took and energy is how much ESP32-S3 has used during it.

To account for actual use cases it has an IMU(motion sensor) and a microphone which are for gesture recogniton and keyword detector so the usual run look something like this:
1. Record the input from either sensors
2. Run the same model on the same input but with different libraries.(My kernels VS Espressif's ESP-DL VS TensorFlow Lite Micro)
3. Measure each run using the INA226 (power monitor) which reports current/voltage going into the ESP32-S3 which does the power over time math
4. Get the results and make conclusions

(A test pad (TP1) goes high during each inference, so the timing can be checked independently with an oscilloscope or logic analyzer.)

**The WHY behind it all**

Wanting to optimize the field I am currently curious about (AI) I brainstormed for a bit and came to the conclusion that you can't optimize something that you can't measure. After a bit of digging I found few issue it would solve on top of the qol it would provide for me:
1.Most benchmarks only report speed so a faster kernel that uses more power can still drain the battery sooner.
2.The USB power meters are quite slow. They update a few times per second, while one inference takes about 5–50 ms.

TinyDyno fixes this by giving the ESP32-S3 its own power rail with a current-sense resistor, so the power monitor sees only the chip running the model, about once every millisecond.(Also it's not like you can find stuff like this easily to buy)

**how does it work?**

![TinyDyno](docs/images/tinydyno.drawio.png)


