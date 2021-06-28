#include <bluefruit.h>
//#include <SPI.h>
#if not defined (_VARIANT_ARDUINO_DUE_X_) && not defined (_VARIANT_ARDUINO_ZERO_)
//  #include <SoftwareSerial.h>
#endif

#include <Adafruit_NeoPixel.h>
// We're coded up right now for the BlueFruit nRF52840 Feather Express
// Important pin values from https://learn.adafruit.com/introducing-the-adafruit-nrf52840-feather/pinouts:
// User switch (also for DFU if held down when booting)
#define USERSW_PIN 7 // D7
#define ADALED_RED 3 // D3 P1.15 - LED_RED, by the battery plug, indicates bootloader mode.
#define ADALED_CONN P1.10 // P1.10
#define PIN_NEOPIXEL P0.16 // P0.16 - how do we access?
// CHG is a yellow LED used when charging.


// I've soldered the two 8-LED RGB NeoPixel strips to pins 6 and 12 
#define LED_PIN 5
#define LED_PIN2 6
#define LED_COUNT 8
Adafruit_NeoPixel strip0(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

Adafruit_NeoPixel strip1(LED_COUNT, LED_PIN2, NEO_GRB + NEO_KHZ800);

uint8_t fold_offsets[]={ 3, 0xb, 0xc, 4, 2, 0xa, 0xd, 5, 1, 9, 0xe, 6, 0, 8, 0xf, 7}; // Carefully calculated!
// When the two 8-LED strips are doubly interleaved, with strip 0 offset by 1/8th the LED width, and strip 1 offset by 3/8th.
// This is the order in which you should light up the lights. Strip 0 is numbered 0-7, Strip 1 is numbered 8-0xf.
uint8_t wing_offsets[]={0, 8, 1, 9, 2, 0xa, 3, 0xb, 4, 0xc, 5, 0xd, 6, 0xe, 7, 0xf}; // Easily calculated!
// When the two 8-LED strips are singly interleaved, with strip 0 offset by 1 width, and strip 1 by 1.5 width.
uint8_t *current_offsets = fold_offsets;

float rots=0.0;
float pi = 4.0 * atan(1.0);
float pi23 = 2.0*pi / 3.0;
float brightness=150.0;
unsigned long nextRead;
struct frame {
  unsigned long microdelay;
  uint32_t color[LED_COUNT+LED_COUNT];
};
uint32_t allred = 0xff0000;
uint32_t allblue = 0x0000ff;
uint32_t allgreen = 0x00ff00;
// Experiments show that we get within ~35-45 microseconds of hitting these timing numbers if they are above 450.
struct frame baseFrame1[] = {
  {500,{allgreen,0,0,0,allred,0,0,0,allblue,0,0,0,allgreen,0,0,0}},
  {500,{0,0,0,allred,0,0,0,allblue,0,0,0,allgreen,0,0,0,allred}},
  {500,{0,0,allred,0,0,0,allblue,0,0,0,allgreen,0,0,0,allred}},
  {500,{0,allred,0,0,0,allblue,0,0,0,allgreen,0,0,0,allred,0,0}},
  {500,{allred,0,0,0,allblue,0,0,0,allgreen,0,0,0,allred,0,0,0}},
  {500,{0,0,0,allblue,0,0,0,allgreen,0,0,0,allred,0,0,0,allblue}},
  {500,{0,0,allblue,0,0,0,allgreen,0,0,0,allred,0,0,0,allblue,0}},
  {500,{0,allblue,0,0,0,allgreen,0,0,0,allred,0,0,0,allblue,0,0}},
  {500,{allblue,0,0,0,allgreen,0,0,0,allred,0,0,0,allblue,0,0,0}},
  {500,{0,0,0,allgreen,0,0,0,allred,0,0,0,allblue,0,0,0,allgreen}},
  {500,{0,0,allgreen,0,0,0,allred,0,0,0,allblue,0,0,0,allgreen,0}},
  {500,{0,allgreen,0,0,0,allred,0,0,0,allblue,0,0,0,allgreen,0,0}},
  {-1,{}}
  }; // a colourful spiral design. Each leg of the spiral is a different colour.
struct frame baseFrame[] = {
  {500,{allred,allred,allred,allred,allred,allred,allred,allred,allblue,allblue,allblue,allblue,allblue,allblue,allblue,allblue}},
  {500,{}},
  {500,{allgreen,allgreen,allgreen,allgreen,allgreen,allgreen,allgreen,allgreen,allred,allred,allred,allred,allred,allred,allred,allred}},
  {500,{}},
  {500,{allblue,allblue,allblue,allblue,allblue,allblue,allblue,allblue,allgreen,allgreen,allgreen,allgreen,allgreen,allgreen,allgreen,allgreen}},
  {500,{}},
  {-1,{}}
};

// The frame set
#define DEFAULT_FRAME baseFrame1
struct frame *currentFrame = NULL;
int myFrameCount = 0;
unsigned long nextFrame; // micros() at next frame
int nextFindex = 0;

void defaultFrames() {
  if (currentFrame != NULL && currentFrame != DEFAULT_FRAME){
    delete [] currentFrame;
  }
  currentFrame = DEFAULT_FRAME;
  myFrameCount = sizeof(DEFAULT_FRAME)/sizeof(*DEFAULT_FRAME);
  nextFrame = micros();
  nextFindex = 0;
}

//    #define FACTORYRESET_ENABLE         0
//    #define MINIMUM_FIRMWARE_VERSION    "0.6.6"
//    #define MODE_LED_BEHAVIOUR          "MODE"

// OTA DFU service
BLEDfu bledfu;

// Uart over BLE service
BLEUart bleuart;

void error(const char* ts)
{
  Serial.println(ts);
  while(1); // Eternal loop!
}

void setup() {
  defaultFrames();
  Serial.begin(9600);
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
  Bluefruit.setName("DiaBLE");

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

  Serial.println("Requesting Bluefruit info:");
  /* Print Bluefruit information */
  strip0.setPin(LED_PIN);
  strip0.begin();
  strip0.clear();
  strip0.show();
  strip1.setPin(LED_PIN2);
  strip1.begin();
  strip1.clear();
  strip1.show();

  Serial.println("Time to connect and send!");

  /* Wait for connection */
//  while (! bleuart.isConnected()) {
//      delay(50);
//  }

/*  // LED Activity command is only supported from 0.6.6
  if ( ble.isVersionAtLeast(MINIMUM_FIRMWARE_VERSION) )
  {
    // Change Mode LED Activity
    Serial.println("******************************");
    Serial.println("Change LED activity to " MODE_LED_BEHAVIOUR);
    ble.sendCommandCheckOK("AT+HWModeLED=" MODE_LED_BEHAVIOUR);
    Serial.println("******************************");
  } */
  nextRead = micros();
  nextFrame = micros();
  nextFindex = 0;
  displayStartup();
//  ble.setMode(BLUEFRUIT_MODE_DATA);
}

void rxCallback(uint16_t conn_hdl) {
  // Serial.println("Rx on UART");
  // Serial.print(bleuart.available());
  // Serial.println(" bytes are available");
  // We could potentially read more from the FIFO, with bool peek(void *buffer)
}
void rxOverflowCallback(uint16_t conn_hdl, uint16_t leftover) {
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
  Bluefruit.Advertising.setInterval(32, 244);    // in unit of 0.625 ms
  Bluefruit.Advertising.setFastTimeout(30);      // number of seconds in fast mode
  Bluefruit.Advertising.start(0);                // 0 = Don't stop advertising after n seconds  
}

void connectCallback(uint16_t conn_handle)
{
  BLEConnection* connection = Bluefruit.Connection(conn_handle);
  char central_name[32] = {0};
  connection->getPeerName(central_name, sizeof(central_name));
  // Serial.print("Connected to ");
  // Serial.println(central_name);
  displayStartup();
}

void disconnectCallback(uint16_t conn_handle, uint8_t reason)
{
  // Don't be tempted to reset the current_offset, because it should be a hardware setting.
  // Serial.println();
  // Serial.print("Disconnected, reason = 0x");
  // Serial.println(reason, HEX);
  displayStartup();
  bleuart.flush();
}

float aveTime = 0;

int width=8, height=2, stride, pixelType, componentsValue, is400Hz;

void TimeLights()
{
  if ((long)(nextFrame-micros()) > 0) return;
  aveTime = (aveTime *9.0 + micros()-nextFrame)/10.0;
  if (currentFrame == NULL) {
    strip0.clear();
    strip0.show();
    strip1.clear();
    strip1.show();
    return;
  }
  long microdelay = (long)currentFrame[nextFindex].microdelay;
  if (microdelay < 0)
  {
    // Special flags.
    switch (microdelay)
    {
      case -2: // Return to default frame.
        defaultFrames();
        return;
        break;
      case -3: // Drop to black.
        defaultFrames();
        currentFrame = NULL;
        return;
        break;
    }
  }
  nextFrame = currentFrame[nextFindex].microdelay+micros();
//  Serial.print("TimeLights says set frame ");
//  Serial.println(nextFindex);
  uint8_t offset_led;
  for (int i=0; i< 2*LED_COUNT; i++) {
    offset_led = current_offsets[i];
    switch ((offset_led&8)>>3) {
      case 0:
        strip0.setPixelColor(offset_led & 7,currentFrame[nextFindex].color[i]);
        break;
      case 1:
        strip1.setPixelColor(offset_led & 7,currentFrame[nextFindex].color[i]);
        break;
    }
  }
  strip0.show();
  strip1.show();
  nextFindex++;
  if (nextFindex > myFrameCount || currentFrame[nextFindex].microdelay==-1) {
    nextFindex = 0;
  }
}

void displayStartup() {
  // This is called on startup, on connect, and when the pins or orientation are changed.
  // Empty the frames...
  defaultFrames();
  // Allocate new frames
  int frameCount = 16;
  // Serial.print("New frame count: ");
  // Serial.println(frameCount);
  currentFrame = new frame[frameCount+1];
  if (currentFrame == NULL) {
    // Serial.println("Badness on the frame allocation!");
  }
  for (int i=0; i< frameCount; i++) { 
  // Green lights on strip 0, starting at middle
  // Red lights on strip 1, starting at middle
    currentFrame[i].microdelay = 100000;
    for (int j=0;j<=i;j++) {
      currentFrame[i].color[j]=(current_offsets[j] & 8)?strip0.Color(0,255,0):strip0.Color(255,0,0);
    }
    for (int j=i+1; j < 16; j++) {
      currentFrame[i].color[j]=0;
    }
  }
  currentFrame[frameCount].microdelay = -2; // Special flag, "return to default" 
  myFrameCount = frameCount;
  // assign nextFindex, nextFrame time
  nextFindex = 0;
  nextFrame=micros();
}

void loop() {
  // put your main code here, to run repeatedly:

  TimeLights();
  if ((long)(nextRead-micros()) > 0 || bleuart.available()==0) {
    return;
  }
  // Serial.print("Average plus time over the last 10 goes: ");
  // Serial.println(aveTime);
  nextRead = micros() + (unsigned long)10000;
  // Check for incoming characters from Bluefruit
  uint8_t color[3],brightness,x,y;
  int i, j, redled, grnled, bluled;
  uint8_t offset_led;
  uint32_t color_set;
  unsigned int frameCount;
  int blecmd = 0;
  int p0,p1;
  blecmd = bleuart.read();
  if (blecmd == 0) return;
  // Commands from the Adafruit demo:
  // V - return version
  // S - setup dimensions, components, stride
  // C - clear with color
  // B - set brightness
  // P - set pixel
  // I - receive image
  switch (blecmd)
  {
    case -1:
      // Serial.println("We hit the minus one break");
      return;
      break;
    case 'V':
      sendbleu("BluLight v2.0");
      break;
    case 'S':
      width=waitread();
      height=waitread();
      stride=waitread();
      componentsValue=waitread();
      is400Hz=waitread();
      // Serial.print("\tsize: ");Serial.print(width);Serial.print("x");Serial.println(height);
      // Serial.print("\tstride: ");Serial.println(stride);
      // Serial.print("\tpixelType ");Serial.println(pixelType);
      // Serial.print("\tcomponents: ");Serial.println(componentsValue);
      sendbleu("OK");
      break;
    case 'C':
      for (int i=0;i<3;i++) {
        color[i]=waitread();
      }
      // Cheat - don't need to do it with the offsets!
      for (int i=0;i<LED_COUNT;i++)
      {
        strip0.setPixelColor(i,strip0.Color(color[0],color[1],color[2]));
      }
      for (int i=0;i<LED_COUNT;i++)
      {
        strip1.setPixelColor(i,strip0.Color(color[0],color[1],color[2]));
      }
      strip0.show();
      strip1.show();
      sendbleu("OK");
      break;
    case 'B':
      // Serial.println("Received Brightness command");
      brightness = waitread();
      strip0.setBrightness(brightness);
      strip1.setBrightness(brightness);
      strip0.show();
      strip1.show();
      sendbleu("OK");
      break;
    case 'P':
      x = waitread();
      y = waitread();
      // Serial.print("\tPixel: ");Serial.print(x);Serial.print(" x ");Serial.println(y);
      for (int i=0;i<3;i++) {
        color[i]=waitread();
      }
      // Serial.print("\tColor: ");Serial.print(color[0]);Serial.print(" ");Serial.print(color[1]);Serial.print(" ");Serial.println(color[2]);
      offset_led = y*width + x;
      color_set = strip0.Color(color[0],color[1],color[2]);
      switch ((offset_led & 8)>>3) {
        case 0:
          strip0.setPixelColor(offset_led & 7, color_set);
          strip0.show();
          break;
        case 1:
          strip1.setPixelColor(offset_led & 7, color_set);
          strip1.show();
          break;
      }
      sendbleu("OK");
      break;
      // ADDED commands - not in the bluefruit sketch from Adafruit.
      // N - clear all the frames out, set to blank.
      // F - Load a set of frames.
      // L - Set LED Pin & configuration
    case 'N':
      // clear frames.
      // Serial.println("\tClear all frames");
      defaultFrames();
      currentFrame = NULL;
      strip0.clear();
      strip1.clear();
      strip0.show();
      strip1.show();
      //
      sendbleu("OK");
      break;
    case 'F':
      // Serial.print("\tFrame: ");
      defaultFrames();
      // Serial.println("Deleted old frames");
      // Allocate currentFrame
      frameCount = (((unsigned int)(unsigned byte)waitread())<<8) | (unsigned int)(unsigned byte)waitread();
      // Serial.print("New frame count: ");
      // Serial.println(frameCount);
      currentFrame = new frame[frameCount+1];
      if (currentFrame == NULL) {
        // Serial.println("Badness on the frame allocation!");
      }
      // Fill with data
      for (int i=0;i<frameCount;i++) {
        // Serial.print("Frame #");
        // Serial.println(i);
        // Read four bytes of microseconds for this frame to show.
        unsigned long lms = (unsigned long)(unsigned byte)waitread();
        lms = lms << 8;
        lms |= (unsigned long)(unsigned byte)waitread();
        lms = lms << 8;
        lms |= (unsigned long)(unsigned byte)waitread();
        lms = lms << 8;
        lms |= (unsigned long)(unsigned byte)waitread();
        // Serial.print("Micros: ");
        // Serial.println(lms);
        if (currentFrame != NULL) {
          currentFrame[i].microdelay = lms;
        }
        // Read three x 2 x LED_COUNT bytes of colour.
        for (int j=0;j<2*LED_COUNT;j++) {
          // Serial.print("LED # ");
          // Serial.print(j);
          redled = waitread();
          grnled = waitread();
          bluled = waitread();
          if (redled==-1 || grnled == -1 || bluled == -1)
          {
            // Serial.println("Error");
          }
          // Serial.print(" Red: ");
          // Serial.print(redled);
          // Serial.print(" Grn: ");
          // Serial.print(grnled);
          // Serial.print(" Blu: ");
          // Serial.print(bluled);
          // Serial.println("");
          if (currentFrame != NULL) {
            currentFrame[i].color[j] = strip0.Color(redled,grnled,bluled);
          }
        }
      }
      // Terminate the frame list.
      // Serial.println("Terminating");
      // Serial.print("Final frameCount is ");
      // Serial.println(frameCount);
      if (currentFrame != NULL) {
        currentFrame[frameCount].microdelay = -1;
        myFrameCount = frameCount;
        // assign nextFindex, nextFrame time
        nextFindex = 0;
        nextFrame=micros();
      }
      sendbleu("OK");
      break;
    case 'L':
      // Two bytes - pin for LED strip 0, pin for LED strip 1
      // One byte - 'W' for unfolded LED wings, 'F' for folded on the lid.
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
      switch (waitread()) {
        case 'W': // unfolded - wings
          current_offsets = wing_offsets;
          break;
        case 'F': // folded - lid
          current_offsets = fold_offsets;
          break;
      }
      // TODO: Give visual feedback that we're good.
      // Idea: circles out from the middle, green on strip 0, red on strip 1, then black.
      // We can do this also on startup / connect.
      displayStartup();
      break;
    case 'O':
      // OK, generally.
      // Serial.println("Received OK?");
      if (waitread()=='K' && waitread()=='\n' || waitread()=='\n')
      {
        return;
      }
      // Serial.println("Not OK?");
      break;
    default:
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
  if (bleuart.available()==0) {
    Serial.println("Entering spin on available()");
    while (bleuart.available()==0) yield();
  }
  Serial.println("Reading a byte");
  return bleuart.read();
}
void sendbleu(const char * sendit)
{
  bleuart.write(sendit,strlen(sendit)*sizeof(char));
  bleuart.println("");
}
