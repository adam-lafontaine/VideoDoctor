GPP := g++-11

GPP += -std=c++20
GPP += -mavx
#GPP += -O3
#GPP += -DNDEBUG

GPP += -DALLOC_COUNT

NO_FLAGS := 
SDL2   := `sdl2-config --cflags --libs`
OPENGL := -lGL -ldl

ALL_LFLAGS := $(SDL2) $(OPENGL)


root       := ../../../..

app   := $(root)/viewer
build := $(app)/build/ubuntu
src   := $(app)/src

pltfm := $(src)/pltfm/ubuntu

libs := $(root)/libs

exe := viewer

program_exe := $(build)/$(exe)


#*** imgui ***

imgui := $(pltfm)
imgui_options_h := $(imgui)/imgui_options.hpp

imgui_include_h := $(imgui)/imgui_include.hpp
imgui_include_h += $(imgui_options_h)

#**************


#*** libs/util ***

util := $(libs)/util

types_h        := $(util)/types.hpp

#************


#*** main cpp ***

main_c := $(pltfm)/viewer_main_ubuntu.cpp
main_o := $(build)/main.o
obj    := $(main_o)

main_dep := $(imgui_include_h)

# main_o.cpp

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