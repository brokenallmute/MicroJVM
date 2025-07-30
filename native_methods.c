#include "jvm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Register native method
int jvm_register_native_method(JVM* jvm, const char* class_name,
                              const char* method_name, const char* descriptor,
                              NativeMethod function) {
    if (!jvm || !class_name || !method_name || !function) {
        return -1;
    }

    // Expand native methods array if needed
    if (jvm->native_methods_count == 0) {
        jvm->native_methods = malloc(32 * sizeof(NativeMethodEntry));
        if (!jvm->native_methods) {
            return -1;
        }
    }

    NativeMethodEntry* entry = &jvm->native_methods[jvm->native_methods_count];
    entry->class_name = class_name;
    entry->method_name = method_name;
    entry->descriptor = descriptor;
    entry->function = function;
    jvm->native_methods_count++;
    return 0;
}

// Create Java string
JString* jvm_create_string(JVM* jvm, const char* str) {
    if (!jvm || !str) {
        return NULL;
    }

    if (jvm->string_pool.count >= MAX_STRING_POOL) {
        return NULL;
    }

    JString* jstr = &jvm->string_pool.strings[jvm->string_pool.count];
    size_t len = strlen(str);
    jstr->capacity = len + 1;
    jstr->data = malloc(jstr->capacity);
    if (!jstr->data) {
        return NULL;
    }

    strcpy(jstr->data, str);
    jstr->length = len;
    jvm->string_pool.count++;
    return jstr;
}

// Print string to output
void jvm_print_string(JVM* jvm, JString* str) {
    if (!jvm || !str || !str->data) {
        return;
    }

    printf("%s", str->data);
    fflush(stdout);
}

// Read integer from input
int jvm_read_int(JVM* jvm) {
    (void)jvm; // Suppress warning
    int value;
    if (scanf("%d", &value) == 1) {
        return value;
    }
    return 0;
}

// Read line from input
char* jvm_read_line(JVM* jvm) {
    (void)jvm; // Suppress warning
    char* line = malloc(256);
    if (line && fgets(line, 256, stdin)) {
        // Remove newline character
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') {
            line[len-1] = '\0';
        }
        return line;
    }

    if (line) {
        free(line);
    }
    return NULL;
}

// System.out.print(String)
int native_system_out_print(JVM* jvm, jvalue* args, int arg_count) {
    if (!jvm || !args || arg_count < 1) {
        return -1;
    }

    JString* str = (JString*)args[0].ref;
    if (str) {
        jvm_print_string(jvm, str);
    }
    return 0;
}

// System.out.println(String)
int native_system_out_println(JVM* jvm, jvalue* args, int arg_count) {
    if (!jvm || !args || arg_count < 1) {
        return -1;
    }

    JString* str = (JString*)args[0].ref;
    if (str) {
        jvm_print_string(jvm, str);
    }

    printf("\n");
    fflush(stdout);
    return 0;
}

// System.out.print(int)
int native_system_out_print_int(JVM* jvm, jvalue* args, int arg_count) {
    (void)jvm; // Suppress warning
    if (!args || arg_count < 1) {
        return -1;
    }

    printf("%d", args[0].i);
    fflush(stdout);
    return 0;
}

// System.out.println(int)
int native_system_out_println_int(JVM* jvm, jvalue* args, int arg_count) {
    (void)jvm; // Suppress warning
    if (!args || arg_count < 1) {
        return -1;
    }

    printf("%d\n", args[0].i);
    fflush(stdout);
    return 0;
}

// System.out.println() - no arguments
int native_system_out_println_void(JVM* jvm, jvalue* args, int arg_count) {
    (void)jvm;
    (void)args;
    (void)arg_count;
    printf("\n");
    fflush(stdout);
    return 0;
}

// Scanner constructor
int native_scanner_init(JVM* jvm, jvalue* args, int arg_count) {
    (void)jvm;
    (void)args;
    (void)arg_count;
    // Scanner initialization - nothing to do in this implementation
    return 0;
}

// Scanner.nextInt()
int native_scanner_next_int(JVM* jvm, jvalue* args, int arg_count) {
    (void)args;
    (void)arg_count;
    return jvm_read_int(jvm);
}

// Scanner.nextLine()
int native_scanner_next_line(JVM* jvm, jvalue* args, int arg_count) {
    (void)args;
    (void)arg_count;
    char* line = jvm_read_line(jvm);
    if (line) {
        JString* jstr = jvm_create_string(jvm, line);
        free(line);
        return (int)(intptr_t)jstr;
    }
    return 0;
}

// Register all standard native methods
void register_standard_native_methods(JVM* jvm) {
    // System.out methods
    jvm_register_native_method(jvm, "java/lang/System", "out.print", "(Ljava/lang/String;)V",
                              native_system_out_print);
    jvm_register_native_method(jvm, "java/lang/System", "out.println", "(Ljava/lang/String;)V",
                              native_system_out_println);
    jvm_register_native_method(jvm, "java/lang/System", "out.print", "(I)V",
                              native_system_out_print_int);
    jvm_register_native_method(jvm, "java/lang/System", "out.println", "(I)V",
                              native_system_out_println_int);
    jvm_register_native_method(jvm, "java/lang/System", "out.println", "()V",
                              native_system_out_println_void);

    // Scanner methods
    jvm_register_native_method(jvm, "java/util/Scanner", "<init>", "(Ljava/io/InputStream;)V",
                              native_scanner_init);
    jvm_register_native_method(jvm, "java/util/Scanner", "nextInt", "()I",
                              native_scanner_next_int);
    jvm_register_native_method(jvm, "java/util/Scanner", "nextLine", "()Ljava/lang/String;",
                              native_scanner_next_line);
}