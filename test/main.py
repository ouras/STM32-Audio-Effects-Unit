import matplotlib.pyplot as plt
import numpy as np

path = 'out.bin'
data = np.fromfile(path, dtype='uint16')

plt.plot(data)
plt.show()