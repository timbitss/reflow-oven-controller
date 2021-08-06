import os
import serial
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
import matplotlib.ticker as ticker
import csv
import time

# Sampling time in ms.
Tsample = 500

# Start and stop messages messages to transmit. 
start_msg = b'log set * OFF\n log set REFLOW INFO\n reflow stop\n reflow start\n'
stop_msg = b'reflow stop\n'

# Expected response message from microcontroller.
start_response = '\x1b[0m\x1b[KStarting reflow process\r\n'

# Path for CSV file.
csv_path = "./csv/temp_ctrl.csv"

# Path for temperature plot.
plot_path = "./imgs/temp_ctrl.png"

# Open serial port.
ser = serial.Serial(port='COM3', baudrate=115200)

# Data storage.
state_lst = []         # State   
sp_lst = []            # Setpoint temperature.
pv_lst = []            # Measured temperature.
proportional_lst = []  # Proportional term.
integral_lst = []      # Integral term.
derivative_lst = []    # Derivative term.
pwm_lst = []           # PWM duty cycle.
time_lst = []          # Time samples.

# Check which port is being used.
print("Connected to", ser.name)

# Define Figure and Axes.
fig, (ax1, ax2) = plt.subplots(nrows=1, ncols=2, figsize=(12, 6))

# Define Line2D objects. 
proportional_line, = ax1.plot([], [], 'y', label='P', alpha=1, linewidth=2)
integral_line, = ax1.plot([], [], 'm', label='I', alpha=1, linewidth=2)
derivative_line, = ax1.plot([], [], 'c', label='D', alpha=1, linewidth=2) 
pwm_line, = ax1.plot([], [], 'b', label='PWM', alpha=0.7, linewidth=2)
sp_line, = ax2.plot([], [], 'r', label='Setpoint', linewidth=2)
pv_line, = ax2.plot([], [], 'g', label='Measured', linewidth=2)

# Define Text objects.
time_txt = ax2.text(x=280, y=-40, s="Time: {} s".format(0))
state_txt = ax2.text(x=280, y=-55, s="State: {}".format('NA'))

# Initialize animation function.
def init():
    # Set x and y view limits.
    ax1.set_xlim(0, 450)
    ax2.set_xlim(0, 450)
    ax1.set_ylim(-5000, 5000)
    ax2.set_ylim(0, 300)
    # Set title.
    ax1.set_title('Reflow Oven Controller', fontweight='bold')
    ax2.set_title('Oven Temperature', fontweight='bold')
    # Set x and y labels.
    ax1.set_xlabel("Time (s)")
    ax2.set_ylabel("Temperature (°C)")
    ax2.set_xlabel("Time (s)")
    # Set tick locators.
    ax1.yaxis.set_major_locator(ticker.MultipleLocator(1000))
    ax1.yaxis.set_minor_locator(ticker.MultipleLocator(500))
    ax2.yaxis.set_major_locator(ticker.MultipleLocator(50))
    ax2.yaxis.set_minor_locator(ticker.MultipleLocator(10))
    # Turn visibility of right and top spines off. 
    ax1.spines['right'].set_visible(False)
    ax1.spines['top'].set_visible(False)
    ax2.spines['right'].set_visible(False)
    ax2.spines['top'].set_visible(False)
    # Display ticks only on left y-axis and bottom x-axis.
    ax1.yaxis.set_ticks_position('left')
    ax1.xaxis.set_ticks_position('bottom')
    ax2.yaxis.set_ticks_position('left')
    ax2.xaxis.set_ticks_position('bottom')
    # Setup legend.
    ax1.legend(ncol=4, loc='upper center')
    ax2.legend()
    # Auto-adjust padding.
    plt.tight_layout()
    # Leave whitespace at bottom for extra text.
    plt.subplots_adjust(bottom=0.15)
    
    # Flush input serial buffer.
    ser.reset_input_buffer()

    # Return iterable of objects to be re-drawn for blitting algorithm.
    return sp_line, pv_line, proportional_line, integral_line, derivative_line, pwm_line

# For some reason, must turn on grid outside of init().
ax1.grid()
ax2.grid()

# Get a time reference (s) for animation.
timeref = time.time()

# Function called each frame.
def update(frame):
    # Read and parse line from serial buffer.
    num_attempts = 0
    while True:
        line = ser.readline().decode('UTF-8')
        words = line.split()
        try:
            state = words[3]
            # NOTE: Numbers still in string format.
            sp, pv, proportional, integral, derivative, pwm = words[4:] 
            # print(state, sp, pv, proportional, integral, derivative, pwm)
        except:
            print('Failed to parse values, trying again...')
            num_attempts = num_attempts + 1
            if num_attempts == 3:
                print('Reflow process stopped.')
                print('Close window to save data.')
                ani.pause()
            continue
        break

    # Store data with appropriate conversions.
    state_lst.append(state)
    sp_lst.append(float(sp))
    pv_lst.append(float(pv))
    proportional_lst.append(float(proportional))
    integral_lst.append(float(integral))
    derivative_lst.append(float(derivative))
    pwm_lst.append(float(pwm)) 
    time_lst.append(time.time()-timeref)

    # Update Line2D objects.
    sp_line.set_data(time_lst, sp_lst)
    pv_line.set_data(time_lst, pv_lst)
    proportional_line.set_data(time_lst, proportional_lst)
    integral_line.set_data(time_lst, integral_lst)
    derivative_line.set_data(time_lst, derivative_lst)
    pwm_line.set_data(time_lst, pwm_lst)

    # Update time and state text.
    time_txt.set_text("Time: {:.2f} s".format(time_lst[frame]))
    state_txt.set_text("State: {}".format(state_lst[frame]))

    return sp_line, pv_line, proportional_line, integral_line, derivative_line, pwm_line

# Post reflow start command and wait for response.
ser.write(start_msg)
num_start_attempts = 0
while True:
    line = ser.readline().decode('UTF-8')
    print(line)
    if line == start_response:
        print('Start message received.')
        break
    num_start_attempts = num_start_attempts + 1
    if num_start_attempts == 20:
        print('Did not receive response from microcontroller.')
        ser.write(stop_msg)
        exit()

# Start animation.
ani = FuncAnimation(fig, update, init_func=init, blit=False, interval=Tsample-20)
plt.show()

# Close serial connection once window is closed.
print("Number of bytes remaining in rx buffer:", ser.in_waiting)
print("Closing serial connection.")
ser.write(stop_msg) # For safety measures, turn reflow controller off regardless of status.
ser.close()

# Save plot.
version_num = 1

if os.path.isfile(plot_path) == False:
    fig.savefig(plot_path)
else:
    filename, ext = os.path.splitext(plot_path)
    while os.path.isfile(filename + '_' + str(version_num) + ext) == True:
        version_num += 1
    plot_path = filename + '_' + str(version_num) + ext
    fig.savefig(plot_path)

# Save CSV file.
if os.path.isfile(csv_path) == True:
    csv_path = './csv/temp_ctrl_{}.csv'.format(version_num)

with open(csv_path, 'w', newline='') as f:
    writer = csv.writer(f)
    writer.writerow(["State", "Time (s)", "P", "I", "D", "PWM/4095", "Set point (°C)",
                    "Measured (°C)", "Sample Period (s)"])
    for s_num in range(len(pv_lst)):
        if s_num == 0:
            writer.writerow([state_lst[s_num], time_lst[s_num], proportional_lst[s_num], integral_lst[s_num],
                        derivative_lst[s_num], pwm_lst[s_num], sp_lst[s_num], pv_lst[s_num], Tsample/1000])
        else:
             writer.writerow([state_lst[s_num], time_lst[s_num], proportional_lst[s_num], integral_lst[s_num],
                        derivative_lst[s_num], pwm_lst[s_num], sp_lst[s_num], pv_lst[s_num]])

print("Plot and csv file saved at", plot_path, 'and', csv_path, 'respectively.')
