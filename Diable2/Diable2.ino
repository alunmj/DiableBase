#include <bluefruit.h>
//#include <SPI.h>
#if not defined(_VARIANT_ARDUINO_DUE_X_) && not defined(_VARIANT_ARDUINO_ZERO_)
//  #include <SoftwareSerial.h>
#endif

#include <Adafruit_NeoPixel.h>

#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>
using namespace Adafruit_LittleFS_Namespace;

class PersistSetting
{
  File settingsFile;
  char const *_fname = "diableset.txt";

  char _unitName[16];
  char _fold;
  int _pin0;
  int _pin1;
  bool _dirty;

  void defaults()
  {
    strcpy(_unitName, "DiaBLEINI");
    _fold = 'F'; // Folded
    _pin0 = 5;
    _pin1 = 6;
  }

public:
  PersistSetting() : settingsFile(InternalFS) { defaults(); }
  bool LoadFile()
  {
    int p0, p1;
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
      Serial.println(readlen);
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
        case 'L':
          jmdex = index;
          // TODO: AMJ: this isn't robust against malformed lines.
          // Next numeric value is pin 0.
          p0 = atoi(&buffer[jmdex]);
          for (; jmdex < readlen && isDigit(buffer[jmdex]); jmdex++)
            ;
          // Skip any non-numeric value - don't care!
          for (; jmdex < readlen && !isDigit(buffer[jmdex]); jmdex++)
            ;
          // Next numeric value is pin 1.
          p1 = atoi(&buffer[jmdex]);
          for (; jmdex < readlen && isDigit(buffer[jmdex]); jmdex++)
            ;
          // Next character is fold status - W for wings, F for folded
          foldState = buffer[jmdex];
          if (p0 != 0)
            SetPin0(p0);
          if (p1 != 0)
            SetPin1(p1);
          if (foldState == 'W' || foldState == 'F')
            SetFold(foldState);
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
      char buffer[5];
      settingsFile.write("L");
      settingsFile.write(itoa(_pin0, buffer, 10));
      settingsFile.write(",");
      settingsFile.write(itoa(_pin1, buffer, 10));
      settingsFile.write(&_fold, 1);
      settingsFile.write("\n");
      settingsFile.truncate(settingsFile.position());

      settingsFile.flush();

      settingsFile.close();
      _dirty = false;
      // Serial.println("Finished writing - let's load it back and check!");
      // LoadFile();
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

// I don't see any reason not to hard-code this to the number of pixels on the sticks.
#define LED_COUNT 8

// Two strips of flashy lights
Adafruit_NeoPixel strip0(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel strip1(LED_COUNT, LED_PIN2, NEO_GRB + NEO_KHZ800);

// I don't really use this neo-pixel on the board.
Adafruit_NeoPixel neo(1, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

uint8_t fold_offsets[] = {3, 0xb, 0xc, 4, 2, 0xa, 0xd, 5, 1, 9, 0xe, 6, 0, 8, 0xf, 7}; // Carefully calculated!
// When the two 8-LED strips are doubly interleaved, with strip 0 offset by 1/8th the LED width, and strip 1 offset by 3/8th.
// This is the order in which you should light up the lights. Strip 0 is numbered 0-7, Strip 1 is numbered 8-0xf.
uint8_t wing_offsets[] = {0, 8, 1, 9, 2, 0xa, 3, 0xb, 4, 0xc, 5, 0xd, 6, 0xe, 7, 0xf}; // Easily calculated!
// When the two 8-LED strips are singly interleaved, with strip 0 offset by 1 width, and strip 1 by 1.5 width.
uint8_t *current_offsets = fold_offsets;

enum commandStates
{
  NONE,
  FRAME_COMMAND
} current_state = NONE; // Commands that might need extra bytes beyond what's read in a single message.

float rots = 0.0;
float pi = 4.0 * atan(1.0);
float pi23 = 2.0 * pi / 3.0;
float brightness = 150.0;
struct frame
{
  unsigned long microdelay;
  uint32_t color[LED_COUNT + LED_COUNT];
};
uint32_t allred = 0xff0000;
uint32_t allblue = 0x0000ff;
uint32_t allgreen = 0x00ff00;

unsigned int frameOffset = 0;

// Experiments show that we get within ~35-45 microseconds of hitting these timing numbers if they are above 450.
struct frame baseFrame1[] = {
    {500, {allgreen, 0, 0, 0, allred, 0, 0, 0, allblue, 0, 0, 0, allgreen, 0, 0, 0}},
    {500, {0, 0, 0, allred, 0, 0, 0, allblue, 0, 0, 0, allgreen, 0, 0, 0, allred}},
    {500, {0, 0, allred, 0, 0, 0, allblue, 0, 0, 0, allgreen, 0, 0, 0, allred}},
    {500, {0, allred, 0, 0, 0, allblue, 0, 0, 0, allgreen, 0, 0, 0, allred, 0, 0}},
    {500, {allred, 0, 0, 0, allblue, 0, 0, 0, allgreen, 0, 0, 0, allred, 0, 0, 0}},
    {500, {0, 0, 0, allblue, 0, 0, 0, allgreen, 0, 0, 0, allred, 0, 0, 0, allblue}},
    {500, {0, 0, allblue, 0, 0, 0, allgreen, 0, 0, 0, allred, 0, 0, 0, allblue, 0}},
    {500, {0, allblue, 0, 0, 0, allgreen, 0, 0, 0, allred, 0, 0, 0, allblue, 0, 0}},
    {500, {allblue, 0, 0, 0, allgreen, 0, 0, 0, allred, 0, 0, 0, allblue, 0, 0, 0}},
    {500, {0, 0, 0, allgreen, 0, 0, 0, allred, 0, 0, 0, allblue, 0, 0, 0, allgreen}},
    {500, {0, 0, allgreen, 0, 0, 0, allred, 0, 0, 0, allblue, 0, 0, 0, allgreen, 0}},
    {500, {0, allgreen, 0, 0, 0, allred, 0, 0, 0, allblue, 0, 0, 0, allgreen, 0, 0}},
    {~0UL, {}}}; // a colourful spiral design. Each leg of the spiral is a different colour.
struct frame baseFrame[] = {
    {500, {allred, allred, allred, allred, allred, allred, allred, allred, allblue, allblue, allblue, allblue, allblue, allblue, allblue, allblue}},
    {500, {}},
    {500, {allgreen, allgreen, allgreen, allgreen, allgreen, allgreen, allgreen, allgreen, allred, allred, allred, allred, allred, allred, allred, allred}},
    {500, {}},
    {500, {allblue, allblue, allblue, allblue, allblue, allblue, allblue, allblue, allgreen, allgreen, allgreen, allgreen, allgreen, allgreen, allgreen, allgreen}},
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

void defaultFrames()
{
  Serial.println("Reverting to default frames!");
  if (currentFrame != NULL && currentFrame != DEFAULT_FRAME)
  {
    delete[] currentFrame;
  }
  SetCurrentFrame(DEFAULT_FRAME, sizeof(DEFAULT_FRAME) / sizeof(*DEFAULT_FRAME));
}

long minFramesTime, maxFramesTime;

long totalFrameStartTime, expectedTotalFramesTime = 0;

void SetCurrentFrame(frame *_nextFrameSet, int _nextFrameCount)
{
  myFrameCount = _nextFrameCount;
  nextFindex = 0;
  totalFrameStartTime = ~0L;
  nextFrame = micros();
  currentFrame = _nextFrameSet;
  minFramesTime = 0x7fffffffL;
  maxFramesTime = 0L;
}

//    #define FACTORYRESET_ENABLE         0
//    #define MINIMUM_FIRMWARE_VERSION    "0.6.6"
//    #define MODE_LED_BEHAVIOUR          "MODE"

// OTA DFU service
BLEDfu bledfu;

// Uart over BLE service
BLEUart bleuart;

void error(const char *ts)
{
  Serial.println(ts);
  while (1)
    ; // Eternal loop!
}

void setup()
{
  neo.begin();
  neo.setPixelColor(0, 0, 0, 255); // blue!
  neo.setPixelColor(0, 0, 0, 0);   // black.
  neo.show();
  Serial.begin(115200);
  InternalFS.begin();
  defaultFrames();
  // while (!Serial) delay(10); // Wait for serial?
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

  char const *ident = persistentSettings.GetName();
  switch (persistentSettings.GetFold())
  {
  case 'F':
    current_offsets = fold_offsets;
    break;
  case 'W':
    current_offsets = wing_offsets;
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

  // The next animation frame happens "right now" - micros()
  nextFrame = micros();
  nextFindex = 0;
  displayStartup();
}

void rxCallback(uint16_t conn_hdl)
{
  int readable = bleuart.available();
  Serial.print("Bytes read in from BLE: ");
  Serial.println(readable);
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
  //connection->requestConnectionParameter(6); // in unit of 1.25

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

int width = 8, height = 2, stride, pixelType, componentsValue, is400Hz;
int lastloop = 0, looptime = 0;
float aveLoop = 0.0;

void TimeLights()
{
  long themicros = micros();
  if (lastloop != 0)
    looptime = themicros - lastloop;
  lastloop = themicros;
  aveLoop = (aveLoop * 9.0 + (float(looptime))) / 10.0;
  if ((long)(nextFrame - themicros) > 0L)
    return;
  aveTime = (aveTime * 9.0 + (float)(themicros - nextFrame)) / 10.0;
  if (0)
  {
    Serial.print("Average jitter is ");
    Serial.println(aveTime);
  }
  if (0)
  {
    Serial.print("Average loop time is ");
    Serial.println(aveLoop);
  }
  if (currentFrame == NULL)
  {
    strip0.clear();
    strip0.show();
    strip1.clear();
    strip1.show();
    return;
  }
  long microdelay = (long)currentFrame[nextFindex].microdelay;
  expectedTotalFramesTime += microdelay;
  if (nextFindex == 0)
  {
    long totalFramesTime = themicros - totalFrameStartTime;
    if (0 && totalFrameStartTime != ~0L)
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
  if (microdelay < 0L)
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
  uint8_t offset_led;
  for (int i = 0; i < 2 * LED_COUNT; i++)
  {
    offset_led = current_offsets[i];
    switch ((offset_led & 8) >> 3)
    {
    case 0:
      strip0.setPixelColor(offset_led & 7, currentFrame[nextFindex].color[i]);
      break;
    case 1:
      strip1.setPixelColor(offset_led & 7, currentFrame[nextFindex].color[i]);
      break;
    }
  }
  strip0.show();
  strip1.show();
  nextFindex++;
  if (nextFindex > myFrameCount || currentFrame[nextFindex].microdelay == -1L)
  {
    nextFindex = 0;
  }
}

void displayStartup()
{
  // This is called on startup, on connect, and when the pins or orientation are changed.
  // Empty the frames...
  defaultFrames();
  // Allocate new frames
  int frameCount = 16;
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
    nextFrameSet[i].microdelay = 100000;
    for (int j = 0; j <= i; j++)
    {
      nextFrameSet[i].color[j] = (current_offsets[j] & 8) ? strip0.Color(0, 63, 0) : strip0.Color(63, 0, 0);
    }
    for (int j = i + 1; j < 16; j++)
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
  if (0 && nextFrame - micros() > 500)
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
   * -1 - always sends you back to NONE - useful if we overrun/underrun.
   * N - New - clear animation frames. No parameters
   * F - Frames - 2 byte count of frames. Each frame: 4 bytes microseconds time, then 2 x LED_COUNT sets of 3 bytes rgb.
   * L - LED positions - two bytes for the pins for each stick, one byte "W" for "wings" or "F" for "folded".
   * T - Title the DiaBLE, so you can have more than one board.
   */
  // Check for incoming characters from Bluefruit
  char foldState;
  uint8_t color[3], brightness, x, y;
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
  switch (current_state)
  {
  case NONE:
    blecmd = bleuart.read();
    if (blecmd == 0)
      return;
    // Commands from the Adafruit demo:
    // V - return version
    // S - setup dimensions, components, stride
    // C - clear with color
    // B - set brightness
    // P - set pixel
    // I - receive image
    // N - clear all frames, go to black.
    // F - Frames
    // O - OK
    // T - Title this unit. Sets its name.
    Serial.print("current_state is NONE, command is ");
    Serial.println(blecmd);
    switch (blecmd)
    {
    case -1:
      Serial.println("We hit the minus one break");
      return;
      break;
    case 'V':
    {
      char response[30]; // overkill, its really only 22 characters including the null. Today.
      sprintf(response, "LDiaBLE v%1.2f:L%02d%02d%c\r\n", 2.00, persistentSettings.GetPin0(), persistentSettings.GetPin1(),
              persistentSettings.GetFold());
      Serial.print(response);
      sendbleu(response);
    }
      if (readable > 1)
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
      stride = waitread();
      componentsValue = waitread();
      is400Hz = waitread();
      // Serial.print("\tsize: ");Serial.print(width);Serial.print("x");Serial.println(height);
      // Serial.print("\tstride: ");Serial.println(stride);
      // Serial.print("\tpixelType ");Serial.println(pixelType);
      // Serial.print("\tcomponents: ");Serial.println(componentsValue);
      sendbleuok();
      break;
    case 'C':
      if (readable < 4)
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
      // Cheat - don't need to do it with the offsets!
      for (int i = 0; i < LED_COUNT; i++)
      {
        strip0.setPixelColor(i, strip0.Color(color[0], color[1], color[2]));
      }
      for (int i = 0; i < LED_COUNT; i++)
      {
        strip1.setPixelColor(i, strip0.Color(color[0], color[1], color[2]));
      }
      strip0.show();
      strip1.show();
      sendbleuok();
      break;
    case 'B':
      if (readable != 2)
      {
        Serial.println("Brightness command with weird number of params!");
      }
      brightness = waitread();
      strip0.setBrightness(brightness);
      strip1.setBrightness(brightness);
      strip0.show();
      strip1.show();
      sendbleuok();
      break;
    case 'P':
      x = waitread();
      y = waitread();
      // Serial.print("\tPixel: ");Serial.print(x);Serial.print(" x ");Serial.println(y);
      for (int i = 0; i < 3; i++)
      {
        color[i] = waitread();
      }
      // Serial.print("\tColor: ");Serial.print(color[0]);Serial.print(" ");Serial.print(color[1]);Serial.print(" ");Serial.println(color[2]);
      offset_led = y * width + x;
      color_set = strip0.Color(color[0], color[1], color[2]);
      switch ((offset_led & 8) >> 3)
      {
      case 0:
        strip0.setPixelColor(offset_led & 7, color_set);
        strip0.show();
        break;
      case 1:
        strip1.setPixelColor(offset_led & 7, color_set);
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
      current_state = FRAME_COMMAND;
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
      readable -= 3;
      goto command_loop;
      break;
    case 'L':
      // Two bytes - pin for LED strip 0, pin for LED strip 1
      // One byte - 'W' for unfolded LED wings, 'F' for folded on the lid.
      if (readable < 4)
      {
        Serial.println("Too few characters in L command");
      }
      else if (readable > 4)
      {
        Serial.println("Too many characters in L command");
      }
      else
      {
        Serial.println("L command received - pins, fold");
      }
      p0 = waitread();
      p1 = waitread();
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
        current_offsets = wing_offsets;
        break;
      case 'F': // folded - lid
        current_offsets = fold_offsets;
        break;
      }
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
        return;
      }
      Serial.println("Not OK?");
      break;
    case 'T':
      // Read in the unit name. Everything up to end of line or end of transmission, whichever comes first!
      char buffer[21];
      j = 0;
      for (int i = 0; i < readable - 1; i++)
      {
        char c = waitread();
        if (c > ' ' && j < 20)
        {
          buffer[j++] = c;
        }
      }
      buffer[j] = 0;
      persistentSettings.SetName(buffer);
      persistentSettings.WriteSettings();
      break;
    default:
      // TODO: Send "NOK"
      break;
    }
    break;
  case FRAME_COMMAND:
    const int frameSize = 3 * 16 + 4;
    bool bSerDbg = false;
    while (frameOffset < nextFrameCount * frameSize && readable > 0)
    {
      int frameIndex = frameOffset / frameSize;
      int innerFrameOffset = frameOffset % frameSize;
      uint8_t readbyte;
      readable--;
      frameOffset++;
      readbyte = waitread();
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
        // Read three x 2 x LED_COUNT bytes of colour.
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
    current_state = NONE;
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
  bleuart.write("OK\r\n", 4);
}
