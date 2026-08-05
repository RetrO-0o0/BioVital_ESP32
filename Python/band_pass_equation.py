from scipy.signal import butter
import numpy as np

def main():
    # High-Pass
    print("High-Pass")

    fs    = 100.0
    fc    = 0.5
    order = 2

    b, a = butter(N=order, Wn=fc, btype='highpass', fs=fs)
    np.set_printoptions(precision=15)
    print(f"Numerator (a) :\n{a} \nNumerator (b) :\n{b}")

    # Low-Pass
    print("\nLow-Pass")

    fs    = 100.0
    fc    = 4
    order = 2

    b, a = butter(N=order, Wn=fc, btype='lowpass', fs=fs)
    np.set_printoptions(precision=15)
    print(f"Numerator (a) :\n{a} \nNumerator (b) :\n{b}")


if __name__ == "__main__":
    main()