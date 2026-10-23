
**TinyDyno board test**

This is the first thing to flash onto a new TinyDyno after soldering it. It checks every part on the board one by one, prints PASS or FAIL for each over USB, and then keeps printing live readings so you can see what's up.

**What it checks**

The ESP32-S3: that it sees 16 MB of flash and 8 MB of PSRAM, and that it can write and read back 1 MB of PSRAM.

The LED blinks 3 times and the test mark pin (TP1) pulses 5 times. These two you check by eye or with a scope.

The I2C bus: it scans for devices and should find the power monitor at 0x40 and the IMU at 0x6A.

The INA226 power monitor: it reads the chip's ID, then the supply voltage (should be about 3.3 V) and the current the ESP32 is drawing (somewhere around 20 to 150 mA). It also checks that the alert line on IO6 actually reaches the ESP32.

The LSM6DS3TR-C IMU: it reads the chip's ID, then the accelerometer and gyro. Lying still on a table the accelerometer should read about 1 g and the gyro close to zero. It also checks the interrupt line on IO7.

The microphone: it records 1 second and checks that the signal isn't flat. Clap or talk near it and the number goes up.

The BOOT button gets checked in the live readout: press it and it prints a line.

**How to flash**

Open this folder in VS Code with PlatformIO and plug the board in with USB-C.

Hold the BOOT button, tap EN, then let go of BOOT. The board is now waiting for new firmware.

Click Upload in the PlatformIO bar at the bottom. When it's done, tap EN to restart and open the Serial Monitor.

Keep the board flat and still while the tests run, otherwise the IMU test can fail.

**What you should see**

The tests print one line each, something like:

```
[PASS] flash          16 MB (expect 16)
[PASS] i2c scan       found: 0x40 0x6a
[PASS] ina226 bus     3.298 V (expect ~3.3)
[PASS] imu accel      x 12 y -8 z 1003 mg
[PASS] mic            16000 frames, L rms 35.2, using L
```

and at the end a line saying how many passed. After that it prints voltage, current, IMU and mic readings every half second.

**If something fails**

If PSRAM shows 0, check that the module is the N16R8 and that platformio.ini wasn't changed.

If the I2C scan finds nothing, check the soldering on the INA226 and the IMU, the two 5.1k pull-up resistors, and the second 3.3 V regulator that powers them.

If only one I2C address shows up, check that one chip's soldering.

If the current reads zero or negative, check that the JP1 jumper is bridged and the 75 mΩ shunt resistor is soldered properly.

If the alert or interrupt test fails but the chip itself passed, the trace from that chip to the ESP32 is the problem.

If the mic reads flat, check the mic's soldering. It's a bottom-port part, so the pads underneath are easy to miss.
