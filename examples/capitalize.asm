; This program takes characters from terminal input and echoes them back capitalized until Return is pressed.

loop:
ld IO
st character
jmz loop

; if character == '\n' (10) then end
add #-'\n'
jmz onReturn
add #'\n'

; if not character < 'a' (97) and character < 'z'+1 (123) then print character - 32; else print character
add #-'a'
jmn echo
add #'a' 

add #-'z'-1
jmn capitalize
add #'z'+1 

echo:
ld character
st IO
jmp loop

capitalize:
ld character
add #32
st IO
jmp loop

onReturn:
ld character
st IO

end: jmp end

character: ' '

IO: .org 0x1fff