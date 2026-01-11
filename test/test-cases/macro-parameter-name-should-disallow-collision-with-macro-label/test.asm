.MACRO JNZ, destination
  JMZ next
  JMP destination
  destination: next:
.ENDMACRO