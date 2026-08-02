import serial
import serial.tools.list_ports
import time
import csv

def main():
    print("Available Ports: ")
    print(serial.tools.list_ports)
    port: str = str(input("Enter port name: "))

    ser = serial.Serial(port, 115200)
    time.sleep(2)
    ser.reset_input_buffer()

    NUM_SAMPLES: int = 5000
    
    with open("ppg.csv", "w", newline="") as csv_file:
        writer = csv.writer(csv_file)
        writer.writerow(["sample_num", "ir"])

        for i in range(NUM_SAMPLES):
            value = int(ser.readline().decode().strip())
            writer.writerow([str(i), str(value)])

    csv_file.close()
    
    print("Done!")


if __name__ == "__main__":
    main()
