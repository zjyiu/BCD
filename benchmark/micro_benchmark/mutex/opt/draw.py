import matplotlib.pyplot as plt
import numpy as np
import matplotlib

def load_numbers(filepath):
    arr = []
    with open(filepath, "r") as f:
        for line in f:
            line = line.strip()
            if line:
                arr.append(int(line))
    return arr

threads_number = [1, 2, 4, 6, 8, 12, 16, 20, 24, 28, 32, 40, 48, 56, 64, 80, 96, 112, 128, 192, 256]

ops_ub = load_numbers("./mutex/opt/ub.txt")
ops_vw = load_numbers("./mutex/opt/vw.txt")
ops_new = load_numbers("./mutex/opt/new.txt")
ops_old = load_numbers("./mutex/opt/old.txt")

ops_ub = np.array(ops_ub) / 1e6
ops_vw = np.array(ops_vw) / 1e6
ops_new = np.array(ops_new) / 1e6
ops_old = np.array(ops_old) / 1e6

# 横坐标转换为等间距的对数坐标
log_x = np.log2([2, 4, 8, 16, 32, 64, 128, 256])  # 定义关键点的对数刻度

# 绘制折线图
plt.rcParams.update({'font.family': 'Times New Roman','font.size': 25})
matplotlib.rcParams['pdf.fonttype'] = 42
plt.figure(figsize=(7.5, 4.5))
plt.plot(threads_number, ops_old,linestyle='-', marker='o',markersize=8,linewidth=2,color='red', label='Baseline')
plt.plot(threads_number, ops_ub,linestyle='-', marker='X',markersize=8, linewidth=2,color='grey', label='+ delegation')
plt.plot(threads_number, ops_vw,linestyle='-', marker='^',markersize=8, linewidth=2,color='purple', label='+ velvet wait queue')
plt.plot(threads_number, ops_new,linestyle='-', marker='s',markersize=8,linewidth=2,color='orange', label='+ delegate election')

# 设置对数刻度的横轴
plt.xscale('log', base=2)
plt.xticks([1, 2, 4, 8, 16, 32, 64, 128, 256], [1, 2, 4, 8, 16, 32, 64, 128, 256])   # 只显示指定刻度
# plt.yticks([0,1,2,3,4,5])
plt.ylim((0,4))

ax = plt.gca()
ax.spines['top'].set_visible(False)
ax.spines['right'].set_visible(False)

plt.axvline(x=32, color='black', linestyle='-', linewidth=1)

plt.tick_params(axis='both', direction='in', length=6)

# 添加标题、标签和图例
# plt.title("Performance vs Thread Count",pad=20)
plt.xlabel("# of threads")
plt.ylabel("M Ops/sec")
plt.legend(fontsize=15)

# 显示网格和图形
plt.grid(True, which="both", linestyle='dotted', linewidth=0.1)       
plt.tight_layout()

plt.savefig('mutex/opt/result.pdf', format='pdf')
plt.show()
