jmp main
puts:
  ld r1, [r8]
  add r8, 1
  or r1, r1
  jz .end
  int 1
  jmp puts
.end:
  mov r12, 0
ret

gets:
  int 2
  int 1
  cmp r1, 10
  jz .end
  jmp gets
.end:
  ret

main:
  mov r8, prompt
  call puts

  call gets

  mov r8, hello
  call puts
hlt

prompt: bytes "> ^@"
hello: bytes "ok$^@"

