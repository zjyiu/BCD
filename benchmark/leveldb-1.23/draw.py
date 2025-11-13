import matplotlib.pyplot as plt
import numpy as np
import matplotlib

def load_numbers(filepath):
    arr = []
    with open(filepath, "r") as f:
        for line in f:
            line = line.strip()
            if line:
                arr.append(float(line))
    return arr

threads_number = [4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 44, 48, 52, 56, 60, 64]

ops_new = load_numbers("new.txt")
ops_old = load_numbers("old.txt")
ops_tcs = load_numbers("tcs.txt")
ops_tcb = load_numbers("tcb.txt")


# 创建图形
plt.rcParams.update({'font.family': 'Times New Roman','font.size': 25})
matplotlib.rcParams['pdf.fonttype'] = 42
plt.figure(figsize=(7.5, 4.5))

# 绘制OLD数据的折线图
plt.plot(threads_number, ops_old,linewidth=2, label='Baseline', marker='o',markersize=8, linestyle='-', color='red')
plt.plot(threads_number, ops_tcs,linewidth=2, label='TCLock SP', marker='^',markersize=8, linestyle='-', color='green')
plt.plot(threads_number, ops_tcb,linewidth=2, label='TCLock B', marker='X',markersize=8, linestyle='-', color='royalblue')
plt.plot(threads_number, ops_new,linewidth=2, label='BCD', marker='s',markersize=8, linestyle='-', color='orange')
 
ax = plt.gca()
ax.spines['top'].set_visible(False)
ax.spines['right'].set_visible(False)

plt.axvline(x=32, color='black', linestyle='-', linewidth=1)

plt.tick_params(axis='both', direction='in', length=6)

# 添加标题和标签
plt.xlabel('# of threads')
plt.ylabel("\u03BCs/op")

plt.ylim((0,100))
plt.xticks([4, 16, 28, 40, 52, 64])

# 添加网格
plt.grid(True, which="both", linestyle='dotted', linewidth=0.1)   

# 添加图例
plt.legend(fontsize=15)
plt.tight_layout()


# 保存图表为PDF文件
plt.savefig('result.pdf', format='pdf')

# 显示图表
plt.show()
