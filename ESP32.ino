#include <Adafruit_HMC5883_U.h>
#include <Adafruit_Sensor.h>

Adafruit_HMC5883_Unified mag = Adafruit_HMC5883_Unified(12345);

sensors_event_t event;

boolean Start_Signal = false;
boolean Delay = false;
boolean Data = false;
boolean Iter = false;
boolean Ready_To_Send = false;
boolean Completed_Cycle = false;

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

  mag.begin();
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

    if (Serial.available() && !Ready_To_Send){

      //Reads in the Delay Amount (Refresh Rate) from Jupyter.
      if (user.Delay_Amount == 0 || !Delay){
        user.Delay_Amount = Serial.readString().toFloat();
        Delay = true;
        Serial_Clear();
      }

      //Reads in Start Signal from Jupyter.
      else if (!Start_Signal){
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
      if (Delay && Start_Signal && Iter){
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

  //Loops until correct amount of data has been sent over the serial port.
  for (int i = 0; i < user.Iteration_Amount; i++){

    //Clears serial port of all unwanted or unneccessary information.
    Serial_Clear();

    //Gathers most current magnetometer data.
    mag.getEvent(&event);

    //Sends data to jupyter in correct format.
    Serial.print(event.magnetic.x, 3);
    Serial.print(",");
    Serial.print(event.magnetic.y, 3);
    Serial.print(",");
    Serial.println(event.magnetic.z, 3);

    delay(user.Delay_Amount);
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