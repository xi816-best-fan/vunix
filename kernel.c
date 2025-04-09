#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdint.h>
#include <stddef.h>

#define MAX_TTY_LENGTH 256
#define MAX_PROCESSES 32
#define MAX_FILES 64
#define MAX_FILE_NAME 32
#define MAX_FILE_CONTENT 1024
#define STACK_SIZE 1024
#define MAX_PROG_SIZE 2048
#define MAX_ARGS 16
#define MAX_ENV 16
#define PAGE_SIZE 4096 
#define NUM_PAGES 256
#define PHYS_MEM_SIZE (NUM_PAGES * PAGE_SIZE)
#define SWAP_FILE "swap.bin" //свопчэкэ
#define DISK_FILE "disk.bin"
#define MAX_TASKS 8
#define SIGTERM 0x0001 //вежливо просит сдохнуть
#define SIGKILL 0x0002 //убивает
#define SIGABRT 0x0003 //самоубивается
#define PROCESS_DEAD 0
#define PROCESS_EXEC 1
#define PROCESS_WAIT 2
#define MAX_FDS MAX_FILES
#define CONSOLE_FD 0
#define STDOUT_FD 1
#define STDERR_FD 2

typedef struct {
	int type;
	uint32_t pos;
	void* data;
	size_t size;
} file_desc_t;

typedef enum {
	TASK_READY,
	TASK_RUNNING,
	TASK_SLEEPING,
	TASK_DEAD
} TaskState; //таск стате

typedef struct {
	uint32_t eax;
	uint32_t ebx;
	uint32_t ecx;
	uint32_t edx;
	uint32_t esi;
	uint32_t edi;
	uint32_t esp;
	uint32_t ebp;
	uint32_t EFLAGS;
	uint32_t eip;
} TaskContext; //пон

typedef struct {
	TaskContext ctx;
	TaskState state;
	int id; //task id
	uint8_t stack[STACK_SIZE]
} Task; //поток(не поток);

/*
Обращение с задачами(Task) в этой псевдо-ОС
В нашей псевдо-ОС задачи(Тоске) представляют собой базированные based базовые единичко выполнения, аналогичные процессам или потокам в рили Оперейшн систем.
Они позволяют организовать многозадачность управлять выполнением программ и распределять ресурсы.

1. Создание задачи
Для создания задачи как правило используется task_create() который:
* находит свободный слот в таблице задач
* настраивает стек и контекст выполнения
* возвращает пид(ор)
Пример:
int pid = task_create(entry_point);
if(pid < 0) {
	printf("чот короче пошло не так, динах\n");
}

2. Состояния задачи:
Каждая задача может находится в одном из состояний
* TASK_READY
* TASK_RUNNING
* TASK_SLEEPING
* TASK_DEAD

(P.S Для корректной работы scheduler в конце каждой задачи добавляйте schedule() )
*/
typedef struct {
	int pid;
	int ppid;
	int eip;
	uint8_t *memory;
	uint32_t size;
	char* argv[MAX_ARGS];
	char* envp[MAX_ENV];
	char name[50];
	uint8_t stack[STACK_SIZE];
	char status; //0 - no exec 1 - exec 2 - wait
	int exit_status;
} Process;

typedef struct {
	char name[MAX_FILE_NAME];
	char content[MAX_FILE_CONTENT];
	char is_executable;
} VirtualFile;

typedef struct {
	bool present;
	bool modified;
	int frame;
	size_t swap_offset;
} PageTableEntry;

typedef struct {
	uint8_t data[PAGE_SIZE];
} MemoryFrame;

Process processes[MAX_PROCESSES];
VirtualFile filesystem[MAX_FILES];
PageTableEntry page_table[NUM_PAGES];
MemoryFrame physical_mem[NUM_PAGES];
short stack[STACK_SIZE];
int stack_point = 1;
int current_pid = 0;
int file_count = 0;
int free_frames[NUM_PAGES];
int free_frame_count = NUM_PAGES;
FILE* swap_file = NULL;
Task tasks[MAX_TASKS];
int current_task = -1;
int task_count = 0;
//void(*syscalls[])(void) = {sys_open, sys_read, sys_write, sys_fork, sys_execve, sys_exit, sys_wait, sys_waitpid, sys_getpid, sys_getppid, sys_abort, sys_pipe, sys_shmget, sys_shmat, sys_msgget, sys_msgsnd, sys_clone, sys_uname};
//struct file_desc_t fd_table[MAX_FDS];

volatile void panic(const char *panic) {
	printf("Kernel panic! - not syncing: %s\n", panic);
	printf("Stack in file stack.log\n");
	FILE *stack = fopen("stack.log", "w+b");
	if(stack == NULL) {
		printf("error while opening stack.log\nprinting to screen\n");
		for(int i = 0; i < STACK_SIZE; i++) {
			printf("%x ", stack[i]);
		}
		printf("\n");
	} else {
		printf("writing stack to stack.log... ");
		fprintf(stack, "Stack: \n");
		for(int i = 0; i < STACK_SIZE; i++) {
			fprintf(stack, "%x ", stack[i]);
		}
		printf("done\n");
		fclose(stack);
	}
	for(;;) {}
}

void push(short byte) {
	if(stack_point > STACK_SIZE - 1) {
		panic("Stack Overflow!\n");
	}
	stack[++stack_point] = byte;
	//printf("Pushed %x to stack(position %i)\n", byte, stack_point);
}

short pop() {
	if(stack_point < 0) {
		panic("Stack underflow!\n");
	}
	short byte = stack[stack_point--];
	//printf("Popped %x from stack (position %x)\n", byte, stack_point);
	return byte;
}

void init_virtual_memory() {
	push(0x1488);
	printf("init virtual memory...\n");
	for(int i = 0; i < NUM_PAGES; i++) {
		page_table[i].present = false;
		page_table[i].modified = false;
		page_table[i].frame = -1;
		page_table[i].swap_offset = -1;
		free_frames[i] = i;
	}
	free_frame_count = NUM_PAGES;
	swap_file = fopen(SWAP_FILE, "w+b");
	if(!swap_file) {
		printf("error while creating swap file\n");
		pop();
		exit(EXIT_FAILURE);
	}
	pop();
}

void swap_out(int page_num) {
	push(0xd7bb);
	if(page_table[page_num].present) {
		int frame = page_table[page_num].frame;
		if(page_table[page_num].modified) {
			fseek(swap_file, page_table[page_num].swap_offset, SEEK_SET);
			fwrite(physical_mem[frame].data, PAGE_SIZE, 1, swap_file);
		}
		free_frames[free_frame_count++] = frame;
		page_table[page_num].present = false;
		printf("page %i swapped to disk\n", page_num);
	}
	pop();
}

void swap_in(int page_num) {
	push(0x10ac);
	if(!page_table[page_num].present) {
		if(free_frame_count == 0) {
			for(int i = 0; i < NUM_PAGES; i++) {
				if(page_table[i].present) {
					swap_out(i);
					break;
				}
			}
		}
		int frame = free_frames[--free_frame_count];
		page_table[page_num].frame = frame;
		page_table[page_num].present = true;

		if(page_table[page_num].swap_offset != 0) {
			fseek(swap_file, page_table[page_num].swap_offset, SEEK_SET);
			fread(physical_mem[frame].data, PAGE_SIZE, 1, swap_file);
		} else {
			memset(physical_mem[frame].data, 0, PAGE_SIZE);
			page_table[page_num].swap_offset = (page_num + 1) * PAGE_SIZE;
		}
		pop();
		printf("Page %i loaded to frame%i\n", page_num, frame);
	}
}

void* vm_alloc(size_t size) {
	push(0x1fab);
	if(size == 0 || size > NUM_PAGES * PAGE_SIZE) {
		pop();
		return NULL;
	}	
	int pages_needed = (size + PAGE_SIZE - 1) / PAGE_SIZE;
	int start_page = -1;
	int consecutive = 0;

	for(int i = 0; i < NUM_PAGES; i++) {
		if(!page_table[i].present && page_table[i].swap_offset == 0) {
			if(start_page == -1) start_page = i;
			consecutive++;
			if(consecutive >= pages_needed) {
				break;
			}
		} else {
			start_page = -1;
			consecutive = 0;
		}
	}
	if(consecutive < pages_needed) {
		printf("Not enough contiguous virtual memory!\n");
		pop();
		return NULL;
	}
	for(int i = start_page; i < start_page + pages_needed; i++) {
		page_table[i].swap_offset = (i+1) * PAGE_SIZE;
	}
	swap_in(start_page);
	printf("Allocated %zu bytes of virtual memory(pages %i-%i)\n", size, start_page, start_page + pages_needed - 1);
	pop();
	return (void*)((uintptr_t)start_page * PAGE_SIZE);
}

void vm_free(void* ptr, size_t size) {
	push(0x00b2);
	if(!ptr || size == 0) {
		pop();
		return;
    }
	int start_page = (uintptr_t)ptr / PAGE_SIZE;
	int pages_needed = (size + PAGE_SIZE - 1) / PAGE_SIZE;
	printf("freeing %zu bytes virtual memory(pages %i-%i)\n", size, start_page, start_page + pages_needed - 1);
	for(int i = start_page; i < start_page + pages_needed; i++) {
		if(page_table[i].present) {
			free_frames[free_frame_count++] = page_table[i].frame;
			page_table[i].present = false;
		}
		page_table[i].modified = false;
		page_table[i].swap_offset = 0;
	}
	pop();
}

int new_file(char* name, char* content, char executable) {
	push(0xf11e);
	if(file_count >= MAX_FILES) {
		panic("VFS: error: Can`t create file");
	}
	strncpy(filesystem[file_count].name, name, MAX_FILE_NAME - 1);
	strncpy(filesystem[file_count].content, content, MAX_FILE_CONTENT - 1);
	filesystem[file_count].is_executable = executable;

	printf("Created file %s\n", name);
	pop();
	return file_count++;
}

void scheduler_init() {
	push(0x1007);
	for(int i = 0; i < MAX_TASKS; i++) {
		tasks[i].state = TASK_DEAD;
	}
	printf("Scheduler initialized");
	pop();
}

int task_create(void(*entry_point)()) {
	push(0x2280);
	if(task_count >= MAX_TASKS) {
		pop();
		return -1;
	}
	int tid = -1;
	for(int i = 0; i < MAX_TASKS; i++) {
		if(tasks[i].state == TASK_DEAD) {
			tid = i;
			break;
		}
	}
	if(tid == -1) return -1;
	Task *task = &tasks[tid];
	task->id = tid;
	task->state = TASK_READY;

	uint32_t *stack_top = (uint32_t*)&task->stack[STACK_SIZE - sizeof(TaskContext)];
	task->ctx.esp = (uint32_t)stack_top;
	task->ctx.eip = (uint32_t)entry_point;
	task->ctx.ebp = (uint32_t)stack_top;
	task_count++;
	printf("Task %i created\n", tid);
	pop();
	return tid;
}

void schedule() {
	push(0x1789);
	if(task_count == 0) {
		pop();
		return;
	}
	int next_task = current_task;
	do {
		next_task = (next_task + 1) % MAX_TASKS;
	} while(tasks[next_task].state != TASK_READY && next_task != current_task);

	if(tasks[next_task].state != TASK_READY) {
		pop();
		return;
	}
	if(current_task >= 0) {
		tasks[current_task].state = TASK_READY;
	}
	printf("Switched to task %i\n", current_task);
	void (*task_entry)() = (void(*)())tasks[current_task].ctx.eip;
	task_entry();
}

void init_vfs() {
	push(0x100d);
	/*memset(fd_table, 0, sizeof(fd_table));
	fd_table[CONSOLE_FD].type = 1;
	fd_table[STDOUT_FD].type = 1;
	fd_table[STDERR_FD].type = 1;*/
	new_file("README.md", "sosal?", 0);
	new_file("hello", "echo \"Hello world\"\nhelp\n", 1);
	new_file("forkbomb", "start forkbomb\nrun forkbomb\n", 1);
	pop();
}

VirtualFile* sys_open(const char* name) {
	push(0xf18d);
	for(int i = 0; i < file_count; i++) {
		if(strcmp(filesystem[i].name, name) == 0) {
			pop();
			return &filesystem[i];
		}
	}
	pop();
	return NULL;
}

/*ssize_t sys_read(int fd, void* buf, size_t count) {
	push(0x1111);
	if(fd < 0 || fd >= MAX_FDS || fd_table[fd].type == 0) {
		pop();
		return -1;
	}
	if(fd_table[fd].type == 1) {
		static const char* msg = 
	}
	pop();
}

void sys_write(VirtualFile* fd, char* string, int size) {
	push(0x7711);

	pop();
}

int sys_getpid(Process process) {
	push(0x111f);
	pop();
	return process.pid;
}

int sys_getppid(Process process) {
	push(0x1120);
	pop();
	return process.ppid;
}

int sys_fork() {
	push(0x11fa);
	int new_pid = -1;
	for(int i = 1; i < MAX_PROCESSES; i++) {
		if(processes[i].status == 0) {
			new_pid = i;
			break;
		}
	}
	if(new_pid == -1) {
		pop();
		return -1;
	}
	Process* parent = &processes[current_pid];
	Process* child = &processes[new_pid];
	memcpy(child, parent, sizeof(Process));
	child->pid = ++current_pid;
	child->ppid = parent->pid;
	child->state = 1;
	pop();
	return child->pid;
}

void sys_exit(Process process, int value) {
	push(0xbf90);
	process.exit_status = value;
	process.status = PROCESS_DEAD;
	pop();
}

void sys_abort(Process process) {
	push(0x9999);
	process.pid = -1;
	process.ppid = -1;
	process.name = "";
	process.status = PROCESS_DEAD;
	process.exit_status = SIGABRT;
	pop();
}

int sys_waitpid(int pid, int* status, int options) {
	push(0xf1cc);
	Process* current = &processes[current_pid];
	int has_children = 0;
	for(int i = 0; i < MAX_PROCESSES; i++) {
		if(processes[i].ppid == current->pid) {
			has_children = 1;
			break;
		}
	}
	if(!has_children) {
		pop();
		return -1;
	}
	for(int i = 0; i < MAX_PROCESSES; i++) {
		Process* p = &processes[i];
		if(p->status == PROCESS_DEAD && p->ppid == current->pid && (pid == -1 || p->pid == pid)) {
			if(status) *status = p->exit_status;
			int child_pid = p->pid;
			memset(p, 0, sizeof(Process));
			pop();
			return child_pid;
		}
	}
	if(options & WNOHANG) {
		pop();
		return 0;
	}
	pop();
	return -2;
}

int sys_wait(int* status) {
	push(0xbeef);
	pop();
	return sys_waitpid(-1, status, 0);
}*/

void list_files() {
	push(0xf11e);
	printf("Directory of /\n");
	for(int i = 0; i < file_count; i++) {
		printf("%s %s\n", filesystem[i].name, filesystem[i].is_executable ? "Executable" : "Non-executable");
	}
	printf("   %i File(s)\n\n", file_count);
	pop();
}

int new_process(const char* name) {
	push(0xdead);
	if(current_pid >= MAX_PROCESSES) {
		panic("Too much processes\n");
	}	
	processes[current_pid].pid = current_pid;
	strncpy(processes[current_pid].name, name, 49);
	processes[current_pid].status = 1;
	printf("Created process %s (pid: %i)\n", name, current_pid);
	pop();
	return current_pid++;
}

void init_kernel() {
	push(0x1000);
	printf("Vunix version 0.1 (root@vunix)\n");
	printf("Command line: root=0 init=sh\n");
	printf("e820: BIOS-provided physical RAM map:\n");
	printf("GovnBIOS-e820: [mem 0x000000000000000-0x00000000000FACE0]\n");
	printf("smpboot: Allowing 1 CPUs, 0 hotplug CPUs\n");
	printf("Perfomance Events: PEBS fmt3+, Skylake events, 24-deep LBR, full-width counters, GovnoCore driver\n");
	printf("GovnPCI: MMCONFIG for domain 0000 [bus 00-3f] at [mem 0xF8000000-0xfbffffff] (base 0xf8000000)\n");
	printf("GovnACPI: EC: EC started");
	scheduler_init();
	usleep(364039);
	printf("ahci 0000:00:1f.2: version 0.1\n");
	printf("ahci 0000:00:1f.2: AHCI 0001.0300 32 slots 6 ports 6 Gbps 0x5 impl SATA mode\n");
	printf("ata1: SATA max UDMA/133 abar m2048@0xf7d10000 port 0xf7d10100 irq 28\n");
	printf("VUNIX-fs (sda1): mounted filesystem with ordered data mode. Opts: (null)\n");
	printf("e1000e: GovnoCore 24 PRO Driver - 0.1\n");
	printf("e1000e 0000:00:19.0 eth: (GovnPCI Express:2.5GT/s:Width x1) 00:11:22:33:44:55\n");
	printf("running /init as init process...\n");
	usleep(1000000);
	for(int i = 0; i < MAX_PROCESSES; i++) {
		processes[i].pid = -1;
		processes[i].pid = 0;
		strcpy(processes[i].name, "");
	}
	init_virtual_memory();
	init_vfs();
	new_process("init");
	pop();
}

void list_processes() {
	push(0xf1ca);
	printf("Processes:\n");
	printf("PID\tName\t\tStatus\n");
	for(int i = 0; i < MAX_PROCESSES; i++) {
		if(processes[i].pid != -1) {
			printf("%s\t%s\t\t", processes[i].pid, processes[i].name);
			switch(processes[i].status) {
				case 0: printf("no exec\n"); break;
				case 1: printf("exec\n"); break;
				case 2: printf("wait\n"); break;
				default: printf("undefined\n"); break;
			}
		}
	}
	printf("\n");
	pop();
}

void execute_script(const char* script_content) {
	push(0xec00);
	char* script_copy = strdup(script_content);
	char* line = strtok(script_copy, "\n");

	while(line != NULL) {
		if(strlen(line) > 0) {
			printf("executing %s\n", line);
			if(strncmp(line, "echo ", 5) == 0) {
				printf("%s\n", line+5);
			} else if(strcmp(line, "ps") == 0) {
				list_processes();
			} else if(strcmp(line, "ls") == 0) {
				list_files();
			} else if(strncmp(line, "start ", 6) == 0) {
				new_process(line+6);
			} else if(strncmp(line, "cat ", 4) == 0) {
				VirtualFile* file = sys_open(line + 4);
				if(file) {
					printf("%s\n", file->content);
				} else {
					printf("can`t find file with name %s\n", line+4);
				}
			} else if(strncmp(line, "run ", 4) == 0){
				VirtualFile* file = sys_open(line+4);
				if(file) {
					if(file->is_executable) {
						printf("running %s\n", file->name);
						execute_script(file->content);
					} else {
						printf("Error: file %s is non-executable\n", file->name);
					}
				} else {
					printf("can`t find file with name %s\n", line+4);
				}
			} else if(strcmp(line, "help") == 0) {
				printf("help - get help\nps - list of all processes\nls = list files\ncat <file> - get file content\nrun <file> - run executable file\nstart <name> - start new process\nexit - exit\n");
			} else {
				printf("undefined command: %s\n", line);
			}
		}
		line = strtok(NULL, "\n");
	}
	free(script_copy);
	pop();
}

void sh() {
	push(0x78ac);
	char command[MAX_TTY_LENGTH];
	while(1) {
		printf("root@vunix: ");
		fgets(command, MAX_TTY_LENGTH, stdin);
		command[strcspn(command, "\n")] = '\0';
		if(strcmp(command, "exit") == 0) {
			printf("Exiting...\n");
			break;
		} else if(strcmp(command, "ps") == 0) {
			list_processes();
		} else if(strcmp(command, "ls") == 0) {
			list_files();
		} else if(strncmp(command, "start ", 6) == 0) {
			new_process(command+6);
		} else if(strncmp(command, "cat ", 4) == 0) {
			VirtualFile* file = sys_open(command + 4);
			if(file) {
				printf("%s\n", file->content);
			} else {
				printf("can`t find file with name %s\n", command+4);
			}
		} else if(strncmp(command, "run ", 4) == 0){
			VirtualFile* file = sys_open(command+4);
			if(file) {
				if(file->is_executable) {
					printf("running %s\n", file->name);
					execute_script(file->content);
				} else {
					printf("Error: file %s is non-executable\n", file->name);
				}
			} else {
				printf("can`t find file with name %s\n", command+4);
			}
		} else if(strcmp(command, "help") == 0) {
			printf("help - get help\nps - list of all processes\nls = list files\ncat <file> - get file content\nrun <file> - run executable file\nstart <name> - start new process\nexit - exit\n");
		} else {
			printf("undefined command: %s\n", command);
		}
	}
	pop();
}

int main() {
	init_kernel();
	sh();
	return 0;
}
