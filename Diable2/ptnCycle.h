class pattern
{
protected:
  static pattern *singleton;
  unsigned long starttime = 0;
  enum displayStates display_state = NONE_DISPLAY;
  pattern()
  {
    starttime = micros();
    display_state_now = display_state;
    Serial.printf("Start time = %lx\n", starttime);
    tick();
  }

public:
  static void switch_pattern(pattern *newPattern)
  {
    if (singleton != nullptr)
    {
      pattern *tpat = singleton;
      singleton = nullptr;
      delete tpat;
    }
    singleton = newPattern;
  }
  static bool tickNow()
  {
    if (singleton != nullptr)
    {
      singleton->tick();
      return true;
    }
    else
    {
      return false;
    }
  }
  virtual void tick(){};
};
pattern *pattern::singleton = nullptr;

class pcycle : public pattern
{
private:
  int hue1;
  int speed;
  int step;
  enum displayStates display_state = CYCLE_DISPLAY;
  pcycle(int _speed, int _step) : hue1(0), speed(_speed), step(_step) {}

public:
  static pattern *Create(int _speed, int _step)
  {
    pattern *pat = new pcycle(_speed, _step);
    display_state_now = CYCLE_DISPLAY;
    return pat;
  }
  void tick()
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
