
.section .rodata
.align 4
 
.global model0_rodata_begin
.global model0_rodata_end
model0_rodata_begin:
.incbin "japanese_air.bin"
model0_rodata_end:

.global model1_rodata_begin
.global model1_rodata_end
model1_rodata_begin:
.incbin "english_air.bin"
model1_rodata_end:

