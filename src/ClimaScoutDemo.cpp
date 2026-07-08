#include <IRremote.h>
#include <Servo.h>

// Inputs
const int tmpSensorPin = A0;
const int trigPin = 2;
const int echoPin = 3;
const int bumpLeftPin  = 5;
const int bumpRightPin = 4;
const int irReceiverPin = 6;   

// Outputs
const int servoPin = 7;
const int ledBlue = 9;   // Reservoir LED
const int ledGreen = 10;  //  Battery LED
// FIXME: Add ledRed post-prototype

// Motor driver #1 (wheel pairs)
const int leftDriveMotorsPin1 = 11;
const int leftDriveMotorsPin2 = 12;
const int rightDriveMotorsPin1 = 8;
const int rightDriveMotorsPin2 = A1;

// Motor driver #2 (vertical + fan)
const int climbingMotorsPin1 = A2;
const int climbingMotorsPin2 = A3;
const int fanPin1 = A4;
const int fanPin2 = A5;

Servo clampServo;
bool patrolRunComplete = false;

// Power / Resource Status
int batteryCharge = 100;              // hardcoded full for demo (%)
bool batterySufficient = false;
int mistReservoirLevels = 100;        // hardcoded full for demo (%)
bool reservoirSufficient = false;

// Startup / Mode Selection
bool preconfiguredAreaSet = true;     // user already selected a mapped patrol area
bool patrolModeSelected = false;

// Temperature / Environment
bool thermostatConnected = true;      // hardcoded for demo
bool optimalTempMet = false;          // set after comparing assessed vs optimal temp
float thermostatTemp = 23.33;         // measured area temp (C)
float optimalAreaTemp = 23.33;        // target area temp (C)
float chargingDockTemp = 0.0;         // local plant-level sampled temp (C)
float sampledTempC = 0.0;

// Current Target Station Metadata -> HARDCODED FOR DEMO PURPOSES
const char* plantStationID = "STATION_A1";
unsigned long rfidTag = 0xEA15BF00;   // Hardcoded IR proxy for RFID tag
const char* plantSpecies = "Orchid";  // Hardcoded for demo purposes 
float stationIdealTempC = 22;       // ideal plant temp (C) -> harcoded for demo purposes
int canopyHeightInches = 18;          // harcoded for demo purposes

// Routing / Progress Tracking
int targetStationCount = 1;           // set after building priority list
int attemptCount = 0;
int distanceToNextStation = 650;
int climbHeight = 0;
int maxAttempts = 3;


// Functions

void LEDSteady(int ledPin) {
  digitalWrite(ledPin, HIGH);
}

void LEDBlink(int ledPin) {
	//FIXME: PLACEHOLDER blank for demo purposes;
}

void goForward() {
  digitalWrite(leftDriveMotorsPin1, HIGH);
  digitalWrite(leftDriveMotorsPin2, LOW);
  digitalWrite(rightDriveMotorsPin1, HIGH);
  digitalWrite(rightDriveMotorsPin2, LOW);
}

void stopHorizontalDriveMotors() {
  digitalWrite(leftDriveMotorsPin1, LOW);
  digitalWrite(leftDriveMotorsPin2, LOW);
  digitalWrite(rightDriveMotorsPin1, LOW);
  digitalWrite(rightDriveMotorsPin2, LOW);
}

void goReverse() {
  digitalWrite(leftDriveMotorsPin1, LOW);
  digitalWrite(leftDriveMotorsPin2, HIGH);
  digitalWrite(rightDriveMotorsPin1, LOW);
  digitalWrite(rightDriveMotorsPin2, HIGH);
}

void turnLeft() {
  digitalWrite(leftDriveMotorsPin1, LOW);
  digitalWrite(leftDriveMotorsPin2, LOW);
  digitalWrite(rightDriveMotorsPin1, HIGH);
  digitalWrite(rightDriveMotorsPin2, LOW);
}

void pivot180() {
  // Pivot in place: Left wheels forward, Right wheels reverse
  digitalWrite(leftDriveMotorsPin1, HIGH);
  digitalWrite(leftDriveMotorsPin2, LOW);
  digitalWrite(rightDriveMotorsPin1, LOW);
  digitalWrite(rightDriveMotorsPin2, HIGH);
  delay(4000);   
  stopHorizontalDriveMotors();
}

void openClamp() {
  clampServo.write(90);
  delay(1000);
}

float readDistanceInches() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  float time = pulseIn(echoPin, HIGH);  // microseconds
  // Convert to inches
  float distInches = time / 148.1 * 1.5; //FIXME: Multiplying by 1.5 due to intereference in circuit bringing readings down 
  return distInches;
}

void climbUp() {
  // Vertical drive ON (up direction)
  digitalWrite(climbingMotorsPin1, HIGH);
  digitalWrite(climbingMotorsPin2, LOW);
}

void stopVerticalMotor() {
  digitalWrite(climbingMotorsPin1, LOW);
  digitalWrite(climbingMotorsPin2, LOW);
}

float readLocalTempC() {
  int raw = analogRead(tmpSensorPin);
  float voltage = raw * (5.0 / 1023.0);
  float tempC = (voltage - 0.5) * 100.0;
  return tempC;
}

void fanOn() {
  digitalWrite(fanPin1, HIGH);
  digitalWrite(fanPin2, LOW);
}

void fanOff() {
  digitalWrite(fanPin1, LOW);
  digitalWrite(fanPin2, LOW);
}

void mistOn() {
  // Mist is simulated in TinkerCAD (no real mister component)
  Serial.println(F("Mist system: FULL POWER ON"));
}

void mistOff() {
  Serial.println(F("Mist system: OFF (simulated)"));
}

void climbDown() {
  // Vertical motor ON (reverse/down direction)
  digitalWrite(climbingMotorsPin1, LOW);
  digitalWrite(climbingMotorsPin2, HIGH);
}

void closeClamp() {
  clampServo.write(0);   // closed / locked position
  delay(800);
}

void ledOff(int ledPin) {
  digitalWrite(ledPin, LOW);
}

// Setup
void setup() {
  Serial.begin(9600);
  
  clampServo.attach(servoPin);
  IrReceiver.begin(irReceiverPin, ENABLE_LED_FEEDBACK);
  
  pinMode(ledGreen, OUTPUT);
  pinMode(ledBlue, OUTPUT);
  pinMode(leftDriveMotorsPin1, OUTPUT);
  pinMode(leftDriveMotorsPin2, OUTPUT);
  pinMode(rightDriveMotorsPin1, OUTPUT);
  pinMode(rightDriveMotorsPin2, OUTPUT);
  pinMode(trigPin, OUTPUT);
  pinMode(climbingMotorsPin1, OUTPUT);
  pinMode(climbingMotorsPin2, OUTPUT);
  pinMode(fanPin1, OUTPUT);
  pinMode(fanPin2, OUTPUT);
  
  pinMode(bumpRightPin, INPUT_PULLUP);
  pinMode(echoPin, INPUT);
  pinMode(tmpSensorPin, INPUT);
}

// Loop
void loop() {
  if (patrolRunComplete) return;
  patrolRunComplete = true;

  // one-time reset
  clampServo.write(0);
  digitalWrite(ledGreen, LOW);
  digitalWrite(ledBlue, LOW);
  digitalWrite(leftDriveMotorsPin1, LOW);
  digitalWrite(leftDriveMotorsPin2, LOW);
  digitalWrite(rightDriveMotorsPin1, LOW);
  digitalWrite(rightDriveMotorsPin2, LOW);
  digitalWrite(climbingMotorsPin1, LOW);
  digitalWrite(climbingMotorsPin2, LOW);
  
  //ClimaScoutPowerOn
  Serial.println(F("POWER: ON\n"));
  delay(200);
  delay(200);

  // batteryCheck
  if (batteryCharge >= 25) {
    LEDSteady(ledGreen);
    Serial.print(F("SUFFICIENT BATTERY DETECTED: "));
    Serial.print(batteryCharge);
    Serial.println(F("%"));
  }else {
    LEDBlink(ledGreen);
    Serial.print(F("INSUFFICIENT BATTERY DETECTED: "));
    Serial.print(batteryCharge);
    Serial.println(F("%"));
  }
  
  delay(1000);
  
  // mistReservoirCheck
  if (mistReservoirLevels >= 25) {
    LEDSteady(ledBlue);
    Serial.print(F("SUFFICIENT WATER LEVELS IN MIST RESERVOIR: "));
    Serial.print(mistReservoirLevels);
    Serial.println(F("%"));
    
  }else {
    LEDBlink(ledBlue);
    Serial.print(F("INSUFFICIENT WATER LEVELS IN MIST RESERVOIR: "));
    Serial.print(mistReservoirLevels);
    Serial.println(F("%"));
  }
  
  delay(1000);
  
  // Selecting Run Mode: Mapping or Patrol?
  if (preconfiguredAreaSet == true) {
    Serial.println(F("Area is pre-configured. INITIALIZING PATROL MODE."));
    Serial.println(F("The Run Mode LED lights up continuously."));
  } else {
    Serial.println(F("AREA CONFIGURATION NOT FOUND. INITIALIZING MAPPING MODE."));
    Serial.println(F("The Run Mode LED blinks rapidly."));
  }
  
  
  // Measuring overall area temperature
  boolean patrolMode;
  float measuredValue;
  if (thermostatConnected == true) {
    measuredValue = thermostatTemp;
    Serial.print("The connected device measures an overall room temperature of: ");
    Serial.print(measuredValue);
    Serial.println(" (C)");
  }else {
    measuredValue = chargingDockTemp;
    Serial.print("UNABLE TO FIND AN ACTIVE DEVICE. Local sensors read a temperature of: ");
    Serial.print(measuredValue); 
    Serial.print(" (C)");
  }

  if (optimalAreaTemp == measuredValue) {
    Serial.println(F("Measured value is within range of optimal parameters."));
    
    Serial.println(F("Retrieving all mapped plant stations"));
    Serial.print(targetStationCount);
    Serial.println(F(" plant station(s) identified and located"));
    Serial.println(rfidTag, HEX);
    
    Serial.println(F("Compiling Proximity-based routing list"));
    Serial.print(F("First target station: "));
    Serial.println(plantStationID);
    
    patrolMode = true;
  }else {
      Serial.println(F("INITIATING ALERT MODE"));
  }
  
  // Powering ON Horizontal Drive Motors + IMUs
  if (patrolMode == true) {
    goForward();
  }
  
  // Approaching Next Station On Priority List
  while (targetStationCount > 0) {
    while (digitalRead(bumpRightPin) != HIGH) {
      Serial.print(F("approaching "));
      Serial.println(plantStationID);
      Serial.println();

      Serial.print(F("Distance: "));
      Serial.print(distanceToNextStation);
      Serial.println(F(" inches"));

      goForward();
      delay(500);
      distanceToNextStation = distanceToNextStation - 5;
    }
    
    // Bumper triggered -> obstacle avoidance
    Serial.println(F("Obstacle detected - executing avoidance turn")); 
    stopHorizontalDriveMotors();
    delay(2000);
    goReverse();
    delay(2000);
    turnLeft();
    delay(2000);
    goForward();
    
    // After avoidance, simulate continued approach by decrementing distance
    while (distanceToNextStation > 50) {
      distanceToNextStation -= 5;
      Serial.print(F("Distance: "));
      Serial.print(distanceToNextStation);
      Serial.println(F(" inches"));
    }
    
    // Reached near station
    stopHorizontalDriveMotors();
    Serial.print(F("approaching "));
    Serial.println(plantStationID);
    Serial.println(F("Activating RFID Scan"));
    
    // RFID Scan 
    Serial.print(F("Waiting for station tag: 0x"));
    Serial.println(rfidTag, HEX);

    while (true) {
      if (IrReceiver.decode()) {
        unsigned long scannedCode = IrReceiver.decodedIRData.decodedRawData;

        Serial.print(F("IR code (HEX): 0x"));
        Serial.println(scannedCode, HEX);

        if (scannedCode == rfidTag) {
          Serial.println(F("Station ID identified and processed."));
          Serial.println(F("Loading metadata..."));
          Serial.print(F("Plant species: "));
          Serial.println(plantSpecies);
          Serial.print(F("Ideal Temperature (C): "));
          Serial.println(stationIdealTempC);
          Serial.print(F("Canopy Height (in): "));
          Serial.println(canopyHeightInches);

          IrReceiver.resume();
          break;  // stop waiting after successful scan
        } else {
          Serial.println(F("Unknown station tag. Try again."));
          IrReceiver.resume();
        }
      }
    }
    
    pivot180();
    
    delay(1000);
    
    openClamp();
    
    // Align to station pole until within 5 inches
    Serial.println(F("Aligning to Station ID Pole"));

    float distanceToStationPole = -1.0;

    // Start slow reverse motion while aligning
    goReverse();

    while (true) {
      distanceToStationPole = readDistanceInches();

      Serial.print(F("Distance to Station Pole: "));
      Serial.print(distanceToStationPole);
      Serial.println(F(" inches"));

      // Stop when pole is close enough
      if (distanceToStationPole < 5.0) {
        stopHorizontalDriveMotors();
        delay(1000);
        closeClamp();
        break;
      }
    }
    delay(1000);
    
    // Climb vertically
    while (climbHeight < canopyHeightInches) {
      climbUp();
      climbHeight = climbHeight + 1;
      Serial.print("Height (inches): ");
      Serial.println(climbHeight);
      delay(200);
    }
    stopVerticalMotor();
    Serial.print(canopyHeightInches);
    Serial.println(F(" inches reached")); 
    
    // sample Temperature
    sampledTempC = readLocalTempC();
    Serial.print(F("Recorded Temperature (C): "));
    Serial.println(sampledTempC);

    Serial.println(F("Comparing sample to optimal temperature"));

    while (sampledTempC - stationIdealTempC > 1.0) {
      ++attemptCount;
      sampledTempC = readLocalTempC();
      Serial.print(F("Recorded Temperature (C): "));
      Serial.println(sampledTempC);
      if (attemptCount <= maxAttempts) {
        Serial.println(F("Sampled Temperature is higher than optimal temperature"));
        delay(1000);
        Serial.print(F("Sampled Temp (C): "));
        Serial.println(sampledTempC);
        Serial.print(F("Optimal Temp (C): "));
        Serial.println(stationIdealTempC);
        delay(1000);
        Serial.print(F("Initiating stabilization attempt # "));
        Serial.println(attemptCount);
        delay(1000);
           
        // Cooling System Powering ON
        mistOn();
        fanOn();
        delay(8000);
      }else {
        // Log as Unsatisfactory
        Serial.print("LOGGING ");
        Serial.print(plantStationID);
        Serial.println(" AS UNRESOLVED. NOTIFYING USER");
        --targetStationCount;
        break;
      }
    }
    
    // log as Satisfactory
    Serial.println(F("Sample is within optimal temperature range."));
    Serial.print(F("Station ID "));
    Serial.print(plantStationID);
    Serial.println(F(" logged as SATISFACTORY."));
    Serial.println(F("Updating system records and patrol log..."));
    --targetStationCount;
    
    // Shutoff cooling system
    fanOff();
    mistOff();
    
    //Climb back DOWN
    while (climbHeight > 0) {
      climbDown();
      climbHeight = climbHeight - 1;
      Serial.print("Height (inches): ");
      Serial.println(climbHeight);
      delay(200);
    }
    stopVerticalMotor();
    Serial.println(F("Ground height reached"));
    
    openClamp();
    
    goForward();
    
    while (true) {
      float distanceToStationPole = readDistanceInches();

      Serial.print(F("Distance from Station Pole: "));
      Serial.print(distanceToStationPole);
      Serial.println(F(" inches"));

      // Stop when pole is close enough
      if (distanceToStationPole > 15.0) {
        stopHorizontalDriveMotors();
        Serial.println(F("Sufficient distance reached. Closing clamp mechanism."));
        closeClamp();
        break;
      }
      delay(1000);
    }
  }
  Serial.println(F("Patrol run: COMPLETE"));
  delay(1000);
  Serial.println(F("Returning to docking station"));
  delay(1000);
  Serial.println(F("Docking station connected."));
  delay(1000);
  Serial.println(F("Powering OFF motors and sensors"));
  delay(1000);
  stopHorizontalDriveMotors();
  ledOff(ledGreen);
  delay(200);
  ledOff(ledBlue);
  delay(200);
  Serial.println(F("The Run Mode is powered OFF"));
  delay(200);
  Serial.println(F("POWER: OFF\n"));
}
