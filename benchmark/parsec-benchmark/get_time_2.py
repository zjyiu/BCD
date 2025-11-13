import re
import argparse

def parse_args():
    parser = argparse.ArgumentParser(description="get and culculate the average of the last 10 real time")
    parser.add_argument('file_path', help="file path")
    return parser.parse_args()

def extract_real_times(file_path):
    with open(file_path, 'r') as file:
        text = file.read()

    pattern = r'real\s+(\d+)m(\d+\.\d+)s'
    return re.findall(pattern, text)

def calculate_average_time(times):
    last_10_times = times[-10:]
    total_seconds = 0

    for match in last_10_times:
        minutes = int(match[0])
        seconds = float(match[1])
        total_second = minutes * 60 + seconds
        total_seconds+=total_second

    average_seconds = total_seconds / len(last_10_times)
    return average_seconds

def main():
    args = parse_args()
    
    real_times = extract_real_times(args.file_path)
    
    if real_times:
        average_time = calculate_average_time(real_times)
        print(f"Average time: {average_time:.2f}s")
    else:
        print("no real time")

if __name__ == '__main__':
    main()
