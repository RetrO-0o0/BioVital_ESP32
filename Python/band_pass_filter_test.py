import pandas as pd
import matplotlib.pyplot as plt

class IIRFilter:

    def __init__(self, b, a):

        self.b0 = b[0]
        self.b1 = b[1]
        self.b2 = b[2]

        self.a1 = a[1]
        self.a2 = a[2]

        self.x1 = 0
        self.x2 = 0

        self.y1 = 0
        self.y2 = 0


    def process(self, x):

        y = (
            self.b0*x + self.b1*self.x1 + self.b2*self.x2 - self.a1*self.y1 - self.a2*self.y2
        )

        self.x2 = self.x1
        self.x1 = x

        self.y2 = self.y1
        self.y1 = y

        return y

# High Pass Fc=0.5Hz Fs=100Hz Order=2

hp_b = [
    0.978030479206559,
   -1.956060958413119,
    0.978030479206559
]

hp_a = [
    1.0,
   -1.955578240315035,
    0.956543676511203
]



# Low Pass Fc=4Hz Fs=100Hz Order=2

lp_b = [
    0.013359200027857,
    0.026718400055713,
    0.013359200027857
]

lp_a = [
    1.0,
   -1.647459981076977,
    0.700896781188403
]



high_pass = IIRFilter(hp_b, hp_a)
low_pass = IIRFilter(lp_b, lp_a)

file_path = r"D:\ALI007\Programming\Summer Project\Python\ppg_samples.csv"
data = pd.read_csv(file_path)


ir = data["ir"].values

filtered = []

for sample in ir:

    hp = high_pass.process(sample)

    lp = low_pass.process(hp)

    filtered.append(lp)

plt.figure()
plt.plot(filtered)
plt.title("Filtered PPG Signal")
plt.xlabel("Sample")
plt.ylabel("Amplitude")
plt.grid(True)

manager = plt.get_current_fig_manager()
manager.full_screen_toggle()

plt.show()