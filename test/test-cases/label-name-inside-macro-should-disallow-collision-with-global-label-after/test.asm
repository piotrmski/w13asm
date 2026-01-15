.MACRO JNZ, destination
  JMZ next
  JMP destination
  next:
.ENDMACRO
next: 0