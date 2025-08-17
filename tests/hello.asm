xui:
  mov   r2, hello
loop:
  ld    r1, [r2]
  add   r2, 1
  or    r1, r1
  jz    end
  int   1
  jmp   loop
inf:
  jmp   inf
end:
  int   3
  cmp   r1, $1001
  jnz   inf
  hlt


hello:  bytes "Hello епта$^@"

