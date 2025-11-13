import re
import sys

threads_number = [4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 44, 48, 52, 56, 60, 64]

def extract_readrandom_numbers(file_path):
    numbers = []
    with open(file_path, 'r') as file:
        text = file.read()
        # 使用正则表达式提取 readrandom 后的数字
        matches = re.findall(r'readrandom\s*:\s*([\d\.]+)', text)
        # 将匹配到的数字转换为整型
        numbers = [float(num) for num in matches]
    return numbers

def main(file_path):
    numbers = extract_readrandom_numbers(file_path)
    if len(numbers)>len(threads_number):
        numbers=numbers[1:]
    for i in range(0,len(threads_number)):
        # print('thread ',i,': ',numbers[i],' micros/op')
        print(numbers[i])

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python script.py <file_path>")
    else:
        main(sys.argv[1])
