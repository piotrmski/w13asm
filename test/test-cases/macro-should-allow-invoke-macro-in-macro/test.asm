.macro a
    ld #'a'
.endmacro

.macro b
    ld #'b'
    a
.endmacro

b