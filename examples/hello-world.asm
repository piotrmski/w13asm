; This program prints "Hello world!" to terminal.

loop: ld text       ; Load a character
jmz END             ; If end of string, then terminate
st IO               ; Put character to terminal

ld loop             ; Increment the character pointer
add #1
st loop

jmp loop            ; Loop to character loading

END: jmp END        ; End of program    

text: .align 4 "Hello, world!\n"
IO: .org 0x1fff