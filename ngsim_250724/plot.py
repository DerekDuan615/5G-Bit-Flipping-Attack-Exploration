import matplotlib.pyplot as plt
import numpy as np

# Generate x values
x = np.linspace(0, 8, 100)

# Exponential decay equation: y = 2^(-0.5x)
y_equation = 2**(-0.5 * x)

# Sample data points (you can replace these with your actual data)
x_data = np.array([2,2,2,4,4,4,6,6,6,8,8,8])
y_data = np.array([0.514, 0.482, 0.482, 0.280, 0.258, 0.284, 0.110, 0.090, 0.108, 0.064, 0.034, 0.054])

# Create the plot
plt.figure(figsize=(10, 6))
plt.plot(x, y_equation, 'b-', linewidth=3, label='Exp. Decay: y = 2^(-0.5x)')
plt.scatter(x_data, y_data, color='red', s=70, label='Mutation Rate', zorder=5)

plt.xlabel('# of Flips', fontsize=16)
plt.xticks(np.arange(0, 10, 2), fontsize=20)
plt.ylabel('Rate', fontsize=16)
plt.yticks(fontsize=20)
# plt.title('Mutation Rate of Bit-Flipping Attacks')
plt.legend(fontsize=16)
plt.grid(True, alpha=0.3)
plt.show()