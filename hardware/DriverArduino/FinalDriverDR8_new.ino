#define IDL 1
#define LIQUID_ON 2
#define SENDING_FINISH 3

volatile int number_of_pulses = 0;  // with this variable, we will count the pulses from the water flow sensor
float vol = 0.0, l_minute, milliliters_from_pulses, number_of_milliliters;
int stat = IDL;
unsigned char flowsensor = 2;  // Sensor Input
unsigned long currentTime, cloopTime;
int timer = 200;

void flow()  // interrupt handling function
{
  number_of_pulses++;
}

void setup() {
  pinMode(flowsensor, INPUT_PULLUP);
  Serial.begin(38400);

  attachInterrupt(digitalPinToInterrupt(flowsensor), flow, FALLING);  // Setup Interrupt
  currentTime = millis();
  cloopTime = currentTime;
}

void loop() {
  if (stat == LIQUID_ON) {
    currentTime = millis();
    if (milliliters_from_pulses > number_of_pulses) {
      if (currentTime >= (cloopTime + timer)) {
        cloopTime = currentTime;
        Serial.println("P" + (String)(int)(number_of_pulses / 40.0 * 1000) + ";");
      }
    } else {
      stat = SENDING_FINISH;
    }
  }
  if (stat == SENDING_FINISH) {
    Serial.println("F" + (String)(int)number_of_milliliters + ";");
    stat = IDL;
  }
  if (Serial.available() > 0) {
    String str = Serial.readString();
    Serial.setTimeout(5);
    if (str[0] == 'S') {
      Serial.println("SOK;");
      str.remove(0, 1);
      stat = LIQUID_ON;
      number_of_pulses = 0;
      number_of_milliliters = (float)str.toInt();
      milliliters_from_pulses = ceil(number_of_milliliters / 40);  // count the volume ip pulses
    } else {
      if (str == "UID;") {
        Serial.print("YF-S201;");
      }
      if (str == "PING;") {
        Serial.println("OK-PING;");
      }
      if (str == "ERR;") {
        stat = IDL;
        Serial.println("FOK;");
      }
      if (str == "FOK;") {
        stat = IDL;
      }
    }
    str = "";
    Serial.flush();
  }
}
