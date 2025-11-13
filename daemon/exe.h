#define un_init -1
#define normal 0
#define call_miss 1
#define ij_miss 2
#define error_inst 3

#define WRITE_PID 0
#define WRITE_EXE 1

struct exe_info {
	int status_code;
	int pid;
	int tgid;
	unsigned long RIP;
	void* code_block;
	unsigned long jump_from;
	unsigned long tgt_addr;
	unsigned long fsbase;
	int success_cnt;
	int fail_cnt;
}__attribute__((aligned(64)));

struct recv_pkt {
	int status_code;
	int pid;
	unsigned long RIP;
	unsigned long offset;
};
