class PersistSetting
{
  File settingsFile;
  char const *_fname = "diableset.txt";

  char _unitName[16]="DiaBLEINI";
  char _fold;
  int _pin0;
  int _pin1;
  bool _dirty;
  byte _width;
  byte _height;
  byte _componentsValue;
  byte _stride;
  byte _is400Hz;

  void defaults()
  {
    strcpy(_unitName, "DiaBLEINI");
    _fold = 'S'; // Single - other values are 'F' for folded, 'W' for wings
    _pin0 = 10;
    _pin1 = 6;
    _width = 16;  // LED_COUNT does not need to be hard-set!
    _height = 1; // Effectively, number of sticks.
    _componentsValue = 0;
    _stride = 1;
    _is400Hz = 0;
  }

  int readInt(const char *buffer, int &jmdex, int readlen, bool skippost = true)
  {
    int rv = atoi(&buffer[jmdex]);
    // Serial.printf("Read integer %d from buffer at index %d", rv, jmdex);
    // Skip numeric characters
    for (; jmdex < readlen && ((buffer[jmdex] == '-') || isDigit(buffer[jmdex])); jmdex++)
      ;
    // Skip any non-numeric value - don't care!
    if (skippost)
    {
      for (; jmdex < readlen && !isDigit(buffer[jmdex]); jmdex++)
        ;
    }
    // Serial.printf(". New index %d\n", jmdex);
    
    return rv;
  }

public:
  PersistSetting() : settingsFile(InternalFS) { defaults(); }
  bool LoadFile()
  {
    int p0, p1;
    byte lheight, lwidth, lstride, lcomponentsValue, lis400Hz;
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
      Serial.printf("Contents: (%d) bytes\n%s\n", readlen, buffer);
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
        case 'S':
          jmdex = index;
          lwidth = readInt(buffer, jmdex, readlen);
          lheight = readInt(buffer, jmdex, readlen);
          lstride = readInt(buffer, jmdex, readlen);
          lcomponentsValue = readInt(buffer, jmdex, readlen);
          // is400Hz = 0;
          lis400Hz = readInt(buffer, jmdex, readlen, false);
          SetSize(lwidth, lheight, lstride, lcomponentsValue, lis400Hz);
          index = jmdex;
          Serial.println("Read in the 'S' parts");
          break;
        case 'q':
          while (buffer[index] != '\n')
            index++;
          Serial.println("Skipped the 'q' parts");
          break;
        case 'L':
          jmdex = index;
          // TODO: AMJ: this isn't robust against malformed lines.
          // Next numeric value is pin 0.
          p0 = readInt(buffer, jmdex, readlen);
          // Next numeric value is pin 1.
          p1 = readInt(buffer, jmdex, readlen, false);
          // Next character is fold status - W for wings, F for folded
          foldState = buffer[jmdex++];
          if (p0 != 0)
            SetPin0(p0);
          if (p1 != 0)
            SetPin1(p1);
          if (foldState == 'W' || foldState == 'F' || foldState == 'S')
            SetFold(foldState);
          index = jmdex;
          Serial.println("Read in the 'L' parts");
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
          Serial.println("Read in the 'T' parts");
          Serial.printf("Unit name is %s from Load.\n",_unitName);
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
      Serial.printf("Unit name is %s from Write.\n",_unitName);
      // Fold state and pin numbers
      char buffer[15];
      settingsFile.write("L");
      settingsFile.write(itoa(_pin0, buffer, 10));
      settingsFile.write(",");
      settingsFile.write(itoa(_pin1, buffer, 10));
      settingsFile.write(&_fold, 1);
      settingsFile.write("\n");

      // Size
      settingsFile.write("S");
      settingsFile.write(itoa(_width, buffer, 10));
      settingsFile.write(",");
      settingsFile.write(itoa(_height, buffer, 10));
      settingsFile.write(",");
      settingsFile.write(itoa(_stride, buffer, 10));
      settingsFile.write(",");
      settingsFile.write(itoa(_componentsValue, buffer, 10));
      settingsFile.write(",");
      settingsFile.write(_is400Hz ? "1" : "0");
      settingsFile.write("\n");

      settingsFile.truncate(settingsFile.position());

      settingsFile.flush();

      settingsFile.close();
      Serial.println("Finished writing - let's load it back and check!");
      LoadFile();
      _dirty = false;
      return true;
    }
    return false;
  }

  char const *GetName() { return _unitName; }
  bool SetName(const char *newName)
  {
    _dirty = true;
    strncpy(_unitName, newName, sizeof _unitName - 1);
    //Serial.printf("Unit name is %s from Set.\n",_unitName);
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
  bool SetSize(byte iwidth, byte iheight, byte istride, byte icomponentsValue, byte iis400Hz)
  {
    _dirty = true;
    _width = iwidth;
    _height = iheight;
    _stride = istride;
    _componentsValue = icomponentsValue;
    _is400Hz = iis400Hz;
    return true;
  }

  int GetWidth() { return (int)_width; }
  int GetHeight() { return (int)_height; }

};
