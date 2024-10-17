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
    //tick();
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
    tickNow();
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
