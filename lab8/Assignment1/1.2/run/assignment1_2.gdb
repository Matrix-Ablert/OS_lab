file ../build/kernel.o
target remote :1234
set disassembly-flavor intel

b *0xc00226bd
b *0xc0022667
b *0xc00226a2

c
printf "\n===== before int 0x80 =====\n"
info registers cs ss esp eip eflags eax ebx ecx edx esi edi
p/x tss
x/16wx $esp

si
printf "\n===== asm_system_call_handler entry =====\n"
info registers cs ss esp eip eflags eax ebx ecx edx esi edi
x/16wx $esp

c
printf "\n===== before iret =====\n"
info registers cs ss esp eip eflags eax
x/16wx $esp

si
printf "\n===== after iret =====\n"
info registers cs ss esp eip eflags eax
x/16wx $esp
