#include <Adafruit_BNO055.h>
#include <Adafruit_Sensor.h>

Adafruit_BNO055 bno = Adafruit_BNO055(55);
 
imu::Vector<3> event;

boolean Start_Signal = false;
boolean Delay = false;
boolean Data = false;
boolean Iter = false;
boolean Ready_To_Send = false;
boolean Completed_Cycle = false;

struct data{
    String Data_Type = "";
    int Iteration_Amount = 0;
    float Delay_Amount = 0.0;
};
struct data user;

/**
 * Initailzation of: serial port, freeRTOS tasks, and BNO055.
 */
void setup(){

    Serial.begin(115200);
    bno.begin();

    delay(1000);

    xTaskCreatePinnedToCore(Task_Serial_Read, "Task_Serial_Read", 10000, NULL, 1, NULL, 0);

    xTaskCreatePinnedToCore(Task_Main, "Task_Main", 10000, NULL, 1, NULL, 1);

}

/**
 * Program should not iterate this far. 
 */
void loop(){

    delay(1000);

}

/**
 * Executes the main program.
 */
void Task_Main(void * parameter){

  while(1){
    
    if(Completed_Cycle){
      //Resets all global variables.
      Reset();
    }

    if(Ready_To_Send){
      //Sends selected data type for a certain number of iterations.
      Send_Data();
    }

    delay(1000);
  }
}

/**
 * Checks for serial input to be read in. Deciphers the message and assigns the message
 *    to its appropirate location or variable.
 * Returns status: True if all info was recieved and ready to send data. False otherwise.
 */
void Task_Serial_Read(void * parameter){

  while(1){
    if(Serial.available() && !Ready_To_Send){
      if(user.Delay_Amount == 0 || !Delay){
        user.Delay_Amount = Serial.readString().toFloat();
        Delay = true;
        Serial_Clear();
      }
      else if(!Start_Signal){
        String text = Serial.readString();
        Start_Signal = true;
        Serial_Clear();

        //Signals to Jupyter to send Data_Type.
        Serial.print(1);
      }
      else if(user.Data_Type == "" || !Data){
        user.Data_Type = Serial.readString();
        Data = true;
        Serial_Clear();

        //Signals to Jupyter to send Iteration_Amount.
        Serial.print(2);
      }
      else if(user.Iteration_Amount == 0 || !Iter){
        user.Iteration_Amount = Serial.readString().toInt();
        Iter = true;
        Serial_Clear();
      }

      if(Delay && Start_Signal && Data && Iter){
        Ready_To_Send = true;
        Serial_Clear();
      }
    }
    delay(100);
  }
}

/**
 * Selects the correct type of event based on the Data_Type.
 *    Parameter event: Object used to collect data from the IMU.
 */
void Event_Type(){

  if(user.Data_Type == "Accel"){
    event = bno.getVector(Adafruit_BNO055::VECTOR_ACCELEROMETER);
  }
  else if(user.Data_Type == "Euler"){
    event = bno.getVector(Adafruit_BNO055::VECTOR_EULER);
  }
  else if(user.Data_Type == "Grav"){
    event = bno.getVector(Adafruit_BNO055::VECTOR_GRAVITY);
  }
  else if(user.Data_Type == "Gyro"){
    event = bno.getVector(Adafruit_BNO055::VECTOR_GYROSCOPE);
  }
  else if(user.Data_Type == "LinAccel"){
    event = bno.getVector(Adafruit_BNO055::VECTOR_LINEARACCEL);
  }
  else if(user.Data_Type == "Mag"){
    event = bno.getVector(Adafruit_BNO055::VECTOR_MAGNETOMETER);
  }
}

/**
 * Resets the program for next iteration.
 */
void Reset(){

  user.Data_Type = "";
  user.Iteration_Amount = 0;

  Data = false;
  Iter = false;
  Start_Signal = false;
  Ready_To_Send = false;
  Completed_Cycle = false;

}

/**
 * Prints to the serial port the desired data type from the BNO055 IMU.
 */
void Send_Data(){

  // Possible event types are:
  // - VECTOR_ACCELEROMETER - m/s^2
  // - VECTOR_MAGNETOMETER  - uT
  // - VECTOR_GYROSCOPE     - rad/s
  // - VECTOR_EULER         - degrees
  // - VECTOR_LINEARACCEL   - m/s^2
  // - VECTOR_GRAVITY       - m/s^2

  for(int i=0; i<user.Iteration_Amount; i++){

    //Clears serial port of all unwanted or unneccessary information.
    Serial_Clear();

    if(user.Data_Type == "Temp"){
      
      float temp = bno.getTemp();
      
      //Sends data to jupyter in correct format.      
      Serial.print(temp, 3);
      Serial.print(",");
      Serial.print(0.0);
      Serial.print(",");
      Serial.println(0.0);
    }
    else{
      Event_Type();

      //Sends data to jupyter in correct format.
      Serial.print(event.x(), 3);
      Serial.print(",");
      Serial.print(event.y(), 3);
      Serial.print(",");
      Serial.println(event.z(), 3);
    }

    delay(user.Delay_Amount);
  }
  Completed_Cycle = true;
}

/**
 * Clears the serial port.
 */
void Serial_Clear(){
  while(Serial.available()){
    int junk = Serial.read();
  }
}