class pcycle : public pattern
{
private:
  int hue1;
  int speed;
  int step;
  static const enum displayStates display_state = CYCLE_DISPLAY;
  pcycle(int _speed, int _step) : pattern(), hue1(0), speed(_speed), step(_step) {}

public:
  static pattern *Create(int _speed, int _step)
  {
    pattern *pat = new pcycle(_speed, _step);
    display_state_now = display_state;
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
