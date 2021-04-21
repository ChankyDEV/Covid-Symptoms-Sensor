#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <Wire.h>
#include <MAX30105.h>
#include <heartRate.h>
#include <ArduinoJson.h>


HTTPClient http;
const char* wifi_name = "LEON.PL_8214_2G";
const char* password = "27b482aa";
String serverName = "http://192.168.0.107:3000";

bool canSendData = false;
uint32_t measurementCount = 0;
constexpr size_t measurementSize = 4;

MAX30105 particleSensor;
double R = 0;
uint32_t treshold = 500;
uint32_t IR = 0;
uint32_t RED = 0;
uint32_t maxIR = 0;
uint32_t minIR = 0;
uint32_t maxRED = 0;
uint32_t minRED = 0;
uint32_t size = 1000;
constexpr size_t numberOfSamples = 1000;


double hrValues[measurementSize]{0};
double spo2Values[measurementSize]{0};
float temperatureValues[measurementSize]{0};

uint32_t heartRateValues[numberOfSamples]{0};
uint32_t iter = 0;
long lastBeat = 0;
float beatsPerMinute;
boolean configuring = true; 

void setup() {
  Serial.begin(115200);
  WiFi.begin(wifi_name,password);
  Serial.println("Connecting..."); 
  
    Serial.println("MAX30105 SATURATION SENSOR");
 
    while(particleSensor.begin(Wire, I2C_SPEED_FAST)==false) {
      Serial.println("Put finger");
    }
  
  
  byte ledBrightness = 70; 
  byte sampleAverage = 1; 
  byte ledMode = 2; 
  int sampleRate = 400; 
  int pulseWidth = 69; 
  int adcRange = 16384; 

  particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange); //Configure sensor with these settings

}

void setupMeasurements(){
      if(configuring==true && IR != 0 && RED != 0) {
        minIR = IR;
        maxIR = IR;
        minRED=RED;
        maxRED=RED;
        configuring = false;
      }
}

void addHeartRateValue(uint32_t irValue) {
  
if(checkForBeat(irValue)==true ){
        long delta = millis() - lastBeat;
        lastBeat = millis();
        beatsPerMinute = 60 / (delta / 1000.0);
        if(beatsPerMinute>20 && beatsPerMinute<255) {
          heartRateValues[iter] = beatsPerMinute;
        } 
        

      }
}

double getBloodSaturation() {
        double meanRED =  (maxIR+minIR)/2; 
        double meanIR = (maxRED+minRED)/2;
        double acRed = maxIR-minIR;
        double acIr = maxRED-minRED;
        R = (acRed/meanRED)/(acIr/meanIR);
        Serial.println(R);
        double SPO2 = 110 - 25*R;
        if(SPO2 > 100) {
          SPO2 = 100.0;
        }
        return SPO2;
}

double getHeartRate() {
          uint32_t hrBeatsCount = 0;
          uint32_t hrSum = 0;
          
          for(size_t i=0; i<size;i++) {
            if(heartRateValues[i]!=0){
              hrBeatsCount++;
              hrSum += heartRateValues[i];
            }

          }
          double pulse = (hrSum)/hrBeatsCount;
  return pulse;
}

void reset() {
    measurementCount +=1;
    iter = 0;
    maxRED = RED;
    minRED = RED;
    maxIR = IR;
    minIR = IR;
}

void setMaxAndMinValues(uint32_t IR,uint32_t RED) {
if(maxIR < IR) {
        maxIR = IR;
      }
     
      if(minIR > IR) {
        minIR = IR;
      }

      if(maxRED < RED) {
        maxRED = RED;
      }
      if(minRED > RED) {
        minRED = RED;
      }
}

void getSampleAndCheckSensor(){
  particleSensor.nextSample();
  particleSensor.check();
}

bool sensorCanSendData(){
  return particleSensor.available()>0;
}

void sendData(double bloodSaturation,double heartRate, float temperature) {
    Serial.println("");
    Serial.println("oooooooooooooooooooooooooo");
    Serial.println("Wynik końcowy: ");
    Serial.println("oooooooooooooooooooooooooo");
    Serial.print("SATURACJA: ");
    Serial.println(bloodSaturation);
    Serial.print("TETNO: ");
    Serial.println(heartRate);
    Serial.print("TEMPERATURA: ");
    Serial.println(temperature);

  if(canSendData) {
    DynamicJsonDocument doc(2048);
    doc["saturation"] = bloodSaturation;
    doc["pulse"] = heartRate;
    doc["temperature"] = temperature;
    String json;
    serializeJson(doc, json);
    http.begin("http://192.168.0.107:3000/addMeasurement");
    http.addHeader("Content-Type", "application/json");
    http.POST(json);
    Serial.print(http.getString());
    http.end();
    canSendData = false;
  }
}

void loop() {

  if(WiFi.isConnected()) {

    getSampleAndCheckSensor();
    if (sensorCanSendData()) {

      
      IR = particleSensor.getFIFOIR();
      RED = particleSensor.getFIFORed();
      
      uint32_t irValue = particleSensor.getIR();
      
      if(iter > treshold) {

        setupMeasurements();
        addHeartRateValue(irValue);
        
        if(iter==numberOfSamples) {

        Serial.print("MAX RED: ");
        Serial.println(maxRED);
        Serial.print("MIN RED: ");
        Serial.println(minRED);

        Serial.print("MAX IR: ");
        Serial.println(maxIR);
        Serial.print("MIN IR: ");
        Serial.println(minIR);

        double bloodSaturation = getBloodSaturation();
        double heartRate = getHeartRate();
        float temperature = particleSensor.readTemperature();
        Serial.print("Numer pomiaru: ");
        Serial.println(measurementCount);
        Serial.println("-------------");
        Serial.print("SATURACJA: ");
        Serial.println(bloodSaturation);
        Serial.print("TETNO: ");
        Serial.println(heartRate);
        Serial.print("TEMPERATURA: ");
        Serial.println(temperature);

        spo2Values[measurementCount] = bloodSaturation;
        hrValues[measurementCount] = heartRate;
        temperatureValues[measurementCount] = temperature;

        if (measurementCount == measurementSize)
        {
          canSendData = true;
          double spo2Sum =0;
          double hrSum =0;
          double tempSum =0;

          for(size_t i=0; i<measurementSize;i++) {
            spo2Sum += spo2Values[i];
            hrSum += hrValues[i];
            tempSum += temperatureValues[i];
          }          
          double spo2 = spo2Sum/measurementSize;
          double hr = hrSum/measurementSize;
          float temp = tempSum/measurementSize;

          float standard = 24.5;
          if(temp > standard) {
            float diffUpper = temp - standard;
            float cTempFactorUpper = (diffUpper*0.1);
            float tempUpper = 36.6+cTempFactorUpper;
            sendData(spo2, hr,tempUpper);
          } else {
            float diffUnder = standard - temp;
            float cTempFactorUnder = (diffUnder*0.1);
            float tempUnder = 36.6-cTempFactorUnder;
            sendData(spo2, hr,tempUnder);
          }


          measurementCount = 0;
        }
        
          
        

        
        reset();

      }  
      

      setMaxAndMinValues(IR,RED);
      }
     
      particleSensor.nextSample();   
      iter += 1;
  }
  
    Serial.flush();
  }

}
