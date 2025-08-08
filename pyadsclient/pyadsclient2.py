# -*- coding: utf-8 -*-
"""
Created on Wed Aug 14 14:07:32 2024

@author: Jing
"""


import pyads
#import winsound
import csv

import tkinter as tk
import sys
import time


### take the output file name argument
outfile_name = "Default"
if len(sys.argv) > 1:
   print(f"Arguments passed: {sys.argv[1:]}")
   outfile_name = sys.argv[1]
else:
   print("No arguments provided.")


print(outfile_name)
# Write the header to the csv file
with open(outfile_name+"_py-event-timestamp.csv", 'a', newline='') as file:
    writer = csv.writer(file)
    writer.writerow(['timestamp', 'entity(0:controller 1:sensor)', 'event(0:go home 1:start 2:detect)']) 



### PLC connection ###
TARGET_NETID = '5.53.143.24.1.1'
plc_port = 851
#plc = pyads.Connection(TARGET_NETID, pyads.PORT_TC3PLC1)
plc = pyads.Connection(TARGET_NETID, plc_port)
plc.open()
print("OPENED")


print(plc.ams_net_port)
plc.read_state()
print("READ")

### tkinter windows ###
status_stop = 0
status_run = 1
status_reset = 2
PLC_status = status_stop

def save_data_to_csv():
    global outfile_name
    #print("outfile_name : " +  outfile_name+"_py-event-timestamp.csv")
    with open(outfile_name+"_py-event-timestamp.csv", mode='a', newline='') as file:
        writer = csv.writer(file)
        # write the detect event in the event timestamp csv
        # get current time since epoch in millisecond
        epoch_time = int(time.time()*1000)
        writer.writerow([epoch_time, "1", "2"])

def sensing():
    global PLC_status
    if PLC_status == status_run:
        sensorOut = plc.read_by_name('GVL_input.sensor[2]')
        if sensorOut:
            #frequency = 2500    # Set Frequency To 2500 Hertz
            #duration = 1000     # Set Duration To 1000 ms == 1 second
            #winsound.Beep(frequency, duration)
            #print("Detect!!!")
            Result.config(fg="white", bg="red")
            Result["text"] = "Target Detect !!!"
            PLC_status = status_reset
            # record the detect event
            save_data_to_csv()
            
    elif PLC_status == status_reset:
        sensorOut = plc.read_by_name('GVL_input.sensor[2]')
        if not sensorOut:
            Result.config(fg="black", bg="white")
            Result["text"] = "Undetect"
            PLC_status = status_run
    
    root.after(20, sensing)

def start_sensing():
    global PLC_status
    print("Start")
    Result.config(fg="black", bg="white")
    Result["text"] = "Undetect"
    PLC_status = status_run

def stop_sensing():
    global PLC_status
    PLC_status = status_stop
    print("Stop")
    Result.config(fg="black", bg="white")
    Result["text"] = ""

def close_window():
    print("Close tkinter window!!")
    root.destroy()





#def reset_sensing():   
#    print("Reset")
#    global PLC_status
#    PLC_status = status_reset
#    Result.config(fg="black", bg="white")
#    Result["text"] = "Resetting"
    
    
    



root = tk.Tk()
root.geometry("1000x700")
root.title("Target Detection")



Description = tk.Label(root, text="Target Status : ", font=('Arial', 12))
# Set the label position. Here use pack as example
Description.pack(padx=0, pady=20)

   







#Result Display
Result = tk.Label(root, text="", height=2, font=('Arial', 40), bg="white")
Result.pack(padx=0, pady=30, fill=tk.X)

# Button Control
buttonframe = tk.Frame(root)
buttonframe.columnconfigure(0, weight = 1)
buttonframe.columnconfigure(1, weight = 1)
buttonframe.columnconfigure(2, weight = 1)

btn_start = tk.Button(buttonframe, text="Start", font=('Arial', 18), command=start_sensing)
btn_start.grid(row=0, column=0, sticky=tk.W+tk.E)

#btn_reset = tk.Button(buttonframe, text="Reset", font=('Arial', 18), command=reset_sensing)
#btn_reset.grid(row=0, column=1, sticky=tk.W+tk.E)

btn_stop = tk.Button(buttonframe, text="Stop", font=('Arial', 18), command=stop_sensing)
btn_stop.grid(row=0, column=1, sticky=tk.W+tk.E)

btn_exit = tk.Button(buttonframe, text="Exit", font=('Arial', 18), command=close_window)
btn_exit.grid(row=0, column=2, sticky=tk.W+tk.E)

buttonframe.pack(fill='x')







root.after(20, sensing)

root.mainloop()

print("Program Terminate!!")













# =============================================================================
# while True:
#     sensorOut = plc.read_by_name('GVL_input.sensor[2]')
#     if sensorOut:
#         
#         frequency = 2500    # Set Frequency To 2500 Hertz
#         duration = 1000     # Set Duration To 1000 ms == 1 second
#         winsound.Beep(frequency, duration)
#         print("Detect!!!")
# =============================================================================


























