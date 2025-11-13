import matplotlib
import matplotlib.pyplot as plt
import numpy as np

def load_numbers(filepath):
    arr = []
    with open(filepath, "r") as f:
        for line in f:
            line = line.strip()
            if line:
                arr.append(int(line))
    return arr

threads_number = [1, 2, 4, 6, 8, 12, 16, 20, 24, 28, 32, 40, 48, 56, 64, 80, 96, 112, 128, 192, 256]
ops_new = load_numbers("./mutex/threads/2/new.txt")
ops_old = load_numbers("./mutex/threads/2/old.txt")
ops_tcs= load_numbers("./mutex/threads/2/tcs.txt") 
ops_tcb= load_numbers("./mutex/threads/2/tcb.txt") 

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

plt.plot(threads_number, ops_old,linestyle='-', marker='o',markersize=8,linewidth=2,color='red', label='Baseline')
plt.plot(threads_number, ops_tcs,linestyle='-', marker='^',markersize=8,linewidth=2,color='green', label='TCLock SP')
plt.plot(threads_number, ops_tcb,linestyle='-', marker='X',markersize=8,linewidth=2,color='royalblue', label='TCLock B')
plt.plot(threads_number, ops_new,linestyle='-', marker='s',markersize=8,linewidth=2,color='orange', label='BCD ')


# 设置对数刻度的横轴
plt.xscale('log', base=2)
plt.xticks([1,2, 4, 8, 16, 32, 64, 128, 256], [1,2, 4, 8, 16, 32, 64, 128, 256])   # 只显示指定刻度
plt.yticks([0,1,2,3,4,5,6])
plt.ylim((0,5.5))

ax = plt.gca()
ax.spines['top'].set_visible(False)
ax.spines['right'].set_visible(False)

plt.axvline(x=32, color='black', linestyle='-', linewidth=1)

plt.tick_params(axis='both', direction='in', length=6)

# 添加标题、标签和图例
# plt.title("Performance vs Thread Count",pad=20)
plt.xlabel("# of threads")
plt.ylabel("M Ops/sec")
# plt.legend(ncol=2,labelspacing=0.4,columnspacing=0.4,  fontsize=15)

# 显示网格和图形
plt.grid(True, which="both", linestyle='dotted', linewidth=0.1)       
plt.tight_layout()

plt.savefig('mutex/threads/2/result.pdf', format='pdf')
plt.show()
