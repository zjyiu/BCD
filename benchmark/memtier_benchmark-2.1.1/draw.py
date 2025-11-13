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

threads_number = [4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 44, 48, 52, 56]

ops_new = load_numbers("result_new.txt")

ops_old= load_numbers("result_old.txt")

ops_new = np.array(ops_new) / 1e6
ops_old = np.array(ops_old) / 1e6

# 创建图形
plt.rcParams.update({'font.family': 'Times New Roman','font.size': 20})
matplotlib.rcParams['pdf.fonttype'] = 42
plt.figure(figsize=(7.5, 4.5))

# 绘制OLD数据的折线图
plt.plot(threads_number, ops_old, label='Baseline', marker='o',markersize=8,linewidth=2, linestyle='-', color='red')

# 绘制NEW数据的折线图
plt.plot(threads_number, ops_new, label='BCD', marker='s',markersize=8, linewidth=2, linestyle='-', color='orange')

plt.ylim((0,1.5))

ax = plt.gca()
ax.spines['top'].set_visible(False)
ax.spines['right'].set_visible(False)

plt.axvline(x=32, color='black', linestyle='-', linewidth=1)

plt.tick_params(axis='both', direction='in', length=6)

plt.xticks([4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 44, 48, 52, 56]) 

plt.xlabel('# of threads')
plt.ylabel('M Ops/sec')

# 添加图例
plt.legend(ncol=2,fontsize=15)

# 添加网格
plt.grid(True, which="both", linestyle='dotted', linewidth=0.1)   
plt.tight_layout()

# 保存图表为PDF文件
plt.savefig('result.pdf', format='pdf')

# 显示图表
plt.show()
