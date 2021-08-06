# Reflow Oven Controller
PCB designers today often utilize SMT components to minimize the physical size of the circuit board. The industry standard of soldering SMT components is through [reflow soldering](https://en.wikipedia.org/wiki/Reflow_soldering) using a reflow oven. Hobbyists, however, often perform SMT soldering using a hot air gun, a hot plate, a soldering iron, or combination thereof due to cost. Reflow ovens on the market currently range from $300 CAD up to $1500 CAD, not including [necessary modifications](https://hackaday.io/project/175048-t-962a-reflow-oven-modifications). Since reflow soldering is the most time-efficient, reliable, and safest method of SMT soldering, a Nucleo-L476RG PID reflow oven controller was designed to transform any low-cost, convectional toaster oven into a full-fledged reflow oven. 

![Reflow Test](/images/reflow_test.jpg "Reflow Test")

## Table of Contents
- [Reflow Oven Controller](#reflow-oven-controller)
  - [Table of Contents](#table-of-contents)
  - [Installation](#installation)
      - [Real-time Plotting using Python](#real-time-plotting-using-python)
  - [Usage](#usage)
    - [Materials Required](#materials-required)
    - [Connections (based on configuration file)](#connections-based-on-configuration-file)
    - [PID Tuning](#pid-tuning)
  - [Command-line Interface](#command-line-interface)
    - [UART Commands](#uart-commands)
    - [Log Commands](#log-commands)
    - [Reflow Commands](#reflow-commands)
  - [Data Logging](#data-logging)
  - [Credits](#credits)

## Installation 
1. Clone this repository using `git clone https://github.com/timbitss/reflow-oven-controller.git`
2. Install and open [STM32CubeIDE](https://www.st.com/en/development-tools/stm32cubeide.html).
3. Import the project to the current workspace by selecting File->General->Existing Projects into Workspace.
4. Build the project. If no build errors exist, flash the project using the run command. 
#### Real-time Plotting using Python
5. [Create a virtual environment](https://packaging.python.org/guides/installing-using-pip-and-virtual-environments/#creating-a-virtual-environment) in the project repository.
6. Install the necessary packages in the new virtual environment by entering `python3 -m pip install -r requirements.txt` for Unix/macOS users and `py -m pip install -r requirements.txt` for Windows users.

## Usage
### Materials Required
| Component                                                                 | Qty           | 
| :-----------------------------------------------------------------------: | :-----------: | 
| [>1000 W Convection Oven](https://tinyurl.com/5y66d4mf)<sup>1</sup>       | 1             | 
| [Male-Female Jumper Wires](https://tinyurl.com/ka4yxnmx)                  | 6             |   
| [K-Type Thermocouple](https://www.digikey.ca/short/b2zfmn1b)              | 1             |
| [SparkFun Thermocouple Breakout](https://www.sparkfun.com/products/13266) | 1             |
| [STM32 Nucleo-L476RG Board](https://www.digikey.ca/short/v0bqhwhd)        | 1             |
| [IoT Power Relay](https://www.digikey.ca/short/4q59345b)                  | 1             |

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
2. Place the 'hot' end of the thermocouple centered inside the oven.
3. Turn the oven temperature knob to its maximum **bake** temperature.
4. Run the real-time plotting script, [`plot_temp.py`] (plot_temp.py), and let the reflow process complete.
5. Tune one or more PID parameters based on the system's response using the `reflow set` CLI command (see [Reflow Commands](#reflow-commands)).
6. Repeat steps 3 and 4 until a reasonable reflow thermal profile is achieved.

Once the PID tuning process is complete, the oven is ready for reflow applications. It is important to **keep a record of the final PID settings**, as they must be manually inputted again if the Nucleo board resets or powers off. 

<sup>1</sup> It is recommended to purchase a used toaster oven to save on costs.

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
- The oven temperature must be less than the reach temperature of the cooldown phase (see profile.c) to start the reflow process, otherwise an error message is shown.

To **stop** the reflow process and turn PWM off at any point in time, enter `reflow stop`.

To set one or more PID parameters (Kp, Ki, Kd, Tau), enter `reflow set <param> <value> [param2 value2 ...]`. 
- Note: PID parameters adjusted using the `reflow set` command are not saved in flash memory and are overwritten to their default values upon reset.

## Data Logging
Users are recomended to run the [plot_temp.py](plot_temp.py)) program to view the oven temperature and PID terms in real-time. The program automatically starts the reflow process by transmitting the `reflow start` command. After the reflow process has completed or the animation window is closed, both the plot and the raw data in CSV format are saved for further review. An example of a completed reflow process is shown below: 

For added safety, the program transmits the `reflow stop` command to stop the reflow process upon error in parsing data or when the animation window is closed by the user. 


## Credits
