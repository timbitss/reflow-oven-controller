# Reflow Oven Controller
PCB designers today often utilize SMT components to minimize the physical size of the circuit board. The industry standard of soldering SMT components is through [reflow soldering](https://en.wikipedia.org/wiki/Reflow_soldering) using a reflow oven. Hobbyists, however, often perform SMT soldering using a hot air gun, a hot plate, a soldering iron, or combination thereof due to cost. Reflow ovens on the market currently range from $300 CAD up to $1500 CAD, not including [necessary modifications](https://hackaday.io/project/175048-t-962a-reflow-oven-modifications). Since reflow soldering is the most time-efficient, reliable, and safest method of SMT soldering, a Nucleo-L476RG PID reflow oven controller was designed to transform any low-cost, convectional toaster oven into a full-fledged reflow oven. 

![Reflow test](/images/reflow_test.jpg "Reflow test")

For individual module descriptions, please view the [wiki](https://github.com/timbitss/reflow-oven-controller/wiki). 

## Table of Contents
- [Reflow Oven Controller](#reflow-oven-controller)
  - [Table of Contents](#table-of-contents)
  - [Installation and Setup](#installation-and-setup)
      - [Real-time Plotting using Python](#real-time-plotting-using-python)
  - [Usage](#usage)
    - [Materials Required](#materials-required)
    - [Connections (based on configuration file)](#connections-based-on-configuration-file)
    - [PID Tuning](#pid-tuning)
  - [Command-line Interface](#command-line-interface)
    - [UART Commands](#uart-commands)
    - [Log Commands](#log-commands)
    - [Reflow Commands](#reflow-commands)
  - [User Safety](#user-safety)
  - [Credits](#credits)
  - [Additional Resources](#additional-resources)

## Installation and Setup
1. Clone this repository using `git clone https://github.com/timbitss/reflow-oven-controller.git`.
2. Install and open an [STM32CubeIDE](https://www.st.com/en/development-tools/stm32cubeide.html) instance.
3. Import the project to the current workspace by selecting File->General->Existing Projects into Workspace.
4. Build the project. If no build errors exist, flash the project using the run command. 
#### Real-time Plotting using Python
5. [Create a virtual environment](https://packaging.python.org/guides/installing-using-pip-and-virtual-environments/#creating-a-virtual-environment) in the project repository.
6. Install the necessary packages in the new virtual environment by entering `python3 -m pip install -r requirements.txt` for Unix/macOS users or `py -m pip install -r requirements.txt` for Windows users.
7. Create two folders in the root directory, one for CSV files and the other for temperature plots, and update the `csv_path` and `plot_path` variables in [plot_temp.py](plot_temp.py) to match their respective path. 

## Usage
### Materials Required
| Component                                                                 | Qty           | 
| :-----------------------------------------------------------------------: | :-----------: | 
| [>1000 W Convection Oven](https://tinyurl.com/5y66d4mf)<sup>1</sup>       | 1             | 
| [Male-Female Jumper Wires](https://tinyurl.com/ka4yxnmx)                  | 7             |   
| [K-Type Thermocouple](https://www.digikey.ca/short/b2zfmn1b)              | 1             |
| [SparkFun Thermocouple Breakout](https://www.sparkfun.com/products/13266) | 1             |
| [STM32 Nucleo-L476RG Board](https://www.digikey.ca/short/v0bqhwhd)        | 1             |
| [IoT Power Relay](https://www.digikey.ca/short/4q59345b)                  | 1             |

<sup>1</sup> It is recommended to purchase a used toaster oven to save on costs.

### Connections (based on configuration file)
| Thermocouple Breakout     | Nucleo-L476RG     | 
| :-----------------------: | :---------------: |
| GND                       |  GND              |
| VCC                       |  3V3              |
| SCK                       |  PB10             | 
| SO                        |  PC2              |
| CS                        |  PC4              |

| K-Type Thermocouple       | Thermocouple Breakout     |
| :-----------------------: | :-----------------------: |
| Chromel (yellow)          | +                         |
| Alumel (red)              | -                         |

| IoT Relay                 | Nucleo-L476RG             |
| :-----------------------: | :-----------------------: |
| +                         | PA6 (PWM Output)          |
| -                         | GND                       |

### PID Tuning
Test runs should be performed before using the oven for reflow soldering. The aim is to achieve an oven temperature profile that closely matches a [standard leaded or lead-free solder profile](https://www.x-toaster.com/resources/basics-on-reflow-soldering/), depending on the user's application. An example of a reasonably-tuned reflow profile is shown below (P = 225, I = 0, D = 500, Tau = 1):

![Temperature Plot](/images/temp_plot.png "Temperature Plot")

The PID tuning process is outlined below:  

1. Power the Nucleo board using a USB connected to a PC or laptop.      
2. Place the 'hot' end of the thermocouple inside the oven.
3. Turn the oven temperature knob to its maximum **bake** temperature.
4. Run the real-time plotting script, [plot_temp.py](plot_temp.py), and let the reflow process complete.
5. Tune one or more PID parameters based on the system's response using the `reflow set` CLI command (see [Reflow Commands](#reflow-commands)).
6. Repeat steps 3 and 4 until a reasonable reflow thermal profile is achieved.

Once the PID tuning process is complete, the oven is ready for reflow applications. The user should **keep a record of the final PID settings**, as they must be manually inputted again if the Nucleo board resets or powers off. 

## Command-line Interface
Users may access the CLI using a serial terminal with the serial line configured for 115200 baud rate, 8 data bits, 1 stop bit, and no parity.

To see the commands that are available, enter `help` or `?`.

![Help Command](/images/help_command.PNG "Help Command")

To get help on a specific command within a module, enter `<module> <command> help` or `<module> <command> ?`. E.g. entering `log set help` produces the following message:

![Module Help Command](/images/module_cmd_help.PNG "Module Help Command")

### UART Commands
To view performance measures related to the UART module, enter `uart pm`.

![uart pm](/images/uart_pm.PNG "uart pm")

To clear performance measures related to the UART module, enter `uart pm clear`.

### Log Commands
To display log levels at the global and module scope, enter `log status`.

![Log status](/images/uart_pm.PNG "log status")

To set a module's log level, enter `log set <module tag> <level>`.
- Acceptable log levels are OFF, ERROR, WARNING, INFO, DEBUG, VERBOSE.
- This command accepts the wildcard (*) argument for the `<module tag>` argument.

### Reflow Commands
To view relevant information about the reflow oven controller, enter `reflow status`.

![Reflow Status](/images/reflow_status.PNG "Reflow Status")

To **start** the reflow process, enter `reflow start`. 
- The oven temperature must be less than the reach temperature of the cooldown phase to start the reflow process, otherwise an error message is shown.

To **stop** the reflow process and turn PWM off at any point in time, enter `reflow stop`.

To set one or more PID parameters (Kp, Ki, Kd, Tau), enter `reflow set <param> <value> [param2 value2 ...]`. 
- Note: PID parameters adjusted using the `reflow set` command are not saved in flash memory and are overwritten to their default values upon reset.

## User Safety 
Safety must be a priority when using this project. Please do not leave the reflow oven unattended during the reflow process. In addition, the following safety measures are included within the software:
  - The reflow process starts **only** when the thermocouple reads a valid temperature below the cooldown temperature (default value = 35°C). 
  - If the controller reads an invalid temperature **at any point** in the reflow process, the process shuts down and the relay is turned off. 
  - Users can manually turn off the reflow process by entering `reflow stop` from a serial terminal (see [Reflow Commands](#reflow-commands)).
  - The real-time plotter, [plot_temp.py](plot_temp.py), will automatically transmit `reflow stop` to stop the reflow process when either the data can not be parsed or the user closes the animation window. 

## Credits 
- Prof. Dave Marples and Timothy Woo for inspiring me with their open-source reflow controller projects ([Leater](https://github.com/mubes/leater) and [Reflowduino](https://github.com/botletics/Reflowduino)).
- Philip Salmony for his [digital PID implementation](https://github.com/pms67/PID).
- Gene Schroeder for his [series on bare-metal programming](https://youtube.com/playlist?list=PL4cGeWgaBTe155QQSQ72DksLIjBn5Jn2Z).
- Dr. Miro Samek for his [active object](https://youtube.com/playlist?list=PLPW8O6W-1chx8Y7Oq2gOE0NUPXmQxu2Wr) and [state machine](https://youtube.com/playlist?list=PLPW8O6W-1chxym7TgIPV9k5E8YJtSBToI) tutorials. 

## Additional Resources
- [Basics on Reflow Soldering](https://www.x-toaster.com/resources/basics-on-reflow-soldering/)
- [PID Tuning for Toaster Reflow Ovens](https://www.x-toaster.com/resources/pid-tuning-for-toaster-reflow-oven/)
- General Control Theory and Implementation:
  - [Brian Douglas Lecture Videos](https://www.youtube.com/channel/UCq0imsn84ShAe9PBOFnoIrg)
  - [Control Guru](https://controlguru.com/table-of-contents/)
  - [Control Tutorials for MATLAB and Simulink](https://ctms.engin.umich.edu/CTMS/index.php?aux=Home)
- [Nucleo-L476RG Pinout Diagram](https://os.mbed.com/platforms/ST-Nucleo-L476RG/)
- [SparkFun Thermocouple Breakout Hookup Guide](https://learn.sparkfun.com/tutorials/max31855k-thermocouple-breakout-hookup-guide/all)
