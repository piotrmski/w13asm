.MACRO JNZ, destination
  JMZ next
  JMP destination
  next:
.ENDMACRO
destination: 0