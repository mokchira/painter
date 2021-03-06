
ifeq ($(OS), Windows_NT)
	OS = WIN
else 
	OS = UNIX
endif

CC = gcc
GLC = glslc

CFLAGS = -Wall -Wno-missing-braces -Wno-attributes -fPIC
DEFS = -DROOT=\"$(PWD)\"
LIBS = -lm -lcoal -lhell -lobsidian 
CFLAGS += $(DEFS)
ifeq ($(OS), WIN)
	OS_HEADERS = $(WIN_HEADERS)
	LIBEXT = dll
	LIBS += -lvulkan-1
	HOMEDIR = C:
	INEXTRA = -IC:\VulkanSDK\1.2.170.0\Include -IC:\msys64\mingw64\include\freetype2
	LDFLAGS = -LC:\VulkanSDK\1.2.170.0\Lib
else
	OS_HEADERS = $(UNIX_HEADERS)
	LIBEXT = so
	LIBS +=  -lvulkan
	HOMEDIR =  $(HOME)
	INEXTRA = -I/usr/include/freetype2 
	LDFLAGS = -L/opt/hfs18.6/dsolib
endif
LIBDIR  = $(HOMEDIR)/lib
DEV = $(HOMEDIR)/dev
LDFLAGS += -L$(HOMEDIR)/lib
INFLAGS = -I$(DEV) $(INEXTRA)
GLFLAGS = --target-env=vulkan1.2
LIBNAME = painter
LIBPATH = $(LIBDIR)/lib$(LIBNAME).$(LIBEXT)

O = build
GLSL = shaders
SPV  = shaders/spv

NAME = painter
X    = $(NAME)

DEPS =  \
		render.h \
		painter.h \
		paint.h \
		layer.h \
		undo.h \
		common.h \
		shaders/common.glsl \
		shaders/selcommon.glsl \
		shaders/raycommon.glsl \
		shaders/brush.glsl

OBJS =  \
		$(O)/paint.o \
		$(O)/render.o \
		$(O)/painter.o \
		$(O)/layer.o \
		$(O)/undo.o

FRAG  := $(patsubst %.frag,$(SPV)/%-frag.spv,$(notdir $(wildcard $(GLSL)/*.frag)))
VERT  := $(patsubst %.vert,$(SPV)/%-vert.spv,$(notdir $(wildcard $(GLSL)/*.vert)))
RGEN  := $(patsubst %.rgen,$(SPV)/%-rgen.spv,$(notdir $(wildcard $(GLSL)/*.rgen)))
RCHIT := $(patsubst %.rchit,$(SPV)/%-rchit.spv,$(notdir $(wildcard $(GLSL)/*.rchit)))
RMISS := $(patsubst %.rmiss,$(SPV)/%-rmiss.spv,$(notdir $(wildcard $(GLSL)/*.rmiss)))

debug: CFLAGS += -g -DVERBOSE=1
debug: all

release: CFLAGS += -DNDEBUG -O2
release: all

all: standalone houdini chalkboard bin lib 

win: shaders standalone bin 

shaders: $(FRAG) $(VERT) $(RGEN) $(RCHIT) $(RMISS)

.PHONY: obsidian
obsidian:
	cd obsidian/ ; make ; cd ..

clean: 
	rm -f $(O)/* $(LIBPATH) $(X) $(SPV)/*

tags:
	ctags -R .

standalone: g_standalone.c
	$(CC) $(CFLAGS) $(INFLAGS) $(LDFLAGS) -shared -o $@.$(LIBEXT) $< $(LIBS)

houdini: g_houdini.c
	$(CC) $(CFLAGS) $(INFLAGS) $(LDFLAGS) -shared -o $@.$(LIBEXT) $< $(LIBS)

chalkboard: g_chalkboard.c
	$(CC) $(CFLAGS) $(INFLAGS) $(LDFLAGS) -shared -o $@.$(LIBEXT) $< $(LIBS)

bin: main.c $(OBJS) $(DEPS) shaders
	$(CC) $(CFLAGS) $(INFLAGS) $(LDFLAGS) $(OBJS) $< -o $(X) $(LIBS)

lib: $(OBJS) $(DEPS) shaders
	$(CC) $(LDFLAGS) -shared -o $(LIBPATH) $(OBJS) $(LIBS)

staticlib: $(OBJS) $(DEPS) shaders
	ar rcs $(LIB)/lib$(NAME).a $(OBJS)

$(O)/%.o:  %.c $(DEPS)
	$(CC) $(CFLAGS) $(INFLAGS) -c $< -o $@

$(SPV)/%-vert.spv: $(GLSL)/%.vert $(DEPS)
	$(GLC) $(GLFLAGS) $< -o $@

$(SPV)/%-frag.spv: $(GLSL)/%.frag
	$(GLC) $(GLFLAGS) $< -o $@

$(SPV)/%-rchit.spv: $(GLSL)/%.rchit
	$(GLC) $(GLFLAGS) $< -o $@

$(SPV)/%-rgen.spv: $(GLSL)/%.rgen
	$(GLC) $(GLFLAGS) $< -o $@

$(SPV)/%-rmiss.spv: $(GLSL)/%.rmiss
	$(GLC) $(GLFLAGS) $< -o $@
