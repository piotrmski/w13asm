.MACRO JNZ, destination
  JMZ next
  JMP destination
  next:
.ENDMACRO

JNZ: jmp 0