GCC_FLAGS = -m64 -fPIC  -fno-strict-aliasing -fno-exceptions -Wno-multichar -fmessage-length=0 -W -Wall -Wno-unused -Winit-self -Wpointer-arith -Wl,--no-undefined
GCC_FLAGS += -Wno-format -O2
;GCC_FLAGS += -g -ggdb -Wno-format

DEFS = -DSST64   
CFLAGS += $(DEFS)  
C_COMMON_FILES = \
 main.c \
 mrdump.c \
 callstack.c \
 ac.c \
 exception.c \
 module.c \
 armdis.c \
 armelf.c \
 memspace.c \
 symfile.c \
 os.c \
 \
 interact/feedback.c \
 interact/reference.c \
 interact/command.c \
 interact/cJSON.c \
 interact/logeigenclient.c \
 \
 dwarf/dwarf.c \
 dwarf/dwabbrev.c \
 dwarf/dwaranges.c \
 dwarf/dwframe.c \
 dwarf/dwinfo.c \
 dwarf/dwline.c \
 dwarf/dwloc.c \
 dwarf/dwranges.c \
 \
 script/cmm_script.c \
 script/script_gen.c \
 script/gdb_script.c \
 \
 db/db.c \
 db/rb_reason.c \
 db/rb_reason_tracker.c \
 db/interrupt_maps.c \
 \
 mm/mm.c \
 mm/page.c \
 mm/slub.c \
 mm/slub_tracker.c \
 \
 kernel/kernel.c \
 kernel/task.c \
 kernel/task_tracker.c \
 \
 drivers/drivers.c \
 drivers/android/logger.c \
 drivers/android/logger_tracker.c \
 \

all: KSST KSST64
KSST: $(C_COMMON_FILES)
	@gcc $(GCC_FLAGS) -I. -o $@ $^ -lm
	@strip --strip-all $@
	@echo build $@ success

KSST64: $(C_COMMON_FILES) 
	@gcc $(GCC_FLAGS) -DSST64 -I. -o $@ $^ -lm
	@strip --strip-all $@
	@echo build $@ success