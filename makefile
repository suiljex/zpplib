# Пути установки библиотеки
inc = "/usr/include/
lib = "/usr/local/lib/"

LIBNAME = zpplib.so
TESTNAME = $(addprefix test_, $(basename $(LIBNAME)))

CXX = g++
LINK = g++

SOURCE_DIR = src
INCLUDE_DIR = include
TEST_DIR = test

INCPATH = -I. -I$(INCLUDE_DIR)

CXXFLAGS = -fPIC -MD
CXXFLAGS += -Wall -W -Wextra -Wcast-qual -Wunreachable-code
CXXFLAGS += $(INCPATH)
LIBFLAGS = -shared
LIBFLAGS += -lz

HEADERS = $(notdir $(wildcard $(addsuffix /*.hpp,$(INCLUDE_DIR))))
SOURCES = $(notdir $(wildcard $(addsuffix /*.cpp,$(SOURCE_DIR))))
OBJECTS = $(patsubst %.cpp,%.o,$(SOURCES))
TESTOBJ = $(patsubst %.cpp,%.o,$(notdir $(wildcard $(addsuffix /*.cpp,$(TEST_DIR)))))

COPY_FILE = cp -f
COPY_DIR = $(COPY_FILE) -R
DEL_FILE = rm -f
DEL_DIR = $(DEL_FILE) -R
MK_DIR = mkdir --parents

DIRS = $(SOURCE_DIR) $(INCLUDE_DIR) $(TEST_DIR)

VPATH := $(SOURCE_DIR) $(TEST_DIR)

# ЦЕЛИ
# ==============================================================================

all: $(LIBNAME)

# Отслеживание зависимостей от заголовочных файлов
include $(wildcard *.d)

# Сборка библиотеки демона
$(LIBNAME): $(OBJECTS)
	$(LINK) $(LIBFLAGS) $(CXXFLAGS) -o $@ $^

# Очистка папки от объектных файлов
soft_clean:
	-$(DEL_FILE) *.d *.o

# Очистка папки от созданных файлов
clean: soft_clean
	-$(DEL_FILE) $(LIBNAME) $(TESTNAME)
	
test: CXXFLAGS += -DTESTING -lgtest_main -lgtest -lpthread
test: $(TESTNAME)
	./$(TESTNAME)
	
$(TESTNAME): $(TESTOBJ) $(OBJECTS)
	$(LINK) $(CXXFLAGS) -o $@ $^

# Копирование заголовочных файлов и библиотеки в общие директории
install: $(LIBNAME)
	-$(MK_DIR) $(lib)
	$(COPY_FILE) $(LIBNAME) $(lib)/
	-$(MK_DIR) $(inc)
	$(COPY_FILE) $(INCLUDE_DIR)/* $(inc)/

# Удаление заголовочных файлов и библиотеки из общих директорий
uninstall:
	-$(DEL_FILE) $(lib)/$(LIBNAME)
	-for FILE in $(HEADERS) ; do \
		$(DEL_FILE) $(inc)/$$FILE ; \
	done

TARGET_DIR := $(lib)
TMP_DIR := "/var/tmp/"
BUILD_DATE_TIME :=`date +%Y.%m.%d.%H.%M`

PACKAGE_NAME:="lib-"`echo $(LIBNAME) | sed -e's/_//g' -e's/\.so//'`
PACKAGE_VERSION:="$(BUILD_DATE_TIME)"
deb: $(LIBNAME)
	@$(shell [ -d "./deb" ] && rm -rf "./deb" )
	@$(shell mkdir "./deb" )
	@rm -rf      "$(TMP_DIR)/$(PACKAGE_NAME)"
	@mkdir -p -- "$(TMP_DIR)/$(PACKAGE_NAME)/DEBIAN"
	@mkdir -p -- "$(TMP_DIR)/$(PACKAGE_NAME)/$(TARGET_DIR)"
	@cp ./$(LIBNAME) "$(TMP_DIR)/$(PACKAGE_NAME)/$(TARGET_DIR)"
	@echo "Package     : $(PACKAGE_NAME)"       >> "$(TMP_DIR)/$(PACKAGE_NAME)/DEBIAN/control"
	@echo "Version     : $(PACKAGE_VERSION)"    >> "$(TMP_DIR)/$(PACKAGE_NAME)/DEBIAN/control"
	@echo "Maintainer  : root@localhost"        >> "$(TMP_DIR)/$(PACKAGE_NAME)/DEBIAN/control"
	@echo "Architecture: amd64"                 >> "$(TMP_DIR)/$(PACKAGE_NAME)/DEBIAN/control"
	@echo "Section     : misc"                  >> "$(TMP_DIR)/$(PACKAGE_NAME)/DEBIAN/control"
	@echo "Description : module"                >> "$(TMP_DIR)/$(PACKAGE_NAME)/DEBIAN/control"
	@echo " lib"                                >> "$(TMP_DIR)/$(PACKAGE_NAME)/DEBIAN/control"
	@echo ""                                    >> "$(TMP_DIR)/$(PACKAGE_NAME)/DEBIAN/control"
	@echo "Build deb $(PACKAGE_NAME)"
	dpkg-deb --build    "$(TMP_DIR)/$(PACKAGE_NAME)" "./deb"
	@rm -rf             "$(TMP_DIR)/$(PACKAGE_NAME)"

deb-dev: $(LIBNAME)
ifeq ($(INCLUDE_DIR),)
	@echo "Для компонента не определен порядок создания пакета разработки"
else
	@$(shell [ -d "./deb" ] || mkdir "./deb" )
	@rm -rf      "$(TMP_DIR)/$(PACKAGE_NAME)"
	@mkdir -p -- "$(TMP_DIR)/$(PACKAGE_NAME)/DEBIAN"
	@mkdir -p -- "$(TMP_DIR)/$(PACKAGE_NAME)/$(inc)"
	@cp -r ./$(INCLUDE_DIR)/* "$(TMP_DIR)/$(PACKAGE_NAME)/$(inc)"
	@echo "Package     : $(PACKAGE_NAME)-dev"   >> "$(TMP_DIR)/$(PACKAGE_NAME)/DEBIAN/control"
	@echo "Version     : $(PACKAGE_VERSION)"    >> "$(TMP_DIR)/$(PACKAGE_NAME)/DEBIAN/control"
	@echo "Maintainer  : root@localhost"        >> "$(TMP_DIR)/$(PACKAGE_NAME)/DEBIAN/control"
	@echo "Architecture: amd64"                 >> "$(TMP_DIR)/$(PACKAGE_NAME)/DEBIAN/control"
	@echo "Section     : misc"                  >> "$(TMP_DIR)/$(PACKAGE_NAME)/DEBIAN/control"
	@echo "Description : module includes"       >> "$(TMP_DIR)/$(PACKAGE_NAME)/DEBIAN/control"
	@echo " lib headers"                        >> "$(TMP_DIR)/$(PACKAGE_NAME)/DEBIAN/control"
	@echo ""                                    >> "$(TMP_DIR)/$(PACKAGE_NAME)/DEBIAN/control"
	@echo "Build deb $(PACKAGE_NAME)"
	dpkg-deb --build    "$(TMP_DIR)/$(PACKAGE_NAME)" "./deb"
	@rm -rf             "$(TMP_DIR)/$(PACKAGE_NAME)"
endif
