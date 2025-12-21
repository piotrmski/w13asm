; This program takes characters from terminal input and echoes them back capitalized until Return is pressed.

loop:
ld IO
st character
jmz loop

; if character == '\n' (10) then end
add constNegativeLF
jmz onReturn
add constLF

; if not character < 'a' (97) and character < 'z'+1 (123) then print character - 32; else print character
add constNegativea
jmn echo
add consta 

add constNegativezPlus1
jmn capitalize
add constzPlus1 

echo:
ld character
st IO
jmp loop

capitalize:
ld character
add constNegative32
st IO
jmp loop

onReturn:
ld character
st IO

end: jmp end

character: ' '
const1: 1
constNegativeLF: -10
constLF: 10
constNegativea: -97
consta: 97
constNegativezPlus1: -123
constzPlus1: 123
constNegative32: -32

IO: .org 0x1fff