#define IDL 1
#define LIQUID_ON 2
#define SENDING_FINISH 3

volatile int number_of_pulses = 0;
float vol = 0.0;
int stat = IDL;
unsigned char flowsensor = 2;  // Sensor Input
unsigned long currentTime, cloopTime;
int timer = 200, total_count = 10, switch_count = 4, count = 0;
int state[10];
int sensor_read_position = 0, status = 0, l_minute, milliliters_finish, number_of_milliliters;

int countPin(int *state) {
  int res = 0;
  for (int i = 0; i < total_count; i++) {
    if (state[i]) {
      res++;
    }
  }
  return res;
}

void setup() {
  pinMode(flowsensor, INPUT_PULLUP);
  Serial.begin(38400);

  currentTime = millis();
  cloopTime = currentTime;
}

void loop() {
  if (stat == LIQUID_ON) {
    int cur_state = digitalRead(flowsensor);
    state[sensor_read_position] = cur_state;
    sensor_read_position++;

    if (sensor_read_position >= total_count) {
      sensor_read_position = 0;
    }

    count = countPin(state);

    if (count > total_count - switch_count) {
      status = 1;
    } else {
      if (count < switch_count && status == 1) {
        number_of_pulses++;
        status = 0;
      }
    }
    currentTime = millis();
    if (milliliters_finish > vol) {
      if (currentTime >= (cloopTime + timer)) {
        cloopTime = currentTime;
        vol = number_of_pulses / 7.5 / 60.0 * 1000;
        Serial.println("P" + (String)(int)(vol) + ";");
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
      for (int i = 0; i < total_count; i++) {
        state[i] = 0;        
      }
      stat = LIQUID_ON;
      number_of_pulses = 0;
      status = 0;
      number_of_milliliters = str.toInt();
      milliliters_finish = number_of_milliliters / 1000;
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