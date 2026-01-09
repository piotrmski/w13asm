JNZ: jmp 0

.MACRO JNZ, destination
  JMZ next
  JMP destination
  next:
.ENDMACRO
