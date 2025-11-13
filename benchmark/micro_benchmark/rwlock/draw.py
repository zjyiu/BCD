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

write_ratios = [1, 5, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100]
ops_new = load_numbers("./rwlock/new.txt")
ops_old = load_numbers("./rwlock/old.txt")

ops_new = np.array(ops_new) / 1e6
ops_old = np.array(ops_old) / 1e6

# 横坐标转换为等间距的对数坐标
log_x = np.log2([2, 4, 8, 16, 32, 64, 128, 256])  # 定义关键点的对数刻度

# 绘制折线图
plt.rcParams.update({ 'font.family': 'Times New Roman','font.size' : 25 })
matplotlib.rcParams['pdf.fonttype'] = 42
plt.figure(figsize = (7.5, 4.5))

plt.plot(write_ratios, ops_old, linestyle = '-', marker = 'o', markersize = 8,linewidth=2, color = 'red', label = 'Baseline')
plt.plot(write_ratios, ops_new, linestyle = '-', marker = 's', markersize = 8,linewidth=2, color = 'orange', label = 'BCD')

# 设置对数刻度的横轴
# plt.xticks([2, 4, 8, 16, 32, 64, 128, 256])   # 只显示指定刻度
plt.yticks([0, 0.5, 1, 1.5, 2, 2.5, 3])
plt.ylim((0, 2.7))

ax = plt.gca()
ax.spines['top'].set_visible(False)
ax.spines['right'].set_visible(False)

plt.tick_params(axis = 'both', direction = 'in', length = 6)

# 添加标题、标签和图例
# plt.title("Performance vs Thread Count", pad = 20)
plt.xlabel("#% writes")
plt.ylabel("M Ops/sec")
# plt.legend(ncol=2,loc=9, fontsize = 15)

# 显示网格和图形
plt.grid(True, which = "both", linestyle = 'dotted', linewidth = 0.1)
plt.tight_layout()

plt.savefig('rwlock/result.pdf', format = 'pdf')
plt.show()
