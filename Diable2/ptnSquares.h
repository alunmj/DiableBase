class psquares : public pattern
{
private:
  int nStart;
  int nEnd;
  uint32_t myColor;
  static const enum displayStates display_state = CYCLE_DISPLAY;
  psquares() : nStart(-1), nEnd(-1), myColor(0xffffff) {}

public:
  static pattern *Create()
  {
    pattern *pat = new psquares();
    display_state_now = display_state;
    return pat;
  }
  void tick()
  {
    // Note: 35 minutes is 0x80 00 00 00 / 1,000,000 - so, clocking from signed to unsinged in micros()).
    unsigned long tnow = micros() - starttime;
    nextFrame = micros() + 500UL;      // Prevents TimeLights() from deciding we don't need to be run!
    unsigned long dura = tnow / 500UL; // 500 micros is 1 speed 'tick' for the cycle.

    if (Serial)
      brightness = 30; // If we're at the end of a cable, we need less brights.

    if (nStart < 0)
    {
        // Not in a square currently - randomly start one
        if (random(255)<10) {
            nStart = random(width * height);
            nEnd = random(width * height);
            if (nStart > nEnd) {
                // Swap the two values, so start < end
                nStart = nStart + nEnd;
                nEnd = nStart - nEnd;
                nStart = nStart - nEnd;
            }
            for (int i = nStart; i < nEnd; i++)
                setOffsetPixel(i, myColor);
        } else {
            ClearStrips();
        }
    } else {
        // In a square - randomly close it.
        if (random(255)<20) {
            // Light up the line. Then set nStart / nEnd to -1
            for (int i = nStart; i < nEnd; i++)
                setOffsetPixel(i, myColor);
            nStart = -1;
            nEnd = -1;
        } else {
            ClearStrips();
            setOffsetPixel(nStart, myColor);
            setOffsetPixel(nEnd-1, myColor);
        }
    }
    ShowStrips();

  }
};
