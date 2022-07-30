#define GYRO_CODE
#include <bluefruit.h>
//#include <SPI.h>
#if not defined(_VARIANT_ARDUINO_DUE_X_) && not defined(_VARIANT_ARDUINO_ZERO_)
//  #include <SoftwareSerial.h>
#endif

#include <Adafruit_NeoPixel.h>

#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>
using namespace Adafruit_LittleFS_Namespace;
#ifdef GYRO_CODE
#include <Adafruit_LSM6DS33.h>

Adafruit_LSM6DS33 lsm6ds33;
#endif

class PersistSetting
{
  File settingsFile;
  char const *_fname = "diableset.txt";

  char _unitName[16];
  char _fold;
  int _pin0;
  int _pin1;
  bool _dirty;
  byte _width;
  byte _height;
  byte _componentsValue;
  byte _stride;
  byte _is400Hz;

  void defaults()
  {
    strcpy(_unitName, "DiaBLEINI");
    _fold = 'F'; // Folded
    _pin0 = 5;
    _pin1 = 6;
    _width = 8;  // LED_COUNT hard-set!
    _height = 2; // Don't think we'll have other than 2 lights.
    _componentsValue = 0;
    _stride = 1;
    _is400Hz = 0;
  }

  int readInt(const char *buffer, int &jmdex, int readlen, bool skippost = true)
  {
    int rv = atoi(&buffer[jmdex]);
    // Skip numeric characters
    for (; jmdex < readlen && isDigit(buffer[jmdex]); jmdex++)
      ;
    // Skip any non-numeric value - don't care!
    if (skippost)
    {
      for (; jmdex < readlen && !isDigit(buffer[jmdex]); jmdex++)
        ;
    }
    return rv;
  }

public:
  PersistSetting() : settingsFile(InternalFS) { defaults(); }
  bool LoadFile()
  {
    int p0, p1;
    byte lheight, lwidth, lstride, lcomponentsValue, lis400Hz;
    char foldState;
    defaults();
    _dirty = false;
    Serial.println("Opening settings file");

    if (settingsFile.open(_fname, FILE_O_READ))
    {
      char buffer[128] = {0};
      uint32_t readlen;
      readlen = settingsFile.read(buffer, sizeof(buffer) - 1);
      buffer[readlen] = 0;
      settingsFile.close();
      Serial.println("Contents:");
      Serial.println(readlen, 10);
      Serial.println(buffer);
      // Read in the settings from the config file.
      // Read by lines, maybe, parse the following:
      // L0,1W - LED pins 0 & 1, and W=Wings, F=Folded
      // TName - The name of the unit.
      // Other options will be added over time.
      int index = 0, jmdex = 0;
      char c;
      while (index < readlen && buffer[index] != 0)
      {
        switch (buffer[index++])
        {
        case '\r':
        case '\n':
          break;
        case 'S':
          jmdex = index;
          lwidth = readInt(buffer, jmdex, readlen);
          lheight = readInt(buffer, jmdex, readlen);
          lstride = readInt(buffer, jmdex, readlen);
          lcomponentsValue = readInt(buffer, jmdex, readlen);
          // is400Hz = 0;
          lis400Hz = readInt(buffer, jmdex, readlen, false);
          SetSize(lwidth, lheight, lstride, lcomponentsValue, lis400Hz);
          index = jmdex;
          Serial.println("Read in the 'S' parts");
          break;
        case 'q':
          while (buffer[index] != '\n')
            index++;
          Serial.println("Skipped the 'S' parts");
          break;
        case 'L':
          jmdex = index;
          // TODO: AMJ: this isn't robust against malformed lines.
          // Next numeric value is pin 0.
          p0 = readInt(buffer, jmdex, readlen);
          // Next numeric value is pin 1.
          p1 = readInt(buffer, jmdex, readlen, false);
          // Next character is fold status - W for wings, F for folded
          foldState = buffer[jmdex];
          if (p0 != 0)
            SetPin0(p0);
          if (p1 != 0)
            SetPin1(p1);
          if (foldState == 'W' || foldState == 'F')
            SetFold(foldState);
          index = jmdex;
          Serial.println("Read in the 'L' parts");
          break;
        case 'T':
          // Everything up to \r, \n, \0 or end of file is Unit Name.
          for (jmdex = index; jmdex < readlen && buffer[jmdex] > ' '; jmdex++)
          {
            // Do nothing.
          }
          c = buffer[jmdex];
          buffer[jmdex] = 0;
          SetName(&buffer[index]);
          buffer[jmdex] = c; // Put back the line-break, if there was one...
          index = jmdex;
          Serial.println("Read in the 'T' parts");
          break;
        default:
          break;
        }
      }
      _dirty = false;
      return true;
    }
    return false;
  }

  ~PersistSetting()
  {
    WriteSettings();
  }

  bool WriteSettings()
  {
    if (!_dirty)
      return true; // Don't write if we've written recently.
    if (settingsFile.open(_fname, FILE_O_WRITE))
    {
      settingsFile.seek(0);

      // Unit name - what it'll be called when we connect to it.
      settingsFile.write("T");
      settingsFile.write(_unitName);
      settingsFile.write("\n");
      // Fold state and pin numbers
      char buffer[15];
      settingsFile.write("L");
      settingsFile.write(itoa(_pin0, buffer, 10));
      settingsFile.write(",");
      settingsFile.write(itoa(_pin1, buffer, 10));
      settingsFile.write(&_fold, 1);
      settingsFile.write("\n");

      // Size
      settingsFile.write("S");
      settingsFile.write(itoa(_width, buffer, 10));
      settingsFile.write(",");
      settingsFile.write(itoa(_height, buffer, 10));
      settingsFile.write(",");
      settingsFile.write(itoa(_stride, buffer, 10));
      settingsFile.write(",");
      settingsFile.write(itoa(_componentsValue, buffer, 10));
      settingsFile.write(",");
      settingsFile.write(_is400Hz ? "1" : "0");
      settingsFile.write("\n");

      settingsFile.truncate(settingsFile.position());

      settingsFile.flush();

      settingsFile.close();
      Serial.println("Finished writing - let's load it back and check!");
      LoadFile();
      _dirty = false;
      return true;
    }
    return false;
  }

  char const *GetName() { return _unitName; }
  bool SetName(const char *newName)
  {
    _dirty = true;
    strncpy(_unitName, newName, sizeof _unitName - 1);
    return true;
  }
  int GetPin0() { return _pin0; }
  bool SetPin0(int pin0)
  {
    _dirty = true;
    _pin0 = pin0;
    return true;
  }
  int GetPin1() { return _pin1; }
  bool SetPin1(int pin1)
  {
    _dirty = true;
    _pin1 = pin1;
    return true;
  }
  char GetFold() { return _fold; }
  bool SetFold(char fold)
  {
    _dirty = true;
    _fold = fold;
    return true;
  }
  bool SetSize(byte iwidth, byte iheight, byte istride, byte icomponentsValue, byte iis400Hz)
  {
    _dirty = true;
    _width = iwidth;
    _height = iheight;
    _stride = istride;
    _componentsValue = icomponentsValue;
    _is400Hz = iis400Hz;
    return true;
  }

  int GetWidth() { return (int)_width; }

} persistentSettings; // Singleton

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
Adafruit_NeoPixel neo(1, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

uint8_t fold_offsets8[] = {3, 0xb, 0xc, 4, 2, 0xa, 0xd, 5, 1, 9, 0xe, 6, 0, 8, 0xf, 7}; // Carefully calculated!
// When the two 8-LED strips are doubly interleaved, with strip 0 offset by 1/8th the LED width, and strip 1 offset by 3/8th.
// This is the order in which you should light up the lights. Strip 0 is numbered 0-7, Strip 1 is numbered 8-0xf.
uint8_t wing_offsets8[] = {0, 8, 1, 9, 2, 0xa, 3, 0xb, 4, 0xc, 5, 0xd, 6, 0xe, 7, 0xf}; // Easily calculated!

uint8_t wing_offsetsN(int index, int numPixels)
{
  uint8_t retval;
  retval = (index / 2) + (numPixels * (index % 2));
  if (numPixels == 8)
  {
    if (retval != wing_offsets8[index])
      Serial.println("Failure in wing_offsetsN!");
  }
  return retval;
};

uint8_t fold_offsetsN(int index, int numPixels)
{
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
  if (numPixels == 8)
  {
    if (retval != fold_offsets8[index])
    {
      Serial.println("Failure in fold_offsetsN!");
      Serial.print("Index is ");
      Serial.print(index);
      Serial.print(" - ");
      Serial.print(retval);
      Serial.print(" should be ");
      Serial.println(fold_offsets8[index]);
    }
  }
  return retval;
}

enum commandStates
{
  NONE_COMMAND,
  FRAME_COMMAND
} command_state_now = NONE_COMMAND; // Commands that might need extra bytes beyond what's read in a single message.

enum displayStates
{
  NONE_DISPLAY,
  FRAME_DISPLAY,
  CYCLE_DISPLAY,
  SPARKLE_DISPLAY,
  GYRO_DISPLAY,
} display_state_now = NONE_DISPLAY;

enum foldedStates
{
  FOLDED_SHUT,
  FOLDED_OPEN
} folded_state_now = FOLDED_SHUT;

// When the two 8-LED strips are singly interleaved, with strip 0 offset by 1 width, and strip 1 by 1.5 width.
uint8_t *current_offsets = fold_offsets8;
uint8_t current_offsets_fun(int i, int numPixels)
{
  switch (folded_state_now)
  {
  case FOLDED_SHUT:
    return fold_offsetsN(i, numPixels);
  case FOLDED_OPEN:
    return wing_offsetsN(i, numPixels);
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

void setOffsetPixel(int i, uint32_t colorToSet)
{
  uint8_t offset_led;
  offset_led = current_offsets_fun(i, strip0.numPixels());
  uint16_t inset_led = offset_led % strip0.numPixels();
  uint8_t strip_num = offset_led / strip0.numPixels();
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
    Serial.println("Start time = " + String(starttime, HEX));
    display_state_now = CYCLE_DISPLAY;
    tick();
  }

  static void tick()
  {
    // TODO: Find out why this locks out at thirty-five minutes.
    // Note: 35 minutes is 0x80 00 00 00 / 1,000,000 - so, clocking from signed to unsinged in micros()).
    unsigned long tnow = micros() - starttime;
    nextFrame = micros() + 500UL;      // Prevents TimeLights() from deciding we don't need to be run!
    unsigned long dura = tnow / 500UL; // 500 micros is 1 speed 'tick' for the cycle.
    int newhue1 = (int)((dura * (long)speed) & 0x0000ffffUL);

    if (Serial)
      brightness = 100; // If we're at the end of a cable, we need less brights.

    hue1 = newhue1;
    for (int i = 0; i < strip0.numPixels() + strip1.numPixels(); i++)
    {
      uint16_t hue = (uint16_t)(hue1 + i * step);
      uint32_t col = Adafruit_NeoPixel::ColorHSV(hue, 255, brightness);
      uint32_t gam = Adafruit_NeoPixel::gamma32(col);
      setOffsetPixel(i, gam);
    }
    strip0.show();
    strip1.show();
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
    strip0.fill(background);
    strip1.fill(background);
    if (random(255) <= chance)
    {
      int pick = random(strip0.numPixels() + strip1.numPixels());
      if (pick < strip0.numPixels())
      {
        strip0.setPixelColor(pick, foreground);
      }
      else
      {
        strip1.setPixelColor(pick - strip0.numPixels(), foreground);
      }
    }
    strip0.show();
    strip1.show();
  }
};
uint32_t sparkle::background;
uint32_t sparkle::foreground;
int sparkle::chance;

#ifdef GYRO_CODE
struct my_gyro_range
{
  gyro_range range;
  uint32_t color;
  float top;
} const ranges[] = {
    {LSM6DS_GYRO_RANGE_125_DPS, Adafruit_NeoPixel::Color(255, 0, 0), 125.0},
    {LSM6DS_GYRO_RANGE_250_DPS, Adafruit_NeoPixel::Color(255, 255, 0), 250.0},
    {LSM6DS_GYRO_RANGE_500_DPS, Adafruit_NeoPixel::Color(0, 255, 0), 500.0},
    {LSM6DS_GYRO_RANGE_1000_DPS, Adafruit_NeoPixel::Color(0, 255, 255), 1000.0},
    {LSM6DS_GYRO_RANGE_2000_DPS, Adafruit_NeoPixel::Color(0, 0, 255), 2000.0},
    {ISM330DHCX_GYRO_RANGE_4000_DPS, Adafruit_NeoPixel::Color(255, 0, 255), 4000.0}};

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
  static int current_range_index;
  // Static functions
  static void start()
  {
    getRange();
    //    Serial.println(String("Gyro Start time = ") + String(micros(), HEX));
    display_state_now = GYRO_DISPLAY;
    tick();
  }
  static void getRange()
  {
    gyro_range current_range = lsm6ds33.getGyroRange();
    current_range_index = 0;
    int i = 0;
    for (i = 0; i < sizeof ranges / sizeof *ranges; i++)
    {
      if (ranges[i].range == current_range)
      {
        current_range_index = i;
      }
    }
    //    Serial.println(String("range = ") + String((int)current_range) + String(" maps to index ") + String(current_range_index) + String(" of ") + String(i));

    // Convert from degrees per second to radians per second
    range_top = ranges[current_range_index].top / 57.2957795; // 180/pi
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

    range_value = abs(gyro.gyro.z);

    range_percent = (range_value)*100.0 / (range_top);

    if (range_percent < 25.0 && current_range_index != 0)
    {
      // range_down
      lsm6ds33.setGyroRange(ranges[current_range_index - 1].range);
    }
    else if (range_percent > 75.0 && current_range_index != 5)
    {
      // range_up
      lsm6ds33.setGyroRange(ranges[current_range_index + 1].range);
    }
    //    Serial.println(String("Gyro reading received: z=")+String(range_value)+String(" - %=") + String(range_percent));
  }
  static void tick()
  {
    nextFrame = micros() + 500UL;
    getReading();
    int pixelCount = 0;
    int pixelMax = strip0.numPixels() + strip1.numPixels();
    switch (current_range_index)
    {
    case 0:
      pixelCount = pixelMax * (range_percent) / 75.0;
      break;
    case 6:
      pixelCount = pixelMax * (range_percent - 25.0) / 75.0;
      if (range_percent > 98.0)
      {
        pixelCount = pixelMax;
      }

      break;
    default:
      pixelCount = pixelMax * (range_percent - 25.0) / 50.0;
      break;
    }
    uint32_t pixelColor = ranges[current_range_index].color;
    //    Serial.println(String("Lighting up pixels: ") + String(pixelCount));
    //    Serial.println(String("Pixel color is ") + String(pixelColor));
    // light up pixelCount pixels at range_color
    for (int i = 0; i < strip0.numPixels() + strip1.numPixels(); i++)
    {
      if (i < pixelCount)
      {
        setOffsetPixel(i, pixelColor);
      }
      else
      {
        setOffsetPixel(i, 0);
      }
    }
    strip0.show();
    strip1.show();
  }
};
float gyroColor::range_top, gyroColor::range_value, gyroColor::range_percent;
int gyroColor::current_range_index;
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
  neo.setPixelColor(0, inColor);
  neo.show();
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

int width = 8, height = 2, stride = 1, componentsValue = 0, is400Hz = 0;

void setup()
{
  neo.begin();
  neo.setPixelColor(0, 0, 0, 255); // blue!
  neo.setPixelColor(0, 0, 0, 0);   // black.
  neo.show();
  Serial.begin(115200);
  InternalFS.begin();
  defaultFrames();
  //while (!Serial)
  //   delay(10); // Wait for serial?
  // put your setup code here, to run once:
  if (Serial)
  {
    Serial.println("Diable2");
    Serial.println("=======");
  }
  Bluefruit.configPrphBandwidth(BANDWIDTH_MAX);

  Bluefruit.begin();
  Bluefruit.setTxPower(4);
  persistentSettings.LoadFile();
  width = persistentSettings.GetWidth();
  strip0.clear();
  strip1.clear();
  strip0.show();
  strip1.show();
  strip0.updateLength(width);
  strip1.updateLength(width);
  strip0.clear();
  strip1.clear();
  strip0.show();
  strip1.show();

  char const *ident = persistentSettings.GetName();
  switch (persistentSettings.GetFold())
  {
  case 'F':
    current_offsets = fold_offsets8;
    folded_state_now = FOLDED_SHUT;
    break;
  case 'W':
    current_offsets = wing_offsets8;
    folded_state_now = FOLDED_OPEN;
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

  strip0.setPin(persistentSettings.GetPin0());
  strip0.begin();
  strip0.clear();
  strip0.show();
  strip1.setPin(persistentSettings.GetPin1());
  strip1.begin();
  strip1.clear();
  strip1.show();

  Serial.println("Time to connect and send!");
#ifdef GYRO_CODE
  if (!lsm6ds33.begin_I2C())
  {
    // if (!lsm6ds33.begin_SPI(LSM_CS)) {
    // if (!lsm6ds33.begin_SPI(LSM_CS, LSM_SCK, LSM_MISO, LSM_MOSI)) {
    Serial.println("Failed to find LSM6DS33 chip");
    while (1)
    {
      delay(10);
    }
  }

  Serial.println("LSM6DS33 Found!");

  // lsm6ds33.setGyroRange(LSM6DS_GYRO_RANGE_250_DPS);
  Serial.print("Gyro range set to: ");
  switch (lsm6ds33.getGyroRange())
  {
  case LSM6DS_GYRO_RANGE_125_DPS:
    Serial.println("125 degrees/s");
    break;
  case LSM6DS_GYRO_RANGE_250_DPS:
    Serial.println("250 degrees/s");
    break;
  case LSM6DS_GYRO_RANGE_500_DPS:
    Serial.println("500 degrees/s");
    break;
  case LSM6DS_GYRO_RANGE_1000_DPS:
    Serial.println("1000 degrees/s");
    break;
  case LSM6DS_GYRO_RANGE_2000_DPS:
    Serial.println("2000 degrees/s");
    break;
  case ISM330DHCX_GYRO_RANGE_4000_DPS:
    break; // unsupported range for the DS33
  }
  // lsm6ds33.setGyroDataRate(LSM6DS_RATE_12_5_HZ);
  Serial.print("Gyro data rate set to: ");
  switch (lsm6ds33.getGyroDataRate())
  {
  case LSM6DS_RATE_SHUTDOWN:
    Serial.println("0 Hz");
    break;
  case LSM6DS_RATE_12_5_HZ:
    Serial.println("12.5 Hz");
    break;
  case LSM6DS_RATE_26_HZ:
    Serial.println("26 Hz");
    break;
  case LSM6DS_RATE_52_HZ:
    Serial.println("52 Hz");
    break;
  case LSM6DS_RATE_104_HZ:
    Serial.println("104 Hz");
    break;
  case LSM6DS_RATE_208_HZ:
    Serial.println("208 Hz");
    break;
  case LSM6DS_RATE_416_HZ:
    Serial.println("416 Hz");
    break;
  case LSM6DS_RATE_833_HZ:
    Serial.println("833 Hz");
    break;
  case LSM6DS_RATE_1_66K_HZ:
    Serial.println("1.66 KHz");
    break;
  case LSM6DS_RATE_3_33K_HZ:
    Serial.println("3.33 KHz");
    break;
  case LSM6DS_RATE_6_66K_HZ:
    Serial.println("6.66 KHz");
    break;
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
  Serial.println("Bytes read in from BLE: " + String(readable));
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
  BLEConnection *connection = Bluefruit.Connection(conn_handle);
  char central_name[32] = {0};
  connection->getPeerName(central_name, sizeof(central_name));
  Serial.print("Connected to ");
  Serial.println(central_name);

  // request PHY changed to 2MB
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
  Serial.print("Disconnected, reason = 0x");
  Serial.println(reason, HEX);
  displayStartup();
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
  if ((long)(nextFrame - themicros) > 0L)
    return;

  // More timing statistics - how much average jitter (actual micros - planned micros) is there
  aveTime = (aveTime * 9.0 + (float)(themicros - nextFrame)) / 10.0;

  // Output timing statistics to serial port for debugging.
  if (0) // Choice to output jitter time to debug
  {
    Serial.print("Average jitter is ");
    Serial.println(aveTime);
  }
  if (0) // Choice to output loop avg time to debug
  {
    Serial.print("Average loop time is ");
    Serial.println(aveLoop);
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
      strip0.clear();
      strip1.clear();
      strip0.show();
      strip1.show();
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
        Serial.print("Back to frame 0 - total animation duration: ");
        Serial.println(totalFramesTime);
        if (totalFramesTime < minFramesTime)
          minFramesTime = totalFramesTime;
        if (totalFramesTime > maxFramesTime)
          maxFramesTime = totalFramesTime;
        Serial.print("min frame time: ");
        Serial.print(minFramesTime);
        Serial.print("max frame time");
        Serial.println(maxFramesTime);
        Serial.print("Expected total frame time: ");
        Serial.println(expectedTotalFramesTime);
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
    //  Serial.print("TimeLights says set frame ");
    //  Serial.println(nextFindex);
    // ASSUME: strip1 & strip0 have the same number of pixels.
    for (int i = 0; i < strip0.numPixels() + strip1.numPixels(); i++)
    {
      setOffsetPixel(i, currentFrame[nextFindex].color[i]);
    }
    strip0.show();
    strip1.show();
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
  int pixelCount = strip0.numPixels() + strip1.numPixels();
  int frameCount = pixelCount;
  // Serial.print("New frame count: ");
  // Serial.println(frameCount);
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
      nextFrameSet[i].color[j] = (current_offsets_fun(j, strip0.numPixels()) / strip0.numPixels()) ? Adafruit_NeoPixel::Color(0, 0x3f, 0) : Adafruit_NeoPixel::Color(0x3f, 0, 0);
    }
    for (int j = i + 1; j < pixelCount; j++)
    {
      nextFrameSet[i].color[j] = 0;
    }
  }
  nextFrameSet[frameCount].microdelay = -2L; // Special flag, "return to default"
  SetCurrentFrame(nextFrameSet, frameCount);
}

void loop()
{
  // put your main code here, to run repeatedly:

  TimeLights();
  // AMJ - If we have poor communication of commands, remove the following line again:
  if (0 && nextFrame - micros() > 500L)
    delayMicroseconds(100); // Reduce power usage, maybe? TimeLights() is kind of a busy loop.
}

void asyncReadAndProcess(int readable)
{
  /*
   * V - return version - "DiaBLE v2.0:L01F", where '0' is the pin for the 0 row of LEDs and '1' is the pin for the 1 row. 'F' or 'W' indicates fold state of wings. Folded or Wings. I prefer Wings, because bigger clearer image.
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
    Serial.print("command_state_now is NONE_COMMAND, command is ");
    Serial.println(blecmd);
    switch (blecmd)
    {
    case -1:
      Serial.println("We hit the minus one break");
      return;
      break;
    case 'V':
    {
      char response[40]; // overkill, its really only 30 characters including the null. Today.
      sprintf(response, "VDiaBLE v%1.2f:L%02d%02d%c:S%d,%d,%d,%d,%d\r\n", 2.00, persistentSettings.GetPin0(), persistentSettings.GetPin1(),
              persistentSettings.GetFold(), persistentSettings.GetWidth(), height, stride, componentsValue, is400Hz);
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
      strip0.clear();
      strip1.clear();
      strip0.show();
      strip1.show();
      strip0.updateLength(width);
      strip1.updateLength(width);
      // If going up a size, we want new pixels not to be random, so they get set black.
      strip0.clear();
      strip1.clear();
      strip0.show();
      strip1.show();
      stride = waitread();
      componentsValue = waitread();
      is400Hz = waitread();
      Serial.print("\tsize: ");
      Serial.print(width, 10);
      Serial.print("x");
      Serial.println(height, 10);
      Serial.print("\tstride: ");
      Serial.println(stride, 10);
      Serial.print("\tcomponents: ");
      Serial.println(componentsValue, 10);
      Serial.print("\tis400Hz: ");
      Serial.println(is400Hz, 10);
      Serial.println("\n\n");
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
      strip0.fill(Adafruit_NeoPixel::Color(color[0], color[1], color[2]), 0, strip0.numPixels());
      strip1.fill(Adafruit_NeoPixel::Color(color[0], color[1], color[2]), 0, strip0.numPixels());
      strip0.show();
      strip1.show();
      sendbleuok();
      break;
    case 'B':
      if (readable != 1)
      {
        Serial.println("Brightness command with weird number of params!");
      }
      brightness = waitread();
      readable--;
      strip0.setBrightness(brightness);
      strip1.setBrightness(brightness);
      strip0.show();
      strip1.show();
      sendbleuok();
      break;
    case 'P':
      x = waitread();
      y = waitread();
      readable -= 2;
      // Serial.print("\tPixel: ");Serial.print(x);Serial.print(" x ");Serial.println(y);
      for (int i = 0; i < 3; i++)
      {
        color[i] = waitread();
      }
      readable -= 3;
      width = strip0.numPixels();
      // Serial.print("\tColor: ");Serial.print(color[0]);Serial.print(" ");Serial.print(color[1]);Serial.print(" ");Serial.println(color[2]);
      offset_led = y * width + x;
      color_set = Adafruit_NeoPixel::Color(color[0], color[1], color[2]);
      switch (offset_led / width) // y
      {
      case 0:
        strip0.setPixelColor(offset_led % width, color_set); // x
        strip0.show();
        break;
      case 1:
        strip1.setPixelColor(offset_led % width, color_set); // x
        strip1.show();
        break;
      }
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
      strip0.clear();
      strip1.clear();
      strip0.show();
      strip1.show();
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
      nextFrameCount = (((unsigned int)(unsigned byte)waitread()) << 8) | (unsigned int)(unsigned byte)waitread();
      Serial.print("New frame count: ");
      Serial.println(nextFrameCount);
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
      strip0.clear();
      strip0.show();
      strip0.setPin(p0);
      strip0.begin();
      strip0.clear();
      strip0.show();
      strip1.clear();
      strip1.show();
      strip1.setPin(p1);
      strip1.begin();
      strip1.clear();
      strip1.show();
      switch (foldState = waitread())
      {
      case 'W': // unfolded - wings
        current_offsets = wing_offsets8;
        folded_state_now = FOLDED_OPEN;
        break;
      case 'F': // folded - lid
        current_offsets = fold_offsets8;
        folded_state_now = FOLDED_SHUT;
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
    const int frameSize = 3 * (strip0.numPixels() + strip1.numPixels()) + 4;
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
          Serial.print("Frame #");
          Serial.println(frameIndex);
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
          Serial.print("Micros: ");
          Serial.println(nextFrameSet[frameIndex].microdelay);
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
            Serial.print("LED # ");
            Serial.print(ledOffset);
            Serial.print(" Red: ");
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
      Serial.print("Final frameCount is ");
      Serial.println(nextFrameCount);
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
  // Serial.print("Processed command character: ");
  // Serial.print((int)blecmd);
  // Serial.print(" ");
  // Serial.print((char)blecmd);
  // Serial.println();
  //  Serial.println("Waiting for OK");
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
