import serial
import csv
import time
import datetime

#  Update this with your Arduino's COM port
COM_PORT = "COM6"  # Windows: "COM3", Mac/Linux: "/dev/ttyUSB0"
BAUD_RATE = 9600    # Must match Serial.begin() in Arduino
timestamp = datetime.datetime.now().strftime("%Y-%m-%d_%H-%M")
OUTPUT_FILE = f"data_{timestamp}.csv"

# Open the serial connection
ser = serial.Serial(COM_PORT, BAUD_RATE, timeout=1)
time.sleep(2)  # Wait for Arduino to initialize

# Open CSV file for writing
with open(OUTPUT_FILE, mode="w", newline="") as file:
    writer = csv.writer(file)
    writer.writerow(["Timestamp", "Sensor1", "Sensor2"])  # CSV header

    print(" Reading from Serial and saving to CSV (Press Ctrl+C to stop)...")

    try:
        while True:
            line = ser.readline().decode().strip()  # Read & decode serial input
            if line:
                data = line.split(",")  # Split data if comma-separated
                timestamp = time.strftime("%H:%M:%S")  # Add timestamp
                writer.writerow([timestamp] + data)  # Save to CSV
                print(f"{timestamp}, {data}")  # Print live data
    except KeyboardInterrupt:
        print("\n Stopped by user.")

# Close the serial connection
ser.close()
print(f" Data saved to {OUTPUT_FILE}")
