%:: %,v
%:: RCS/%,v
%:: RCS/%
%:: s.%
%:: SCCS/s.%

EXTENSION :=
DIR := $(subst /,\,${CURDIR})
LIB_DIR := $(ASSEMBLY)/lib
BUILD_DIR := bin
OBJ_DIR := obj
INCLUDE_FLAGS := -I$(ASSEMBLY)/src -I$(ASSEMBLY)/vendor
COMPILER_FLAGS := -std=c++17 -Wall -g -O0
LINKER_FLAGS := -g -L$(LIB_DIR) $(ADDITIONAL_LFLAGS)
DEFINES := -D_CRT_SECURE_NO_WARNINGS

ifeq ($(OS),Windows_NT)
	BUILD_PLATFORM := WIN32
	EXTENSION := .exe

# Make does not offer a recursive wildcard function, so here's one:
	rwildcard=$(wildcard $1$2) $(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2))
	SRC_FILES := $(call rwildcard,$(ASSEMBLY)/,*.cpp) # Get all .c files
	DIRECTORIES := \$(ASSEMBLY)\src $(subst $(DIR),,$(shell dir $(ASSEMBLY)\src /S /AD /B | findstr /i src)) # Get all directories under src.
	DIRECTORIES += \$(ASSEMBLY)\vendor $(subst $(DIR),,$(shell dir $(ASSEMBLY)\vendor /S /AD /B | findstr /i vendor)) # Get all directories under vendor.
else
	UNAME_S := $(shell uname -s)
	ifeq ($(UNAME_S),Darwin)
		BUILD_PLATFORM := OSX
# Use -Wno-deprecated-declarations on OSX because Apple clang considers sprintf() as deprecated (sprintf() is used by logger)
		COMPILER_FLAGS += -Wno-deprecated-declarations
	endif

	SRC_FILES := $(shell find $(ASSEMBLY) -type f \( -name "*.cpp" -o -name "*.mm" \))
	DIRECTORIES := $(shell find ${ASSEMBLY} -type d)
endif

OBJ_FILES := $(SRC_FILES:%=$(OBJ_DIR)/%.o) # Get all compiled .c.o objects for engine

all: scaffold compile link

.PHONY: prep
prep:
ifeq ($(BUILD_PLATFORM),WIN32)
	-@setlocal enableextensions enabledelayedexpansion && copy $(LIB_DIR) $(BUILD_DIR)
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