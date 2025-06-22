#include <stdio.h>
#include <stdlib.h>

void compile(char* command, char* name) {
  printf("compiling %s\n", name);
  system(command);
}

int main() {
  compile("gcc -Wextra -Wall -o mall mall.c", "mall");
  compile("gcc -Wextra -Wall -o vmvuniz-vunix kernel/main.c kernel/init_kernel.c kernel/panic.c kernel/shell.c kernel/vfs.c kernel/rescue.c kernel/v.out.c", "kernel");
  compile("gcc -Wextra -Wall -o create_disk src/create_disk.c", "create_disk");
  compile("gcc src/writevfs.c -o writevfs", "writevfs");
  compile("gcc src/cpu.c -o vunicore", "vunicore");
  return 0;
}
