#include <Adafruit_NeoPixel.h>
#include <Adafruit_LSM6DS33.h>
class rolling_average
{
  // Implemented using a ring of floats
  float *avestore = nullptr;
  int size = 0, offset = 0, count = 0;
  float total = 0.0;
  float local_min = 1000000.0, local_max = -1000000.0;

public:
  rolling_average(int _size = 100)
  {
    if (avestore)
    {
      delete[] avestore;
    }
    size = _size;
    avestore = new float[size];
    offset = 0;
    count = 0;
    total = 0.0;
  }
  void add(float _value)
  {
    if (count != size)
    {
      avestore[offset++] = _value;
      count++;
    }
    else
    {
      total -= avestore[offset];
      avestore[offset++] = _value;
    }
    if (offset >= size)
    {
      offset = 0;
    }
    total += _value;
  }
  float average()
  {
    if (count == 0)
    {
      return 0.0; // Let's not be dividing by zero any time soon.
    }
    return total / (float)count;
  }
  ~rolling_average()
  {
    if (avestore)
    {
      delete[] avestore;
    }
  }
};

struct my_gyro_range
{
  gyro_range range;
  uint32_t color;
  float top;
} const ranges[] = {
    // Top values are more / less in rad/s, but I really don't know how to trust those numbers.
    {LSM6DS_GYRO_RANGE_125_DPS, Adafruit_NeoPixel::Color(255, 0, 0), 2.50},    // red
    {LSM6DS_GYRO_RANGE_250_DPS, Adafruit_NeoPixel::Color(255, 255, 0), 5.0},   // yellow
    {LSM6DS_GYRO_RANGE_500_DPS, Adafruit_NeoPixel::Color(0, 255, 0), 10.0},    // green
    {LSM6DS_GYRO_RANGE_1000_DPS, Adafruit_NeoPixel::Color(0, 255, 255), 20.0}, // cyan
    {LSM6DS_GYRO_RANGE_2000_DPS, Adafruit_NeoPixel::Color(0, 0, 255), 40.0},   // blue
    //    {ISM330DHCX_GYRO_RANGE_4000_DPS, Adafruit_NeoPixel::Color(255, 0, 255), 80.0}, // magenta
};
const int gyro_range_count = sizeof ranges / sizeof *ranges;

class pgyroColor : public pattern
{
  // First display mode to use the gyroscope readings.
  // Intent: Pick a colour based on the range we are using:
  // 125 - red
  // 250 - yellow
  // 500 - green
  // 1000 - cyan
  // 2000 - blue
  // 4000 - magenta
  //
  // Then fill the LEDs, one to nPixels, with the speed within that range.
  // Range percent = (rangeValue - rangeBottom) * 100.0 / (rangeTop-rangeBottom)
  //
  // Remember to change range - thinking that if we change range up if range percent is > 75
  // Change range down if range percent is < 25
  //
  // Then lights filled can be (range percent-25) * nPixels/50
  //
  // A little special casing when range is 125 or 4000:
  // 125: lights filled should be (range percent) * nPixels / 75
  // 4000: lights filled should be (range percent-25) * nPixels / 75
  // 4000: if range percent >= 100, set all pixels white to indicate overflow.
private:
  float range_top, range_value, range_percent;
  float accel_y;
  int current_range_index;
  rolling_average average_x, average_y;

  pgyroColor() : average_x(1000), average_y(1000) {
    getRange();
  }
  public:
  static pattern *Create() {
    pattern *pat = new pgyroColor();
    display_state_now = GYRO_DISPLAY;
    return pat;
  }
  void getRange()
  {
    gyro_range current_range = lsm6ds33.getGyroRange();
    current_range_index = 0;
    int i = 0;
    for (i = 0; i < gyro_range_count; i++)
    {
      if (ranges[i].range == current_range)
      {
        current_range_index = i;
      }
    }
    Serial.printf("range = %d maps to index %d of %d\n", (int)current_range, current_range_index, i);

    range_top = ranges[current_range_index].top;
  }
  void getReading()
  {
    sensors_event_t accel;
    sensors_event_t gyro;
    sensors_event_t temp;
    getRange();

    lsm6ds33.getEvent(&accel, &gyro, &temp);

    // Z rotation is in gyro.gyro.z as a float in rads / s
    // I don't care, positive or negative, for this. Maybe if you can reverse a diabolo?

    range_value = gyro.gyro.z > 0.0 ? gyro.gyro.z : -gyro.gyro.z;

    range_percent = (range_value)*100.0 / (range_top);

    /*if (range_percent < 25.0 && current_range_index != 0)
    {
      // range_down
      lsm6ds33.setGyroRange(ranges[current_range_index - 1].range);
      Serial.printf("Range down! -> %d\n", current_range_index - 1);
    }
    else if (range_percent > 75.0 && current_range_index != gyro_range_count - 1)
    {
      // range_up
      lsm6ds33.setGyroRange(ranges[current_range_index + 1].range);
      Serial.printf("Range up! -> %d\n", current_range_index + 1);
    }
    */
    // TODO: Instead of using average y over the last 100 samples, why not count local minimum and maximum of x and y?
    Serial.printf("Gyro reading received: z=%f - %%=%f\n", range_value, range_percent);
    accel_y = accel.acceleration.x;
    average_y.add(accel.acceleration.y);
    average_x.add(accel.acceleration.x);
    Serial.printf("Accel reading received: y=%f; x=%f\n", accel.acceleration.y, accel.acceleration.x);
  }
  void tick()
  {
    nextFrame = micros() + 20000UL;
    getReading();
    if (accel_y > average_x.average())
    {
      HalfFillStrips(Adafruit_NeoPixel::Color(0, 255, 0), Adafruit_NeoPixel::Color(255, 0, 0));
    }
    else
    {
      HalfFillStrips(Adafruit_NeoPixel::Color(255, 0, 0), Adafruit_NeoPixel::Color(0, 255, 0));
    }
    if (0)
    {
      float pixelCount = 0;
      float pixelMax = width * height;
      uint32_t pixelColor = ranges[current_range_index].color;
      if (current_range_index == 0)
      {
        pixelCount = pixelMax * (range_percent) / 75.0;
      }
      else if (current_range_index == gyro_range_count - 1)
      {
        pixelCount = pixelMax * (range_percent - 25.0) / 75.0;
        if (range_percent > 98.0)
        {
          pixelCount = pixelMax;
          pixelColor = Adafruit_NeoPixel::Color(0xff, 0xff, 0xff); // White for 'pegged over full'.
        }
      }
      else
      {
        pixelCount = pixelMax * (range_percent - 25.0) / 50.0;
      }
      int ipixelCount = (int)pixelCount;
      int ipixelMax = (int)pixelMax;
      //    Serial.printf("Lighting up pixels: %d\n", ipixelCount);
      //    Serial.printf("Pixel color is %d\n", (int)pixelColor);
      // light up pixelCount pixels at range_color
      for (int i = 0; i < ipixelMax; i++)
      {
        if (i < ipixelCount)
        {
          setOffsetPixel(i, pixelColor);
        }
        else
        {
          setOffsetPixel(i, 0);
        }
      }
    }
    ShowStrips();
    sendbleu((String(">") + String(accel_y) + String(",") + String(range_value) + String(",") + String(average_y.average())).c_str());
  }
};

