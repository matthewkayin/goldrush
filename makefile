%:: %,v
%:: RCS/%,v
%:: RCS/%
%:: s.%
%:: SCCS/s.%

ASSEMBLY := gold
DIR := $(subst /,\,${CURDIR})
LIB_DIR := lib
BUILD_DIR := bin
OBJ_DIR := obj
INCLUDE_FLAGS := -Isrc -Ivendor
COMPILER_FLAGS := -std=c++17 -Wall
LINKER_FLAGS := -std=c++17
DEFINES := -D_CRT_SECURE_NO_WARNINGS
RC_FILES :=

ifneq ($(APP_VERSION),)
	DEFINES += -DAPP_VERSION=$(APP_VERSION)
endif

ifeq ($(RELEASE),true)
	COMPILER_FLAGS += -O3
	DEFINES += -DGOLD_RELEASE
else
	COMPILER_FLAGS += -O0 -g
	LINKER_FLAGS += -g
endif

ifeq ($(OS),Windows_NT)
	SHELL := cmd.exe
	BUILD_PLATFORM := win64
	EXTENSION := .exe
	LIB_DIR := lib\win64

# Make does not offer a recursive wildcard function, so here's one:
	rwildcard=$(wildcard $1$2) $(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2))
	SRC_FILES := $(call rwildcard,src/,*.cpp) # Get all .c files
	SRC_FILES += $(call rwildcard,vendor/,*.cpp) # Get all .c files
	RC_FILES := src/win_icon.rc
	DIRECTORIES := \src $(subst $(DIR),,$(shell dir src /S /AD /B | findstr /i src)) # Get all directories under src.
	DIRECTORIES += \vendor $(subst $(DIR),,$(shell dir vendor /S /AD /B | findstr /i vendor)) # Get all directories under vendor.

# ws2_32 and winmm are linked for enet
	LINKER_FLAGS += -L$(LIB_DIR) -lSDL3 -lSDL3_image -lSDL3_ttf -luser32 -lws2_32 -lwinmm -lenet64 -lsteam_api64 -ldbghelp
else
	UNAME_S := $(shell uname -s)
	ifeq ($(UNAME_S),Darwin)
		BUILD_PLATFORM := macos
		EXTENSION :=
# Use -Wno-deprecated-declarations on MacOS because Apple clang considers sprintf() as deprecated (sprintf() is used by logger)
		COMPILER_FLAGS += -Wno-deprecated-declarations -mmacos-version-min=14.5
		LINKER_FLAGS += -Llib/macos -lenet -lsteam_api -Flib/macos -framework SDL3 -framework SDL3_image -framework SDL3_ttf
	endif
	ifeq ($(UNAME_S),Linux)
		BUILD_PLATFORM := linux
		LINKER_FLAGS += -Llib/linux64 -lSDL3 -lSDL3_image -lSDL3_ttf -lenet -lsteam_api -ldl
	endif

	SRC_FILES := $(shell find src -type f \( -name "*.cpp" -o -name "*.mm" \))
	SRC_FILES += $(shell find vendor -type f \( -name "*.cpp" -o -name "*.mm" \))
	DIRECTORIES := $(shell find src -type d)
	DIRECTORIES += $(shell find vendor -type d)
endif

OBJ_FILES := $(SRC_FILES:%=$(OBJ_DIR)/%.o) # Get all compiled .c.o objects for engine
ifeq ($(BUILD_PLATFORM),win64)
	OBJ_FILES += $(RC_FILES:%=$(OBJ_DIR)/%.res)
endif

all: scaffold compile link

.PHONY: prep
prep:
ifeq ($(BUILD_PLATFORM),win64)
	-@setlocal enableextensions enabledelayedexpansion && xcopy $(LIB_DIR) $(BUILD_DIR)
endif

.PHONY: scaffold
scaffold:
	@echo Scaffolding...
ifeq ($(BUILD_PLATFORM),win64)
	-@setlocal enableextensions enabledelayedexpansion && mkdir $(addprefix $(OBJ_DIR), $(DIRECTORIES)) 2>NUL || cd .
	-@setlocal enableextensions enabledelayedexpansion && mkdir $(BUILD_DIR) 2>NUL || cd .
else
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(addprefix $(OBJ_DIR)/,$(DIRECTORIES))
endif
	@echo Done.

.PHONY: link
link: scaffold $(OBJ_FILES) # link
	@echo Linker flags $(LINKER_FLAGS)
	@echo Linking $(ASSEMBLY)...
ifeq ($(BUILD_PLATFORM),win64)
	@clang++ $(OBJ_FILES) -o $(BUILD_DIR)\$(ASSEMBLY)$(EXTENSION) $(LINKER_FLAGS)
else
	@clang++ $(OBJ_FILES) -o $(BUILD_DIR)/$(ASSEMBLY)$(EXTENSION) $(LINKER_FLAGS)
endif

.PHONY: compile
compile: #compile .c files
	@echo Compiler flags $(COMPILER_FLAGS)
	@echo Defines $(DEFINES)
	@echo Compiling...

.PHONY: clean
clean: # clean build directory
ifeq ($(BUILD_PLATFORM),win64)
	@echo Cleaning...
	if exist $(BUILD_DIR)\$(ASSEMBLY)$(EXTENSION) del $(BUILD_DIR)\$(ASSEMBLY)$(EXTENSION)
	rmdir /s /q $(OBJ_DIR)
else
	@rm -rf $(OBJ_DIR)
	@rm -rf $(BUILD_DIR)/$(ASSEMBLY)$(EXTENSION)
endif
ifeq ($(BUILD_PLATFORM),macos)
	@rm -rf $(BUILD_DIR)/Gold\ Rush.app
endif

$(OBJ_DIR)/%.cpp.o: %.cpp # compile .c to .c.o object
	@echo   $<...
	@clang++ $< $(COMPILER_FLAGS) -c -o $@ $(DEFINES) $(INCLUDE_FLAGS)

ifeq ($(BUILD_PLATFORM),win64)
$(OBJ_DIR)/%.rc.res: %.rc # Windows resource scripts
	@rc -fo $@ $^
endif

.PHONY: bundle
bundle:
ifeq ($(BUILD_PLATFORM),win64)
	-@setlocal enableextensions enabledelayedexpansion && mkdir $(BUILD_DIR)\sprite
	-@setlocal enableextensions enabledelayedexpansion && xcopy /y .\res\sprite $(BUILD_DIR)\sprite
	-@setlocal enableextensions enabledelayedexpansion && mkdir $(BUILD_DIR)\shader
	-@setlocal enableextensions enabledelayedexpansion && xcopy /y .\res\shader $(BUILD_DIR)\shader
	-@setlocal enableextensions enabledelayedexpansion && mkdir $(BUILD_DIR)\font
	-@setlocal enableextensions enabledelayedexpansion && xcopy /y .\res\font $(BUILD_DIR)\font
	-@setlocal enableextensions enabledelayedexpansion && mkdir $(BUILD_DIR)\sfx
	-@setlocal enableextensions enabledelayedexpansion && xcopy /y .\res\sfx $(BUILD_DIR)\sfx
	-@setlocal enableextensions enabledelayedexpansion && cd $(BUILD_DIR) && tar.exe -acvf goldrush_windows.zip gold.exe *.dll *.lib font sfx shader sprite
endif
ifeq ($(BUILD_PLATFORM),macos)
	@./appify.sh -s $(BUILD_DIR)/gold -i icon.icns
	@mv $(ASSEMBLY).app $(BUILD_DIR)/Gold\ Rush.app
	@cp -a ./res/ $(BUILD_DIR)/Gold\ Rush.app/Contents/Resources/
	@cp -a ./lib/macos/ $(BUILD_DIR)/Gold\ Rush.app/Contents/MacOS/
	@mkdir $(BUILD_DIR)/Gold\ Rush.app/Contents/Frameworks
	@cp -r ./lib/macos/*.framework $(BUILD_DIR)/Gold\ Rush.app/Contents/Frameworks/
	@install_name_tool -add_rpath @executable_path/../Frameworks $(BUILD_DIR)/Gold\ Rush.app/Contents/MacOS/gold
	@cd $(BUILD_DIR) && zip -vr ./goldrush_macos.zip ./Gold\ Rush.app/
endif
ifeq ($(BUILD_PLATFORM),linux)
	@cp -a ./res/* $(BUILD_DIR)/
	@cp -a ./lib/linux64/* $(BUILD_DIR)/
	@tar -czvf goldrush_linux.tar.gz -C bin .
endif