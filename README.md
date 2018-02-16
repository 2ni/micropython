See README_MICROPYTHON.md for micropython readme.


## Keep fork up-to-date with original repo
https://help.github.com/articles/syncing-a-fork/
```
git fetch upstream
git checkout master
git merge upstream/master
```

```
git clone git@github.com:2ni/micropython.git
cd micropython
git remote add upstream git@github.com:micropython/micropython.git
git fetch upstream
git pull upstream master
```

## Differences with original micropython repo
This clone added a mymodule.c and frq.c.
- mymodule.c contains simple helloworld examples
- frq.c contains functions to get the frequency on an input pin with GPIO interrupt

## Howto compile things
install esp-open-sd if not yet done (see INSTALL_XTENSA.md)

```
git submodule update --init
export PATH="<dir to esp-open-sdk>/xtensa-lx106-elf/bin/:$PATH"
cd ports/esp8266
make
```

# Config files changed
```
diff --git a/ports/esp8266/Makefile b/ports/esp8266/Makefile
index 9d6e502c..6e9fc66f 100644
--- a/ports/esp8266/Makefile
+++ b/ports/esp8266/Makefile
@@ -59,6 +59,8 @@ LDFLAGS += --gc-sections
 endif
 
 SRC_C = \
+	frq.c \
+	mymodule.c \
 	strtoll.c \
 	main.c \
 	help.c \
```

```
diff --git a/ports/esp8266/mpconfigport.h b/ports/esp8266/mpconfigport.h
index c45ed92c..792d50ab 100644
--- a/ports/esp8266/mpconfigport.h
+++ b/ports/esp8266/mpconfigport.h
@@ -159,8 +159,12 @@ extern const struct _mp_obj_module_t uos_module;
 extern const struct _mp_obj_module_t mp_module_lwip;
 extern const struct _mp_obj_module_t mp_module_machine;
 extern const struct _mp_obj_module_t mp_module_onewire;
+extern const struct _mp_obj_module_t mp_module_mymodule;
+extern const struct _mp_obj_module_t mp_module_frq;
 
 #define MICROPY_PORT_BUILTIN_MODULES \
+    { MP_OBJ_NEW_QSTR(MP_QSTR_mymodule), (mp_obj_t)&mp_module_mymodule }, \
+    { MP_OBJ_NEW_QSTR(MP_QSTR_frq), (mp_obj_t)&mp_module_frq }, \
     { MP_ROM_QSTR(MP_QSTR_esp), MP_ROM_PTR(&esp_module) }, \
     { MP_ROM_QSTR(MP_QSTR_usocket), MP_ROM_PTR(&mp_module_lwip) }, \
     { MP_ROM_QSTR(MP_QSTR_network), MP_ROM_PTR(&network_module) }, \
```

```
diff --git a/ports/esp8266/qstrdefsport.h b/ports/esp8266/qstrdefsport.h
index 8f301a69..0455a323 100644
--- a/ports/esp8266/qstrdefsport.h
+++ b/ports/esp8266/qstrdefsport.h
@@ -29,3 +29,6 @@
 // Entries for sys.path
 Q(/)
 Q(/lib)
+Q(mymodule)
+Q(hello)
+Q(frq)
```
