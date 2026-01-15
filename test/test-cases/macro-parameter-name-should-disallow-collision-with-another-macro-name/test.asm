.macro destination .endmacro
.MACRO JNZ, destination
  JMZ next
  JMP destination
  next:
.ENDMACRO
