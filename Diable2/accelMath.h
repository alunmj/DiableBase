bool isGyro = false;
// #define AF_LSM
#undef AF_LSM
#ifdef AF_LSM
#include "Adafruit_LSM6DS3TRC.h"

Adafruit_LSM6DS3TRC lsm6ds33;
#else
#include "LSM6DS3.h"
LSM6DS3 lsm6ds33(I2C_MODE, 0x6a);
#endif

class accel
{
    static float accel_x, accel_y, last_x, last_y;
public:
    static bool Setup()
    {
        accel_x = accel_y = last_x = last_y = 0.0;
#ifdef AF_LSM
// This section of code turns on the
#ifdef PIN_LSM6DS3TR_C_POWER
        pinMode(PIN_LSM6DS3TR_C_POWER, OUTPUT);
#if defined(TARGET_SEEED_XIAO_NRF52840_SENSE)
        NRF_P1->PIN_CNF[8] = ((uint32_t)NRF_GPIO_PIN_DIR_OUTPUT << GPIO_PIN_CNF_DIR_Pos) | ((uint32_t)NRF_GPIO_PIN_INPUT_DISCONNECT << GPIO_PIN_CNF_INPUT_Pos) | ((uint32_t)NRF_GPIO_PIN_NOPULL << GPIO_PIN_CNF_PULL_Pos) | ((uint32_t)NRF_GPIO_PIN_H0H1 << GPIO_PIN_CNF_DRIVE_Pos) | ((uint32_t)NRF_GPIO_PIN_NOSENSE << GPIO_PIN_CNF_SENSE_Pos);
#endif
        digitalWrite(PIN_LSM6DS3TR_C_POWER, HIGH);
        delay(10);
#endif

        if (!lsm6ds33.begin_I2C(0x6a, &Wire1))
#else
        if (IMU_SUCCESS != lsm6ds33.begin())
#endif
        {
            // if (!lsm6ds33.begin_SPI(LSM_CS)) {
            // if (!lsm6ds33.begin_SPI(LSM_CS, LSM_SCK, LSM_MISO, LSM_MOSI)) {
            Serial.println("Failed to find LSM6DS33 chip");
            /*    while (1)
                {
                  delay(10);
                }*/
            isGyro = false;
        }
        else
        {
#ifdef AF_LSM
            lsm6ds33.setAccelRange(LSM6DS_ACCEL_RANGE_16_G);
            lsm6ds33.setAccelDataRate(LSM6DS_RATE_1_66K_HZ);

            lsm6ds33.setGyroRange(LSM6DS_GYRO_RANGE_2000_DPS);
            lsm6ds33.setGyroDataRate(LSM6DS_RATE_3_33K_HZ);
#else
            lsm6ds33.settings.accelSampleRate = 13330;
#endif

            Serial.println("Successfully found LSM6DS33 chip");
            isGyro = true;
        }
        return isGyro;
    }
    static bool getReading() {
        float accelX, accelY, accelZ, gyroX, gyroY, gyroZ;

        //getRange();
    
    #ifdef AF_LSM
        lsm6ds33.readAcceleration(accelX, accelY, accelZ);
        lsm6ds33.readGyroscope(gyroX, gyroY, gyroZ);
    #else
        accelX = lsm6ds33.readFloatAccelX();
        accelY = lsm6ds33.readFloatAccelY();
        accelZ = lsm6ds33.readFloatAccelZ();
        gyroZ = lsm6ds33.readFloatGyroZ();
    #endif
        accel_y = accelY;
        accel_x = accelX;
        /*
        if (accel_y < average_y.average())
        {
          HalfFillStrips(Adafruit_NeoPixel::Color(0, 255, 0), Adafruit_NeoPixel::Color(255, 0, 0));
        }
        else
        {
          HalfFillStrips(Adafruit_NeoPixel::Color(255, 0, 0), Adafruit_NeoPixel::Color(0, 255, 0));
        }
       */
        return true;
    }
    static int segmentFromAccel(int segmentCount) {
        if (!isGyro) return -1; // No gyro - can't get segment number!
        getReading();
        float alpha = atan2(accel_y - last_y, accel_x - last_x); // Angle from -pi to +pi
        last_y = accel_y;
        last_x = accel_x;
       if (last_x > 15.9 || last_y > 15.9 || last_x < -15.9 || last_y < -15.9)
        {
            // TODO: Add code to capture recent flips from pegged to non-pegged and back, to get an idea of speed.
          return -1; // Not really an error
        } else {
          int nSegments = segmentCount; // clamp to the number of segments sent in the frame...
          int nSegment = floor(((float)nSegments * (alpha+PI) / (PI*2.0)));
          return nSegment;
        }
    }
};
float accel::last_x,accel::last_y,accel::accel_x,accel::accel_y;