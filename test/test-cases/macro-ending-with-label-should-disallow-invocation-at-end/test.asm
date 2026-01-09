.macro JNZ, destination
  JMZ next
  JMP destination
  next:
.endmacro

ld #5
JNZ end