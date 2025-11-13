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
ops_new = load_numbers("./mutex/latency/99/new.txt")
ops_old = load_numbers("./mutex/latency/99/old.txt")
ops_tcs= load_numbers("./mutex/latency/99/tcs.txt") 
ops_tcb= load_numbers("./mutex/latency/99/tcb.txt") 

plt.rcParams.update({'font.family': 'Times New Roman','font.size': 25})
matplotlib.rcParams['pdf.fonttype'] = 42
plt.figure(figsize=(7.5, 4.5))


plt.plot(threads_number, ops_old,linestyle='-', marker='o',markersize=8,linewidth=2,color='red', label='Baseline')
plt.plot(threads_number, ops_tcs,linestyle='-', marker='^',markersize=8,linewidth=2,color='green', label='TCLock SP')
plt.plot(threads_number, ops_tcb,linestyle='-', marker='X',markersize=8,linewidth=2,color='royalblue', label='TCLock B')
plt.plot(threads_number, ops_new,linestyle='-', marker='s',markersize=8,linewidth=2,color='orange', label='BCD ')

plt.xscale('log', base=2)
plt.yscale('log', base=10)

plt.xticks([1, 2, 4, 8, 16, 32, 64, 128, 256], [1, 2, 4, 8, 16, 32, 64, 128, 256]) 
plt.yticks([10,1e2,1e3,1e4,1e5,1e6,1e7]) 


# plt.yticks([0,10,100,1000,10000,100000,1000000,10000000,1e8]) 
plt.ylim([40,4e7])

ax = plt.gca()
ax.spines['top'].set_visible(False)
ax.spines['right'].set_visible(False)

plt.axvline(x=32, color='black', linestyle='-', linewidth=1)

plt.tick_params(axis='both', direction='in', length=6)

# plt.title("Performance vs Thread Count",pad=20)
plt.xlabel("# of threads")
plt.ylabel("Cycles")
# plt.legend(ncol=2,fontsize=15)


for i in range(5,10):
    plt.axhline(y=i*10, color='grey', linestyle='dotted', linewidth=0.1)
for i in range(1,5):
    plt.axhline(y=i*1e7, color='grey', linestyle='dotted', linewidth=0.1)
for i in range(1,10):
    for j in range(2,7):
        plt.axhline(y=i*10**j, color='grey', linestyle='dotted', linewidth=0.1)


plt.grid(True, which='both', axis='both', linestyle='dotted', linewidth=0.1)     

# for i in range(2,10):
#     plt.axhline(y=i*100, color='black', linestyle='dotted', linewidth=0.1)

plt.tight_layout()

plt.savefig('mutex/latency/99/result.pdf', format='pdf')
plt.show()
