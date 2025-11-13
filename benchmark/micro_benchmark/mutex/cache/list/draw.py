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

cacheline = [1, 2, 4, 8, 16, 32, 64, 128]
ops_new = load_numbers("./mutex/cache/list/new.txt")
ops_old = load_numbers("./mutex/cache/list/old.txt")
ops_tcs= load_numbers("./mutex/cache/list/tcs.txt") 
ops_tcb= load_numbers("./mutex/cache/list/tcb.txt") 

# cacheline = np.log2(np.array(cacheline))
ops_new = np.array(ops_new) / 1e6
ops_old = np.array(ops_old) / 1e6
ops_tcs = np.array(ops_tcs) / 1e6
ops_tcb = np.array(ops_tcb) / 1e6

# 绘制折线图
plt.rcParams.update({'font.family': 'Times New Roman','font.size': 25})
matplotlib.rcParams['pdf.fonttype'] = 42
plt.figure(figsize=(7.5, 4.5))
plt.plot(cacheline, ops_old,linestyle='-', marker='o',markersize=8,linewidth=2,color='red', label='Baseline')
plt.plot(cacheline, ops_tcs,linestyle='-', marker='^',markersize=8,linewidth=2,color='green', label='TCLock SP')
plt.plot(cacheline, ops_tcb,linestyle='-', marker='X',markersize=8,linewidth=2,color='royalblue', label='TCLock B')
plt.plot(cacheline, ops_new,linestyle='-', marker='s',markersize=8,linewidth=2,color='orange', label='BCD ')

# 设置对数刻度的横轴
plt.xscale('log', base=2)
plt.xticks([1, 2, 4, 8, 16, 32, 64, 128],[1, 2, 4, 8, 16, 32, 64, 128])
plt.yticks([0,1,2,3,4])
plt.ylim((0,4.5))

ax = plt.gca()
ax.spines['top'].set_visible(False)
ax.spines['right'].set_visible(False)

plt.tick_params(axis='x', which='both', labelbottom=True, pad=10)
plt.tick_params(axis='both', direction='in', length=6)

# 添加标题、标签和图例
# plt.title("Performance vs Cache Line Count (list)",pad=20)
plt.xlabel("Cache Lines in CS")
plt.ylabel("M Ops/sec")
# plt.legend(ncol=2,fontsize=15)

# 显示网格和图形
plt.grid(True, which="both", linestyle='dotted', linewidth=0.1)       
plt.tight_layout()

plt.savefig('mutex/cache/list/result.pdf', format='pdf')
plt.show()
