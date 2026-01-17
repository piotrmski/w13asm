.MACRO LDI, addr
  LD addr
  ST instruction
  LD ${addr}+1
  ST instruction+1
  instruction: LD 0

  .MACRO STI, addr
    LD addr
    ST instruction
    LD ${addr}+1
    ADD #0x80
    ST instruction+1
    instruction: ST 0
  .ENDMACRO
.ENDMACRO

LDI ptr1
STI ptr2
LDI ptr2
ptr1: 0
ptr2: 1