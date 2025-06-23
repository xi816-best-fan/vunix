jmp main
put_u:
  mov r1, 'U'
  int 1
ret

main:
  mov r1, 'B'
  int 1
  call put_u
  mov r1, 'B'
  int 1
  call put_u
  call put_u
  mov r1, '$'
  int 1
hlt

