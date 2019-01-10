# esk8Project
Credit to https://github.com/SolidGeek/VescUart for VESC-related code.

### Todos:
- ~Make sure than when startup: the throttle is ignored until it has returned to 0 (127-ish)~
- ~[FastLED](https://github.com/FastLED/FastLED) with task on Controller ([here](https://github.com/skelstar/esk8Project/blob/feature/m5stick-HUD/Investigations/m5stickHUD/ESPNow/m5stickHUD-EspNow/m5stickHUD-EspNow.ino)). Perhaps have second core run two task threads?~ (actually using Adafruit pixel library still)
- Get rpm from VESC and evaluate some 'isMoving' boolean on the Board which is returned to Controller
- ~Display battery voltage?~
  - maybe when not moving?
  - on startup?
- Get ampHours (are they current/live?)
- _Logging to uSD card_
- 'Trip meter' - maybe store ampHours "collected/consumed" on the ride at powerdown
- Store total amps used for odometer, and reset the total when battery voltage is much higher than last ride
  - perhaps prompt when Controller thinks time to reset odometer? [Yes] [No] [Ignore]


```
struct dataPackage {
  float avgMotorCurrent;
  float avgInputCurrent;
  float dutyCycleNow;
  long rpm;
  float inpVoltage;
  float ampHours;
  float ampHoursCharged;
  long tachometer;
  long tachometerAbs;
};
```
