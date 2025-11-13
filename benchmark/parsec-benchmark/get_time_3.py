import re
import argparse

# 设置命令行参数解析
def parse_arguments():
    parser = argparse.ArgumentParser(description="Extract 'Total time with initialization' values from a text file.")
    parser.add_argument("file_path", help="Path to the text file to process")
    return parser.parse_args()

# 从文件中读取文本并处理
def extract_times(file_path):
    try:
        # 打开并读取文件内容
        with open(file_path, "r") as file:
            text = file.read()

        # 使用正则表达式提取所有的 Total time with initialization 后的数字
        pattern = r"Total time without initialization :\s+(\d+)"
        times = re.findall(pattern, text)

        # 将提取的数字转换为整数
        times = [int(time) for time in times]

        # 返回结果
        return times
    except FileNotFoundError:
        print(f"Error: The file '{file_path}' was not found.")
        return []
    except Exception as e:
        print(f"An error occurred: {e}")
        return []

def main():
    # 解析命令行参数
    args = parse_arguments()

    # 提取所有的时间
    times = extract_times(args.file_path)

    # 如果提取到了时间，打印最后十个并计算平均值
    if times:
        # 获取最后十个时间
        last_ten_times = times[-10:]
        
        # 打印最后十个时间
        # print("Last 10 Total times without initialization:", last_ten_times)
        
        # 计算并打印平均值
        if last_ten_times:
            average_time = sum(last_ten_times) / len(last_ten_times) / 1e6
            print(f"Average of the last 10 times: {average_time:.2f}")
        else:
            print("Not enough data to calculate the average.")
    else:
        print("No times found.")

if __name__ == "__main__":
    main()
