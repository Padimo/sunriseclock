struct Devices {
  bool ringLamp;
  bool wallLeft;
  bool wallRight;
  bool wordClock;
  bool yNetwork;
};

struct ColorScheme {
  byte hue;
  byte variation;
  byte saturation;
  int brightness;
};

struct Keytimes {
  int bed;
  int leave;
  int ret;
  int wake;
  int winddown;
};

struct Roomdata {
  Devices devices;
  ColorScheme colorscheme;
  Keytimes keytimes;
  int ringlamp;
  int brightness;
};

struct LinearAnimation {
  int startHue;
  int startSat;
  int startVib;
  int endHue;
  int endSat;
  int endVib;
  int animationtime;
  bool indicator;
};