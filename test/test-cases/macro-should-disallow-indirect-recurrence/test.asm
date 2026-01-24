.macro a
    ld #'a'
    b
.endmacro

.macro b
    ld #'b'
    a
.endmacro

b