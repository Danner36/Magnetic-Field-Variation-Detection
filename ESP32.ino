#include <Wire.h>

//Address of the HMC5883L.
#define MAG_ADDRESS ((char) 0x1E)
//Buffer array used to store bytes from the HMC5883L.
uint8_t mag_buffer[6];
//Finalized array that holds joined bytes from mag_buffer[]
int16_t mag_raw[3];

//Used to indicate that Jupyter/user is ready.
boolean Start_Signal = false;
//Status of transmission of Iteration_Amount. True when recieved, false otherwise.
boolean Iter = false;
//True when all needed information is recieved. (Start_Sginal & Iter)
boolean Ready_To_Send = false;
//Status of program state. True causes program to be idle, false triggers the transmission of data.
boolean Completed_Cycle = false;

//Struct used to hold values used throughout program.
struct data
{
  //Desired amount of data to be sent from the HMC5883L to Jupyter over the serial port.
  int Iteration_Amount = 0;
  //Used to track the amount of data being send. (Goes from 0 to Iteration_Amount)
  int this_iter = 0;
};
struct data user;

/**
 * Initailzation of: serial port, freeRTOS tasks, and HMC5883L.
 */
void setup()
{

  Serial.begin(115200);

  Wire.begin();

  configMag();

  xTaskCreatePinnedToCore(Task_Serial_Read, "Task_Serial_Read", 10000, NULL, 1, NULL, 0);

  xTaskCreatePinnedToCore(Task_Main, "Task_Main", 10000, NULL, 1, NULL, 1);
}

/**
 * Program should never reach this point. 
 */
void loop(){

}

/**
 * Initalizes the HMC5883L by checking for a physical connection, setting the desired mode, and sensitivity range.
 */
void configMag() {
  uint8_t mag_name;
 
  // Checks for connection of HMC5883L.
  Wire.beginTransmission(MAG_ADDRESS);
  Wire.write((byte) 0x0A); // Identification Register A
  Wire.endTransmission();
 
  Wire.beginTransmission(MAG_ADDRESS);
  Wire.requestFrom(MAG_ADDRESS, 1);
  mag_name = Wire.read();
  Wire.endTransmission();
 
  // Register 0x00: CONFIG_A
  // Normal measurement mode (0x00) and 75 Hz ODR (0x18)
  Wire.beginTransmission(MAG_ADDRESS);
  Wire.write((byte) 0x00);
  Wire.write((byte) 0x18);
  Wire.endTransmission();
  delay(5);
 
  // Register 0x01: CONFIG_B
  // Default range of +/- 130 uT (0x20)
  Wire.beginTransmission(MAG_ADDRESS);
  Wire.write((byte) 0x01);
  Wire.write((byte) 0x20);
  Wire.endTransmission();
  delay(5);
 
  // Register 0x02: MODE
  // Continuous measurement mode at configured ODR (0x00)
  // Achieves 160 Hz by using single measurement mode (0x01) and DRDY
  Wire.beginTransmission(MAG_ADDRESS);
  Wire.write((byte) 0x02);
  Wire.write((byte) 0x01);
  Wire.endTransmission();
 
  delay(5);
 
}

/**
 * Task_Main is repsonsible for handling the process of the program.
 *   Either promts the transmission of data to Jupyter, or resets all values in preperation of next cycle.
 */
void Task_Main(void *parameter){

  while (1){
    
    if (Completed_Cycle){
      //Resets all global variables.
      Reset();
    }

    //Ieration_Amount and Start_Sginal were recieved.
    if (Ready_To_Send){

      //Iterates up to desired data amount.
      while(user.this_iter<user.Iteration_Amount){
        
        //Sends x,y,z values in csv format.
        Send_Data();
        //Delays 6.25 milliseconds. Forces 160Hz DOR.
        delay(6.25);
      }

      //Signals the program has completed.
      Completed_Cycle = true;
    }

    //Sends task to idle for 1 second to satisfy watchdog timer.
    delay(10);
  }
}

/**
 * Checks for serial input to be non-empty. Deciphers the message and assigns the message
 *    to its appropirate location or variable.
 */
void Task_Serial_Read(void *parameter){

  while (1){

    //Serial port must be non-empty and ESP32 isn't currently sending information.
    if (Serial.available() && !Ready_To_Send){

      //Reads in Start Signal from Jupyter.
      if (!Start_Signal){

        String text = Serial.readString();
        if(text == "START"){

          Start_Signal = true;
          //Clears serial port of all unwanted or unneccessary information.
          Serial_Clear();

          //Signals to Jupyter to send Data_Type.
          Serial.print(1);

        }
        //Clears serial port of all unwanted or unneccessary information.
        Serial_Clear();

      }

      //Reads iteration amount from Jupyter.
      else if (user.Iteration_Amount == 0 || !Iter){
        //Reads & convers to integer.
        user.Iteration_Amount = Serial.readString().toInt();
        Iter = true;
        //Clears serial port of all unwanted or unneccessary information.
        Serial_Clear();
      }

      //If all signals have been recieved, begins data transmission.
      if (Start_Signal && Iter){
        Ready_To_Send = true;
        //Clears serial port of all unwanted or unneccessary information.
        Serial_Clear();
      }

    }
    //Sends task to idle for 1/10th second to satisfy watchdog timer.
    delay(10);
  }
}

// Reads in 6 bytes (2 for each x,y,z) from the HMC5883L and merges them together.
void readMag() {
 
  //Onboard the HMC5883L data is kept in registers 0x03 through 0x08. 
  //   This forces the HMC5883L to hand over the data on these registers.
  Wire.beginTransmission(MAG_ADDRESS);
  Wire.write((byte) 0x03); 
  Wire.endTransmission();
 
  //Requests 6 bytes from HMC5883L.
  Wire.beginTransmission(MAG_ADDRESS);
  Wire.requestFrom(MAG_ADDRESS, 6);  
  int i = 0;
  while(Wire.available())
  {
    mag_buffer[i] = Wire.read();
    i++;
  }
  Wire.read();
  Wire.endTransmission();
 
  //Combines the bytes into full integers (HMC588L sends MSB first)
  //           ________ MSB _______   _____ LSB ____
  mag_raw[0] = (mag_buffer[0] << 8) | mag_buffer[1];
  mag_raw[1] = (mag_buffer[2] << 8) | mag_buffer[3];
  mag_raw[2] = (mag_buffer[4] << 8) | mag_buffer[5];
 
  //Places device into Single Measurement Mode to refill the registers after short idling period of 250 microSeconds.
  Wire.beginTransmission(MAG_ADDRESS);
  Wire.write((byte) 0x02);
  Wire.write((byte) 0x01);
  Wire.endTransmission();
 
}

/**
 * Resets the program's varaibles for next iteration.
 */
void Reset(){

  user.Iteration_Amount = 0;
  user.this_iter = 0;
  Iter = false;
  Start_Signal = false;
  Ready_To_Send = false;
  Completed_Cycle = false;
}

/**
 * Prints the data brought in from the HMC5883L to the serial port.
 */
void Send_Data(){

  //Clears serial port of all unwanted or unneccessary information.
  Serial_Clear();

  //Reads in current magnetometer information.
  readMag();

  //Sends data to jupyter in CSV format.
  Serial.print(mag_raw[0], DEC); Serial.print(",");
  Serial.print(mag_raw[1], DEC); Serial.print(",");
  Serial.print(mag_raw[2], DEC); Serial.println();

  //Increments iterated by 1.
  user.this_iter += 1;
}

/**
 * Clears the serial port by reading in all available data until empty.
 */
void Serial_Clear(){

  while (Serial.available()){

    int junk = Serial.read();

  }
}