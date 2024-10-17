class pcircles : public pattern
{
private:
    int nCircles; // Number of circles to have
    int speed;    // How fast to move the circles
    int cycle;    // 0 to 2*(width*height-1) - where we're at with our 1st circle
    static const enum displayStates display_state = SPARKLE_DISPLAY;
    pcircles(int _nCircles, int _speed) : nCircles(_nCircles), speed(_speed), cycle(0)
    {
    }

public:
    static pattern *Create()
    {
        pattern *pat = new pcircles(3, 100000UL);
        display_state_now = display_state;
        return pat;
    }
    void tick()
    {
        nextFrame = micros() + speed; // Prevents TimeLights() from deciding we don't need to be run!
        ClearStrips();
        cycle++;
        int pixelCount = width * height;
        // e.g. let's say pixelCount is 8
        // cycle -> pick
        // 0 -> 0
        // 1 -> 1
        // 2 -> 2
        // 3 -> 3
        // 4 -> 4
        // 5 -> 5
        // 6 -> 6
        // 7 -> 7
        // 8 -> 6
        // 9 -> 5
        // 10 -> 4
        // 11 -> 3
        // 12 -> 2
        // 13 -> 1
        if (cycle >= 2 * (pixelCount - 1))
            cycle = 0;
        for (int i = 0; i < nCircles; i++)
        {
            // If there's one circle, it's at the position of cycle if cycle < pixelCount, or 2*(pixelCount-1)-cycle, if cycle>= pixelCount
            int pick = cycle + (i * 2 * (pixelCount-1)) / nCircles;
            pick = (pick % (2 * (pixelCount - 1)));
            if (pick >= pixelCount)
                pick = 2 * (pixelCount - 1) - pick;
            uint16_t hue = (uint16_t)(i * 65535 / nCircles);
            uint32_t col = Adafruit_NeoPixel::ColorHSV(hue, 255, brightness);
            uint32_t gam = Adafruit_NeoPixel::gamma32(col);
            setOffsetPixel(pick, gam);
        }
        ShowStrips();
    }
};
