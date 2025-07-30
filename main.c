#include "jvm.h"
#include "class_loader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Print usage information
void print_usage(const char* program_name) {
    printf("Usage: %s <class_file> [method_name]\n", program_name);
    printf("  class_file  - Path to .class file\n");
    printf("  method_name - Method to execute (default: main)\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s HelloWorld.class\n", program_name);
    printf("  %s Calculator.class add\n", program_name);
}

// Print detailed class information
void print_class_info(const LoadedClass* loaded_class) {
    printf("Magic: 0x%X\n", loaded_class->magic);
    printf("Version: %d.%d\n", loaded_class->major_version, loaded_class->minor_version);
    printf("Access flags: 0x%X\n", loaded_class->access_flags);
    printf("Constant pool count: %d\n", loaded_class->constant_pool_count);
    printf("Methods count: %d\n", loaded_class->methods_count);
    printf("Fields count: %d\n", loaded_class->fields_count);

    // Print class name
    if (loaded_class->this_class > 0 &&
        loaded_class->this_class < loaded_class->constant_pool_count) {
        ConstantPoolEntry* class_entry = &loaded_class->constant_pool[loaded_class->this_class];
        if (class_entry->tag == CONST_CLASS) {
            ConstantPoolEntry* name_entry = &loaded_class->constant_pool[class_entry->class_info.string_index];
            if (name_entry->tag == CONST_UTF8) {
                printf("Class name: %.*s\n", name_entry->utf8_info.length, name_entry->utf8_info.bytes);
            }
        }
    }

    // Print methods information
    for (uint16_t i = 0; i < loaded_class->methods_count; i++) {
        const LoadedMethodInfo* method = &loaded_class->methods[i];
        
        // Get method name
        if (method->name_index > 0 && method->name_index < loaded_class->constant_pool_count) {
            ConstantPoolEntry* name_entry = &loaded_class->constant_pool[method->name_index];
            if (name_entry->tag == CONST_UTF8) {
                printf("Method %d: %.*s", i, name_entry->utf8_info.length, name_entry->utf8_info.bytes);
                
                // Get method descriptor
                if (method->descriptor_index > 0 && method->descriptor_index < loaded_class->constant_pool_count) {
                    ConstantPoolEntry* desc_entry = &loaded_class->constant_pool[method->descriptor_index];
                    if (desc_entry->tag == CONST_UTF8) {
                        printf(" %.*s", desc_entry->utf8_info.length, desc_entry->utf8_info.bytes);
                    }
                }
                printf("\n");
                
                printf("  Access flags: 0x%X\n", method->access_flags);
                printf("  Max stack: %d\n", method->max_stack);
                printf("  Max locals: %d\n", method->max_locals);
                printf("  Code length: %d\n", method->code_length);
                
                if (method->code_length > 0 && method->code) {
                    printf("  Bytecode: ");
                    for (uint32_t j = 0; j < method->code_length && j < 20; j++) {
                        printf("%02X ", method->code[j]);
                    }
                    if (method->code_length > 20) {
                        printf("...");
                    }
                    printf("\n");
                }
            }
        }
        printf("\n");
    }
}

// Find method by name in loaded class
int find_method_by_name(const LoadedClass* loaded_class, const char* method_name) {
    for (uint16_t i = 0; i < loaded_class->methods_count; i++) {
        const LoadedMethodInfo* method = &loaded_class->methods[i];
        if (method->name_index > 0 && method->name_index < loaded_class->constant_pool_count) {
            ConstantPoolEntry* name_entry = &loaded_class->constant_pool[method->name_index];
            if (name_entry->tag == CONST_UTF8) {
                if (name_entry->utf8_info.length == strlen(method_name) &&
                    strncmp(name_entry->utf8_info.bytes, method_name, name_entry->utf8_info.length) == 0) {
                    return i;
                }
            }
        }
    }
    return -1;
}

// Main entry point
int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    const char* class_file = argv[1];
    const char* method_name = (argc >= 3) ? argv[2] : "main";

    // Load .class file
    LoadedClass loaded_class;
    if (load_class_file(class_file, &loaded_class) != 0) {
        printf("Error: Failed to load class file '%s'\n", class_file);
        return 1;
    }

    // Convert to JVM format
    ClassInfo jvm_class;
    if (convert_to_jvm_class(&loaded_class, &jvm_class) != 0) {
        printf("Error: Failed to convert class to JVM format\n");
        free_loaded_class(&loaded_class);
        return 1;
    }

    // Initialize JVM
    JVM jvm;
    if (jvm_init(&jvm) != 0) {
        printf("Error: Failed to initialize JVM\n");
        free_jvm_class(&jvm_class);
        free_loaded_class(&loaded_class);
        return 1;
    }

    // Register standard native methods
    register_standard_native_methods(&jvm);

    // Load class into JVM
    if (jvm_load_class(&jvm, &jvm_class) != 0) {
        printf("Error: Failed to load class into JVM\n");
        jvm_destroy(&jvm);
        free_jvm_class(&jvm_class);
        free_loaded_class(&loaded_class);
        return 1;
    }

    // Find target method
    int method_index = find_method_by_name(&loaded_class, method_name);
    if (method_index == -1) {
        printf("Error: Method '%s' not found in class\n", method_name);
        printf("Available methods:\n");
        for (uint16_t i = 0; i < loaded_class.methods_count; i++) {
            const LoadedMethodInfo* method = &loaded_class.methods[i];
            if (method->name_index > 0 && method->name_index < loaded_class.constant_pool_count) {
                ConstantPoolEntry* name_entry = &loaded_class.constant_pool[method->name_index];
                if (name_entry->tag == CONST_UTF8) {
                    printf("  %.*s\n", name_entry->utf8_info.length, name_entry->utf8_info.bytes);
                }
            }
        }
        jvm_destroy(&jvm);
        free_jvm_class(&jvm_class);
        free_loaded_class(&loaded_class);
        return 1;
    }

    // Execute method
    int result = jvm_execute_method(&jvm, jvm_class.name, method_name);

    // Cleanup resources
    jvm_destroy(&jvm);
    free_jvm_class(&jvm_class);
    free_loaded_class(&loaded_class);

    return result;
}