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
