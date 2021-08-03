# Reflow Oven Controller
Most PCBs today utilize SMT components to minimize the physical size of the circuit board. The industry standard and most reliable way of soldering SMT components is through [reflow soldering](https://en.wikipedia.org/wiki/Reflow_soldering) using a reflow oven. Most hobbyists, however, perform SMT soldering using a hot air gun, a hot plate, a soldering iron, or combination thereof due to cost. Reflow ovens on the market currently range from $300 CAD up to $1500 CAD, not including [necessary modifications](https://hackaday.io/project/175048-t-962a-reflow-oven-modifications). Since reflow soldering is the most time-efficient, reliable, and safest method of SMT soldering, a Nucleo-L476RG PID reflow oven controller was designed to transform any low-cost, convectional toaster oven into a full-fledged reflow oven. 

## User Interface
A command-line interface was developed for user interaction at runtime and for safety measures. Users may access the CLI using a serial terminal with serial line configured for 115200 baud rate, 8 data bits, 1 stop bit, and no parity.

To see the commands that are available, type *help*.

![Help Command](https://github.com/timbitss/reflow-oven-controller/blob/main/imgs/help_command.PNG "Help Command")

To get help on a specific command within a module, type *module command help*. E.g. entering *log set help* produces the following message:

![Module Help Command](https://github.com/timbitss/reflow-oven-controller/blob/main/imgs/module_cmd_help.PNG "Module Help Command")

### Reflow Oven Controller Commands
The most important commands are related to the reflow module. 

To view relevant information about the reflow oven controller, type *reflow status*.

![Reflow Status](https://github.com/timbitss/reflow-oven-controller/blob/main/imgs/reflow_status.PNG "Reflow Status")

To **start** the reflow process, type *reflow start*. Note that the oven temperature must be less than the reach temperature of the cooldown phase (see profile.c) to start the reflow process, otherwise an error message is shown.

To **stop** the reflow process and turn PWM off at any point in time, type *reflow stop*.

To set one or more PID parameters (Kp, Ki, Kd, Tau), type *reflow set param value [param2 value2 ...]*. Example:

![Reflow Set](https://github.com/timbitss/reflow-oven-controller/blob/main/imgs/reflow_set.PNG "Reflow Set")

## Data Logging
Live plotting of the oven temperature and PID terms was achieved using Matplotlib and pySerial (see plot_temp.py). The python script starts the reflow process on its own by transmitting the *reflow start* command. After the reflow process has completed or the animation window is closed, both the plot and the raw data in CSV format are saved. An example of a completed reflow process is shown below: 

![Temperature Plot](https://github.com/timbitss/reflow-oven-controller/blob/main/imgs/temp_plot.png "Temperature Plot")

Users are recommended to analyze the reflow plots during the PID tuning process for optimal system performance.

For added safety, the program transmits the *reflow stop* command to stop the reflow process upon error in parsing data or when the animation window is closed by the user. 


## Installation and Flash 
1. Clone this repository into an appropriately-named project folder.
2. Inside the STM32CubeIDE, import the project to the current workspace by selecting File->General->Existing Projects into Workspace
3. Build and run the project. 
