class pdart : public pattern
{
private:
  static const enum displayStates display_state = DART_DISPLAY;
  pdart() : pattern() {}

public:
  static pattern *Create()
  {
    pattern *pat = new pdart();
    display_state_now = display_state;
    return pat;
  }
  void tick()
  {
    static bool cycled = false;
    static int lastnStep = -1;
    // Note: 35 minutes is 0x80 00 00 00 / 1,000,000 - so, clocking from signed to unsinged in micros()).
    unsigned long tnow = micros() - starttime;
    nextFrame = micros() + 500UL; // Prevents TimeLights() from deciding we don't need to be run!
    unsigned long dura = tnow / 100UL;
    int newhue1 = (int)((dura / 10L));

    if (Serial)
      brightness = 100; // If we're at the end of a cable, we need less brights.

    // "dart" - black background, red darts.
    // 1000 0001 1000 0001
    // 1100 0011 1100 0011
    // 1110 0111 1110 0111
    // 1111 1111 1111 1111
    // 0111 1110 0111 1110
    // 0011 1100 0011 1100
    // 0001 1000 0001 1000
    // 0000 0000 0000 0000
    // This looks wonky in code, these are rotating, so they will look like darts!
    int nPixels = width * height;
    int nSteps = nPixels / 2; // Size of pattern
    int nStep = newhue1 % nSteps;
    uint32_t cols[] = {
        Adafruit_NeoPixel::Color(255, 0, 0),
        Adafruit_NeoPixel::Color(0, 255, 0),
        Adafruit_NeoPixel::Color(0, 0, 255),
        Adafruit_NeoPixel::Color(0, 0, 0),
    };
    int nCols = sizeof cols / sizeof *cols;
    int nColor = (newhue1 / nSteps) % nCols;
    int colindex = nColor, colindex2;
    uint32_t newcol = Adafruit_NeoPixel::Color(255, 0, 0);
    uint32_t oldcol = Adafruit_NeoPixel::Color(0, 0, 0);
    uint32_t thiscol;
    int i2, i3;
    colindex2 = (colindex + 1) % nCols;
    for (int i = 0; i < width * height; i++)
    {
      i2 = i % (nSteps);                  // Left half equals right half
      i3 = i2 > 3 ? nSteps - 1 - i2 : i2; // left quarter opposite of 2nd quarter
      if ((i3 + nStep) % (nSteps) < nSteps / 2)
      {
        thiscol = cols[colindex2];
      }
      else
      {
        thiscol = cols[colindex];
      }
      setUnoffsetPixel(i, thiscol);
    }
    if (lastnStep != nStep)
    {
      ShowStrips();
    }
    lastnStep = nStep;
  }
};
