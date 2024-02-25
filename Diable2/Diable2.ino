#define GYRO_CODE
//#define WAIT_FOR_SERIAL
//#define NO_NEO
// New pattern ideas:
// Chasing circles - a number of circles, going in and out - linearly, sinusoidally? - each a different colour light, different speeds
// Random squares - rectangles at random sizes and positions.
// Cubes - just like squares, but wireframe.
// Plaid / Argyle

#include <SPI.h>

#include <Adafruit_NeoPixel.h>

#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>
using namespace Adafruit_LittleFS_Namespace;

#include <bluefruit.h>

#ifdef GYRO_CODE
#include <Adafruit_LSM6DS33.h>

Adafruit_LSM6DS33 lsm6ds33;
#endif // GYRO_CODE

#include "dbleSettings.h"
PersistSetting persistentSettings; // Singleton

// We're coded up right now for the BlueFruit nRF52840 Feather Express
// Important pin values from https://learn.adafruit.com/introducing-the-adafruit-nrf52840-feather/pinouts:
// User switch (also for DFU if held down when booting)
#define USERSW_PIN 7 // D7
// Red LED (used by the bootloader, but can be used here)
// D3 P1.15 - RED_LED, by the battery plug, indicates bootloader mode.
// BLUE LED is used by BLE samples to indicate BLE connectivity.
// BLUE_LED #define ADALED_CONN P1.10 // P1.10
// PIN_NEOPIXEL P0.16 // P0.16 - Treat as a 1-pixel wide NeoPixel strip.

// So we could do Adafruit_NeoPixel pixels = Adafruit_NeoPixel(1, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800) - are those constants right?
// CHG is a yellow LED used when charging. Not software controllable

// I've soldered the two 8-LED RGB NeoPixel strips to pins 5 and 6 (so the solder doesn't impact the screw hole in the middle)
// These are only the default, you can set these using the "L" command.
#define LED_PIN 5
#define LED_PIN2 6

// Start with a hard-coded value of 8 - can be changed in INI settings or by command.
#define LED_COUNT 8

// Two strips of flashy lights
// Can we declare these dynamically at startup from INI? Yes - call updateLength(numPixels)
Adafruit_NeoPixel strip0(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel strip1(LED_COUNT, LED_PIN2, NEO_GRB + NEO_KHZ800);

// I don't really use this neo-pixel on the board.
#ifndef NO_NEO
Adafruit_NeoPixel neo(1, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);
#endif // NO_NEO

int buttonState = HIGH;
int lastButtonState = HIGH;
ulong lastDebounceTime = 0L;
ulong debounceDelay = 50L; // Milliseconds of 'settling' allowed before a button press is allowed.

uint8_t wing_offsetsN(int index, int numPixels)
{
  uint8_t retval;
  retval = (index / 2) + (numPixels * (index & 1));
  return retval;
};

uint8_t one_offsetsN(int index, int numPixels)
{
  // Start in the center - numpixels/2 (+1 because we start with pixel #8)
  uint8_t retval;
  int offset = numPixels / 2 - 1 - index / 2;
  if (index & 1)
    offset = numPixels - 1 - offset;
  retval = offset;
  //  Serial.printf("Inputs: %d, %d => one_offsetsN returns %d\n",index,numPixels,retval);
  return retval;
}

uint8_t fold_offsetsN(int index, int numPixels)
{
  // DEPRECATION WARNING: This worked two years ago, but I don't like the look of the fold.
  // This diagram shows why we aren't working properly with the way we send pictures in.
  // So I'm not too concerned about this being correct.
  // fold_offsets8[] = {3, 0xb, 0xc, 4, 2, 0xa, 0xd, 5, 1, 9, 0xe, 6, 0, 8, 0xf, 7};
  // x       x       6       2     | 1       5       x       x Strip 1 + => -
  //      x       x       4       0|      3       7       x       x Strip 0 - => +
  int strip = ((index + 1) % 4) / 2;   // 0 => 0, 1 => 1, 2 => 1, 3 => 0
  int direction = (index % 2) ^ strip; // 0 => 0; 1 => 0; 2 => 1; 3 => 1 - so 1 is positive, 0 is negative
  int count = index / 4;               // 0,1,2,3 => 0; 4,5,6,7 => 1

  int retval = strip * numPixels;
  switch (direction)
  {
  case 0:
    retval += 3;
    retval -= count;
    break;
  case 1:
    retval += 4;
    retval += count;
    break;
  }
  return retval;
}

enum commandStates
{
  NONE_COMMAND,
  FRAME_COMMAND
};
enum commandStates command_state_now = NONE_COMMAND; // Commands that might need extra bytes beyond what's read in a single message.

enum displayStates
{
  NONE_DISPLAY,
  FRAME_DISPLAY,
  CYCLE_DISPLAY,
  SPARKLE_DISPLAY,
  GYRO_DISPLAY,
};
enum displayStates display_state_now = NONE_DISPLAY;

enum foldedStates
{
  FOLDED_SHUT,
  FOLDED_OPEN,
  FOLDED_SINGLE
};
enum foldedStates folded_state_now = FOLDED_SHUT;

// When the two 8-LED strips are singly interleaved, with strip 0 offset by 1 width, and strip 1 by 1.5 width.
uint8_t current_offsets_fun(int i, int numPixels)
{
  switch (folded_state_now)
  {
  case FOLDED_SHUT:
    return fold_offsetsN(i, numPixels);
  case FOLDED_OPEN:
    return wing_offsetsN(i, numPixels);
  case FOLDED_SINGLE:
    return one_offsetsN(i, numPixels);
  }
  return 0;
}

float rots = 0.0;
float pi = 4.0 * atan(1.0);
float pi23 = 2.0 * pi / 3.0;

// TODO: This is not good to be statically sized.
// LED_COUNT issue!
struct frame
{
  unsigned long microdelay;
  uint32_t color[20];
};
uint32_t allred = 0xff0000;
uint32_t allblue = 0x0000ff;
uint32_t allgreen = 0x00ff00;
uint8_t brightness = 0xff;

unsigned int frameOffset = 0;

// Experiments show that we get within ~35-45 microseconds of hitting these timing numbers if they are above 450.
// LED_COUNT issue!
struct frame baseFrame1[] = {
    {500, {allgreen, 0, 0, 0, allred, 0, 0, 0, allblue, 0, 0, 0, allgreen, 0, 0, 0, allred, 0, 0, 0}},
    {500, {0, 0, 0, allred, 0, 0, 0, allblue, 0, 0, 0, allgreen, 0, 0, 0, allred, 0, 0, 0, allblue}},
    {500, {0, 0, allred, 0, 0, 0, allblue, 0, 0, 0, allgreen, 0, 0, 0, allred, 0, 0, 0, allblue, 0}},
    {500, {0, allred, 0, 0, 0, allblue, 0, 0, 0, allgreen, 0, 0, 0, allred, 0, 0, 0, allblue, 0, 0}},
    {500, {allred, 0, 0, 0, allblue, 0, 0, 0, allgreen, 0, 0, 0, allred, 0, 0, 0, allblue, 0, 0, 0}},
    {500, {0, 0, 0, allblue, 0, 0, 0, allgreen, 0, 0, 0, allred, 0, 0, 0, allblue, 0, 0, 0, allgreen}},
    {500, {0, 0, allblue, 0, 0, 0, allgreen, 0, 0, 0, allred, 0, 0, 0, allblue, 0, 0, 0, allgreen, 0}},
    {500, {0, allblue, 0, 0, 0, allgreen, 0, 0, 0, allred, 0, 0, 0, allblue, 0, 0, 0, allgreen, 0, 0}},
    {500, {allblue, 0, 0, 0, allgreen, 0, 0, 0, allred, 0, 0, 0, allblue, 0, 0, 0, allgreen, 0, 0, 0}},
    {500, {0, 0, 0, allgreen, 0, 0, 0, allred, 0, 0, 0, allblue, 0, 0, 0, allgreen, 0, 0, 0, allred}},
    {500, {0, 0, allgreen, 0, 0, 0, allred, 0, 0, 0, allblue, 0, 0, 0, allgreen, 0, 0, 0, allred, 0}},
    {500, {0, allgreen, 0, 0, 0, allred, 0, 0, 0, allblue, 0, 0, 0, allgreen, 0, 0, 0, allred, 0, 0}},
    {~0UL, {}}};             // a colourful spiral design. Each leg of the spiral is a different colour.
struct frame baseFrame[] = { // This isn't what I thought it was going to be. But it's pretty.
    {500, {allred, allred, allred, allred, allred, allred, allred, allred, allred, allblue, allblue, allblue, allblue, allblue, allblue, allblue, allblue, allblue}},
    {500, {}},
    {500, {allgreen, allgreen, allgreen, allgreen, allgreen, allgreen, allgreen, allgreen, allgreen, allred, allred, allred, allred, allred, allred, allred, allred, allred}},
    {500, {}},
    {500, {allblue, allblue, allblue, allblue, allblue, allblue, allblue, allblue, allblue, allgreen, allgreen, allgreen, allgreen, allgreen, allgreen, allgreen, allgreen, allgreen}},
    {500, {}},
    {~0UL, {}}};

// The frame set
#define DEFAULT_FRAME baseFrame1
struct frame *currentFrame = NULL;
struct frame *nextFrameSet = NULL;
unsigned int nextFrameCount = 0;
int myFrameCount = 0;
unsigned long nextFrame; // micros() at next frame
int nextFindex = 0;

long minFramesTime, maxFramesTime;

long totalFrameStartTime, expectedTotalFramesTime = 0;

// These things should go into a "strips" class.
int width = 8, height = 2, stride = 1, componentsValue = 0, is400Hz = 0;

void ClearStrips()
{
  strip0.clear();
  if (height > 1)
    strip1.clear();
}

void FillStrips(uint32_t nColor)
{
  strip0.fill(nColor, 0, strip0.numPixels());
  if (height > 1)
    strip1.fill(nColor, 0, strip1.numPixels());
}

void HalfFillStrips(uint32_t nColor1, uint32_t nColor2)
{
  strip0.fill(nColor1);
  if (height > 1)
    strip1.fill(nColor2);
  else
    strip0.fill(nColor2, width / 2, width / 2);
}

void SetStripsBrightness(uint8_t brightness)
{
  strip0.setBrightness(brightness);
  if (height > 1)
    strip1.setBrightness(brightness);
}

void ShowStrips()
{
  strip0.show();
  if (height > 1)
    strip1.show();
}

void SetStripsLength(int length)
{
  strip0.updateLength(length);
  if (height > 1)
    strip1.updateLength(length);
}

void SetStripsPin(int pin0, int pin1)
{
  strip0.setPin(pin0);
  strip0.begin();
  strip0.clear();
  strip0.show();
  if (height > 1)
  {
    strip1.setPin(pin1);
    strip1.begin();
    strip1.clear();
    strip1.show();
  }
}

void setOffsetPixel(int i, uint32_t colorToSet)
{
  uint8_t offset_led;
  offset_led = current_offsets_fun(i, width);
  uint16_t inset_led = offset_led % width;
  uint8_t strip_num = offset_led / width;
  switch (strip_num)
  {
  case 0:
    strip0.setPixelColor(inset_led, colorToSet);
    break;
  case 1:
    strip1.setPixelColor(inset_led, colorToSet);
    break;
  }
}

class cycle
{
public:
  static int hue1;  // The hue of the first pixel, from 0-65535.
  static int speed; // How fast we go, in 500 micros jumps - each jump being one step around the colour wheel
  static int step;  // How many steps around the colour wheel between each LED. 0-65535
  static unsigned long starttime;

public:
  static void start(int _speed, int _step)
  {
    speed = _speed;
    step = _step;
    hue1 = 0;
    starttime = micros();
    Serial.printf("Start time = %lx\n", starttime);
    display_state_now = CYCLE_DISPLAY;
    tick();
  }

  static void tick()
  {
    // Note: 35 minutes is 0x80 00 00 00 / 1,000,000 - so, clocking from signed to unsinged in micros()).
    unsigned long tnow = micros() - starttime;
    nextFrame = micros() + 500UL;      // Prevents TimeLights() from deciding we don't need to be run!
    unsigned long dura = tnow / 500UL; // 500 micros is 1 speed 'tick' for the cycle.
    int newhue1 = (int)((dura * (long)speed) & 0x0000ffffUL);

    if (Serial)
      brightness = 100; // If we're at the end of a cable, we need less brights.

    hue1 = newhue1;
    for (int i = 0; i < width * height; i++)
    {
      uint16_t hue = (uint16_t)(hue1 + i * step);
      uint32_t col = Adafruit_NeoPixel::ColorHSV(hue, 255, brightness);
      uint32_t gam = Adafruit_NeoPixel::gamma32(col);
      setOffsetPixel(i, gam);
    }
    ShowStrips();
  }
};
int cycle::hue1 = 0;
int cycle::speed = 0;
int cycle::step = 0;
unsigned long cycle::starttime = 0;

class sparkle
{
public:
  static int chance; // 0..255 out of 255. 255 = almost always a sparkle, 0 = never. Good values? 200? 100?
  static uint32_t foreground;
  static uint32_t background;
  static void start(int _chance = 20, uint32_t _foreground = 0xffffffff, uint32_t _background = 0)
  {
    chance = _chance;
    foreground = _foreground;
    background = _background;
    display_state_now = SPARKLE_DISPLAY;
    tick();
  }
  static void tick()
  {
    nextFrame = micros() + 500UL; // Prevents TimeLights() from deciding we don't need to be run!
    FillStrips(background);
    if (random(255) <= chance)
    {
      int pick = random(width * height);
      setOffsetPixel(pick, foreground);
    }
    ShowStrips();
  }
};
uint32_t sparkle::background;
uint32_t sparkle::foreground;
int sparkle::chance;

#ifdef GYRO_CODE
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

class gyroColor
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
public:
  // Static variables, because we only have a singleton.
  static float range_top, range_value, range_percent;
  static float accel_y;
  static int current_range_index;
  static rolling_average average_x, average_y;
  // Static functions
  static void start()
  {
    getRange();
    //    Serial.printf(String("Gyro Start time = %lx\n",micros());
    display_state_now = GYRO_DISPLAY;
    tick();
  }
  static void getRange()
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
  static void getReading()
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
  static void tick()
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
float gyroColor::range_top, gyroColor::range_value, gyroColor::range_percent, gyroColor::accel_y;
int gyroColor::current_range_index;
rolling_average gyroColor::average_x(1000);
rolling_average gyroColor::average_y(1000);
#endif // GYRO_CODE

void defaultFrames()
{
  Serial.println("Reverting to default frames!");
  if (currentFrame != NULL && currentFrame != DEFAULT_FRAME)
  {
    delete[] currentFrame;
  }
  SetCurrentFrame(DEFAULT_FRAME, sizeof(DEFAULT_FRAME) / sizeof(*DEFAULT_FRAME));
}

void SetCurrentFrame(struct frame *_nextFrameSet, int _nextFrameCount)
{
  myFrameCount = _nextFrameCount;
  nextFindex = 0;
  totalFrameStartTime = ~0L;
  nextFrame = micros();
  currentFrame = _nextFrameSet;
  minFramesTime = 0x7fffffffL;
  maxFramesTime = 0L;
  display_state_now = FRAME_DISPLAY;
}

//    #define FACTORYRESET_ENABLE         0
//    #define MINIMUM_FIRMWARE_VERSION    "0.6.6"
//    #define MODE_LED_BEHAVIOUR          "MODE"

// OTA DFU service
BLEDfu bledfu;

// Uart over BLE service
BLEUart bleuart;

void neoshow(uint32_t inColor)
{
  #ifndef NO_NEO
  static bool began_neo = false;
  if (!began_neo) { neo.begin(); began_neo = true; }
  neo.setPixelColor(0, inColor);
  neo.show();
  #endif // NO_NEO
}

void error(const char *ts)
{
  Serial.println(ts);
  while (1)
  {
    neoshow(Adafruit_NeoPixel::Color(255, 0, 0)); // red
    delay(500);
    neoshow(Adafruit_NeoPixel::Color(0, 0, 0)); // black
    delay(500);
  } // Eternal loop!
}

void setup()
{
  //  put your setup code here, to run once:
  InternalFS.begin();
  
  lastDebounceTime = millis();
  if (PIN_BUTTON1 < (PINS_COUNT)) {
    pinMode(PIN_BUTTON1, INPUT_PULLUP); // Prepare user switch for input.
  }
  neoshow(Adafruit_NeoPixel::Color(0,0,255)); // blue!

  Serial.begin(115200);
  #ifdef WAIT_FOR_SERIAL
  while (!Serial) {
    delay(10); // Wait for serial?
  }
  #endif // WAIT_FOR_SERIAL
  neoshow(Adafruit_NeoPixel::Color(0,0,0)); // black.

  // InternalFS.format(); // Uncomment to wipe the config.
  defaultFrames();
  Serial.println("Diable2");
  Serial.println("=======");

  Bluefruit.configPrphBandwidth(BANDWIDTH_MAX);

  Bluefruit.begin();
  Bluefruit.setTxPower(4);

  if (persistentSettings.LoadFile()) {
    width = persistentSettings.GetWidth();
    height = persistentSettings.GetHeight();
  }

  ClearStrips();
  ShowStrips();
  SetStripsLength(width);
  ClearStrips();
  ShowStrips();

  char const *ident = persistentSettings.GetName();
  switch (persistentSettings.GetFold())
  {
  case 'F':
    folded_state_now = FOLDED_SHUT;
    break;
  case 'W':
    folded_state_now = FOLDED_OPEN;
    break;
  case 'S':
    folded_state_now = FOLDED_SINGLE;
    break;
  }

  Bluefruit.setName(ident);

  // Little blue light off.
  Bluefruit.autoConnLed(false);
  //	pinIO::setState(LED_BLUE, false);

  Bluefruit.Periph.setConnectCallback(connectCallback);
  Bluefruit.Periph.setDisconnectCallback(disconnectCallback);

  // To be consistent OTA DFU should be added first if it exists
  bledfu.begin();

  // Configure and start the BLE Uart service
  bleuart.begin();
  bleuart.setRxCallback(rxCallback, false);
  bleuart.setRxOverflowCallback(rxOverflowCallback);

  // Set up and start advertising
  startAdv();

  SetStripsPin(persistentSettings.GetPin0(), persistentSettings.GetPin1());

  Serial.println("Time to connect and send!");
#ifdef GYRO_CODE
  if (!lsm6ds33.begin_I2C())
  {
    // if (!lsm6ds33.begin_SPI(LSM_CS)) {
    // if (!lsm6ds33.begin_SPI(LSM_CS, LSM_SCK, LSM_MISO, LSM_MOSI)) {
    Serial.println("Failed to find LSM6DS33 chip");
    /*    while (1)
        {
          delay(10);
        }*/
  }
  else
  {
    lsm6ds33.setAccelRange(LSM6DS_ACCEL_RANGE_16_G);
    lsm6ds33.setAccelDataRate(LSM6DS_RATE_6_66K_HZ);

    lsm6ds33.setGyroRange(LSM6DS_GYRO_RANGE_2000_DPS);
    lsm6ds33.setGyroDataRate(LSM6DS_RATE_3_33K_HZ);
  }
#endif // GYRO_CODE

  neoshow(Adafruit_NeoPixel::Color(0, 255, 0));

  // The next animation frame happens "right now" - micros()
  nextFrame = micros();
  nextFindex = 0;
  displayStartup();
  neoshow(Adafruit_NeoPixel::Color(0, 0, 0)); // - just black for the little pixel.
  // If you want something else, uncomment one of these:
  // sparkle::start(12, Adafruit_NeoPixel::Color(255, 255, 255), Adafruit_NeoPixel::Color(10, 0, 10));
  // cycle::start(20, 0xff);
#ifdef GYRO_CODE
  // gyroColor::start();
#endif // GYRO_CODE
  // TODO: Allow the user to choose a pattern to display at startup?
}

void rxCallback(uint16_t conn_hdl)
{
  int readable = bleuart.available();
  Serial.printf("Bytes read in from BLE: %d\n", readable);
  // We could potentially read more from the FIFO, with bool peek(void *buffer)
  asyncReadAndProcess(readable);
}

void rxOverflowCallback(uint16_t conn_hdl, uint16_t leftover)
{
  // Serial.println("****");
  // Serial.println("Overflow callback hit!");
  // Serial.println("****");
}

void startAdv(void)
{
  // Advertising packet
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();

  // Include the BLE UART (AKA 'NUS') 128-bit UUID
  Bluefruit.Advertising.addService(bleuart);

  // Secondary Scan Response packet (optional)
  // Since there is no room for 'Name' in Advertising packet
  Bluefruit.ScanResponse.addName();

  /* Start Advertising
   * - Enable auto advertising if disconnected
   * - Interval:  fast mode = 20 ms, slow mode = 152.5 ms
   * - Timeout for fast mode is 30 seconds
   * - Start(timeout) with timeout = 0 will advertise forever (until connected)
   *
   * For recommended advertising interval
   * https://developer.apple.com/library/content/qa/qa1931/_index.html
   */
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244); // in unit of 0.625 ms
  Bluefruit.Advertising.setFastTimeout(30);   // number of seconds in fast mode
  Bluefruit.Advertising.start(0);             // 0 = Don't stop advertising after n seconds
}

void connectCallback(uint16_t conn_handle)
{
  Serial.printf("New Bluetooth connection - handle is %d\n", (int)conn_handle);

  neoshow(0);
  BLEConnection *connection = Bluefruit.Connection(conn_handle);
  char central_name[32] = {0};
  connection->getPeerName(central_name, sizeof(central_name)-1);
  Serial.printf("Connected to %s\n", central_name);

  // request PHY changed to 2MB - default is BLE_GAP_PHY_AUTO
  Serial.println("Request to change PHY");
  connection->requestPHY();

  // request to update data length
  Serial.println("Request to change Data Length");
  connection->requestDataLengthUpdate();

  // request mtu exchange
  Serial.println("Request to change MTU");
  connection->requestMtuExchange(247);

  // request connection interval of 7.5 ms
  // connection->requestConnectionParameter(6); // in unit of 1.25

  displayStartup();
}

void disconnectCallback(uint16_t conn_handle, uint8_t reason)
{
  // Don't be tempted to reset the current_offset, because it should be a hardware setting.
  // Serial.println();
  Serial.printf("Disconnected, reason = 0x%x\n", (int)reason);
  Serial.printf("Closing Bluetooth connection - handle is %d\n", (int)conn_handle);
  displayShutup();
  bleuart.flush();
}

float aveTime = 0;

int lastloop = 0, looptime = 0;
float aveLoop = 0.0;

void TimeLights()
{
  unsigned long themicros = micros();
  // Collect timing statistics that we might use
  if (lastloop != 0)
    looptime = themicros - lastloop;
  lastloop = themicros;
  aveLoop = (aveLoop * 9.0 + (float(looptime))) / 10.0;

  // If we aren't ready for the next frame, quit.
  // If you don't set "nextFrame", you will cycle past it and block until coming back around.
  if ((long)(nextFrame - themicros) > 0L)
    return;

  // More timing statistics - how much average jitter (actual micros - planned micros) is there
  aveTime = (aveTime * 9.0 + (float)(themicros - nextFrame)) / 10.0;

  // Output timing statistics to serial port for debugging.
  if (0) // Choice to output jitter time to debug
  {
    Serial.printf("Average jitter is %f\n", aveTime);
  }
  if (0) // Choice to output loop avg time to debug
  {
    Serial.printf("Average loop time is %f\n", aveLoop);
  }

  // Let's see what mode we're in...
  switch (display_state_now)
  {
  case CYCLE_DISPLAY:
    cycle::tick();
    break;
  case SPARKLE_DISPLAY:
    sparkle::tick();
    break;
#ifdef GYRO_CODE
  case GYRO_DISPLAY:
    gyroColor::tick();
    break;
#endif // GYRO_CODE
  case FRAME_DISPLAY:

    if (currentFrame == NULL) // No frame to display - black out.
    {
      display_state_now = NONE_DISPLAY; // no change required.
      ClearStrips();
      ShowStrips();
      return;
    }
    long microdelay = (long)currentFrame[nextFindex].microdelay;
    expectedTotalFramesTime += microdelay;
    if (nextFindex == 0) // Collect animation statistics
    {
      // TODO: Send statistics over bluetooth?
      long totalFramesTime = themicros - totalFrameStartTime;
      if (0 && totalFrameStartTime != ~0L) // Choice to debug statistics
      {
        Serial.printf("Back to frame 0 - total animation duration: %ld\n", totalFramesTime);
        if (totalFramesTime < minFramesTime)
          minFramesTime = totalFramesTime;
        if (totalFramesTime > maxFramesTime)
          maxFramesTime = totalFramesTime;
        Serial.printf("min frame time: %ld max frame time %ld\n", minFramesTime, maxFramesTime);
        Serial.printf("Expected total frame time: %ld\n", expectedTotalFramesTime);
      }
      totalFramesTime = 0L;
      expectedTotalFramesTime = 0L;
      totalFrameStartTime = themicros;
    }
    if (microdelay & 0x80000000L) // 'negative' (but it's unsigned)
    {
      // Special flags.
      switch (-microdelay)
      {
      case 2L: // Return to default frame.
        Serial.println("Return to default frame");
        defaultFrames();
        return;
        break;
      case 3L: // Drop to black.
        Serial.println("Return to black frame");
        SetCurrentFrame(NULL, 0);
        return;
        break;
      }
    }
    nextFrame = microdelay + themicros;
    //  Serial.printf("TimeLights says set frame %d\n", nextFindex);
    // ASSUME: strip1 & strip0 have the same number of pixels.
    for (int i = 0; i < width * height; i++)
    {
      setOffsetPixel(i, currentFrame[nextFindex].color[i]);
    }
    ShowStrips();
    nextFindex++;
    if (nextFindex > myFrameCount || currentFrame[nextFindex].microdelay == -1L)
    {
      nextFindex = 0;
    }
  }
}

void displayStartup()
{
  // This is called on startup, on connect, and when the pins or orientation are changed.
  // Empty the frames...
  defaultFrames();
  // Allocate new frames
  int pixelCount = width * height;
  int frameCount = pixelCount;
  // Serial.printf("New frame count: %d\n", frameCount);
  nextFrameSet = new frame[frameCount + 1];
  if (nextFrameSet == NULL)
  {
    // Serial.println("Badness on the frame allocation!");
  }
  for (int i = 0; i < frameCount; i++)
  {
    // Green lights on strip 0, starting at middle
    // Red lights on strip 1, starting at middle
    nextFrameSet[i].microdelay = 10000;
    for (int j = 0; j <= i; j++)
    {
      nextFrameSet[i].color[j] = (current_offsets_fun(j, width) / (width / 2)) ? Adafruit_NeoPixel::Color(0, 0x3f, 0) : Adafruit_NeoPixel::Color(0x3f, 0, 0);
    }
    for (int j = i + 1; j < pixelCount; j++)
    {
      nextFrameSet[i].color[j] = 0;
    }
  }
  nextFrameSet[frameCount].microdelay = -2L; // Special flag, "return to default"
  SetCurrentFrame(nextFrameSet, frameCount);
}

void displayShutup()
{
  // This is called on disconnect.
  // Empty the frames...
  defaultFrames();
  // Allocate new frames
  int pixelCount = width * height;
  int frameCount = pixelCount;
  //Serial.printf("New frame count: %d\n", frameCount);
  nextFrameSet = new frame[frameCount + 1];
  if (nextFrameSet == NULL)
  {
    //Serial.println("Badness on the frame allocation!");
  }
  for (int i = 0; i < frameCount; i++)
  {
    //Serial.printf("Frame # %d\n",i);
    int blue = 0x0ff;// * (frameCount - i) / frameCount;
    // Blue lights, fade to black.
    nextFrameSet[i].microdelay = 100000;
    for (int j = 0; j < frameCount - i; j++)
    {
      if (j < pixelCount) {
        nextFrameSet[i].color[j] = Adafruit_NeoPixel::Color(0, 0, (uint8_t)blue);
        //Serial.printf("LED %d set to blue\n",j);
      }
      else {
        //Serial.printf("LED %d not set - out of range!\n",j);
      }
    }
    for (int j = frameCount-i; j < pixelCount; j++)
    {
      //Serial.printf("Led %d set black\n",j);
      nextFrameSet[i].color[j] = (uint32_t)0;
    }
  }
  nextFrameSet[frameCount].microdelay = -2L; // Special flag, "return to default"
  SetCurrentFrame(nextFrameSet, frameCount);
}

void onUserButtonClick()
{
  // Represents a full cycle of button-down, button-up.
  // Simple thing to demonstrate it works.
  const uint32_t Colours[] = {
      Adafruit_NeoPixel::Color(255, 0, 0),
      Adafruit_NeoPixel::Color(255, 255, 0),
      Adafruit_NeoPixel::Color(0, 255, 0),
      Adafruit_NeoPixel::Color(0, 255, 255),
      Adafruit_NeoPixel::Color(0, 0, 255),
      Adafruit_NeoPixel::Color(255, 0, 255),
  };
  const int colourCount = sizeof Colours / sizeof *Colours;
  static int thisColour = 0;
  SetCurrentFrame(NULL, 0);
  neoshow(Colours[thisColour]);
  thisColour = (thisColour + 1) % colourCount;
}

void loop()
{
  // put your main code here, to run repeatedly:
  // User Switch?
  int reading = lastButtonState;
  if (PIN_BUTTON1 < (PINS_COUNT)) {
    reading = digitalRead(PIN_BUTTON1);
  }
  ulong nowms = millis();
  if (reading != lastButtonState)
  {
    lastDebounceTime = nowms;
  }
  if (nowms - lastDebounceTime > debounceDelay)
  {
    if (reading != buttonState)
    {
      buttonState = reading;
      if (buttonState == HIGH)
      {
        onUserButtonClick();
      }
    }
    lastDebounceTime = nowms - debounceDelay - debounceDelay; // avoid clocking!
  }
  lastButtonState = reading;

  TimeLights();
  // AMJ - If we have poor communication of commands, remove the following line again:
  if (0 && nextFrame - micros() > 500L)
    delayMicroseconds(100); // Reduce power usage, maybe? TimeLights() is kind of a busy loop.
}

void asyncReadAndProcess(int readable)
{
  /*
   * V - return version - "DiaBLE v2.0:L01F", where '0' is the pin for the 0 row of LEDs and '1' is the pin for the 1 row. 'F' or 'W' or 'S' indicates fold state of wings. Folded or Wings. I prefer Wings, because bigger clearer image.
   * S - setup dimensions - byte width, byte height, byte stride, byte Components, byte is400Hz - basically ignored!
   * C - clear to colour - 3 bytes red, green, blue
   * B - set overall brightness - one byte, 0-255.
   * P - set one pixel - byte x, byte y, 3 bytes red, green, blue
   * I - receive (fixed) image (not implemented)
   * -1 - always sends you back to NONE_COMMAND - useful if we overrun/underrun.
   * N - New - clear animation frames. No parameters
   * F - Frames - 2 byte count of frames. Each frame: 4 bytes microseconds time, then 2 x numPixel() sets of 3 bytes rgb.
   * L - LED positions - two bytes for the pins for each stick, one byte "W" for "wings" or "F" for "folded".
   * T - Title the DiaBLE, so you can have more than one board.
   */
  // Check for incoming characters from Bluefruit
  char foldState;
  uint8_t color[3], x, y;
  int i, j, redled, grnled, bluled;
  uint8_t offset_led;
  uint32_t color_set;
  int blecmd = 0;
  int p0, p1;
  if (readable == 0)
  {
    Serial.println("We got told of a read, but zero bytes available!");
  }
command_loop:
  switch (command_state_now)
  {
  case NONE_COMMAND:
    blecmd = bleuart.read();
    readable--; // reduce by cmd character
    if (blecmd == 0)
      return;
    // Commands from the Adafruit demo:
    // V - return version
    // S - setup dimensions, components, stride
    // C - clear with color
    // B - set brightness
    // P - set pixel
    // I - receive image (we don't implement this)
    // N - clear all frames, go to black.
    // F - Frames
    // Purely ours:
    // L - set LED wing/fold & pin numbers
    // O - OK
    // T - Title this unit. Sets its name.
    // Y - Cycle colours.
    // X - Sparkle
    Serial.printf("command_state_now is NONE_COMMAND, command is %d\n", blecmd);
    switch (blecmd)
    {
    case -1:
      Serial.println("We hit the minus one break");
      return;
      break;
    case 'V':
    {
      char response[80]; // overkill, its really only 30 characters including the null. Today.

      sprintf(response, "VDiaBLE v%1.2f:L%02d%02d%c:S%d,%d,%d,%d,%d\r\n", 2.10, persistentSettings.GetPin0(), persistentSettings.GetPin1(),
              persistentSettings.GetFold(), persistentSettings.GetWidth(), persistentSettings.GetHeight(), stride, componentsValue, is400Hz);
      Serial.print(response);
      sendbleu(response);

    }
      if (readable > 0)
      {
        Serial.println("Single character command V - still have readable bytes!");
      }
      else
      {
        Serial.println("Single character command V");
      }
      break;
    case 'S':
      width = waitread();
      height = waitread();
      // If going down a size, we want to leave unused pixels black.
      ClearStrips();
      ShowStrips();
      SetStripsLength(width);
      // If going up a size, we want new pixels not to be random, so they get set black.
      ClearStrips();
      ShowStrips();
      stride = waitread();
      componentsValue = waitread();
      is400Hz = waitread();

      Serial.printf("\tsize: %dx%d\n\tstride: %d\n\tcomponents: %d\n\tis400Hz: %d\n\n\n", width, height, stride, componentsValue, is400Hz);
      persistentSettings.SetSize((byte)width, (byte)height, (byte)stride, (byte)componentsValue, (byte)is400Hz);
      persistentSettings.WriteSettings();

      readable -= 5;
      sendbleuok();
      break;
    case 'C':
      if (readable < 3)
      {
        Serial.println("We don't have enough characters for C command - seems unlikely!");
      }
      else
      {
        Serial.println("Command C - Setting single colour on all pixels");
      }
      for (int i = 0; i < 3; i++)
      {
        color[i] = waitread();
      }
      readable -= 3;
      // Cheat - don't need to do it with the offsets!
      FillStrips(Adafruit_NeoPixel::Color(color[0], color[1], color[2]));
      ShowStrips();
      sendbleuok();
      break;
    case 'B':
      if (readable != 1)
      {
        Serial.println("Brightness command with weird number of params!");
      }
      brightness = waitread();
      readable--;
      SetStripsBrightness(brightness);
      ShowStrips();
      sendbleuok();
      break;
    case 'P':
      x = waitread();
      y = waitread();
      readable -= 2;
      // Serial.printf("\tPixel: %d x %d\n", x, y);
      for (int i = 0; i < 3; i++)
      {
        color[i] = waitread();
      }
      readable -= 3;
      // Serial.printf("\tColor: %d %d %d\n", (int)color[0], (int)color[1], (int)color[2]);
      offset_led = y * width + x;
      color_set = Adafruit_NeoPixel::Color(color[0], color[1], color[2]);
      setOffsetPixel(offset_led, color_set);
      ShowStrips();
      sendbleuok();
      break;
      // ADDED commands - not in the bluefruit sketch from Adafruit.
      // N - clear all the frames out, set to blank.
      // F - Load a set of frames.
      // L - Set LED Pin & configuration
    case 'N':
      // clear frames.
      Serial.println("Command N\tClear all frames");
      defaultFrames();
      SetCurrentFrame(NULL, 0);

      ClearStrips();
      ShowStrips();
      //
      sendbleuok();
      break;
    case 'F':
      Serial.print("Command F\tFrame: ");
      defaultFrames();
      command_state_now = FRAME_COMMAND;
      frameOffset = 0;
      Serial.println("Deleted old frames");
      // Allocate currentFrame
      nextFrameCount = (((unsigned int)(byte)waitread()) << 8) | (unsigned int)(byte)waitread();
      Serial.printf("New frame count: %d\n", nextFrameCount);
      nextFrameSet = new frame[nextFrameCount + 1];
      if (nextFrameSet == NULL)
      {
        // Serial.println("Badness on the frame allocation!");
      }
      // Fill with data - by looping around again!
      readable -= 2;
      goto command_loop;
      break;
    case 'L':
      // Two bytes - pin for LED strip 0, pin for LED strip 1
      // One byte - 'W' for unfolded LED wings, 'F' for folded on the lid.
      if (readable < 3)
      {
        Serial.println("Too few characters in L command");
      }
      else if (readable > 3)
      {
        Serial.println("Too many characters in L command");
      }
      else
      {
        Serial.println("L command received - pins, fold");
      }
      p0 = waitread();
      p1 = waitread();
      readable -= 2;
      ClearStrips();
      ShowStrips();
      SetStripsPin(p0, p1);
      ClearStrips();
      ShowStrips();
      switch (foldState = waitread())
      {
      case 'W': // unfolded - wings
        folded_state_now = FOLDED_OPEN;
        break;
      case 'F': // folded - lid
        folded_state_now = FOLDED_SHUT;
        break;
      case 'S': // Single - like wings, but both are soldered together.
        folded_state_now = FOLDED_SINGLE;
        break;
      }
      readable--;

      persistentSettings.SetPin0(p0);
      persistentSettings.SetPin1(p1);
      persistentSettings.SetFold(foldState);
      persistentSettings.WriteSettings();

      // Give visual feedback that we're good.
      // Idea: circles out from the middle, green on strip 0, red on strip 1, then black.
      // We can do this also on startup / connect.
      displayStartup();
      break;
    case 'O':
      // OK, generally.
      Serial.println("Received OK?");
      if (waitread() == 'K' && waitread() == '\n' || waitread() == '\n')
      {
        readable = 0;
        return;
      }
      Serial.println("Not OK?");
      readable = 0; // Discard anyway.
      break;
    case 'T':
      // Read in the unit name. Everything up to end of line or end of transmission, whichever comes first!
      char buffer[21];
      j = 0;
      for (int i = 0; i < readable; i++)
      {
        char c = waitread();
        if (c > ' ' && j < 20)
        {
          buffer[j++] = c;
        }
      }
      readable -= i;
      buffer[j] = 0;

      persistentSettings.SetName(buffer);
      persistentSettings.WriteSettings();

      break;
    case 'Y':
      // Colour Cycle - needs parameters to drive how fast we cycle, and how far ahead / behind the edges are from each other.
      // Parameters are binary, signed. First parameter is one byte - how fast we cycle, second parameter is how far one
      // edge is from the other, and is TWO bytes, because 65535 is a loop around the wheel. (so each LED is M/2.numPixels() from its neighbour.)
      {
        int8_t speed = waitread();
        uint16_t step = ((waitread() << 8) | (waitread()));
        cycle::start(speed, step);
      }
      break;
    case 'X':
      // Sparkle - needs parameters for how often, and what colours.
      {
        uint8_t chance = waitread();
        uint32_t foreground = Adafruit_NeoPixel::Color(waitread(), waitread(), waitread());
        uint32_t background = Adafruit_NeoPixel::Color(waitread(), waitread(), waitread());
        sparkle::start(chance, foreground, background);
      }
      break;

#ifdef GYRO_CODE
    case 'G':
      // Gyro - currently, no parameters. Might feel like adding more later.
      gyroColor::start();
      break;
#endif // GYRO_CODE

    default:
      // TODO: Because "NOK", maybe discard all future characters until quiet again?
      // Maybe also send back the command, so the caller can tell what it tried to do that was wrong?
      readable = 0; // This at least discards the current data packet.
      sendbleu("NOK\r\n");
      // sendbleu("NOK - %c",blecmd);
      break;
    }
    break;
  case FRAME_COMMAND:
    const int frameSize = 3 * (width * height) + 4;
    bool bSerDbg = true;
    while (frameOffset < nextFrameCount * frameSize && readable > 0)
    {
      int frameIndex = frameOffset / frameSize;
      int innerFrameOffset = frameOffset % frameSize;
      uint8_t readbyte;
      frameOffset++;
      readbyte = waitread();
      readable--;
      if (innerFrameOffset == 0)
      {
        if (bSerDbg)
        {
          Serial.printf("Frame #%d\n", frameIndex);
        }
        nextFrameSet[frameIndex].microdelay = 0;
      }
      // Read four bytes of microseconds for this frame to show.
      if (innerFrameOffset < 4)
      {
        unsigned long lms = (unsigned long)readbyte;
        nextFrameSet[frameIndex].microdelay |= lms << (8 * (3 - innerFrameOffset));
        if (bSerDbg && innerFrameOffset == 3)
        {
          Serial.printf("Micros: %ld\n", nextFrameSet[frameIndex].microdelay);
        }
      }
      else
      {
        int ledOffset = (innerFrameOffset - 4) / 3;
        // Read three x 2 x numPoxel() bytes of colour.
        uint8_t ledval = readbyte;
        int colorStep = (innerFrameOffset - 1) % 3;
        switch (colorStep)
        {
        case 0: // red
          if (bSerDbg)
          {
            Serial.printf("LED # %d Red: ", ledOffset);
          }
          nextFrameSet[frameIndex].color[ledOffset] = ((uint32_t)ledval) << 16;
          break;
        case 1: // green
          if (bSerDbg)
          {
            Serial.print(" Grn: ");
          }
          nextFrameSet[frameIndex].color[ledOffset] |= ((uint32_t)ledval) << 8;
          break;
        case 2: // blue
          if (bSerDbg)
          {
            Serial.print(" Blu: ");
          }
          nextFrameSet[frameIndex].color[ledOffset] |= ((uint32_t)ledval);
          break;
        }
        if (bSerDbg)
        {
          Serial.print(ledval);
          if (colorStep == 2)
          {
            Serial.println();
          }
        }

        if (ledval == -1)
        {
          Serial.println("Error");
        }
      }
    }
    if (bSerDbg)
    {
      Serial.println("buffer empty or frame finished");
    }
    if (frameOffset < nextFrameCount * frameSize)
      return;
    // We reached the end of the frame count!
    // Terminate the frame list.
    if (bSerDbg)
    {
      Serial.println("Terminating");
      Serial.printf("Final frameCount is %d\n", nextFrameCount);
    }
    if (nextFrameSet != NULL)
    {
      nextFrameSet[nextFrameCount].microdelay = -1;
      SetCurrentFrame(nextFrameSet, nextFrameCount);
    }
    command_state_now = NONE_COMMAND;
    sendbleuok();
    break;
  }
  // Serial.printf("Processed command character: %d %c\nWaiting for OK\n", (int)blecmd, (char)blecmd);
  //  ble.waitForOK();
}

int waitread()
{
  if (bleuart.available() == 0)
  {
    Serial.println("Entering spin on available()");
    while (bleuart.available() == 0)
      ;
  }
  //  Serial.println("Reading a byte");
  return bleuart.read();
}

void sendbleu(const char *sendit)
{
  bleuart.write(sendit, strlen(sendit) * sizeof(char));
}

void sendbleuok()
{
  sendbleu("OK\r\n");
}
