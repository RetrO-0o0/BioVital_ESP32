import pandas as pd
import numpy as np
import matplotlib.pyplot as plt

data = pd.read_csv("D:\\ALI007\\Programming\\Summer Project\\Python\\ppg_samples.csv")

ir = data["ir"].to_numpy()

fs = 100

N = len(ir)

fft = np.fft.rfft(ir)

magnitude = np.abs(fft)

freq = np.fft.fftfreq(N, d=1/fs)

half = N // 2

plt.figure(figsize=(12,5))

plt.plot(freq[:half], magnitude[:half])

plt.title("FFT of Raw PPG")

plt.xlabel("Frequency (Hz)")

plt.ylabel("Magnitude")

plt.grid(True)

plt.show()