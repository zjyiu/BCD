import argparse

def extract_sets_numbers(file_path):
    # 初始化一个列表来存储提取的数字
    sets_numbers = []

    # 从指定文件中读取内容
    with open(file_path, "r") as file:
        content = file.readlines()

    # 遍历每一行内容，找到包含 "Sets" 的行
    total=0
    cnt=0
    for line in content:
        if line.strip().startswith("Sets"):
            parts = line.split()
            if len(parts) > 1:
                try:
                    # 提取第一个数字并添加到列表中
                    number = float(parts[1])
                    total+=number
                    cnt+=1
                except ValueError:
                    continue  # 跳过无法转换的行
    
    # 打印结果数组
    print('%.2f'% (total/cnt))

# 使用 argparse 处理命令行参数
if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Extract numbers from lines starting with 'Sets' in a text file.")
    parser.add_argument("file_path", help="Path to the input text file")
    
    args = parser.parse_args()
    extract_sets_numbers(args.file_path)
