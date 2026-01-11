.MACRO JNZ, destination
  JMZ next
  JMP destination
  JNZ: next:
.ENDMACRO