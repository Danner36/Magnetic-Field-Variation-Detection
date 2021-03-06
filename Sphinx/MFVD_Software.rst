Software
========

**Fair Warning**: This page is very lengthly due to the entire project's code being displayed. 
Using the navigation menu to the left under "Software" is advised.

Embedded Software (Visual Studio Code)
--------------------------------------

This software has been developed for Espressif's ESP32 Development Board. This software 
utilizes FreeRTOS's ability for dual core functionality, while remaining simplistic in 
size and scope in the C++ language.

The following code is responsible for taking the input from the user, given through Jupyter,  
setting up the correct environment with the HMC5883L, pulling the data from the magnetometer, 
and relaying that information back to Jupyter via serial communication.

::

	#include <Wire.h>

	//Address of the HMC5883L.
	#define MAG_ADDRESS ((char) 0x1E)
	
	//Buffer array used to store bytes from the HMC5883L.
	uint8_t mag_buffer[6];
	
	//Finalized array that holds joined bytes from mag_buffer[].
	int16_t mag_raw[3];

	//Delays examples: 0.00555 - 180Hz
	//                 0.00625 - 160Hz (DEFAULT)
	//                 0.00833 - 120Hz
	//                 0.0100 - 100Hz
	//                 0.0125 - 80Hz
	float SYSTEM_DELAY = 0.00625;

	//Used to indicate that Jupyter/user is ready.
	boolean Start_Signal = false;
	
	//Status of transmission of Iteration_Amount. True when recieved, false otherwise.
	boolean Iter = false;
	
	//Status of transmission of Operation_Mode. True when recieved, false otherwise.
	boolean Mode = false;
	
	//True when all needed information is recieved. (Start_Sginal & Iter)
	boolean Ready_To_Send = false;
	
	//Status of program state. True causes program to be idle, false triggers 
	//   the transmission of data.
	boolean Completed_Cycle = false;

	//Struct used to hold values used throughout program.
	struct data{

	  //Desired amount of data to be sent from the HMC5883L to Jupyter 
	  //   over the serial port.
	  int Iteration_Amount = 0;
	  
	  //Used to track the amount of data being send. 
	  //   (Goes from 0 to Iteration_Amount)
	  int this_iter = 0;
	  
	  //Instructs program to either send information infinitely or until a 
	  //   desired amount has been reached.
	  String Operation_Mode = "STATIONARY_MODE";
	  
	};
	struct data user;

	/**
	 * Initailzation of: serial port, freeRTOS tasks, and HMC5883L.
	 */
	void setup(){

	  Serial.begin(115200);

	  Wire.begin();

	  configMag();

	  Serial_Clear();
	  
	  xTaskCreatePinnedToCore(Task_Serial_Read, "Task_Serial_Read", 10000, NULL, 1, NULL, 0);

	  xTaskCreatePinnedToCore(Task_Main, "Task_Main", 10000, NULL, 1, NULL, 1);
	}

	/**
	 * Program should never reach this point. 
	 */
	void loop(){

	}

	/**
	 * Initalizes the HMC5883L by checking for a physical connection, 
	 *   setting the desired mode, and sensitivity range.
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
	 *   Either promts the transmission of data to Jupyter, or resets 
	 *   all values in preperation of next cycle.
	 */
	void Task_Main(void *parameter){

	  while (1){
		
		if (Completed_Cycle){

		  //Resets all global variables.
		  Reset();

		  //Sends task to idle for 1/10th second to satisfy watchdog timer.
		  delay(10);
		}

		//Ieration_Amount and Start_Sginal were recieved.
		if (Ready_To_Send){

		  //Iterates up to desired data amount.
		  while (user.this_iter < user.Iteration_Amount){
			
			//Sends x,y,z values in csv format.
			Send_Data();

			delay(SYSTEM_DELAY * 1000);
		  }
		  
		  //If in CONTINUOUS_MODE, it will infinitely send information 
		  //   on the serial port. Otherwise, it will stop and restart.
		  if (user.Operation_Mode != "CONTINUOUS_MODE"){
			//Signals the program has completed.
			Completed_Cycle = true;
		  }
		  else{
			user.this_iter = 0;
		  }

		}
	  }
	}

	/**
	 * Checks for serial input to be non-empty. Deciphers the message and 
	 *   assigns the message to its appropirate location or variable.
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
			  Serial.print(SYSTEM_DELAY,5);

			}
			else{
			  //Clears serial port of all unwanted or unneccessary information.
			  Serial_Clear();
			}

		  }

		  //Reads Iteration Amount from Jupyter.
		  else if (user.Iteration_Amount == 0 || !Iter){

			//Reads & convers to integer.
			user.Iteration_Amount = Serial.readString().toInt();
			Iter = true;

			//Clears serial port of all unwanted or unneccessary information.
			Serial_Clear();

			//Signals to Jupyter to send Mode of Operation.
			Serial.print(1);
		  }

		  //Reads Operation Mode from Jupyter.
		  else if (user.Operation_Mode == "" || !Mode){
			//Reads in mode.
			int temp = Serial.readString().toInt();

			//Checks to see if valid mode.
			if(temp == 1 || temp == 2){
			  Mode = true;

			  //Assigns appropirate mode.
			  if(temp == 1){
				user.Operation_Mode = "CONTINUOUS_MODE";
			  }
			  else{
				user.Operation_Mode = "STATIONARY_MODE";
			  }

			}
			//Clears serial port of all unwanted or unneccessary information.
			Serial_Clear();
		  }

		  //If all signals have been recieved, begins data transmission.
		  if (Start_Signal && Iter && Mode){
			Ready_To_Send = true;

			//Clears serial port of all unwanted or unneccessary information.
			Serial_Clear();
		  }

		}
		//Sends task to idle for 1/10th second to satisfy watchdog timer.
		delay(10);
	  }
	}

	/**
	 * Reads in 6 bytes (2 for each x,y,z) from the HMC5883L and merges them together.
	 */
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
	 
	  //Places device into Single Measurement Mode to refill the registers 
	  //   after short idling period of 250 microSeconds.
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
	
Human Machine Interface (Jupyter)
---------------------------------

Jupyter was chosen to be the interfacing link between the ESP32 and the user due to its ability to run
python scipts which allows for real time data analytics. Jupyter, being a package of python, also has numerous 
libraries focused on organizing, displaying, and saving data at its disposal. Below are two sets of code, the 
first is the main program which calls upon the library (second) to preform actions upon the incoming data 
stream from the HMC5883L Compass.

**FIRST (Main Program)**

This first section of software prompts the user to input variables such as the operating system, desired serial
port, and so on. After the correct enviroment is established, the user selects between a continuous feed mode or 
a set amount mode. The continuous feed mode consists of reading in an indefinite amount of data from the HMC,
preforming a FFT on that data, and displaying the difference in magnitude of signal between the background noise
and the desired 60Hz. The second mode (set amount) reads in a certain amount entered by the user, than collects
/displays that data through tables and graphs. 

::
	
	import HMC5883L_Library as hmc
	%matplotlib inline
	
	###  MAIN PROGRAM   ###

	# Prompts user to configure settings/options associated with the HMC5883L.
	hmc.Settings_Config()

	i = 0

	# Entire program runs on a loop until explicitly exited.
	while(1):
		
		# Clears output terminal.
		hmc.clear_output()
		
		# Responisble for prompting user and recording the input.
		hmc.Prompt_Iteration_Amount(i)
		
		# Prepares the ESP32 for data transmission.
		hmc.Begin_Signal()

		hmc.Set_Average()
		
		# Constantly reads in 1 seconds worth of data (160 data points). Prints out
		#   strength of signal based on min and max of the FFT's frequency ratio.
		if(hmc.Operation_Mode == "CONTINUOUS_MODE"):
			while(1):
				
				# Collects incoming data from the ESP32/HMC5883L.
				df = hmc.Collect_Data()
				
				# Clears output terminal.
				hmc.clear_output()

				# Displays signal strength to screen.
				hmc.Display_Signal_Strength(df,i)
		
		# Reads in a user set amount of data. Proceeds to save/print out results in table/graph form.
		elif(hmc.Operation_Mode == "STATIONARY_MODE"):
			
			# Estimates time until completion of data collection and plotting.
			hmc.Time_Until_Done()
			
			# Collects incoming data from the ESP32/HMC5883L.
			df = hmc.Collect_Data()
			
			# Displays the data collected from the magnetometer.
			#hmc.Display_Table(df) #DataFrame Table
			hmc.Display_DF(df,i)  #DataFrame Graph
			hmc.Display_FFT(df)   #DataFrame FFT

			# Prompts user to decide if they wish to continue or quit.
			print("Do you want to Quit? [Y / n]")
			quit = input()
			if(quit == 'Y'):
				# Closes serial port.
				hmc.Serial_Close()
				break
			else:
				i = i + 1
	
**SECOND (Library)**

This second block of software, as stated above, is the library for the HMC5883L. This library contains all of 
the methods and variables used within the program. For a more detailed description of what each method does or 
can do, please read that method's commented section that lies directly above that respective method.

::

	import matplotlib.pyplot as plt
	import numpy as np
	import os
	import pandas as pd
	from random import randint
	import serial
	from IPython.display import clear_output


	###   SETTINGS   ###


	# Baudrate setting used in creation of the serial communication line.
	Baud_Rate = 115200

	# Set to True to print out extra information in certain methods.
	Debug_Status = False

	# List of set amount (3) FFT_Strengths. Used to make a smooth average of data.
	FFT_Arr = []

	# Ratio of highest to lowest frequencies in the FFT.
	FFT_Ratio = "Placeholder until calculated."

	# File name generated at random to better catalog results.
	File_Number = 0

	# Used to store the axis of the FFT.
	Freq_Axis = 0

	# Holds the amount of iterations the user inputs.
	Iteration_Amount = 0

	# Length of the list used to create and hold the average values. Secondary purpose is to 
	#   make sure no real data is recorded until the sensor is calibrated or out of the transient state.
	List_Length = 200

	# Maximum frequency of incoming signal.
	Max_Sig = 0

	# Minimum frequency of incoming signal.
	Min_Sig = 0

	# Operating System beign used. (Windows or Linux) Used to create a serial port.
	Operating_System = "Linux"

	# Mode of Operation. Continous or stationary. Stationary by default.
	Operation_Mode = "STATIONARY_MODE"

	# Refresh rate of the system (Hz). SET ON ESP32.
	Refresh_Rate = 0.00625

	# Global serial communication line.
	ser = serial.Serial()

	# Port setting used in creation of the serial communcation line. (Defaulted to Linux)
	Serial_Port = '/dev/ttyUSB0'

	# Array of length (List_Length) used to create average later used to zero out data.
	X_Arr = []
	X_Avg = None

	# Array of length (List_Length) used to create average later used to zero out data.
	Y_Arr = []
	Y_Avg = None

	# Array of length (List_Length) used to create average later used to zero out data.
	Z_Arr = []
	Z_Avg = None


	###   FUNCTIONS   ###


	# Reads in a set amount of data cycles at the beginning of the program to set the average 
	#   for each the X, Y, and Z axes.
	#   Parameter: x - DataFrame axis value for X
	#   Parameter: y - DataFrame axis value for Y
	#   Parameter: z - DataFrame axis value for Z
	def Average_Data(x, y, z):

		global X_Arr
		global X_Avg

		global Y_Arr
		global Y_Avg

		global Z_Arr
		global Z_Avg

		global Debug_Status
		global List_Length

		x = round(float(x),2)
		y = round(float(y),2)
		z = round(float(z),2)

		# Checks for empty lists and if so, assigns first set of values as averages.
		if(X_Avg == 0 or Y_Avg == 0 or Z_Avg == 0):
			X_Avg = x
			X_Arr.append(x)

			Y_Avg = y
			Y_Arr.append(y)

			Z_Avg = z
			Z_Arr.append(z)
		else:
			# Adds value to list and recalculates average.
			# X List
			if(len(X_Arr) < List_Length):
				X_Arr.append(x)
				X_Avg = round((float(sum(X_Arr)) / float(len(X_Arr))), 2)

			# Y List
			if(len(Y_Arr) < List_Length):
				Y_Arr.append(y)
				Y_Avg = round((float(sum(Y_Arr)) / float(len(Y_Arr))), 2)

			# Z List
			if(len(Z_Arr) < List_Length):
				Z_Arr.append(z)
				Z_Avg = round((float(sum(Z_Arr)) / float(len(Z_Arr))), 2)

			if(Debug_Status and Operation_Mode == "STATIONARY_MODE"):
				print("X List: " + str(X_Arr))
				print("X Average: " + str(X_Avg))
				print("Y List: " + str(Y_Arr))
				print("Y Average: " + str(Y_Avg))
				print("Z List: " + str(Z_Arr))
				print("Z Average: " + str(Z_Avg))
				print("--------------------------------------------------")


	# Appends the axis parameters to the DataFrame parameter. Sorts updated DataFrame.
	#   Parameter x,y,z: Values to be entered into the DataFrame.
	#   Parameter df: DataFrame
	#   Parameter direction: True = acsending order / False = decsending order
	#   Returns df: Updated DataFrame
	def Append_Series_to_DataFrame(x, y, z, df, direction):

		df.loc[-1] = [x, y, z]
		df.index = df.index + 1
		df = df.sort_index(ascending=direction)

		return df


	# Establishes a communication channel with the microcontroller.
	#   Meant to be run at beginning of cycle. Communicates the iteration's
	#   desired amount of data. NOTE: Time_Until_Done is a rough estimation. 
	def Begin_Signal():

		global Data_Type
		global Iteration_Amount
		global Operation_Mode
		global Refresh_Rate
		global ser

		global X_Avg
		global Y_Avg
		global Z_Avg

		global X_Arr
		global Y_Arr
		global Z_Arr

		X_Avg = 0.0
		X_Arr = []
		Y_Avg = 0.0
		Y_Arr = []
		Z_Avg = 0.0
		Z_Arr = []

		# Clears serial port of unnessecary data.
		while(ser.in_waiting != 0):
			Serial_Clear()

		# Sends signal to ESP32 to start listening for commands.
		Serial_Send("START")

		# Waits for the ESP32 to prompt this script for the user's Iteration_Amount
		if(Debug_Status):
			print("")
			print("Waiting for Iteration_Amount request...")
		Serial_Recieve()

		# Sends the Iteration_Amount.
		Serial_Send(Iteration_Amount)
		
		if(Debug_Status):
			print("")
			print("Waiting for Operation_Mode request...")
		Serial_Recieve()
		
		# Sends the Operation_Mode.
		temp = 0
		if(Operation_Mode == "CONTINUOUS_MODE"):
			temp = 1
		else:
			temp = 2
		Serial_Send(temp)


	# Reads in data from ESP32 over the serial port defined below.
	#   Returns df: Completed DataFrame from recorded data points.
	def Collect_Data():

		global Iteration_Amount
		global List_Length
		global ser
		
		global X_Avg
		global Y_Avg
		global Z_Avg
		
		# Builds 1st Series.
		s1 = Series_Create("WHOLE")

		# Builds 2nd Series.
		s2 = Series_Create("WHOLE")

		# Creates dataframes with appropriate column names from above  series.
		df = pd.DataFrame([list(s1), list(s2)],  columns=["X", "Y", "Z"])

		#Iterator used in data collection.
		this_iter = 0
		
		#Starts at 2 + List_Length to include averaged amount.
		if(Operation_Mode == "STATIONARY_MODE"):
			this_iter = List_Length + 2
		#Starts at 2 to not include the average (List_Length)
		elif(Operation_Mode == "CONTINUOUS_MODE"):
			this_iter = 1
			Iteration_Amount = 160
		
		while(this_iter < Iteration_Amount):
			
			if(Debug_Status and Operation_Mode == "STATIONARY_MODE"):
				print("#:" + str(this_iter))
				
			# Reads in a string of 3 floats seperates by commas.
			x, y, z = Series_Create("SPLIT")

			# Subtracts or adds average to the incoming data.
			a, b, c = Zero_Data(x, y, z)
			
			if(Debug_Status and Operation_Mode == "STATIONARY_MODE"):
				print("Averages " + "X:" + str(X_Avg) + "  Y:" + str(Y_Avg) + "  Z:" + str(Z_Avg))
				print("Original " + "X:" + str(x) + "  Y:" + str(y) + "  Z:" + str(z))
				print("Zeroed   " + "X:" + str(a) + "  Y:" + str(b) + "  Z:" + str(c))
				print("--------------------------------------------------")

			# Adds new values to DataFrame, then sorts by index value. False = decsending.
			Append_Series_to_DataFrame(a, b, c, df, False)

			# Incrementing the iterator
			this_iter += 1

		# Sorts dataframe to make sure n and n-1 indexes are correct.
		df = df.sort_index(ascending = False)
		return df


	# Plots dataframe's X,Y,Z on a horizontal line graph.
	#   Expected dataframe to have 3 axis worth of data labelled 'X,Y,Z'.
	#   Parameter df: DataFrame to be graphed.
	#   Paramter i: Iteration Count for the entire program.
	def Display_DF(df, i):

		global File_Number
		global Iteration_Amount
		
		# Generates random number to be used for identification.
		File_Number = randint(0,10000)

		# Closes previous instance of plt if beyond first iteration.
		if(i>0):
			plt.close()
			  
		# Plot x,y,z axes.
		plt.plot(df.X, label="X-axis")
		plt.plot(df.Y, label="Y-axis")
		plt.plot(df.Z, label="Z-axis")
		
		# Attach axis and title labels.
		plt.ylabel("microTesla (uT)")
		plt.xlabel("Time (160Hz) (6.25x10^-3(s))")
		plt.title("POWER LINE DETECTION TRIAL #" + str(File_Number))

		# Attach legend box to the top right of graph.
		plt.legend(bbox_to_anchor=(1.05, 1), loc=2, borderaxespad=0.0)
		
		# Name of file.
		filename = "Trial_" + str(File_Number)
		
		# Assign path to be saved.
		if(Operating_System == "Linux"): 
			path = r'/home/jared/Desktop/mfvd/Saves/'
		elif(Operating_System == "Windows"):
			path = r'C:/Users/jd17033/Desktop/mfvd/Saves'
			
		# Creates / Saves the DataFrame's graph to a file.
		print("Saved #" + str(File_Number) + " in: " + path)
		
		# Saves the DataFrame as a CSV file.
		df.to_csv(os.path.join(path, filename + ".csv"), header=True, sep='\t')
		
		# Grabs the plot created by plt and sticks into designated path.
		plt.savefig(os.path.join(path, filename + ".png"), bbox_inches='tight', pad_inches=0.5)
		
		# Displays plot to screen. (If wanting to save the graph, this has to be 
		#   called after the save is complete.)
		plt.show()
		
		print("--------------------------------------------------")

		
	# Applys Fast Fourier Transform to 60Hz signal at 160Hz sampling rate.
	#   Allows for signal indication further from the 60Hz wire by integrating
	#   it back upon itself.
	#   Parameter: df - DataFrame to apply FFT.
	def Display_FFT(df):
		
		global FFT_Ratio
		global File_Number
		global Freq_Axis
		global Iteration_Amount
		global Operating_System
		global Refresh_Rate
	 
		# Closes previous instance of plt.
		plt.close()
		
		# Preforms an Fast Fourier Transform of passed in array.
		X_FFT = Get_FFT(df.X)
		Y_FFT = Get_FFT(df.Y)
		Z_FFT = Get_FFT(df.Z)
		
		#Creation of list to hold combined FFT's of all 3 axes.
		Freq_Sig = []
		
		# Sums together axes to form the maximum magnetic field strength at that
		#   point. Excludes X due to it not having an impact besides adding unwanted frequencies.
		for i in range(0,len(X_FFT)):
			#THIS IS WHAT I AM CHANGING WHEN THE TESTS SAY ADDING/DROPPING AXES.
			#   Line 455 also needs to be updated to reflect a change.
			Freq_Sig.append(X_FFT[i] + Z_FFT[i])
			
		# Plots FFT.
		plt.plot(Freq_Axis, Freq_Sig, label="Frequency Composition")
		   
		# Gets ratio of highest and lowest points in FFT array.
		Get_Ratio(Freq_Sig)
		
		# Finds correct positioning for ratio text.
		Graph_Height = max(Freq_Sig)*.9
		
		# Places text box with ratio of the FFT array.
		plt.text(5, Graph_Height, "Ratio " + str(FFT_Ratio), fontsize=15)
		
		# Attach axis and title labels.
		plt.title("FFT of Signal #" + str(File_Number))
		plt.ylabel("FFT Magnitude")
		plt.xlabel("Frequency (Hz)")
		
		# Attach legend box to the top right of graph.
		plt.legend(bbox_to_anchor=(1.05, 1), loc=2, borderaxespad=0.0)
		
		# Name of file.
		filename = "Trial_" + str(File_Number) + " FFT"
		
		# Assign path to be saved.
		if(Operating_System == "Linux"): 
			path = r'/home/jared/Desktop/mfvd/Saves'
		elif(Operating_System == "Windows"):
			path = r'C:/Users/jd17033/Desktop/mfvd/Saves/'
			
		# Creates / Saves the DataFrame's graph to a file.
		print("Saved #" + str(File_Number) + " in: " + path)
		
		# Grabs the plot created by plt and sticks into designated path.
		plt.savefig(os.path.join(path, filename + ".png"), bbox_inches='tight', pad_inches=0.5)
		
		# Displays plot to screen. (If wanting to save the graph, this has to be 
		#   called after the save is complete.)
		plt.show()
		
		print("--------------------------------------------------")
		
		
	# Prints out entire dataframe to screen.
	#   Parameter df: DataFrame to be printed.
	def Display_Table(df):

		# Print DataFrame.
		print(df)
		print("--------------------------------------------------")
		
		
	# Displays all current settings, their values, and or their status.
	def Display_Settings():

		global Baud_Rate
		global Debug_Status
		global Operation_Mode
		global Operating_System
		global Serial_Port

		
		print("\t\tCURRENT SETTINGS")
		print("Operating System: " + str(Operating_System))
		print("Operation Mode: " + str(Operation_Mode))
		print("Baudrate: " + str(Baud_Rate))
		print("Serial Port: " + str(Serial_Port))
		print("Debug Status: " + str(Debug_Status))
		print("--------------------------------------------------")
		
		
	# Prints out the strength of the signal compared to the background noise.
	#   Uses the ratio to compute strength
	#   Parameter: df - DataFrame passed in to be parsed.
	#   Parameter: i - Iteration Count for the entire program.
	def Display_Signal_Strength(df,i):
		
		global Debug_Status
		global FFT_Arr
		global FFT_Strength
		global Max_Sig
		global Min_Sig
		
		#Checks for first iteration.
		if(i == 0):
			plt.close()
		
		#Computes FFT from passed in array.
		if(Debug_Status):
			print("")
			print("Calculating FFT")
		
		#Computes FFT from df.X.
		X_FFT = Get_FFT(df.X)
		Y_FFT = Get_FFT(df.Y)
		Z_FFT = Get_FFT(df.Z)
		
		#Creation of list to hold combined FFT's of all 3 axes.
		Freq_Sig = []
		
		# Sums together axes to form the maximum magnetic field strength at that
		#   point. Excludes X due to it not having an impact besides adding unwanted frequencies.
		for i in range(0,len(X_FFT)):

			#THIS IS WHAT I AM CHANGING WHEN THE TESTS SAY ADDING/DROPPING AXES.
			#   Line 348 also needs to be updated to reflect a change.
			Freq_Sig.append(X_FFT[i] + Z_FFT[i])
		
		#Computes ratio from FFT data.
		if(Debug_Status):
			print("")
			print("Calculating Ratio")
		
		#Computes all FFT values.
		Get_Ratio(Freq_Sig)
		
		#Establishes variable to hold avg (local variable).
		FFT_AvgStrength = 0
		
		#Checks for invalid ratio. '----' means there is no standout signal.
		if(FFT_Strength == '----'):
			#Skips this case.
			three = 1 + 2
			
		#Checks for non full array.
		elif(len(FFT_Arr) < 3):
			
			# Appends new Strength to array.
			FFT_Arr.append(FFT_Strength)
		
		#Array if full, pops out last and pushes in new. Averages and prints data to screen.
		else:
			# Shifts [0]->[1]->[2] and sets [0] to new value.
			FFT_Arr[2] = FFT_Arr[1]
			FFT_Arr[1] = FFT_Arr[0]
			FFT_Arr[0] = FFT_Strength
			
			# Averages array.
			FFT_AvgStrength = int(sum(FFT_Arr)/len(FFT_Arr))
			
			# Displays Avgerage Signal Strength. 
			print("")
			print("")
			print("          " + str(FFT_AvgStrength))
			print("")
			print("")
			
		if(Debug_Status):
			print("Filling Array: " + str(FFT_Arr))
			print("Individual Signal Strength: " + str(FFT_Strength))
			print("Average Signal Strength: " + str(FFT_AvgStrength))
		

	# Conducts and Fast Fourier Transform of the given data.
	#   Parameter: sig - Array of data to be transformed.
	#   Returns: freqsig - Integrated signal.
	#   Returns: freqaxis - Axis range on which the FFT should be plotted.
	def Get_FFT(sig):
		
		global Debug_Status
		global Freq_Axis
		global Refresh_Rate
		
		#System numbers needed to create appropriate FFT window and axis
		N_fft = 160
		Fs = 1/Refresh_Rate
		
		# Creates correct axis range for data, also creates the fft to be plotted.
		Freq_Sig = np.abs(np.fft.fft(sig, n=N_fft))
		Freq_Axis = np.arange(0, Fs/2, Fs/N_fft)
		
		if(Debug_Status):
			print("Freq_Sig Created: " + str(len(Freq_Sig)))
		
		# Splices list to only include first half.
		Freq_Sig = Freq_Sig[:int(N_fft/2)]
		
		if(Debug_Status):
			print("Splitting Freq_Sig, Length = " + str(len(Freq_Sig)))
			
		return Freq_Sig
		
		
	# Finds ratio between max point and low mean in array.
	#   Return: ratio - Ratio between highest and lower magntiudes.
	def Get_Ratio(Freq_Sig):
		
		global Debug_Status
		global FFT_Strength
		global FFT_Ratio
		global Max_Sig
		global Min_Sig
		
		total = 0.0
		
		# Sums up 10 frequencies near beginning of range. 
		for i in range(10,20):
			total += Freq_Sig[i]
		
		# Averages the magnitudes to find the mean of the lower spectrum.
		Min_Sig = int(total/10.0)
		
		if(Debug_Status):
			print("Minimum Signal: " + str(Min_Sig))
		
		# Finds highest frequency magnitude in the signal.
		Max_Sig = int(max(Freq_Sig))
		
		if(Debug_Status):
			print("Maximum Signal: " + str(Max_Sig))
		
		# Finds the index of the highest magnitude.
		Max_Index = 0
		
		for i in range(0, len(Freq_Sig)):
			if(int(Freq_Sig[i]) == Max_Sig):
				Max_Index = i
				
		if(Debug_Status):
			print("Max_Index: " + str(i))
		
		# If within certain range of frequencies. Returns ratio. Otherwise, '----' (Nothing).
		if(65<Max_Index and Max_Index<80):
			FFT_Ratio = str(int(Max_Sig)) + ":" + str(int(Min_Sig))
			FFT_Strength = int(Max_Sig/Min_Sig)
		else:
			FFT_Strength = "----"
			FFT_Ratio = "----"
			
		if(Debug_Status):
			print("FFT_Ratio: " +str(FFT_Ratio))
			print("FFT_Strength: " + str(FFT_Strength))
		
		
	# Creates a 60Hz signal for test and verification purposes. 
	def Generate_60Hz():
		
		Fs = 0.00625
		t = 1/Fs
		f = 60
		
		Y = np.sin(2*np.pi*f*t)
		signal = np.abs(np.fft.fft(Y))
		
		plt.close()
		plt.title("60Hz TEST SIGNAL")
		plt.plot(signal)
		plt.show()


	# Prompts the user to input an amount of data points they want gathered
	#   from the IMU.
	#   Returns Iteration_Amount: Amount of data points.
	def Prompt_Iteration_Amount(Iteration):

		global Iteration_Amount
		global List_Length
		global Operation_Mode

		if(Operation_Mode == "STATIONARY_MODE"):
			
			# Prints the iteration number.
			print("Iteration #" + str(Iteration))
			print("--------------------------------------------------")

			# Prompts user to enter an amount of data points.
			print("Enter desired data points: ")

			while(1):
				choice = int(input())
				if(isinstance(choice, int)):
					# List_Length is tacked on to be able to form an average 
					#   and still recieve requested amount.
					Iteration_Amount = choice + List_Length
					break
				else:
					print("Enter a integer type")
			print("--------------------------------------------------")
		
		elif(Operation_Mode == "CONTINUOUS_MODE"):
			Iteration_Amount = 160 + List_Length
		

	# Reads in / clears unwanted data from serial port.
	def Serial_Clear():

		global Debug_Status
		global ser

		# Checks if serial port is empty.
		if(ser.in_waiting != 0):
			
			# If not, reads in until empty.
			junk = ser.readline().decode()
			
			if(Debug_Status):
				print("Cleared: " + str(junk))
				

	#Closes the serial port.
	def Serial_Close():
		
		global ser
		
		ser.close()


	# Configures and opens a serical communication line.
	def Serial_Create():

		global Baud_Rate
		global ser
		global Serial_Port


		#Establishes serial port if not already open.
		if(ser.is_open == False):
			ser.port = Serial_Port
			ser.baudrate = Baud_Rate
			ser.timeout = 1
			ser.open()


	# Reads in 1 bit values on the serial communication line.
	#   Returns message: Input read in from serial line.
	def Serial_Recieve():

		global Debug_Status
		global ser

		while(1):
			if(ser.in_waiting != 0):
				message = ser.readline().decode()

				if(Debug_Status):
					print("Received: " + str(message))
				break
		return message


	# Sends given message over serial port.
	#   Parameter message: Object to send over the serial line.
	def Serial_Send(message):

		global Debug_Status
		global ser

		# Converts to string for easier decoding on ESP32.
		message = str(message)

		if(Debug_Status):
			print("Sending: " + str(message))

		# Due to message being of type string, needs to be encoded.
		ser.write(message.encode())


	# Waits for data to be present on the serial port. Reads in and creates Series once present.
	#   Parameter text: Used to indicate if series is wanted, or individual floats.
	#   Returns s: Created Series.
	#   Returns line.split(","): Individual variables x,y,z.
	def Series_Create(text):

		global Debug_Status
		global ser

		if(Debug_Status and Operation_Mode == "STATIONARY_MODE"):
			print("Waiting to create series")

		while(1):
			
			#Checks for non-empty serial port.
			if(ser.in_waiting != 0):
				line = ser.readline().decode()
				
				if(text == "SPLIT"):
					if(Debug_Status and Operation_Mode == "STATIONARY_MODE"):
						print("Series created from " + str(line))
						print("")
					return line.split(",")

				elif(text == "WHOLE"):
					x, y, z = line.split(",")
					# Zeros data to better show alterations in variation.
					a, b, c = Zero_Data(x, y, z)
					s = pd.Series([a, b, c])
					
					if(Debug_Status and Operation_Mode == "STATIONARY_MODE"):
						print("Series created from " + str(line))
						print("")
						print("Averages " + "X:" + str(X_Avg) + "  Y:" + str(Y_Avg) + "  Z:" + str(Z_Avg))
						print("Original " + "X:" + str(x) + "  Y:" + str(y) + "  Z:" + str(z))
						print("Zeroed   " + "X:" + str(a) + "  Y:" + str(b) + "  Z:" + str(c))
					return s

				
	# Reads in certain amount (List_Length) to form average.
	def Set_Average():
		
		# Reads in a set amount of cycles to do both establish an average
		#   to zero out data, and exclude the uncalibrated data from the program.
		for i in range(0, List_Length - 1):
			
			# Reads in a string of 3 floats seperates by commas.
			x, y, z = Series_Create("SPLIT")
			
			# To exclude the transient repsonse from the average, the program only formulates
			#   the average based on the 2nd half of the length of List_Length. 
			#   EXAMPLE: List_Length = 200 so it will discard the first 100 values and form an
			#            average with the last 100.
			if(i>List_Length/2):
				Average_Data(x, y, z)
			if(Debug_Status and Operation_Mode == "STATIONARY_MODE"):
				print(i)
				print("--------------------------------------------------")

				
	# Prompts the user to enter a baudrate to be used in serial communication.
	#   Returns choice: User entered baudrate.
	def Set_Baudrate():

		print("Allowed Baud Rates: \t110\t300\t600 ")
		print("                    \t1200\t2400\t4800")
		print("                    \t9600\t14400\t19200")
		print("                    \t38400\t57600\t115200")
		print("                    \t128000\t256000")
		print("Please enter one of the above rates.")
		choice = int(input())
		print("\t\tSetting Updated")
		print("--------------------------------------------------")
		return choice


	# Used to update the debug state of the program/library.
	#   Parameter input : True = ON / False = OFF
	def Set_Debug():

		print("\t\tSetting Updated")
		print("--------------------------------------------------")
		if(Debug_Status):
			return False
		else:
			return True


	#Sets Mode of operation.
	#   CONTINUOUS - Reads in data over 1 second interval. Prints out signal strength.
	#   STATIONARY - Reads in set amount of data and displays it in graph and table form.
	def Set_Mode():
		
		print("Modes: CONTINUOUS (1)")
		print("       STATIONARY (2)")
		print("Choose Mode: ")
		
		Result = "Placeholder"
		while(1):
			choice = str(input())
			if(choice == '1'):
				Result = "CONTINUOUS_MODE"
				break
			elif(choice == '2'):
				Result = "STATIONARY_MODE"
				break
			else:
				print("Enter valid mode. 1 or 2")
				
		print("\t\tSetting Updated")
		print("--------------------------------------------------")
		return Result

		
	#Sets Operating System. Used if creation of serial port system.
	def Set_OS():
		
		global Serial_Port
		
		print("Select Windows or Linux.")
		while(1):
			choice = input()
			if(choice == "Windows"):
				Serial_Port = 'COM4'
				break
			elif(choice == "Linux"):
				Serial_Port = '/dev/ttyUSB0'
				break
			else:
				print("Enter either 'Windows' or 'Linux'")
				
		print("\t\tSetting Updated")
		print("--------------------------------------------------")
		return choice
		
	# Prompts the user to enter a serial port to be used in serial communication.
	#   Returns specific serial port that was selected.
	def Set_SerialPort():

		while(1):
			if(Operating_System == "Linux"):
				print("Allowed Serial Ports: \tUSB0\tUSB1\tUSB2")
				print("Please enter one of the above ports.")
				port = input()
					
				if(port == "USB0"):
					print("\t\tSetting Updated")
					print("--------------------------------------------------")
					return '/dev/ttyUSB0'
				elif(port == "USB1"):
					print("\t\tSetting Updated")
					print("--------------------------------------------------")
					return '/dev/ttyUSB1'
				elif(port == "USB2"):
					print("\t\tSetting Updated")
					print("--------------------------------------------------")
					return '/dev/ttyUSB2'
				else:
					print("Invalid or wrong connection. Check your port!")
			elif(Operating_System == "Windows"):
				print("Allowed Serial Ports: \tCOM0\tCOM1\tCOM2\tCOM3\tCOM4")
				print("Please enter one of the above ports.")
				port = input()
					
				if(port == "COM0" or "COM1" or "COM2" or "COM3" or "COM4"):
					print("\t\tSetting Updated")
					print("--------------------------------------------------")
					return port
				else:
					print("Invalid or wrong connection. Check your port!")


	#Used in setup of the system settings. 
	def Settings_Config():
		
		global Baud_Rate
		global Debug_Status
		global Operation_Mode
		global Operating_System
		global Serial_Port
		
		while(1):
			
			#Clears screen of all previous output.
			clear_output()
			
			print("--------------------------------------------------")
			print("SETTINGS MENU: ")
			print("")
			
			# Prompts user for OS.
			print("Operating System: " + Operating_System)
			print("Change OS? Y/n")
			Operating_System = Update(input(), "OS")
			 
			# Prompts user for mode. 
			print("Operation Mode: " + Operation_Mode)
			print("Change Mode? Y/n")
			Operation_Mode = Update(input(), "Mode")

			# Prompts user for Baudrate change.
			print("Baudrate: " + str(Baud_Rate))
			print("Change Baudrate? Y/n")
			Baud_Rate = Update(input(), "Baud")

			# Prompts user for serial port change.
			print("Serial Port: " + str(Serial_Port))
			print("Change Port? Y/n")
			Serial_Port = Update(input(), "Port")

			#Prompts user for debug change.
			print("Debug Status: " + str(Debug_Status))
			print("Change Status? Y/n")
			Debug_Status = Update(input(), "Debug")
			
			# Prints all chosen settings to screen to verify they are correct.
			Display_Settings()
			print("Exit to Program (Exit) or Alter Settings (Alter)")
			choice = input()
			if(choice=="Exit"):  
				# Establishes a serial communication line to the ESP32/HMC.
				Serial_Create()
				break


	# Predicts time left until completion of data collection.
	def Time_Until_Done():

		global Iteration_Amount
		global Refresh_Rate
		
		Total_Time = (Iteration_Amount * Refresh_Rate)

		hours = 0
		minutes = 0
		seconds = 0

		while(Total_Time >= 3600):
			Total_time -= 3600
			hours += 1

		while(Total_Time >= 60):
			Total_Time -= 60
			minutes += 1

		while(Total_Time >= 1):
			Total_Time -= 1
			seconds += 1
			if(seconds > 60):
				minutes += 1
				seconds = 0

		if(hours != 0):
			print("Time remaining: " + str(hours) + "hours, " +
				  str(minutes) + "minutes, " + str(seconds) + "seconds")
		elif(minutes != 0):
			print("Time remaining: " + str(minutes) +
				  "minutes, " + str(seconds) + "seconds")
		else:
			print("Time remaining: " + str(seconds) + "seconds")

		print("--------------------------------------------------")

		
	# Responsible for updating specific setting.
	#   Parameter System_Change: Yes or no status.
	#   Parameter Origin: Where the function call originated from.
	#   Return Updated_Setting: New setting to overwrite current configuration.
	def Update(System_Change, Origin):

		global Baud_Rate
		global Debug_Status
		global Serial_Port

		if(Origin == "Baud"):
			if(System_Change == 'Y'):
				return Set_Baudrate()
			else:
				print("--------------------------------------------------")
				return Baud_Rate
		elif(Origin == "Port"):
			if(System_Change == 'Y'):
				return Set_SerialPort()
			else:
				print("--------------------------------------------------")
				return Serial_Port
		elif(Origin == "Debug"):
			if(System_Change == 'Y'):
				return Set_Debug()
			else:
				print("--------------------------------------------------")
				return Debug_Status
		elif(Origin == "Mode"):
			if(System_Change == 'Y'):
				return Set_Mode()
			else:
				print("--------------------------------------------------")
				return Operation_Mode
		elif(Origin == "OS"):
			if(System_Change == 'Y'):
				return Set_OS()
			else:
				print("--------------------------------------------------")
				return Operating_System


	# Collects first List_Length cycles. Averages the results and zeros out data
	#   to better show difference while graphing
	#   Parameter x: x axis value
	#   Parameter y: y axis value
	#   Parameter z: z axis value
	#   Return a,b,c: Updated x,y,z axis values.
	def Zero_Data(x, y, z):

		global X_Avg
		global Y_Avg
		global Z_Avg

		X = float(x)
		Y = float(y)
		Z = float(z)

		# Rounded to 2 decimal places for readability.
		a = round(X - X_Avg, 2)
		b = round(Y - Y_Avg, 2)
		c = round(Z - Z_Avg, 2)

		return a, b, c
	