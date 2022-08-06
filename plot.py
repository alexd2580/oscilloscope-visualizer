import pandas as pd
import matplotlib.pyplot as plt

data = pd.read_csv('long.csv')

sample = data[["Sample"]]
cols = ["hz","idx","noize","avg","thresh","cur","sd","is_beat"]

ax1 = plt.subplot(3, 1, 1)
data1_cols = [x + "1" for x in cols]
ax1.plot(sample, data[["noize1"]], label="noize")
ax1.plot(sample, data[["avg1"]], label="avg")
ax1.plot(sample, data[["thresh1"]], label="thresh")
ax1.plot(sample, data[["cur1"]], label="cur")
ax1.plot(sample, data[["sd1"]], label="sd")
ax1.legend()
ax1_ = ax1.twinx()
ax1_.plot(sample, data[["is_beat1"]], label="beat?", marker=".")

ax2 = plt.subplot(3, 1, 2, sharex=ax1, sharey=ax1)
ax2.plot(sample, data[["noize2"]], label="noize")
ax2.plot(sample, data[["avg2"]], label="avg")
ax2.plot(sample, data[["thresh2"]], label="thresh")
ax2.plot(sample, data[["cur2"]], label="cur")
ax2.plot(sample, data[["sd2"]], label="sd")
ax2.legend()
ax2_ = ax2.twinx()
ax2_.plot(sample, data[["is_beat2"]], label="beat?", marker=".")

ax3 = plt.subplot(3, 1, 3, sharex=ax1, sharey=ax1)
ax3.plot(sample, data[["noize3"]], label="noize")
ax3.plot(sample, data[["avg3"]], label="avg")
ax3.plot(sample, data[["thresh3"]], label="thresh")
ax3.plot(sample, data[["cur3"]], label="cur")
ax3.plot(sample, data[["sd3"]], label="sd")
ax3.legend()
ax3_ = ax3.twinx()
ax3_.plot(sample, data[["is_beat3"]], label="beat?", marker=".")

plt.show()
