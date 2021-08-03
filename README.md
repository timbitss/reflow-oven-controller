# Reflow Oven Controller
Most PCBs today utilize SMT components to minimize the physical size of the circuit board. The industry standard and most reliable way of soldering SMT components is through [reflow soldering](https://en.wikipedia.org/wiki/Reflow_soldering) using a reflow oven. Most hobbyists, however, perform SMT soldering using a hot air gun, a hot plate, a soldering iron, or combination thereof due to cost. Reflow ovens on the market currently range from $300 CAD up to $1500 CAD, not including [necessary modifications](https://hackaday.io/project/175048-t-962a-reflow-oven-modifications). Since reflow soldering is the most time-efficient, reliable, and safest method of SMT soldering, a Nucleo-L476RG PID reflow oven controller was designed to transform any convectional toaster oven into a full-fledged reflow oven. 

## User Interface
A command-line interface was developed for user interaction at runtime. Users may access the CLI using a serial terminal with serial line configured for 115200 baud rate, 8 data bits, 1 stop bit, and no parity.

To see the commands that are available, type *help*.

![Help Command](https://github.com/timbitss/reflow-oven-controller/blob/main/imgs/help_command.PNG "Help Command")


## Installation and Flash 
1. Clone this repository into an appropriately-named project folder.
2. Inside the STM32CubeIDE, import the project to the current workspace by selecting File->General->Existing Projects into Workspace
3. Build and run the project. 
