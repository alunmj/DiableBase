class psparkle : public pattern
{
private:
  int chance; // 0..255 out of 255. 255 = almost always a sparkle, 0 = never. Good values? 200? 100?
  uint32_t foreground;
  uint32_t background;
  enum displayStates display_state = SPARKLE_DISPLAY;
  psparkle (int _chance, uint32_t _foreground, uint32_t _background) : chance(_chance), foreground(_foreground), background(_background) {

  }
public:
  static pattern *Create(int _chance = 20, uint32_t _foreground = 0xffffffff, uint32_t _background = 0)
  {
    pattern *pat = new psparkle(_chance, _foreground, _background);
    display_state_now = SPARKLE_DISPLAY;
    return pat;
  }
  void tick()
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
