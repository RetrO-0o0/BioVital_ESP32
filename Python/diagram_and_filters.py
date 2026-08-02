import pandas as pd
import numpy as np
import matplotlib.pyplot as plt

def main():
    data = pd.read_csv("D:\\ALI007\\Programming\\Summer Project\\Python\\ppg_samples.csv")
    # print(data.head())

    ir = data["ir"].to_numpy()

    #DC Removal
    dc_filtered_data = np.array(exponential_dc_removal(ir))

    plt.figure()
    plt.plot(dc_filtered_data)
    plt.title("MAX30102 SENSOR VALUES")
    plt.xlabel("Samples")
    plt.ylabel("DC Filtered Values")

    manager = plt.get_current_fig_manager()
    manager.full_screen_toggle()

    plt.show()



def exponential_dc_removal(raw_data) -> np.ndarray[np.any]:
    a  = 0.0248
    y  = []
    dc = raw_data[0]

    for x in raw_data:
        dc = dc + a * (x - dc)
        y.append(x - dc)

    return y


if __name__ == "__main__":
    main()