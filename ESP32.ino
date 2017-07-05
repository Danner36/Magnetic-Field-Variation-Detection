#include <Wire.h>

#define MAG_ADDRESS ((char) 0x1E)
uint8_t mag_buffer[6];
int16_t mag_raw[3];

boolean Start_Signal = false;
boolean Iter = false;
boolean Ready_To_Send = false;
boolean Completed_Cycle = false;

struct data
{
  int Iteration_Amount = 0;
  int this_iter = 0;
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

  Wire.beginTransmission(MAG_ADDRESS);
  Wire.write((byte) 0x02);
  Wire.write((byte) 0x01);
  Wire.endTransmission();
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
      while(user.this_iter<user.Iteration_Amount){

        attachInterrupt(digitalPinToInterrupt(16), Send_Data, RISING);

      }

      detachInterrupt(digitalPinToInterrupt(16));
      //Signals the program has completed.
      Completed_Cycle = true;

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

    if (Serial.available() && !Ready_To_Send){

      //Reads in Start Signal from Jupyter.
      if (!Start_Signal){

        String text = Serial.readString();

        if(text == "START"){

          Start_Signal = true;
          Serial_Clear();
          //Signals to Jupyter to send Data_Type.
          Serial.print(1);

        }

        Serial_Clear();
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

  //Clears serial port of all unwanted or unneccessary information.
  Serial_Clear();

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

  //Sends data to jupyter in correct format.
  Serial.print(mag_raw[0], DEC); Serial.print(",");
  Serial.print(mag_raw[1], DEC); Serial.print(",");
  Serial.print(mag_raw[2], DEC); Serial.println();

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