.MACRO JNZ, destination, destination
  JMZ next
  JMP destination
  next:
.ENDMACRO