import matplotlib.pyplot as plt
import os
import pandas as pd
from random import randint
import serial
from IPython.display import clear_output
import pylab


###   SETTINGS   ###


#Operating System
Operating_System = ""

# Global serial communication line.
ser = serial.Serial()

# Baudrate setting used in creation of the serial communication line.
Baud_Rate = 115200

# Port setting used in creation of the serial communcation line.
Serial_Port = '/dev/ttyUSB0'

# Set to True to print out extra information in certain methods.
Debug_Status = False

# Holds the amount of iterations the user inputs.
Iteration_Amount = 0

# Axis_X average taken over 10 most previous cycles within +-5.
X_Arr = []
X_Avg = None

# Axis-Y average taken over 10 most previous cycles within +-5.
Y_Arr = []
Y_Avg = None

# Axis-Z average taken over 10 most previous cycles within +-5.
Z_Arr = []
Z_Avg = None

# Length of the list used to create and hold the average values.
List_Length = 200

#File name generated at random to better catalog results.
File_Number = 0

###   FUNCTIONS   ###


# Reads in 10 data cycles to obtain average.
def Average_Data(x, y, z):

    global X_Arr
    global X_Avg

    global Y_Arr
    global Y_Avg

    global Z_Arr
    global Z_Avg

    global List_Length
    global Debug_Status

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

        if(Debug_Status):
            print("X List: " + str(X_Arr))
            print("X Average: " + str(X_Avg))
            print("Y List: " + str(Y_Arr))
            print("Y Average: " + str(Y_Avg))
            print("Z List: " + str(Z_Arr))
            print("Z Average: " + str(Z_Avg))


# Appends the Series parameter to the DataFrame parameter. Sorts updated DataFrame.
#   Parameter x,y,z: values to be entered into the DataFrame.
#   Parameter df: DataFrame
#   Parameter direction: True = acsending order / False = decsending order
#   Returns df: Updated DataFrame
def Append_Series_to_DataFrame(x, y, z, df, direction):

    df.loc[-1] = [x, y, z]
    df.index = df.index + 1
    df = df.sort_index(ascending=direction)

    return df


# Establishes a communication channel with the microcontroller.
#   Transfers data type and iteration amount to microcontroller.
#   Parameter Iteration_Amount: amount of data points to collect.
#   Parameter Data_Type: type of data to collect.
def Begin_Signal():

    global ser
    global Data_Type
    global Iteration_Amount

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

    # Estimates time until completion of data collection and plotting.
    Time_Until_Done()

    # Clears serial port of unnessecary data.
    Serial_Clear()

    # Sends signal to ESP32 to start listening for commands.
    Serial_Send("START")

    # Waits for the ESP32 to prompt this script for the user's Iteration_Amount
    if(Debug_Status):
        print("")
        print("Waiting for Iteration_Amount request...")
    Serial_Recieve()

    # Sends the Iteration_Amount
    Serial_Send(Iteration_Amount)


# Reads in data from ESP32 over the serial port defined below.
#   Returns df: Created DataFrame from recorded data points.
def Collect_Data():

    global ser
    global Iteration_Amount
    global List_Length
    global Cycle_Count
    
    global X_Avg
    global Y_Avg
    global Z_Avg

    # Reads in a set amount of cycles to establish an average to later help zero out background noise.
    for i in range(0, List_Length - 1):
        x, y, z = Series_Create("SPLIT")
        if(i>List_Length/2):
            Average_Data(x, y, z)
            if(Debug_Status):
                print(i)
        else:
            if(Debug_Status):
                print(i)
        if(Debug_Status):
            print("--------------------------------------------------")
    
    # Builds 1st Series.
    s1 = Series_Create("WHOLE")

    # Builds 2nd Series.
    s2 = Series_Create("WHOLE")

    # Creates dataframes with appropriate column names. 
    df = pd.DataFrame([list(s1), list(s2)],  columns=["X", "Y", "Z"])

    # Continuously read in values and appends to DataFrame for desired amount of iterations.
    #   Iteration_Amount - Amount selected by user. 
    #   this_iter - List Length & first two series right above.
    this_iter = List_Length + 2
    while(this_iter < Iteration_Amount):
        
        if(Debug_Status):
            print("#:" + str(this_iter))
        # Reads in current Orientation Data.
        x, y, z = Series_Create("SPLIT")

        # Subtracts or adds average to the incoming data.
        a, b, c = Zero_Data(x, y, z)
        if(Debug_Status):
            print("Averages " + "X:" + str(X_Avg) + "  Y:" + str(Y_Avg) + "  Z:" + str(Z_Avg))
            print("Original " + "X:" + str(x) + "  Y:" + str(y) + "  Z:" + str(z))
            print("Zeroed   " + "X:" + str(a) + "  Y:" + str(b) + "  Z:" + str(c))
            print("--------------------------------------------------")

        # Adds new values to DataFrame, then sorts by index value.
        Append_Series_to_DataFrame(a, b, c, df, False)

        # Incrementing the iterator
        this_iter += 1

    # Returns completed dataframe
    df = df.sort_index(ascending = False)
    return df


# Plots dataframe's X,Y,Z on a horizontal line graph.
#   Expected dataframe to have 3 axis worth of data labelled 'X,Y,Z'.
#   Parameter df: DataFrame to be graphed.
#   Paramter i: Iteration Count for the entire program. Allows for each test to have unique label.
def DataFrame_Plot(df, i):

    global Iteration_Amount
    global File_Number
    
    #Generates random number to be used for identification.
    File_Number = randint(0,10000)

    #Closes previous instance of plt.
    if(i>0):
        plt.close()
          
    #Plot x,y,z axes.    
    plt.plot(df.X, label="X-axis")
    #plt.plot(df.Y, label="Y-axis")
    #plt.plot(df.Z, label="Z-axis")
    
    #Attach axis and title labels.
    plt.ylabel("microTesla (uT)")
    plt.xlabel("Time (160Hz) (6.25x10^-3(s))")
    plt.title("POWER LINE DETECTION TRIAL #" + str(File_Number))

    #Attach legend box to the top right of graph.
    plt.legend(bbox_to_anchor=(1.05, 1), loc=2, borderaxespad=0.0)
    print("--------------------------------------------------")


# Prints out entire dataframe to screen.
#   Parameter df: DataFrame to be printed.
def DataFrame_Print(df):

    # Print DataFrame.
    print(df)


# Prompts the user to input an amount of data points they want gathered
#   from the IMU.
#   Returns Iteration_Amount: Amount of data points.
def Prompt_Iteration_Amount(Iteration):

    global Iteration_Amount
    global List_Length

    # Prints the iteration number.
    print("Iteration #" + str(Iteration))
    print("--------------------------------------------------")

    # Prompts user to enter an amount of data points.
    print("Enter desired data points: ")
    # List_Length is added to get the average before the data is considered "real"
    while(1):
        choice = int(input())
        if(isinstance(choice, int)):
            Iteration_Amount = choice + List_Length
            break
        else:
            print("Enter a integer type")
    print("--------------------------------------------------")


# Converts completed DataFrame and graph to csv file.
#   Parameter df: DataFrame to be saved.
#   Parameter Data_Type: Type of Data collected. For Documentation purposes.
#   Parameter Iteration: Current cycle of program. Used for documentation.
def Save_To_File(df):

    global File_Number
    
    print("Saved #" + str(File_Number) + " in: " + "/home/jared/Desktop/mfvd/Saves")
        
    # Creates / Saves the DataFrame and its graph to a file.
    filename = "Trial:" + str(File_Number)
    path = r'/home/jared/Desktop/mfvd/Saves'
    df.to_csv(os.path.join(path, filename), header=True, sep='\t')
    pylab.savefig(os.path.join(path, filename), bbox_inches='tight', pad_inches=0.5)

    print("--------------------------------------------------")


# Reads in / clears unwanted data from serial port.
def Serial_Clear():

    global debug
    global ser

    if(ser.in_waiting != 0):
        junk = ser.readline().decode()
        if(Debug_Status):
            print("Cleared: " + str(junk))
            

#Closes the serial port.
def Serial_Close():
    
    global ser
    
    ser.close()


# Configures and opens a serical communication line.
def Serial_Create():

    global ser
    global Serial_Port
    global Baud_Rate

    if(ser.is_open == False):
        ser.port = Serial_Port
        ser.baudrate = Baud_Rate
        ser.timeout = 1
        ser.open()


# Reads in 1 bit values on the serial communication line.
#   Returns prompt: Input read in from serial line.
def Serial_Recieve():

    global debug
    global ser

    while(1):
        if(ser.in_waiting != 0):
            prompt = ser.read().decode()

            if(Debug_Status):
                print("Received: " + str(prompt))
            break
    return prompt


# Sends given message over serial port.
#   Parameter message: Object to send over the serial line.
def Serial_Send(message):

    global debug
    global ser

    message = str(message)

    if(Debug_Status):
        print("Sending: " + str(message))

    ser.write(message.encode())


# Waits for data to be present on the serial port. Reads in and creates Series once present.
#   Parameter ser: serial communication line.
#   Returns s: Created Series.
#   Returns line.split(","): Individual variables.
def Series_Create(text):

    global ser
    global debug

    if(Debug_Status):
        print("--------------------------------------------------")
        print("Waiting to create series")

    while(1):
        if(ser.in_waiting != 0):
            line = ser.readline().decode()
            if(text == "SPLIT"):
                if(Debug_Status):
                    print("Series created from " + str(line))
                    print("")
                return line.split(",")

            elif(text == "WHOLE"):
                x, y, z = line.split(",")
                a, b, c = Zero_Data(x, y, z)
                s = pd.Series([a, b, c])
                if(Debug_Status):
                    print("Series created from " + str(line))
                    print("")
                    print("Averages " + "X:" + str(X_Avg) + "  Y:" + str(Y_Avg) + "  Z:" + str(Z_Avg))
                    print("Original " + "X:" + str(x) + "  Y:" + str(y) + "  Z:" + str(z))
                    print("Zeroed   " + "X:" + str(a) + "  Y:" + str(b) + "  Z:" + str(c))
                return s


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
            print("Allowed Serial Ports: \tCOM0\tCOM1\tCOM2")
            print("Please enter one of the above ports.")
            port = input()
                
            if(port == "COM0" or "COM1" or "COM2" or "COM3"):
                print("\t\tSetting Updated")
                print("--------------------------------------------------")
                return port
            else:
                print("Invalid or wrong connection. Check your port!")


#Used to setup the system settings. 
def Settings_Config():
    
    global Operating_System
    global Debug_Status
    global Baud_Rate
    global Serial_Port
    
    while(1):
        
        clear_output()
        
        print("--------------------------------------------------")
        print("SETTINGS MENU: ")
        print("")
        
        while(Operating_System != "Windows" and Operating_System != "Linux"): 
            print("Select Windows or Linux")
            Operating_System = input()
        print("\t\tSetting Updated")
        print("--------------------------------------------------")

        print("Baudrate: " + str(Baud_Rate))
        print("Change Baudrate? Y/n")
        Baud_Rate = Update(input(), "Baud")

        print("Serial Port: " + str(Serial_Port))
        print("Change Port? Y/n")
        Serial_Port = Update(input(), "Port")

        print("Debug Status: " + str(Debug_Status))
        print("Change Status? Y/n")
        Debug_Status = Update(input(), "Debug")
        
        Settings_Display()
        print("Exit to Program (Exit) or Alter Settings (Alter)")
        choice = input()
        if(choice=="Exit"):
                
            #Establishes a serial communication line to the ESP32/IMU.
            Serial_Create()
            break


# Displays all current settings, their values, and or their status.
def Settings_Display():

    global Debug_Status
    global Baud_Rate
    global Serial_Port
    global Operating_System
    
    print("\t\tCURRENT SETTINGS")
    print("Operating System: " + str(Operating_System))
    print("Baudrate: " + str(Baud_Rate))
    print("Serial Port: " + str(Serial_Port))
    print("Debug Status: " + str(Debug_Status))
    print("--------------------------------------------------")


# Predicts time left until completion of data collection.
#   Parameter Iteration_Amount: Amount of cycles / iterations.
#   parameter Iteration_Speed: Time taken to complete one cycle.
def Time_Until_Done():

    global Iteration_Amount

    Total_Time = (Iteration_Amount * 0.00625)

    hours = 0
    minutes = 0
    seconds = 3

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

# Responsible for updating setting.
#   Parameter System_Change: Single Charater assigned through input
#   Parameter Origin: Where the function call originated from.
#   Return Updated_Setting: New setting to overwrite current configuration.


def Update(System_Change, Origin):

    global Debug_Status
    global Baud_Rate
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


# Collects first 10 cycles. Averages the results, and than zeros out data
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

    a = round(X - X_Avg, 2)
    b = round(Y - Y_Avg, 2)
    c = round(Z - Z_Avg, 2)

    return a, b, c
