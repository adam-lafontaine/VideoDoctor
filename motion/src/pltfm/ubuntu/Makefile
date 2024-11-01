GPP := g++-11

GPP += -std=c++20
GPP += -mavx
#GPP += -O3
#GPP += -DNDEBUG

#GPP += -DALLOC_COUNT

NO_FLAGS := 
SDL2   := `sdl2-config --cflags --libs`
OPENGL := -lGL -ldl
FFMPEG := -lavformat -lavcodec -lavutil -lswscale 
#OPENCV := `pkg-config --cflags --libs opencv4`

ALL_LFLAGS := $(SDL2) $(OPENGL) $(FFMPEG) -lpthread


root       := ../../../..

app   := $(root)/motion
build := $(app)/build/ubuntu
src   := $(app)/src

pltfm := $(src)/pltfm/ubuntu

libs := $(root)/libs

exe := video_motion

program_exe := $(build)/$(exe)


#*** imgui ***

imgui := $(src)/pltfm/imgui_sdl2_ogl3
imgui_options_h := $(imgui)/imgui_options.hpp

imgui_include_h := $(imgui)/imgui_include.hpp
imgui_include_h += $(imgui_options_h)

#**************


#*** libs/util ***

util := $(libs)/util

types_h := $(util)/types.hpp

numeric_h := $(util)/numeric.hpp
numeric_h += $(types_h)

stack_buffer_h := $(util)/stack_buffer.hpp
stopwatch_h    := $(util)/stopwatch.hpp

#************


#*** alloc_type ***

alloc_type := $(libs)/alloc_type

alloc_type_h := $(alloc_type)/alloc_type.hpp
alloc_type_h += $(types_h)

alloc_type_c := $(alloc_type)/alloc_type.cpp
alloc_type_c += $(alloc_type_h)

#*************


#*** memory_buffer ***

memory_buffer_h := $(util)/memory_buffer.hpp
memory_buffer_h += $(alloc_type_h)

#***********


#*** span ***

span := $(libs)/span

span_h := $(span)/span.hpp
span_h += $(memory_buffer_h)
span_h += $(stack_buffer_h)
#span_h += $(qsprintf_h)

span_c := $(span)/span.cpp
span_c += $(span_h)

#************


#*** image ***

image := $(libs)/image

image_h := $(image)/image.hpp
image_h += $(span_h)

image_c := $(image)/image.cpp
image_c += $(image_h)
image_c += $(numeric_h)

#*************


#*** video ***

video := $(libs)/video

video_h := $(video)/video.hpp
video_h += $(image_h)

video_c := $(video)/video.cpp
video_c += $(video_h)

motion_h := $(video)/motion.hpp
motion_h += $(image_h)

motion_c := $(video)/motion.cpp
motion_c += $(motion_h)
motion_c += $(numeric_h)

#*************


#*** video_display ***

video_display := $(src)/video_display

video_display_h := $(video_display)/video_display.hpp
video_display_h += $(video_h)

video_display_c := $(video_display)/video_display.cpp
video_display_c += $(stopwatch_h)
video_display_c += $(numeric_h)

#***********


#*** main cpp ***

main_c := $(pltfm)/motion_main_ubuntu.cpp
main_o := $(build)/main.o
obj    := $(main_o)

main_dep := $(imgui_include_h)
main_dep+= $(video_display_h)

# main_o.cpp
main_dep += $(pltfm)/main_o.cpp
main_dep += $(alloc_type_c)
main_dep += $(image_c)
main_dep += $(span_c)
main_dep += $(video_c)
main_dep += $(motion_c)
main_dep += $(video_display_c)

#****************


#*** imgui cpp ***

imgui_c := $(imgui)/imgui_o.cpp
imgui_o := $(build)/imgui.o
obj     += $(imgui_o)

imgui_dep := $(imgui_options_h)

#****************


#*** app ***


$(main_o): $(main_c) $(main_dep)
	@echo "\n  main"
	$(GPP) -o $@ -c $< $(ALL_LFLAGS)


$(imgui_o): $(imgui_c) $(imgui_dep)
	@echo "\n  imgui"
	$(GPP) -o $@ -c $< $(SDL2) $(OPENGL)

#**************


$(program_exe): $(obj)
	@echo "\n  program_exe"
	$(GPP) -o $@ $+ $(ALL_LFLAGS)


build: $(program_exe)


run: build
	$(program_exe)
	@echo "\n"


clean:
	rm -fv $(build)/*


clean_main:
	rm -fv $(build)/main.o

setup:
	mkdir -p $(build)