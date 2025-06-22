gets:
  int 2 ; Getchar suka
  int 1 ; Putchar suka
  cmp r1, 10
  jz gets_end ; \n
  jmp gets ; loop
gets_end:
  hlt ; pizda

