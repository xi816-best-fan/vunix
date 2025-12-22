#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include "cpu.h"

struct termios oldt;
struct termios newt;

int cpu_read8(risc_gc* cpu, short address) {
  address = address % MEMORY_SIZE;
  return cpu->memory[address];
}

int cpu_read(risc_gc* cpu, short address) {
  address = address % MEMORY_SIZE;
  return (cpu->memory[address] << 24) | (cpu->memory[address+1] << 16) | (cpu->memory[address+2] << 8) | cpu->memory[address+3];
}

uint16_t cpu_read16(risc_gc* cpu, short address) {
  address = address % MEMORY_SIZE;
  return (cpu->memory[address] << 8) | cpu->memory[address+1];
}

void cpu_write(risc_gc* cpu, short address, char value) {
  cpu->memory[address % MEMORY_SIZE] = value;
}

void cpu_write16(risc_gc* cpu, short address, uint16_t value) {
  cpu->memory[address % MEMORY_SIZE] = value >> 8;
  cpu->memory[address % MEMORY_SIZE + 1] = value;
}

void push(risc_gc* cpu, uint16_t val) {
  cpu->SP -= 2;
  cpu_write16(cpu, cpu->SP, val);
}

uint16_t pop(risc_gc* cpu) {
  uint16_t a = cpu_read16(cpu, cpu->SP);
  cpu->SP += 2;
  return a;
}

void update_flags(risc_gc* cpu, short result) {
  if(result == 0) {
    cpu->flags |= FLAG_ZERO;
  } else {
    cpu->flags &= ~FLAG_ZERO;
  }
  if(result & 0x8000) {
    cpu->flags |= FLAG_SIGN;
  } else {
    cpu->flags &= ~FLAG_SIGN;
  }
}

int cpu_step(risc_gc* cpu) {
  cpu->ir = cpu_read(cpu, cpu->pc);
  char opcode = (cpu->ir & 0xFF000000) >> 24;
  char reg1   = (cpu->ir & 0x00F00000) >> 20;
  char reg2   = (cpu->ir & 0x000F0000) >> 16;
  short imm   = (cpu->ir & 0x0000FFFF);

  switch(opcode) {
    case 0x00: //NOP
      break;

    case 0x01: //MOV R0, R1, IMM
      REG(reg1) = REG(reg2) + imm;
      break;

    case 0x03: //ADD R0, R1, IMM
      REG(reg1) = REG(reg1) + REG(reg2) + imm;
      update_flags(cpu, REG(reg1));
      break;
      
    case 0x04: //SUB R0, R1, IMM
      REG(reg1) = REG(reg1) - REG(reg2) - imm;
      update_flags(cpu, REG(reg1));
      break;

    case 0x05: //AND R0, R1, IMM
      REG(reg1) = REG(reg1) & REG(reg2) + imm;
      update_flags(cpu, REG(reg1));
      break;

    case 0x06: //OR R1, R2, IMM
      REG(reg1) = REG(reg1) | REG(reg2);
      update_flags(cpu, REG(reg1));
      break;

    case 0x07: //XOR R1, R2, IMM
      REG(reg1) = REG(reg1) ^ REG(reg2);
      update_flags(cpu, REG(reg1));
      break;

    case 0x08: //NOT R1, R2
      REG(reg1) = ~REG(reg2);
      update_flags(cpu, REG(reg1));
      break;

    case 0x09: //LD R1, [R2+imm]
      REG(reg1) = cpu_read8(cpu, REG(reg2)+imm);
      break;

    case 0x0A: //ST [R1+imm], R2
      cpu_write(cpu, REG(reg1)+imm, REG(reg2));
      break;

    case 0x0C: //JMP R0+R1+IMM
      cpu->pc = REG(reg1)+REG(reg2)+imm-4;
      break;

    case 0x0E: //JZ R0+R1+imm
      if(cpu->flags & FLAG_ZERO) cpu->pc = REG(reg1)+REG(reg2)+imm-4;
      break;

    case 0x10: //JNZ R0+R1+imm
      if(!(cpu->flags & FLAG_ZERO)) cpu->pc = REG(reg1)+REG(reg2)+imm-4;
      break;

    case 0x11: //INT imm
      switch(imm) {
        case 0x01: //PUTCHAR SUKA
          putchar(REG(1));
          break;
        case 0x02: //GETCHAR SUKA
          REG(1) = getchar();
        case 0x03: //get processor info
          REG(1) = 0x1001;
      }
      break;

    case 0x12: //HLT
      return 0;
      break;

    case 0x13: // SHL R1, R2, IMM
      REG(reg1) <<= REG(reg2) + imm;
      break;

    case 0x14: // SHR R1, R2, IMM
      REG(reg1) >>= REG(reg2) + imm;
      break;

    case 0x15: // PSH R1+IMM
      push(cpu, REG(reg1)+imm);
      break;

    case 0x16: // POP R1
      REG(reg1) = pop(cpu);
      break;

    case 0x17: // CALL R1+IMM
      push(cpu, cpu->pc);
      cpu->pc = REG(reg1) + imm - 4;
      break;

    case 0x18: // RET
      cpu->pc = pop(cpu);
      break;

    case 0x19: // LW R1, [R2+IMM12]
      REG(reg1) = cpu_read16(cpu, REG(reg2)+imm);
      break;

    case 0x1A: // SW [R1+imm], R2
      cpu_write16(cpu, REG(reg1)+imm, REG(reg2));
      break;

    case 0x20: // CMP R1, R2, IMM
      update_flags(cpu, REG(reg1)-REG(reg2)-imm);
      break;

    default:
      printf("Unknown command: 0x%02X at 0x%04X\n", opcode, cpu->pc);
      fflush(stdout);
      return -1;
  }
  cpu->pc += 4;
  return 1;
}

void cpu_dump(risc_gc* cpu) {
  printf("\033[3A\033[33mPC\033[0m: 0x%04X, \033[33mIR\033[0m: 0x%08X, \033[33mFLAGS\033[0m: 0b%016b  \n", cpu->pc, cpu->ir, cpu->flags);
  /*for (uint16_t m = 0xBFF0; m < 0xC000; m++) {
    printf("%02X ", cpu->memory[m]);
  }*/
  for(int i = 0; i < NUM_REGS; i++) {
    printf("\033[32mR%02d\033[0m: 0x%04X ", i, cpu->regs[i]);
    if((i+1)%8==0) putchar('\n');
  }
}

int exec(risc_gc* cpu, char debug) {
  int status;
  if (debug) puts("\n\n");
  execcycle:
    status = cpu_step(cpu);
    if (debug) { cpu_dump(cpu); getchar(); }
    if(status == 0) {
      tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
      return 0;
    } else if(status == -1) {
      tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
      fprintf(stderr, "еблан\n");
      return -1;
    }
    goto execcycle;
}

int main(int argc, char** argv) {
  uint8_t argp = 1;
  char* filename = NULL;
  char debug = 0; // чар булеан а что
  while (argp < argc) {
    if ((!strcmp(argv[argp], "-d")) || (!strcmp(argv[argp], "--debug"))) {
      debug = 1;
      argp++;
    }
    else {
      if (filename != NULL) {
        fprintf(stderr, "чмо зачем мне второй файл\n");
        return -1;
      }
      filename = argv[argp];
      argp++;
    }
  }
  if (!filename) {
    puts("чмо файл дай");
    return -1;
  }

  risc_gc cpu;
  cpu.pc = 0;
  cpu.ir = 0;
  cpu.flags = 0;
  for(int i = 0; i < NUM_REGS; i++) {
    cpu.regs[i] = 0;
  }
  cpu.SP = 0xC000;
  cpu.memory = malloc(MEMORY_SIZE);
  FILE* f = fopen(filename, "r");
  if (!f) {
    puts("чмо нет такого файла");
    free(cpu.memory);
    return -1;
  }
  fread(cpu.memory, 1, MEMORY_SIZE, f);
  fclose(f);

  tcgetattr(STDIN_FILENO, &oldt);
  memcpy(&newt, &oldt, sizeof(oldt));
  newt.c_iflag &= ~(IXON);
  newt.c_lflag &= ~(ICANON | ECHO | /*ISIG |*/ IEXTEN);
  tcsetattr(STDIN_FILENO, TCSANOW, &newt);
  exec(&cpu, debug);
  tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
}

