import pandas as pd
import numpy as np
import matplotlib.pyplot as plt

def main():
    data = pd.read_csv("D:\\ALI007\\Programming\\Summer Project\\Python\\ppg_samples.csv")
    print(data.head())

    ir = data["ir"].to_numpy()

    #DC Removal
    # dc_filtered_data = exponential_dc_removal(ir)

    plt.figure()
    plt.plot(ir)
    plt.title("MAX30102 SENSOR VALUES")
    plt.ylabel("Raw IR Values")

    manager = plt.get_current_fig_manager()
    manager.full_screen_toggle()

    plt.show()



# def exponential_dc_removal(raw_data) -> np.np_1darray[np.Any]:
#     dc = 0
#     a  = 0.0125


if __name__ == "__main__":
    main()