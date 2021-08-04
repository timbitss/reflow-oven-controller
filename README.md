# Reflow Oven Controller
PCB designers today often utilize SMT components to minimize the physical size of the circuit board. The industry standard and most reliable way of soldering SMT components is through [reflow soldering](https://en.wikipedia.org/wiki/Reflow_soldering) using a reflow oven. Hobbyists, however, often perform SMT soldering using a hot air gun, a hot plate, a soldering iron, or combination thereof due to cost. Reflow ovens on the market currently range from $300 CAD up to $1500 CAD, not including [necessary modifications](https://hackaday.io/project/175048-t-962a-reflow-oven-modifications). Since reflow soldering is the most time-efficient, reliable, and safest method of SMT soldering, a Nucleo-L476RG PID reflow oven controller was designed to transform any low-cost, convectional toaster oven into a full-fledged reflow oven. 

## Module Descriptions
The project is arranged into two main directories: [core/Inc](Core/Inc/) for header files and [core/Src](Core/Src/) for source files. The program is divided into modules for code reuse and ease of complexity. The following dependency diagram illustrates the interaction between the main modules, excluding the logging module: 

<p align="center">
  <img src="https://github.com/timbitss/reflow-oven-controller/blob/main/imgs/reflow_dependencies.png" alt="Module dependencies"/>
</p>

The main modules of the system are described in the following sections with some implementation details. 

### UART ([uart.c](core/src/uart.c))

The UART module configures the UART peripheral for character transmission and reception using interrupts. Specifically, interrupts are enabled for `RXNE` (receive buffer not empty) and `TXE` (transmit buffer empty) flags. 

When the `RXNE` flag is set, the UART ISR copies the received character into a message queue for further processing from the console. Any reception errors (parity, frame overrun, etc.) are recorded in the UART's performance measurements, accessible from the [CLI](#uart-commands). 

When the `TXE` flag is set, the UART ISR copies a character from the **software** transmit buffer, if non-empty, into the **hardware** transmit buffer. A character may be placed into the **software** transmit buffer by calling `uart_putc()`.

### Console ([console.c](core/src/console.c))

The console module processes received characters from the UART peripheral following the active object framework. Upon character reception, the console will copy a character from the message queue into a command line buffer. When the user presses the Enter key, the console sends a "command received" event signal along with the command line buffer to the command handler for further processing. The console module also handles character deletion, toggling logging on/off, and character echoing. 

### Command Handler ([cmd.c](core/src/cmd.c))

The command handler module process command lines following the active object framework. Clients may register commands to the command handler by calling `cmd_register()`. Upon reception of a command line from the console, the command handler will tokenize the command line. 

If the `help` or `?` command was entered, the command handler will print out the help string of each registered command. Likewise, a `<module> <command> help` or `<module> <command> ?` command will display the help string of that particular command. 

If a `<module> pm` command was entered, the command handler will print out the current performance measurements of that particular module. Likewise, a `<module> pm clear` command will clear the performance measurements of that particular module.

Otherwise, the command handler will verify that the `<module> <command> [args]` command has been registered and invoke the appropriate callback function, passing along the variable number of arguments.

See the [User Interface](#user-interface) section for more details on the commands available.

### MAX31855K Thermocouple Digitizer ([MAX31855K.c](core/src/MAX31855K.c))

The MAX31855K thermocouple digitizer module provides an API for reading the K-type thermocouple temperature from a [MAX31855K IC](https://datasheets.maximintegrated.com/en/ds/MAX31855.pdf). 

Data reads from the thermocouple IC may be achieved in SPI RX blocking mode or in SPI DMA non-blocking mode by calling `MAX31855K_RxBlocking()` or `MAX31855K_RxDMA()`, respectively. 

Data read errors such as an open thermocouple connection or an "all-zero" reading are recorded. An error can be converted into a character string for logging purposes by calling `MAX31855K_Err_Str()`. 

Users have the option to read the hot-junction (thermocouple) temperature and/or the cold-junction temperature by calling `MAX31855K_Get_HJ()` or `MAX31855K_Get_CJ()`, respectively.  

### PID Controller ([pid.c](core/src/pid.c))

The PID controller module is a robust C implementation of a digital PID controller adapted from (Philip Salmony)[https://github.com/pms67/PID]. The module is relatively straightforward and does not require much explanation. Computations are performed using floating-point values to utilize the STM32's FPU peripheral. Anti-windup measures and controller output saturation is included in the PID algorithm. 

### Reflow Oven Controller ([reflow.c](core/src/reflow.c))

The reflow oven controller module is the heart of the program. Since the reflow soldering process consists of multiple stages, the reflow oven controller contains a state machine that follows a standard reflow thermal profile. The state diagram is shown below: 

![reflow state diagram](https://github.com/timbitss/reflow-oven-controller/blob/main/imgs/reflow_state_diagram.png "reflow state diagram")

## User Interface
A command-line interface was developed for user interaction at runtime and for safety measures. Users may access the CLI using a serial terminal with the serial line configured for 115200 baud rate, 8 data bits, 1 stop bit, and no parity.

To see the commands that are available, type `help` or `?`.

![Help Command](https://github.com/timbitss/reflow-oven-controller/blob/main/imgs/help_command.PNG "Help Command")

To get help on a specific command within a module, type `<module> <command> help` or `<module> <command> ?`. E.g. entering *log set help* produces the following message:

![Module Help Command](https://github.com/timbitss/reflow-oven-controller/blob/main/imgs/module_cmd_help.PNG "Module Help Command")

### UART Commands
To view performance measures related to the UART module, type `uart pm`.

![uart pm](https://github.com/timbitss/reflow-oven-controller/blob/main/imgs/uart_pm.PNG "uart pm")

To clear performance measures related to the UART module, type `uart pm clear`.

### Log Commands
To display log levels at the global and module scope, type `log status`.

![Log status](https://github.com/timbitss/reflow-oven-controller/blob/main/imgs/uart_pm.PNG "log status")

To set a module's log level, type `log set <module tag> <level>`.
- Acceptable log levels are OFF, ERROR, WARNING, INFO, DEBUG, VERBOSE.
- This command accepts the wildcard (*) argument for the `<module tag>` argument.

### Reflow Commands
To view relevant information about the reflow oven controller, type `reflow status`.

![Reflow Status](https://github.com/timbitss/reflow-oven-controller/blob/main/imgs/reflow_status.PNG "Reflow Status")

To **start** the reflow process, type `reflow start`. 
- The oven temperature must be less than the reach temperature of the cooldown phase (see profile.c) to start the reflow process, otherwise an error message is shown.

To **stop** the reflow process and turn PWM off at any point in time, type `reflow stop`.

To set one or more PID parameters (Kp, Ki, Kd, Tau), type `reflow set <param> <value> [param2 value2 ...]`. 
- Note: PID parameters adjusted using the `reflow set` command are not saved in flash memory and are thus overwritten upon reset.

## Data Logging
Live plotting of the oven temperature and PID terms was achieved using Matplotlib and pySerial (see [plot_temp.py](plot_temp.py)). The python script starts the reflow process on its own by transmitting the `reflow start` command. After the reflow process has completed or the animation window is closed, both the plot and the raw data in CSV format are saved. An example of a completed reflow process is shown below: 

![Temperature Plot](https://github.com/timbitss/reflow-oven-controller/blob/main/imgs/temp_plot.png "Temperature Plot")

Users are recommended to analyze the reflow plots during the PID tuning process for optimal system performance.

For added safety, the program transmits the *reflow stop* command to stop the reflow process upon error in parsing data or when the animation window is closed by the user. 


## Installation and Flash 
1. Clone this repository into an appropriately-named project folder.
2. Inside the STM32CubeIDE, import the project to the current workspace by selecting File->General->Existing Projects into Workspace
3. Build and run the project. 
