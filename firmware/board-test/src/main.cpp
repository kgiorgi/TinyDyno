#include <Arduino.h>
#include <Wire.h>
#include <ESP_I2S.h>
#include <math.h>

constexpr int PIN_LED = 2;
constexpr int PIN_BIN = 0;
constexpr int PIN_MARK = 1;
constexpr int PIN_SDA = 4;
constexpr int PIN_SCL = 5;
constexpr int PIN_INA_ALERT = 6;
constexpr int PIN_IMU_INT1 = 7;
constexpr int PIN_MIC_CLK = 42;
constexpr int PIN_MIC_DATA = 41;
constexpr bool LED_ON = 0;
constexpr bool LED_OFF = 1;

constexpr uint8_t INA_ADDR = 0x40;
constexpr uint8_t INA_CONFIG = 0x00;
constexpr uint8_t INA_SHUNT = 0x01;
constexpr uint8_t INA_BUS = 0x02;
constexpr uint8_t INA_POWER = 0x03;
constexpr uint8_t INA_CURRENT = 0x04;
constexpr uint8_t INA_CAL = 0x05;
constexpr uint8_t INA_MASK = 0x06;
constexpr uint8_t INA_MFR_ID = 0xFE;
constexpr uint8_t INA_DIE_ID = 0xFF;
constexpr float SHUNT_OHMS = 0.075f;
constexpr float CURRENT_LSB_A = 0.0001f;
constexpr uint16_t INA_CAL_VALUE = 683;
constexpr uint16_t INA_CONFIG_VAL = 0x4527;
constexpr uint16_t INA_MASK_CNVR = 0x0400;

constexpr uint8_t IMU_ADDR = 0x6A;
constexpr uint8_t IMU_INT1_CTRL = 0x0D;
constexpr uint8_t IMU_ID = 0x0F;
constexpr uint8_t IMU_CTRL1_XL = 0x10;
constexpr uint8_t IMU_CTRL2_G = 0x11;
constexpr uint8_t IMU_CTRL3_C = 0x12;
constexpr uint8_t IMU_OUT_TEMP = 0x20;
constexpr uint8_t IMU_ID_VAL = 0x6A;
constexpr float ACCEL_MG_PER_LSB = 0.061f;
constexpr float GYRO_DPS_PER_LSB = 0.00875f;

constexpr uint32_t MIC_RATE = 16000;
I2SClass mic;
bool mic0k = false;
int micChannel = 0;

int passCount = 0;
int testCount = 0;

void report(const char *name, bool passed, const String &detail)
{
    testCount++;
    if (passed)
        passCount++;
    Serial.printf("[%s] %-14s %s\n", passed ? "PASS" : "FAIL", name, detail.c_str());
}

void info(const char *name, const String &detail)
{
    Serial.printf("[INFO] %-14s %s\n", name, detail.c_str());
}

bool i2cWrite8(uint8_t addr, uint8_t reg, uint8_t value)
{
    Wire.beginTransmission(addr);
    Wire.write(reg);
    Wire.write(value);
    return Wire.endTransmission() == 0;
}

bool i2cReadBytes(uint8_t addr, uint8_t reg, uint8_t *buf, size_t n)
{
    Wire.beginTransmission(addr);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0)
        return false;
    if (Wire.requestFrom((int)addr, (int)n) != (int)n)
        return false;
    for (size_t i = 0; i < n; i++)
        buf[i] = Wire.read();
    return true;
}

bool i2cRead8(uint8_t addr, uint8_t reg, uint8_t &value)
{
    return i2cReadBytes(addr, reg, &value, 1);
}

bool i2cWrite16(uint8_t addr, uint8_t reg, uint16_t value)
{
    Wire.beginTransmission(addr);
    Wire.write(reg);
    Wire.write(value >> 8);
    Wire.write(value & 0xFF);
    return Wire.endTransmission() == 0;
}

bool i2cRead16(uint8_t addr, uint8_t reg, uint16_t &value)
{
    uint8_t b[2];
    if (!i2cReadBytes(addr, reg, b, 2))
        return false;
    value = (uint16_t(b[0]) << 8) | b[1];
    return true;
}

bool i2cPresent(uint8_t addr)
{
    Wire.beginTransmission(addr);
    return Wire.endTransmission() == 0;
}

struct PowerReading
{
    bool ok;
    float busV;
    float currentmA;
    float powermW;
    float shuntmV;
};

PowerReading readPower()
{
    PowerReading r{false, 0, 0, 0, 0};
    uint16_t bus, cur, pow, shunt;
    if (!i2cRead16(INA_ADDR, INA_BUS, bus))
        return r;
    if (!i2cRead16(INA_ADDR, INA_CURRENT, cur))
        return r;
    if (!i2cRead16(INA_ADDR, INA_POWER, pow))
        return r;
    if (!i2cRead16(INA_ADDR, INA_SHUNT, shunt))
        return r;
    r.ok = true;
    r.busV = bus * 0.00125f;
    r.currentmA = int16_t(cur) * CURRENT_LSB_A * 1000.0f;
    r.powermW = pow * 25.0f * CURRENT_LSB_A * 1000.0f;
    r.shuntmV = int16_t(shunt) * 0.0025f;
    return r;
}

struct ImuReading
{
    bool ok;
    float tempC;
    float gx, gy, gz;
    float ax, ay, az;
};

ImuReading readImu()
{
    ImuReading r{false, 0, 0, 0, 0, 0, 0, 0};
    uint8_t b[14];
    if (!i2cReadBytes(IMU_ADDR, IMU_OUT_TEMP, b, sizeof(b)))
        return r;
    auto s16 = [&](int i)
    { return int16_t(uint16_t(b[i]) | (uint16_t(b[i + 1]) << 8)); };
    r.ok = true;
    r.tempC = 25.0f + s16(0) / 256.0f;
    r.gx = s16(2) * GYRO_DPS_PER_LSB;
    r.gy = s16(4) * GYRO_DPS_PER_LSB;
    r.gz = s16(6) * GYRO_DPS_PER_LSB;
    r.ax = s16(8) * ACCEL_MG_PER_LSB;
    r.ay = s16(10) * ACCEL_MG_PER_LSB;
    r.az = s16(12) * ACCEL_MG_PER_LSB;
    return r;
}

struct MicStats
{
    size_t frames;
    int16_t minV[2];
    int16_t maxV[2];
    float rms[2];
};

MicStats readMic(size_t frames)
{
    MicStats s{0, {INT16_MAX, INT16_MAX}, {INT16_MIN, INT16_MIN}, {0, 0}};
    static int16_t buf[512];
    double sum[2] = {0, 0}, sumSq[2] = {0, 0};

    while (s.frames < frames)
    {
        size_t want = min(frames - s.frames, sizeof(buf) / 4) * 4;
        size_t got = mic.readBytes((char *)buf, want);
        size_t count = got / 4;
        if (count == 0)
            break;
        for (size_t i = 0; i < count; i++)
        {
            for (int ch = 0; ch < 2; ch++)
            {
                int16_t v = buf[i * 2 + ch];
                if (v < s.minV[ch])
                    s.minV[ch] = v;
                if (v > s.maxV[ch])
                    s.maxV[ch] = v;
                sum[ch] += v;
                sumSq[ch] += double(v) * v;
            }
        }
        s.frames += count;
    }
    for (int ch = 0; ch < 2 && s.frames > 0; ch++)
    {
        double mean = sum[ch] / s.frames;
        double var = sumSq[ch] / s.frames - mean * mean;
        s.rms[ch] = var > 0 ? sqrt(var) : 0;
    }
    return s;
}

void testchip()
{
    info("chip", String(ESP.getChipModel()) + " rev " + ESP.getChipRevision() +
                     ", " + ESP.getChipCores() + " cores, " + ESP.getCpuFreqMHz() + "MHz");

    uint32_t flash = ESP.getFlashChipSize();
    report("flash", flash >= 16u * 1024 * 1024, String(flash / (1024 * 1024)) + " MB ( expect 16 )");

    uint32_t psram = ESP.getPsramSize();
    report("psram size", psram > 0, String(psram / (1024 * 1024)) + "MB expect 8");

    const size_t n = 1024 * 1024 / 4;
    uint32_t *p = (uint32_t *)ps_malloc(n * 4);
    bool ok = p != nullptr;
    if (ok)
    {
        for (size_t i = 0; i < n; i++)
            p[i] = i * 2654435761u;
        for (size_t i = 0; i < n && ok; i++)
            ok = p[i] == i * 2654435761u;
        free(p);
    }
    report("psram r/w", ok, ok ? "1 MB pattern OK" : "allocaion or read-back failed");
}

void testLed()
{
    pinMode(PIN_LED, OUTPUT);
    for (int i = 0; i < 3; i++)
    {
        digitalWrite(PIN_LED, LED_ON);
        delay(200);
        digitalWrite(PIN_LED, LED_OFF);
        delay(200);
    }
    info("led", "D1 should have blinked 3 times");
}

void testMarker()
{
    pinMode(PIN_MARK, OUTPUT);
    for (int i = 0; i < 5; i++)
    {
        digitalWrite(PIN_MARK, HIGH);
        delay(50);
        digitalWrite(PIN_MARK, LOW);
        delay(50);
    }
    info("test marl", "TP1 pulsed 5 times (check with a scope/LED if you want)");
}

void testI2cScan()
{
    bool started = Wire.begin(PIN_SDA, PIN_SCL, 400000);
    String found;
    for (uint8_t a = 1; a < 127; a++)
    {
        if (i2cPresent(a))
            found += String("0x") + String(a, HEX) + " ";
    }
    bool ina = i2cPresent(INA_ADDR), imu = i2cPresent(IMU_ADDR);
    report("i2c scan", started && ina && imu,
           "found: " + (found.length() ? found : String("nothing")) + "(expect 0x40 0x6a)");
}

void testIna226()
{
    uint16_t mfr = 0, die = 0;
    bool id0k = i2cRead16(INA_ADDR, INA_MFR_ID, mfr) && i2cRead16(INA_ADDR, INA_DIE_ID, die) && mfr == 0x5449 && die == 0x2260;
    report("ina226 id", id0k, "mfr 0x" + String(mfr, HEX) + " die 0x" + String(die, HEX) + "(expect 0x5449 0x2260)");
    if (!id0k)
        return;
    i2cWrite16(INA_ADDR, INA_CONFIG, INA_CONFIG_VAL);
    i2cWrite16(INA_ADDR, INA_CAL, INA_CAL_VALUE);
    delay(100);

    PowerReading p = readPower();
    report("ina226 bus", p.ok && p.busV > 3.0f && p.busV < 3.6f,
           String(p.busV, 3) + "V (expect ~3.3)");
    report("ina226 curr", p.ok && p.currentmA > 5.0f && p.currentmA < 500.0f,
           String(p.currentmA, 1) + " mA, " + String(p.powermW, +1) + " mW (ESP32 draw, expect 20/150 mA)");
}

void testInaAlert()
{
    pinMode(PIN_INA_ALERT, INPUT_PULLUP);
    uint16_t dummy;
    i2cWrite16(INA_ADDR, INA_MASK, INA_MASK_CNVR);
    i2cRead16(INA_ADDR, INA_MASK, dummy);

    bool sawLow = false;
    uint32_t t0 = millis();
    while (millis() - t0 < 200 && !sawLow)
        sawLow = digitalRead(PIN_INA_ALERT) == 0;

    i2cRead16(INA_ADDR, INA_MASK, dummy);
    i2cWrite16(INA_ADDR, INA_MASK, 0x0000);
    report("ina alert", sawLow, sawLow ? "IO6 pulled LOW on coversion ready" : "IO6 never went LOW");
}

void testImu()
{
    uint8_t who = 0;
    bool id0k = i2cRead8(IMU_ADDR, IMU_ID, who) && who == IMU_ID_VAL;
    report("imu id", id0k, "ID 0x" + String(who, HEX) + " (expect 0x6a)");
    if (!id0k)
        return;

    i2cWrite8(IMU_ADDR, IMU_CTRL3_C, 0x01);
    delay(20);
    i2cWrite8(IMU_ADDR, IMU_CTRL3_C, 0x44);
    i2cWrite8(IMU_ADDR, IMU_CTRL1_XL, 0x40);
    i2cWrite8(IMU_ADDR, IMU_CTRL2_G, 0x40);
    delay(100);

    ImuReading r = readImu();
    float g = sqrtf(r.ax * r.ax + r.ay * r.ay + r.az * r.az);
    report("imu accel", r.ok && g > 800 && g < 1200,
           "x " + String(r.ax, 0) + "y " + String(r.ay, 0) + "z " + String(r.az, 0) +
               "mg , |a| = " + String(g, 0) + " (expect ~ 1000 at rest)");
    report("imu gyro", r.ok && fabsf(r.gx) < 20 && fabsf(r.gy) < 20 && fabsf(r.gz) < 20,
           "x " + String(r.gx, 1) + "y " + String(r.gy, 1) + "z " + String(r.gz, 1) +
               "dps (expect ~0 at rest)");
    info("imu temp", String(r.tempC, 1) + "C ");
}

void testImuInt()
{
    pinMode(PIN_IMU_INT1, INPUT);
    readImu();
    i2cWrite8(IMU_ADDR, IMU_INT1_CTRL, 0x01);

    bool sawHigh = false;
    uint32_t t0 = millis();
    while (millis() - t0 < 100 && !sawHigh)
        sawHigh = digitalRead(PIN_IMU_INT1) == 1;

    i2cWrite8(IMU_ADDR, IMU_INT1_CTRL, 0x00);
    report("imu int1", sawHigh, sawHigh ? "IO7 went HIGH on data ready" : "IO7 never went HIGH");
}

void testMic()
{
    mic.setPinsPdmRx(PIN_MIC_CLK, PIN_MIC_DATA);
    if (!mic.begin(I2S_MODE_PDM_RX, MIC_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO))
    {
        report("mic", false, "I2S PDM driver failed to start");
        return;
    }
    readMic(MIC_RATE / 5);
    MicStats s = readMic(MIC_RATE);
    bool live[2];
    for (int ch = 0; ch < 2; ch++)
        live[ch] = s.maxV[ch] != s.minV[ch] && s.rms[ch] > 1.0f;
    micChannel = (live[1] && (!live[0] || s.rms[1] > s.rms[0])) ? 1 : 0;
    mic0k = s.frames > 0 && (live[0] || live[1]);

    report("mic", mic0k,
           String(s.frames) + " frames, L rms " + String(s.rms[0], 1) + " [" + s.minV[0] + ".." + s.maxV[0] +
               "], R rms " + String(s.rms[1], 1) + " [" + s.minV[1] + ".." + s.maxV[1] + "]" +
               (mic0k ? String(", using ") + (micChannel ? "R" : "L") : String("")));
}

void setup()
{
    pinMode(PIN_BIN, INPUT_PULLUP);
    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, LED_OFF);

    Serial.begin(115200);
    uint32_t t0 = millis();
    while (!Serial && millis() - t0 < 3000)
        delay(10);
    delay(200);

    Serial.println();
    Serial.println("------");
    Serial.println(" TinyDyno board-test ");
    Serial.println("---------");

    testchip();
    testLed();
    testMarker();
    testI2cScan();
    testIna226();
    testInaAlert();
    testImu();
    testImuInt();
    testMic();

    Serial.println("------------------------------");
    Serial.printf(" %d / %d tests passed %s\n", passCount, testCount,
                  passCount == testCount ? "- BOARD OK" : "- CHECK THE FAILS ABOVE");
    Serial.println(" Live readings every 0.5 s. Press BOOT to test the button.");
    Serial.println("------------------------------");
}

void loop()
{
    static uint32_t last = 0;
    static bool led = false;
    static bool lastbtn = 1;

    bool btn = digitalRead(PIN_BIN);
    if (btn == 0 && lastbtn == 1)
        Serial.println("[INFO] button BOOT pressed");
    lastbtn = btn;

    if (millis() - last < 500)
        return;
    last = millis();
    digitalWrite(PIN_MARK, 1);
    PowerReading p = readPower();
    ImuReading r = readImu();
    float micRms = 0;
    if (mic0k)
        micRms = readMic(MIC_RATE / 20).rms[micChannel];
    digitalWrite(PIN_MARK, 0);

    Serial.printf("%6.3f V %7.1f mA | acc %6.0f %6.0f %6.0f mg | gyro %6.1f %6.1f %6.1f dps | mic %7.1f | btn %s\n",
                  p.busV, p.currentmA, r.ax, r.ay, r.az, r.gx, r.gy, r.gz, micRms,
                  btn == LOW ? "DOWN" : "up");

    led = !led;
    digitalWrite(PIN_LED, led ? LED_ON : LED_OFF);
}