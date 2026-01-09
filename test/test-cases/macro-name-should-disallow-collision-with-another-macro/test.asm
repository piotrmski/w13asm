.macro JNZ, destination
  JMZ next
  JMP destination
  next:
.endmacro
.MACRO JNZ, destination
  JMZ next
  JMP destination
  next:
.ENDMACRO