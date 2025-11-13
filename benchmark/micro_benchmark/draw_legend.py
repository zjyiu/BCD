import matplotlib.pyplot as plt
import matplotlib

matplotlib.rcParams['pdf.fonttype'] = 42
plt.rcParams.update({'font.family': 'Times New Roman','font.size': 15})# 创建一个画布

fig, ax = plt.subplots()

# 虚拟画几条线用于图例（不显示它们）

ax.plot([],[],linestyle='-', marker='o',markersize=8,linewidth=2,color='red', label='Baseline')
ax.plot([],[],linestyle='-', marker='^',markersize=8,linewidth=2,color='green', label='TCLock SP')
ax.plot([],[],linestyle='-', marker='X',markersize=8,linewidth=2,color='royalblue', label='TCLock B')
ax.plot([],[],linestyle='-', marker='s',markersize=8,linewidth=2,color='orange', label='BCD ')

# 关闭坐标轴
ax.axis('off')

# 只显示图例
legend = ax.legend(ncol=4,loc='center',frameon=False)

# 调整画布大小，让图例显得更清晰
fig.set_size_inches(15, 0.3)

plt.savefig('legend.pdf', format='pdf')
plt.show()
