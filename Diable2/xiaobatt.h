#include <Arduino.h>
#include <bluefruit.h>

#define BAT_CHARGE_STATE 23 // LOW for charging, HIGH not charging

class XiaoBattery {
  #define BATTERY_STATES_COUNT 11
// Voltage levels in millivolts and corresponding percentages for a typical LiPo battery.
// Adjust these values based on your battery's datasheet for better accuracy.
int x[2] = {100,100};
  int battery_states[BATTERY_STATES_COUNT][2] = {
    {4200, 100}, // Fully charged
    {4110, 90},
    {4020, 80},
    {3930, 70},
    {3840, 60},
    {3750, 50},
    {3660, 40},
    {3570, 30},
    {3480, 20},
    {3390, 10},
    {3300, 0} // Minimum safe voltage
};
public:
  static void setup();
  XiaoBattery();
  float GetBatteryVoltage();
  int GetBatteryPercentage();
  bool IsChargingBattery();
};

void XiaoBattery::setup() {
  pinMode(PIN_VBAT, INPUT);
  pinMode(VBAT_ENABLE, OUTPUT);
  pinMode(PIN_CHARGING_CURRENT, OUTPUT);
  pinMode(BAT_CHARGE_STATE, INPUT);

  // digitalWrite(VBAT_ENABLE, LOW); // VBAT Read enable - doing this disables the IMU readability
  digitalWrite(PIN_CHARGING_CURRENT, LOW); // low - 100mA charging; high - 50mA charging.
}

XiaoBattery::XiaoBattery() {
  pinMode(VBAT_ENABLE, OUTPUT);
  pinMode(BAT_CHARGE_STATE, INPUT);
  pinMode(PIN_CHARGING_CURRENT, OUTPUT);

  digitalWrite(PIN_CHARGING_CURRENT, LOW); // charge with 100mA
}

#define VBAT_MV_PER_LBS (0.003395996F)

float XiaoBattery::GetBatteryVoltage() {
  digitalWrite(VBAT_ENABLE, LOW); // enable BAT reading through the ADC

  uint32_t adcCount = analogRead(PIN_VBAT);
  float adcVoltage = adcCount * VBAT_MV_PER_LBS;
  float vBat = adcVoltage * (1510.0 / 510.0);

  digitalWrite(VBAT_ENABLE, HIGH); // Re-enable IMU reading through the ADC

  return vBat;
}

int XiaoBattery::GetBatteryPercentage() {
  float volts = GetBatteryVoltage();
  int mv = (int)(volts * 1000.0);
  int last_state = 0;
  int last_pc = 0;
  int this_pc = 0;
  int this_state = 0;
  int frac = 0;
  for (int i=0; i<BATTERY_STATES_COUNT;i++) {
    this_state = battery_states[i][0];
    this_pc = battery_states[i][1];
    if (mv > this_state) {
      if (i>0) {
        frac = (mv-this_state) * 10 / (last_state - this_state);
        return this_pc + (frac * (last_pc-this_pc))/10;
      } else {
        return this_pc;
      }
    }
    last_state = this_state;
    last_pc = this_pc;
  }
  return 0;
}

bool XiaoBattery::IsChargingBattery() { return digitalRead(BAT_CHARGE_STATE) == LOW; }