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

ncs = [10, 30, 100, 300, 1000, 3000, 10000, 30000, 100000, 300000, 1000000]

ops_new = load_numbers("./mutex/ncs/new.txt")
ops_old = load_numbers("./mutex/ncs/old.txt")
ops_tcs= load_numbers("./mutex/ncs/tcs.txt") 
ops_tcb= load_numbers("./mutex/ncs/tcb.txt") 

ops_new = np.array(ops_new) / 1e6
ops_old = np.array(ops_old) / 1e6
ops_tcs = np.array(ops_tcs) / 1e6
ops_tcb = np.array(ops_tcb) / 1e6

# 横坐标转换为等间距的对数坐标
log_x = np.log2([2, 4, 8, 16, 32, 64, 128, 256])  # 定义关键点的对数刻度

# 绘制折线图
plt.rcParams.update({'font.family': 'Times New Roman','font.size': 25})
matplotlib.rcParams['pdf.fonttype'] = 42
plt.figure(figsize=(7.5, 4.5))

plt.plot(ncs, ops_old,linestyle='-', marker='o',markersize=8,linewidth=2,color='red', label='Baseline')
plt.plot(ncs, ops_tcs,linestyle='-', marker='^',markersize=8,linewidth=2,color='green', label='TCLock SP')
plt.plot(ncs, ops_tcb,linestyle='-', marker='X',markersize=8,linewidth=2,color='royalblue', label='TCLock B')
plt.plot(ncs, ops_new,linestyle='-', marker='s',markersize=8,linewidth=2,color='orange', label='BCD ')

# 设置对数刻度的横轴
plt.xscale('log', base=10)
plt.xticks([10, 1e2,1e3,1e4,1e5,1e6])   # 只显示指定刻度
plt.yticks([1, 2, 3, 4, 5])
plt.ylim((0,4))

ax = plt.gca()
ax.spines['top'].set_visible(False)
ax.spines['right'].set_visible(False)

plt.tick_params(axis='x', which='both', labelbottom=True, pad=10)
plt.tick_params(axis='both', direction='in', length=6)

# 添加标题、标签和图例
# plt.title("Performance vs NCS",pad=20)
plt.xlabel("Cycles in NCS")
plt.ylabel("M Ops/sec")
# plt.legend(loc=1, fontsize=15)

# 显示网格和图形
plt.grid(True, which="both", linestyle='dotted', linewidth=0.1)       
plt.tight_layout()

plt.savefig('mutex/ncs/result.pdf', format='pdf')
plt.show()
