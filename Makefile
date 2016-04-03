###############################################################################
# (GNU make is required)
# Usage:
# 		To comple .c to .s	: make x.s
# 		To comple to .nes	: make x.nes
# 		To run emulator		: make x.run
#
# 		character rom -> neschar.inc (ascii code)
# 		memory map -> map.h
###############################################################################
# 2014-12-19 : Initial vertion by T.Furukawa	based on AISoC's Makefile
# 2015-04-15 : For embedded systems class
# 2016-04-02 : Improved
###############################################################################

EMULATOR := java -jar /home/course/embedded/tools/bjne.jar
RM       := rm -rf

TARGET := nes
LIBS   := nes.lib

CC65_DIR   := /usr/share/cc65
CC65       := cc65
CC65_FLAGS := --target $(TARGET) -I $(CC65_DIR)/include/ -Oi
CA65       := ca65
CA65_FLAGS := --target $(TARGET) -I $(CC65_DIR)/asminc/
LD65       := ld65
LD65_FLAGS := --target $(TARGET) --cfg-path $(CC65_DIR)/cfg/ --lib-path $(CC65_DIR)/lib

C_FILES   = $(wildcard *.c)
H_FILES   = $(wildcard *.h)
NES_FILES = $(C_FILES:%.c=%.nes)


.PHONY: all
all: $(NES_FILES)

.PHONY: clean
clean:
	$(RM) *.s *.o *.nes *.hex *.chr *.prg *~ *.map


################################
# Suffix rules
################################
.SUFFIXES:
.SUFFIXES: .nes .s .c

%.s: %.c $(H_FILES)
	$(CC65) $(CC65_FLAGS) $<

%.o: %.s
	$(CA65) $(CA65_FLAGS) $<

%.nes: %.o
	$(LD65) $(LD65_FLAGS) $< --mapfile $*.map -o $@ --lib $(LIBS)

%.run: %.nes
	$(EMULATOR) $<

%.chr: %.nes
	( PRG_N=`od -v -An -t x1 $< |awk 'NR==1 {print strtonum("0x" $$5)*0x4000/0x10}'`; \
	  CHA_N=`od -v -An -t x1 $< |awk 'NR==1 {print strtonum("0x" $$6)*0x2000/0x10}'`; \
	  od -v -An -t x1 $< |awk "NR>$$PRG_N+1&&NR<=($$CHA_N+$$PRG_N+1)" > $@ ; )

%.prg: %.nes
	( PRG_N=`od -v -An -t x1 $< |awk 'NR==1 {print strtonum("0x" $$5)*0x4000/0x10}'`; \
	  CHA_N=`od -v -An -t x1 $< |awk 'NR==1 {print strtonum("0x" $$6)*0x2000/0x10}'`; \
	  od -v -An -t x1 $< |awk "NR>1&&NR<=$$PRG_N+1" > $@ ; )
