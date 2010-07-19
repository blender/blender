
FILE(GLOB SRC intern/*.c operators/*.c)

SET(INC . intern
	../makesdna ../makesrna ../blenkernel ../blenlib ../../../intern/guardedalloc ../editors/include ../editors/mesh)

BLENDERLIB(bf_bmesh "${SRC}" "${INC}")

