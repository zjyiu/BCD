import matplotlib.pyplot as plt
import numpy as np

# 数据
input = ['Raytrace\nteapot', 'Raytrace\nballs4', 'Raytrace\ncar','Radiosity','Volrend']

# new_time = [10860060.50, 27691635.20, 19088492.30,3216838.50,18.62]
# old_time = [19007190.90, 39319425.30, 26742587.60,4505709.10,20.35]
new_time=[11.04,28.25,19.16,3.11,22.64]
old_time=[18.84,39.16,25.83,4.35,24.85]
old_p=[]
new_p=[]

result = [x / y for x, y in zip(new_time, old_time)]
print(result)

for i in range(0,len(input)):
    old_p.append(100)
    new_p.append(100*new_time[i]/old_time[i])

# 创建柱状图
x = np.arange(len(input))  # x轴位置
width = 0.35  # 柱宽

plt.rcParams.update({'font.family': 'Times New Roman','font.size': 15})
fig, ax = plt.subplots()
plt.ylim((0,100))

# 绘制柱状图
bars1 = ax.bar(x - width/2, new_p, width, label='BCD', color='orange')
bars2 = ax.bar(x + width/2, old_p, width, label='w/o BCD', color='royalblue')

ax.spines['top'].set_visible(False)
ax.spines['right'].set_visible(False)

plt.tick_params(axis='both', direction='in', length=6)

# 添加标签和标题
ax.set_ylabel('Ralative Runtime')
ax.set_xticks(x)
ax.set_xticklabels(input)
ax.legend(bbox_to_anchor=(0.2,1),ncol=2)

plt.grid(True,axis='y', which="both", linestyle='dotted', linewidth=1)     
plt.tight_layout()

plt.savefig('splash2x.pdf', format='pdf')
# 显示图表
plt.show()
