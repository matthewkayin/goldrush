%:: %,v
%:: RCS/%,v
%:: RCS/%
%:: s.%
%:: SCCS/s.%

ASSEMBLY := gold
EXTENSION :=
DIR := $(subst /,\,${CURDIR})
LIB_DIR := lib
BUILD_DIR := bin
OBJ_DIR := obj
INCLUDE_FLAGS := -Isrc -Ivendor
COMPILER_FLAGS := -std=c++17 -Wall -g -O0
LINKER_FLAGS := -g -L$(LIB_DIR) -lSDL3 -lSDL3_image -lSDL3_ttf
DEFINES := -D_CRT_SECURE_NO_WARNINGS

ifeq ($(OS),Windows_NT)
	BUILD_PLATFORM := WIN32
	EXTENSION := .exe

# Make does not offer a recursive wildcard function, so here's one:
	rwildcard=$(wildcard $1$2) $(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2))
	SRC_FILES := $(call rwildcard,src/,*.cpp) # Get all .c files
	SRC_FILES += $(call rwildcard,vendor/,*.cpp) # Get all .c files
	DIRECTORIES := \src $(subst $(DIR),,$(shell dir src /S /AD /B | findstr /i src)) # Get all directories under src.
	DIRECTORIES += \vendor $(subst $(DIR),,$(shell dir vendor /S /AD /B | findstr /i vendor)) # Get all directories under vendor.

# ws2_32 and winmm are linked for enet
	LINKER_FLAGS += -luser32 -lws2_32 -lwinmm -lenet64
else
	UNAME_S := $(shell uname -s)
	ifeq ($(UNAME_S),Darwin)
		BUILD_PLATFORM := OSX
# Use -Wno-deprecated-declarations on OSX because Apple clang considers sprintf() as deprecated (sprintf() is used by logger)
		COMPILER_FLAGS += -Wno-deprecated-declarations
	endif

	SRC_FILES := $(shell find src -type f \( -name "*.cpp" -o -name "*.mm" \))
	SRC_FILES += $(shell find vendor -type f \( -name "*.cpp" -o -name "*.mm" \))
	DIRECTORIES := $(shell find src -type d)
	DIRECTORIES += $(shell find vendor -type d)

# LINKER_FLAGS += -L/opt/homebrew/lib -lenet
	LINKER_FLAGS := -g
	LINKER_FLAGS += $(shell pkg-config --libs --static libenet)
	LINKER_FLAGS += $(shell pkg-config --libs --static sdl3)
	LINKER_FLAGS += $(shell pkg-config --libs --static sdl3-image)
	LINKER_FLAGS += $(shell pkg-config --libs --static sdl3-ttf)
endif

OBJ_FILES := $(SRC_FILES:%=$(OBJ_DIR)/%.o) # Get all compiled .c.o objects for engine

all: scaffold compile link

.PHONY: prep
prep:
ifeq ($(BUILD_PLATFORM),WIN32)
	-@setlocal enableextensions enabledelayedexpansion && copy $(LIB_DIR) $(BUILD_DIR)
endif

.PHONY: osx-bundle
osx-bundle:
ifeq ($(BUILD_PLATFORM),OSX)
	@./appify.sh -s $(BUILD_DIR)/gold -i icon.icns
	@mv $(ASSEMBLY).app $(BUILD_DIR)/$(ASSEMBLY).app
	@cp -a ./res/ $(BUILD_DIR)/$(ASSEMBLY).app/Contents/Resources/
endif

.PHONY: scaffold
scaffold:
	@echo Scaffolding...
ifeq ($(BUILD_PLATFORM),WIN32)
	-@setlocal enableextensions enabledelayedexpansion && mkdir $(addprefix $(OBJ_DIR), $(DIRECTORIES)) 2>NUL || cd .
	-@setlocal enableextensions enabledelayedexpansion && mkdir $(BUILD_DIR) 2>NUL || cd .
else
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(addprefix $(OBJ_DIR)/,$(DIRECTORIES))
endif
	@echo Done.

.PHONY: link
link: scaffold $(OBJ_FILES) # link
	@echo Linking $(ASSEMBLY)...
ifeq ($(BUILD_PLATFORM),WIN32)
	@clang++ $(OBJ_FILES) -o $(BUILD_DIR)\$(ASSEMBLY)$(EXTENSION) $(LINKER_FLAGS)
else
	@clang++ $(OBJ_FILES) -o $(BUILD_DIR)/$(ASSEMBLY)$(EXTENSION) $(LINKER_FLAGS)
endif

.PHONY: compile
compile: #compile .c files
	@echo Compiling...

.PHONY: clean
clean: # clean build directory
ifeq ($(BUILD_PLATFORM),WIN32)
	@echo Cleaning...
	if exist $(BUILD_DIR)\$(ASSEMBLY)$(EXTENSION) del $(BUILD_DIR)\$(ASSEMBLY)$(EXTENSION)
	rmdir /s /q $(OBJ_DIR)
else
	@rm -rf $(BUILD_DIR)/$(ASSEMBLY)$(EXTENSION)
	@rm -rf $(BUILD_DIR)/$(ASSEMBLY).app
	@rm -rf $(OBJ_DIR)
endif

$(OBJ_DIR)/%.cpp.o: %.cpp # compile .c to .c.o object
	@echo   $<...
	@clang++ $< $(COMPILER_FLAGS) -c -o $@ $(DEFINES) $(INCLUDE_FLAGS)

# compile .m to .o object only for macos
ifeq ($(BUILD_PLATFORM),OSX)
$(OBJ_DIR)/%.mm.o: %.mm
	@echo   $<...
	@clang $< $(COMPILER_FLAGS) -c -o $@ $(DEFINES) $(INCLUDE_FLAGS)
endif