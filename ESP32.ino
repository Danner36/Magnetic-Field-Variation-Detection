#include <Adafruit_HMC5883_U.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>

#define MAG_ADDRESS ((char) 0x1E)

Adafruit_HMC5883_Unified mag = Adafruit_HMC5883_Unified(12345);

sensors_event_t event;

uint8_t mag_buffer[6];
int16_t mag_raw[3];

boolean Start_Signal = false;
boolean Iter = false;
boolean Ready_To_Send = false;
boolean Completed_Cycle = false;
volatile boolean isDataReady = false;

struct data
{
  int Iteration_Amount = 0;
  float Delay_Amount = 0.0;
};
struct data user;

/**
 * Initailzation of: serial port, freeRTOS tasks, and HMC5883L.
 */
void setup()
{

  Serial.begin(115200);
  delay(250);

  Wire.begin();

  configMag();

  attachInterrupt(16, magDataReady, RISING);

  delay(250);

  xTaskCreatePinnedToCore(Task_Serial_Read, "Task_Serial_Read", 10000, NULL, 1, NULL, 0);

  xTaskCreatePinnedToCore(Task_Main, "Task_Main", 10000, NULL, 1, NULL, 1);
}

/**
 * Program should never reach this point. 
 */
void loop(){

  delay(1000);

}

/**
 * Interrupt function when mag DRDY pin is brought LOW
 */
void magDataReady() {
  isDataReady = true;
}

/**
 * Configures the magnetometer to use a higher Hz than teh default 15Hz.
 */ 
void configMag() {
  uint8_t mag_name;
 
  // make sure that the device is connected
  Wire.beginTransmission(MAG_ADDRESS);
  Wire.write((byte) 0x0A); // Identification Register A
  Wire.endTransmission();
 
  Wire.beginTransmission(MAG_ADDRESS);
  Wire.requestFrom(MAG_ADDRESS, 1);
  mag_name = Wire.read();
  Wire.endTransmission();
 
  if(mag_name != 0x48) {
    delay(1000);
  }
 
  // Register 0x00: CONFIG_A
  // normal measurement mode (0x00) and 75 Hz ODR (0x18)
  Wire.beginTransmission(MAG_ADDRESS);
  Wire.write((byte) 0x00);
  Wire.write((byte) 0x18);
  Wire.endTransmission();
  delay(5);
 
  // Register 0x01: CONFIG_B
  // default range of +/- 130 uT (0x20)
  Wire.beginTransmission(MAG_ADDRESS);
  Wire.write((byte) 0x01);
  Wire.write((byte) 0x20);
  Wire.endTransmission();
  delay(5);
 
  // Register 0x02: MODE
  // continuous measurement mode at configured ODR (0x00)
  // possible to achieve 160 Hz by using single measurement mode (0x01) and DRDY
  Wire.beginTransmission(MAG_ADDRESS);
  Wire.write((byte) 0x02);
  Wire.write((byte) 0x01);
  Wire.endTransmission();
 
  delay(200);
 
}

/**
 * Executes the main program.
 */
void Task_Main(void *parameter){

  while (1){

    if (Completed_Cycle){
      //Resets all global variables.
      Reset();
    }

    if (Ready_To_Send){
      //Sends selected data type for a certain number of iterations.
      Send_Data();
    }

    delay(1000);
  }
}

/**
 * Checks for serial input to be non-empty. Deciphers the message and assigns the message
 *    to its appropirate location or variable.
 */
void Task_Serial_Read(void *parameter){

  while (1){
    delay(10000);
    if (Serial.available() && !Ready_To_Send){

      //Reads in Start Signal from Jupyter.
      if (!Start_Signal){
        String text = Serial.readString();
        Start_Signal = true;
        Serial_Clear();

        //Signals to Jupyter to send Data_Type.
        Serial.print(1);
      }

      //Reads iteration amount from Jupyter.
      else if (user.Iteration_Amount == 0 || !Iter){
        user.Iteration_Amount = Serial.readString().toInt();
        Iter = true;
        Serial_Clear();
      }

      //If all signals have been recieved, begins data transmission.
      if (Start_Signal && Iter){
        Ready_To_Send = true;
        Serial_Clear();
      }

    }
    delay(100);
  }
}

/**
 * Reads 6 bytes (x,y,z magnetic field measurements) from the magnetometer
 */ 
void readMag() {
 
  // multibyte burst read of data registers (from 0x03 to 0x08)
  Wire.beginTransmission(MAG_ADDRESS);
  Wire.write((byte) 0x03); // the address of the first data byte
  Wire.endTransmission();
 
  Wire.beginTransmission(MAG_ADDRESS);
  Wire.requestFrom(MAG_ADDRESS, 6);  // Request 6 bytes
  int i = 0;
  while(Wire.available())
  {
    mag_buffer[i] = Wire.read();  // Read one byte
    i++;
  }
  Wire.read();
  Wire.endTransmission();
 
  // combine the raw data into full integers (HMC588L sends MSB first)
  //           ________ MSB _______   _____ LSB ____
  mag_raw[0] = (mag_buffer[0] << 8) | mag_buffer[1];
  mag_raw[1] = (mag_buffer[2] << 8) | mag_buffer[3];
  mag_raw[2] = (mag_buffer[4] << 8) | mag_buffer[5];
 
  // put the device back into single measurement mode
  Wire.beginTransmission(MAG_ADDRESS);
  Wire.write((byte) 0x02);
  Wire.write((byte) 0x01);
  Wire.endTransmission();
 
}

/**
 * Resets the program for next iteration.
 */
void Reset(){

  user.Iteration_Amount = 0;

  Iter = false;
  Start_Signal = false;
  Ready_To_Send = false;
  Completed_Cycle = false;
}

/**
 * Prints to the serial port the desired data type from the BNO055 IMU.
 */
void Send_Data(){

  //Loops until correct amount of data has been sent over the serial port.
  for (int i = 0; i < user.Iteration_Amount; i++){

    //Clears serial port of all unwanted or unneccessary information.
    Serial_Clear();

    if(isDataReady){
    
      isDataReady = false;

      readMag();

      //Sends data to jupyter in correct format.
      Serial.print(mag_raw[0], DEC);
      Serial.print(",");
      Serial.print(mag_raw[1], DEC);
      Serial.print(",");
      Serial.println(mag_raw[2], DEC);
    }

  }
  //Signals the program has completed.
  Completed_Cycle = true;
}

/**
 * Clears the serial port by reading in all available data until empty.
 */
void Serial_Clear(){

  while (Serial.available()){

    int junk = Serial.read();

  }
}