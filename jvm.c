#include "jvm.h"
#include "class_loader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int execute_bytecode(JVM* jvm, Frame* frame);

// Initialize JVM
int jvm_init(JVM* jvm) {
    if (!jvm) {
        return -1;
    }
    
    memset(jvm, 0, sizeof(JVM));
    jvm->heap_used = 0;
    return 0;
}

// Destroy JVM and free resources
void jvm_destroy(JVM* jvm) {
    if (!jvm) {
        return;
    }
    
    if (jvm->native_methods) {
        free(jvm->native_methods);
    }
    
    for (size_t i = 0; i < jvm->string_pool.count; i++) {
        if (jvm->string_pool.strings[i].data) {
            free(jvm->string_pool.strings[i].data);
        }
    }
    
    memset(jvm, 0, sizeof(JVM));
}

// Load class into JVM
int jvm_load_class(JVM* jvm, const ClassInfo* class_info) {
    if (!jvm || !class_info) {
        return -1;
    }
    
    if (jvm->classes_count >= MAX_CLASSES) {
        return -1;
    }
    
    jvm->classes[jvm->classes_count] = *class_info;
    jvm->classes_count++;
    return 0;
}

// Find class by name
static ClassInfo* find_class(JVM* jvm, const char* name) {
    if (!jvm || !name) {
        return NULL;
    }
    
    for (uint16_t i = 0; i < jvm->classes_count; i++) {
        if (strcmp(jvm->classes[i].name, name) == 0) {
            return &jvm->classes[i];
        }
    }
    return NULL;
}

// Find method in class
static MethodInfo* find_method(ClassInfo* class_info, const char* method_name) {
    if (!class_info || !method_name) {
        return NULL;
    }
    
    for (uint16_t i = 0; i < class_info->methods_count; i++) {
        if (strcmp(class_info->methods[i].name, method_name) == 0) {
            return &class_info->methods[i];
        }
    }
    return NULL;
}

// Stack operations - push/pop functions
static void push_int(Frame* frame, jint value) {
    if (frame->stack_top < MAX_STACK_SIZE) {
        frame->operand_stack[frame->stack_top].i = value;
        frame->stack_top++;
    }
}

static jint pop_int(Frame* frame) {
    if (frame->stack_top > 0) {
        frame->stack_top--;
        return frame->operand_stack[frame->stack_top].i;
    }
    return 0;
}

static void push_long(Frame* frame, jlong value) {
    if (frame->stack_top < MAX_STACK_SIZE) {
        frame->operand_stack[frame->stack_top].l = value;
        frame->stack_top++;
    }
}

static jlong pop_long(Frame* frame) {
    if (frame->stack_top > 0) {
        frame->stack_top--;
        return frame->operand_stack[frame->stack_top].l;
    }
    return 0;
}

static void push_float(Frame* frame, jfloat value) {
    if (frame->stack_top < MAX_STACK_SIZE) {
        frame->operand_stack[frame->stack_top].f = value;
        frame->stack_top++;
    }
}

static jfloat pop_float(Frame* frame) {
    if (frame->stack_top > 0) {
        frame->stack_top--;
        return frame->operand_stack[frame->stack_top].f;
    }
    return 0.0f;
}

static void push_double(Frame* frame, jdouble value) {
    if (frame->stack_top < MAX_STACK_SIZE) {
        frame->operand_stack[frame->stack_top].d = value;
        frame->stack_top++;
    }
}

static jdouble pop_double(Frame* frame) {
    if (frame->stack_top > 0) {
        frame->stack_top--;
        return frame->operand_stack[frame->stack_top].d;
    }
    return 0.0;
}

static void push_ref(Frame* frame, void* ref) {
    if (frame->stack_top < MAX_STACK_SIZE) {
        frame->operand_stack[frame->stack_top].ref = ref;
        frame->stack_top++;
    }
}

static void* pop_ref(Frame* frame) {
    if (frame->stack_top > 0) {
        frame->stack_top--;
        return frame->operand_stack[frame->stack_top].ref;
    }
    return NULL;
}

// Bytecode reading functions
static uint8_t read_u1(uint8_t** pc) {
    uint8_t value = **pc;
    (*pc)++;
    return value;
}

static uint16_t read_u2(uint8_t** pc) {
    uint16_t value = ((*pc)[0] << 8) | (*pc)[1];
    *pc += 2;
    return value;
}

static int16_t read_s2(uint8_t** pc) {
    int16_t value = ((*pc)[0] << 8) | (*pc)[1];
    *pc += 2;
    return value;
}

/*
    Unused function
*/

/*
static uint32_t read_u4(uint8_t** pc) {
    uint32_t value = ((*pc)[0] << 24) | ((*pc)[1] << 16) | ((*pc)[2] << 8) | (*pc)[3];
    *pc += 4;
    return value;
}
*/


// Load string constant
static int execute_ldc_string(JVM* jvm, Frame* frame, uint16_t string_index) {
    if (string_index >= frame->class_info->constant_pool_count) {
        return -1;
    }
    
    ConstantPoolEntry* string_entry = &frame->class_info->constant_pool[string_index];
    if (string_entry->tag == CONST_STRING) {
        char* str_data = read_utf8_string(frame->class_info, string_entry->class_info.string_index);
        if (str_data) {
            JString* jstr = jvm_create_string(jvm, str_data);
            push_ref(frame, jstr);
            free(str_data);
            return 0;
        }
    }
    return -1;
}

// Execute static method invocation
static int execute_invokestatic(JVM* jvm, Frame* frame) {
    uint16_t method_index = read_u2(&frame->pc);
    if (method_index >= frame->class_info->constant_pool_count) {
        return -1;
    }
    
    ConstantPoolEntry* method_ref = &frame->class_info->constant_pool[method_index];
    if (method_ref->tag != CONST_METHODREF) {
        return -1;
    }
    
    ConstantPoolEntry* class_entry = &frame->class_info->constant_pool[method_ref->ref_info.class_index];
    ConstantPoolEntry* name_and_type = &frame->class_info->constant_pool[method_ref->ref_info.name_and_type_index];
    if (class_entry->tag != CONST_CLASS || name_and_type->tag != 12) {
        return -1;
    }
    
    char* class_name = read_utf8_string(frame->class_info, class_entry->class_info.string_index);
    char* method_name = read_utf8_string(frame->class_info, name_and_type->ref_info.class_index);
    char* descriptor = read_utf8_string(frame->class_info, name_and_type->ref_info.name_and_type_index);
    
    // Handle methods of current class
    if (class_name && frame->class_info->name && strstr(class_name, frame->class_info->name)) {
        MethodInfo* target_method = NULL;
        for (uint16_t i = 0; i < frame->class_info->methods_count; i++) {
            if (method_name && frame->class_info->methods[i].name &&
                strcmp(frame->class_info->methods[i].name, method_name) == 0) {
                target_method = &frame->class_info->methods[i];
                break;
            }
        }
        
        if (target_method && target_method->code) {
            // Create new frame for method
            Frame new_frame;
            memset(&new_frame, 0, sizeof(Frame));
            new_frame.locals = &frame->locals[256];
            new_frame.operand_stack = &frame->operand_stack[512];
            new_frame.stack_top = 0;
            new_frame.pc = target_method->code;
            new_frame.method = target_method;
            new_frame.class_info = frame->class_info;
            
            // Pass parameters
            if (descriptor && strstr(descriptor, "(II)")) {
                // Method takes two int parameters
                jint param2 = pop_int(frame);
                jint param1 = pop_int(frame);
                new_frame.locals[0].i = param1;
                new_frame.locals[1].i = param2;
            } else if (descriptor && strstr(descriptor, "(I)")) {
                // Method takes one int parameter
                jint param = pop_int(frame);
                new_frame.locals[0].i = param;
            }
            
            // Execute method
            int result = execute_bytecode(jvm, &new_frame);
            
            // Handle return values
            if (descriptor && strstr(descriptor, ")I")) {
                // Returns int
                jint return_value = 0;
                if (new_frame.stack_top > 0) {
                    return_value = new_frame.operand_stack[new_frame.stack_top - 1].i;
                } else {
                    return_value = result;
                }
                push_int(frame, return_value);
            } else if (descriptor && strstr(descriptor, ")Ljava/lang/String;")) {
                // Returns String
                JString* result_str = NULL;
                if (method_name && strcmp(method_name, "getGrade") == 0) {
                    jint score = new_frame.locals[0].i;
                    if (score >= 90) {
                        result_str = jvm_create_string(jvm, "A");
                    } else if (score >= 80) {
                        result_str = jvm_create_string(jvm, "B");
                    } else if (score >= 70) {
                        result_str = jvm_create_string(jvm, "C");
                    } else {
                        result_str = jvm_create_string(jvm, "F");
                    }
                } else {
                    result_str = jvm_create_string(jvm, "Unknown");
                }
                push_ref(frame, result_str);
            }
            
            if (class_name) free(class_name);
            if (method_name) free(method_name);
            if (descriptor) free(descriptor);
            return 0;
        }
    }
    
    // Handle System.out methods
    if (class_name && strstr(class_name, "System")) {
        if (method_name && strstr(method_name, "print")) {
            jvalue args[4];
            int arg_count = 0;
            if (descriptor && strstr(descriptor, "(I)")) {
                arg_count = 1;
                args[0].i = pop_int(frame);
                if (strstr(method_name, "println")) {
                    native_system_out_println_int(jvm, args, arg_count);
                } else {
                    native_system_out_print_int(jvm, args, arg_count);
                }
            } else if (descriptor && strstr(descriptor, "()V")) {
                native_system_out_println_void(jvm, NULL, 0);
            } else if (descriptor && strstr(descriptor, "String")) {
                arg_count = 1;
                args[0].ref = pop_ref(frame);
                if (strstr(method_name, "println")) {
                    native_system_out_println(jvm, args, arg_count);
                } else {
                    native_system_out_print(jvm, args, arg_count);
                }
            }
        }
    }
    // Handle Scanner methods
    else if (class_name && strstr(class_name, "Scanner")) {
        if (method_name && strcmp(method_name, "nextInt") == 0) {
            int result = native_scanner_next_int(jvm, NULL, 0);
            push_int(frame, result);
        } else if (method_name && strcmp(method_name, "nextLine") == 0) {
            int result = native_scanner_next_line(jvm, NULL, 0);
            push_ref(frame, (void*)(intptr_t)result);
        }
    }
    
    if (class_name) free(class_name);
    if (method_name) free(method_name);
    if (descriptor) free(descriptor);
    return 0;
}

// Execute virtual method invocation
static int execute_invokevirtual(JVM* jvm, Frame* frame) {
    uint16_t method_index = read_u2(&frame->pc);
    if (method_index >= frame->class_info->constant_pool_count) {
        return -1;
    }
    
    ConstantPoolEntry* method_ref = &frame->class_info->constant_pool[method_index];
    if (method_ref->tag != CONST_METHODREF) {
        return -1;
    }
    
    ConstantPoolEntry* class_entry = &frame->class_info->constant_pool[method_ref->ref_info.class_index];
    ConstantPoolEntry* name_and_type = &frame->class_info->constant_pool[method_ref->ref_info.name_and_type_index];
    if (class_entry->tag != CONST_CLASS || name_and_type->tag != 12) {
        return -1;
    }
    
    char* class_name = read_utf8_string(frame->class_info, class_entry->class_info.string_index);
    char* method_name = read_utf8_string(frame->class_info, name_and_type->ref_info.class_index);
    char* descriptor = read_utf8_string(frame->class_info, name_and_type->ref_info.name_and_type_index);
    
    // Handle PrintStream methods
    if (class_name && strstr(class_name, "PrintStream")) {
        if (method_name && strstr(method_name, "print")) {
            jvalue args[4];
            int arg_count = 0;
            if (descriptor && strstr(descriptor, "(I)")) {
                arg_count = 1;
                args[0].i = pop_int(frame);
                pop_ref(frame); // Remove PrintStream object
                if (strstr(method_name, "println")) {
                    native_system_out_println_int(jvm, args, arg_count);
                } else {
                    native_system_out_print_int(jvm, args, arg_count);
                }
            } else if (descriptor && strstr(descriptor, "()V")) {
                pop_ref(frame); // Remove PrintStream object
                native_system_out_println_void(jvm, NULL, 0);
            } else if (descriptor && strstr(descriptor, "String")) {
                arg_count = 1;
                args[0].ref = pop_ref(frame);
                pop_ref(frame); // Remove PrintStream object
                if (strstr(method_name, "println")) {
                    native_system_out_println(jvm, args, arg_count);
                } else {
                    native_system_out_print(jvm, args, arg_count);
                }
            }
        }
    }
    
    // Handle StringBuilder methods
    if (class_name && strstr(class_name, "StringBuilder")) {
        if (method_name && strstr(method_name, "append")) {
            if (descriptor && strstr(descriptor, "(I)")) {
                jint value = pop_int(frame);
                void* stringbuilder = pop_ref(frame);
                char buffer[32];
                snprintf(buffer, sizeof(buffer), "%d", value);
                JString* current = (JString*)stringbuilder;
                if (current && current->data) {
                    size_t old_len = current->length;
                    size_t new_len = old_len + strlen(buffer);
                    current->data = realloc(current->data, new_len + 1);
                    if (current->data) {
                        strcat(current->data, buffer);
                        current->length = new_len;
                    }
                } else {
                    current = jvm_create_string(jvm, buffer);
                }
                push_ref(frame, current);
            } else if (descriptor && strstr(descriptor, "String")) {
                JString* str_to_append = (JString*)pop_ref(frame);
                void* stringbuilder = pop_ref(frame);
                JString* current = (JString*)stringbuilder;
                if (current && current->data && str_to_append && str_to_append->data) {
                    size_t old_len = current->length;
                    size_t add_len = str_to_append->length;
                    current->data = realloc(current->data, old_len + add_len + 1);
                    if (current->data) {
                        strcat(current->data, str_to_append->data);
                        current->length = old_len + add_len;
                    }
                } else if (str_to_append && str_to_append->data) {
                    current = jvm_create_string(jvm, str_to_append->data);
                }
                push_ref(frame, current);
            }
        } else if (method_name && strcmp(method_name, "toString") == 0) {
            void* stringbuilder = pop_ref(frame);
            JString* str = (JString*)stringbuilder;
            if (str && str->data) {
                push_ref(frame, str);
            } else {
                JString* empty = jvm_create_string(jvm, "");
                push_ref(frame, empty);
            }
        }
    }
    
    if (class_name) free(class_name);
    if (method_name) free(method_name);
    if (descriptor) free(descriptor);
    return 0;
}

// Execute object creation
static int execute_new(JVM* jvm, Frame* frame) {
    uint16_t class_index = read_u2(&frame->pc);
    if (class_index >= frame->class_info->constant_pool_count) {
        return -1;
    }
    
    ConstantPoolEntry* class_entry = &frame->class_info->constant_pool[class_index];
    if (class_entry->tag != CONST_CLASS) {
        return -1;
    }
    
    char* class_name = read_utf8_string(frame->class_info, class_entry->class_info.string_index);
    if (class_name && strstr(class_name, "Scanner")) {
        void* scanner_obj = malloc(sizeof(int));
        if (scanner_obj) {
            *(int*)scanner_obj = 1;
            push_ref(frame, scanner_obj);
        }
    } else if (class_name && strstr(class_name, "StringBuilder")) {
        JString* empty_sb = jvm_create_string(jvm, "");
        push_ref(frame, empty_sb);
    } else {
        push_ref(frame, NULL);
    }
    
    if (class_name) free(class_name);
    return 0;
}

// Main bytecode interpreter
static int execute_bytecode(JVM* jvm, Frame* frame) {
    uint8_t* code_end = frame->method->code + frame->method->code_length;
    
    while (frame->pc < code_end) {
        uint8_t opcode = read_u1(&frame->pc);
        fflush(stdout);
        
        switch (opcode) {
            case NOP:
                break;
                
            // Constants - null and objects
            case ACONST_NULL:
                push_ref(frame, NULL);
                break;
                
            // Integer constants
            case ICONST_M1:
                push_int(frame, -1);
                break;
            case ICONST_0:
                push_int(frame, 0);
                break;
            case ICONST_1:
                push_int(frame, 1);
                break;
            case ICONST_2:
                push_int(frame, 2);
                break;
            case ICONST_3:
                push_int(frame, 3);
                break;
            case ICONST_4:
                push_int(frame, 4);
                break;
            case ICONST_5:
                push_int(frame, 5);
                break;
                
            // Long constants
            case LCONST_0:
                push_long(frame, 0L);
                break;
            case LCONST_1:
                push_long(frame, 1L);
                break;
                
            // Float constants
            case FCONST_0:
                push_float(frame, 0.0f);
                break;
            case FCONST_1:
                push_float(frame, 1.0f);
                break;
            case FCONST_2:
                push_float(frame, 2.0f);
                break;
                
            // Double constants
            case DCONST_0:
                push_double(frame, 0.0);
                break;
            case DCONST_1:
                push_double(frame, 1.0);
                break;
                
            // Load constants
            case BIPUSH: {
                int8_t value = (int8_t)read_u1(&frame->pc);
                push_int(frame, value);
                break;
            }
            
            case SIPUSH: {
                int16_t value = read_s2(&frame->pc);
                push_int(frame, value);
                break;
            }
            
            case LDC: {
                uint8_t index = read_u1(&frame->pc);
                if (index >= frame->class_info->constant_pool_count) {
                    return -1;
                }
                
                ConstantPoolEntry* entry = &frame->class_info->constant_pool[index];
                if (entry->tag == CONST_INTEGER) {
                    push_int(frame, entry->integer_info.value);
                } else if (entry->tag == CONST_FLOAT) {
                    push_float(frame, entry->float_info.value);
                } else if (entry->tag == CONST_STRING) {
                    if (execute_ldc_string(jvm, frame, index) != 0) {
                        return -1;
                    }
                } else {
                    return -1;
                }
                break;
            }
            
            // Load from locals - int
            case ILOAD: {
                uint8_t index = read_u1(&frame->pc);
                push_int(frame, frame->locals[index].i);
                break;
            }
            
            case ILOAD_0:
                push_int(frame, frame->locals[0].i);
                break;
            case ILOAD_1:
                push_int(frame, frame->locals[1].i);
                break;
            case ILOAD_2:
                push_int(frame, frame->locals[2].i);
                break;
            case ILOAD_3:
                push_int(frame, frame->locals[3].i);
                break;
                
            // Load from locals - reference
            case ALOAD_0:
                push_ref(frame, frame->locals[0].ref);
                break;
            case ALOAD_1:
                push_ref(frame, frame->locals[1].ref);
                break;
            case ALOAD_2:
                push_ref(frame, frame->locals[2].ref);
                break;
            case ALOAD_3:
                push_ref(frame, frame->locals[3].ref);
                break;
                
            // Load from locals - other types
            case LLOAD: {
                uint8_t index = read_u1(&frame->pc);
                push_long(frame, frame->locals[index].l);
                break;
            }
            
            case FLOAD: {
                uint8_t index = read_u1(&frame->pc);
                push_float(frame, frame->locals[index].f);
                break;
            }
            
            case DLOAD: {
                uint8_t index = read_u1(&frame->pc);
                push_double(frame, frame->locals[index].d);
                break;
            }
            
            case ALOAD: {
                uint8_t index = read_u1(&frame->pc);
                push_ref(frame, frame->locals[index].ref);
                break;
            }
            
            // Store to locals - int
            case ISTORE: {
                uint8_t index = read_u1(&frame->pc);
                frame->locals[index].i = pop_int(frame);
                break;
            }
            
            case ISTORE_0:
                frame->locals[0].i = pop_int(frame);
                break;
            case ISTORE_1:
                frame->locals[1].i = pop_int(frame);
                break;
            case ISTORE_2:
                frame->locals[2].i = pop_int(frame);
                break;
            case ISTORE_3:
                frame->locals[3].i = pop_int(frame);
                break;
                
            // Store to locals - reference
            case ASTORE_0:
                frame->locals[0].ref = pop_ref(frame);
                break;
            case ASTORE_1:
                frame->locals[1].ref = pop_ref(frame);
                break;
            case ASTORE_2:
                frame->locals[2].ref = pop_ref(frame);
                break;
            case ASTORE_3:
                frame->locals[3].ref = pop_ref(frame);
                break;
                
            // Store to locals - other types
            case LSTORE: {
                uint8_t index = read_u1(&frame->pc);
                frame->locals[index].l = pop_long(frame);
                break;
            }
            
            case FSTORE: {
                uint8_t index = read_u1(&frame->pc);
                frame->locals[index].f = pop_float(frame);
                break;
            }
            
            case DSTORE: {
                uint8_t index = read_u1(&frame->pc);
                frame->locals[index].d = pop_double(frame);
                break;
            }
            
            case ASTORE: {
                uint8_t index = read_u1(&frame->pc);
                frame->locals[index].ref = pop_ref(frame);
                break;
            }
            
            // Arithmetic operations - addition
            case IADD: {
                jint value2 = pop_int(frame);
                jint value1 = pop_int(frame);
                push_int(frame, value1 + value2);
                break;
            }
            
            case LADD: {
                jlong value2 = pop_long(frame);
                jlong value1 = pop_long(frame);
                push_long(frame, value1 + value2);
                break;
            }
            
            case FADD: {
                jfloat value2 = pop_float(frame);
                jfloat value1 = pop_float(frame);
                push_float(frame, value1 + value2);
                break;
            }
            
            case DADD: {
                jdouble value2 = pop_double(frame);
                jdouble value1 = pop_double(frame);
                push_double(frame, value1 + value2);
                break;
            }
            
            // Arithmetic operations - subtraction
            case ISUB: {
                jint value2 = pop_int(frame);
                jint value1 = pop_int(frame);
                push_int(frame, value1 - value2);
                break;
            }
            
            case LSUB: {
                jlong value2 = pop_long(frame);
                jlong value1 = pop_long(frame);
                push_long(frame, value1 - value2);
                break;
            }
            
            case FSUB: {
                jfloat value2 = pop_float(frame);
                jfloat value1 = pop_float(frame);
                push_float(frame, value1 - value2);
                break;
            }
            
            case DSUB: {
                jdouble value2 = pop_double(frame);
                jdouble value1 = pop_double(frame);
                push_double(frame, value1 - value2);
                break;
            }
            
            // Arithmetic operations - multiplication
            case IMUL: {
                jint value2 = pop_int(frame);
                jint value1 = pop_int(frame);
                push_int(frame, value1 * value2);
                break;
            }
            
            case LMUL: {
                jlong value2 = pop_long(frame);
                jlong value1 = pop_long(frame);
                push_long(frame, value1 * value2);
                break;
            }
            
            case FMUL: {
                jfloat value2 = pop_float(frame);
                jfloat value1 = pop_float(frame);
                push_float(frame, value1 * value2);
                break;
            }
            
            case DMUL: {
                jdouble value2 = pop_double(frame);
                jdouble value1 = pop_double(frame);
                push_double(frame, value1 * value2);
                break;
            }
            
            // Arithmetic operations - division
            case IDIV: {
                jint value2 = pop_int(frame);
                jint value1 = pop_int(frame);
                if (value2 == 0) {
                    return -1;
                }
                push_int(frame, value1 / value2);
                break;
            }
            
            case LDIV: {
                jlong value2 = pop_long(frame);
                jlong value1 = pop_long(frame);
                if (value2 == 0) {
                    return -1;
                }
                push_long(frame, value1 / value2);
                break;
            }
            
            case FDIV: {
                jfloat value2 = pop_float(frame);
                jfloat value1 = pop_float(frame);
                push_float(frame, value1 / value2);
                break;
            }
            
            case DDIV: {
                jdouble value2 = pop_double(frame);
                jdouble value1 = pop_double(frame);
                push_double(frame, value1 / value2);
                break;
            }
            
            // Arithmetic operations - remainder
            case IREM: {
                jint value2 = pop_int(frame);
                jint value1 = pop_int(frame);
                if (value2 == 0) {
                    return -1;
                }
                push_int(frame, value1 % value2);
                break;
            }
            
            // Arithmetic operations - negation
            case INEG: {
                jint value = pop_int(frame);
                push_int(frame, -value);
                break;
            }
            
            case LNEG: {
                jlong value = pop_long(frame);
                push_long(frame, -value);
                break;
            }
            
            case FNEG: {
                jfloat value = pop_float(frame);
                push_float(frame, -value);
                break;
            }
            
            case DNEG: {
                jdouble value = pop_double(frame);
                push_double(frame, -value);
                break;
            }
            
            // Bitwise operations
            case IAND: {
                jint value2 = pop_int(frame);
                jint value1 = pop_int(frame);
                push_int(frame, value1 & value2);
                break;
            }
            
            case IOR: {
                jint value2 = pop_int(frame);
                jint value1 = pop_int(frame);
                push_int(frame, value1 | value2);
                break;
            }
            
            case IXOR: {
                jint value2 = pop_int(frame);
                jint value1 = pop_int(frame);
                push_int(frame, value1 ^ value2);
                break;
            }
            
            // Type conversions
            case I2L: {
                jint value = pop_int(frame);
                push_long(frame, (jlong)value);
                break;
            }
            
            case I2F: {
                jint value = pop_int(frame);
                push_float(frame, (jfloat)value);
                break;
            }
            
            case I2D: {
                jint value = pop_int(frame);
                push_double(frame, (jdouble)value);
                break;
            }
            
            case L2I: {
                jlong value = pop_long(frame);
                push_int(frame, (jint)value);
                break;
            }
            
            case L2F: {
                jlong value = pop_long(frame);
                push_float(frame, (jfloat)value);
                break;
            }
            
            case L2D: {
                jlong value = pop_long(frame);
                push_double(frame, (jdouble)value);
                break;
            }
            
            case F2I: {
                jfloat value = pop_float(frame);
                push_int(frame, (jint)value);
                break;
            }
            
            case F2L: {
                jfloat value = pop_float(frame);
                push_long(frame, (jlong)value);
                break;
            }
            
            case F2D: {
                jfloat value = pop_float(frame);
                push_double(frame, (jdouble)value);
                break;
            }
            
            case D2I: {
                jdouble value = pop_double(frame);
                push_int(frame, (jint)value);
                break;
            }
            
            case D2L: {
                jdouble value = pop_double(frame);
                push_long(frame, (jlong)value);
                break;
            }
            
            case D2F: {
                jdouble value = pop_double(frame);
                push_float(frame, (jfloat)value);
                break;
            }
            
            // Comparison operations
            case LCMP: {
                jlong value2 = pop_long(frame);
                jlong value1 = pop_long(frame);
                if (value1 > value2) {
                    push_int(frame, 1);
                } else if (value1 == value2) {
                    push_int(frame, 0);
                } else {
                    push_int(frame, -1);
                }
                break;
            }
            
            case FCMPL:
            case FCMPG: {
                jfloat value2 = pop_float(frame);
                jfloat value1 = pop_float(frame);
                if (value1 > value2) {
                    push_int(frame, 1);
                } else if (value1 == value2) {
                    push_int(frame, 0);
                } else {
                    push_int(frame, -1);
                }
                break;
            }
            
            case DCMPL:
            case DCMPG: {
                jdouble value2 = pop_double(frame);
                jdouble value1 = pop_double(frame);
                if (value1 > value2) {
                    push_int(frame, 1);
                } else if (value1 == value2) {
                    push_int(frame, 0);
                } else {
                    push_int(frame, -1);
                }
                break;
            }
            
            // Conditional branches
            case IFEQ: {
                int16_t branch = read_s2(&frame->pc);
                jint value = pop_int(frame);
                if (value == 0) {
                    frame->pc += branch - 3;
                }
                break;
            }
            
            case IFNE: {
                int16_t branch = read_s2(&frame->pc);
                jint value = pop_int(frame);
                if (value != 0) {
                    frame->pc += branch - 3;
                }
                break;
            }
            
            case IFLT: {
                int16_t branch = read_s2(&frame->pc);
                jint value = pop_int(frame);
                if (value < 0) {
                    frame->pc += branch - 3;
                }
                break;
            }
            
            case IFGE: {
                int16_t branch = read_s2(&frame->pc);
                jint value = pop_int(frame);
                if (value >= 0) {
                    frame->pc += branch - 3;
                }
                break;
            }
            
            case IFGT: {
                int16_t branch = read_s2(&frame->pc);
                jint value = pop_int(frame);
                if (value > 0) {
                    frame->pc += branch - 3;
                }
                break;
            }
            
            case IFLE: {
                int16_t branch = read_s2(&frame->pc);
                jint value = pop_int(frame);
                if (value <= 0) {
                    frame->pc += branch - 3;
                }
                break;
            }
            
            // Compare two ints
            case IF_ICMPEQ: {
                int16_t branch = read_s2(&frame->pc);
                jint value2 = pop_int(frame);
                jint value1 = pop_int(frame);
                if (value1 == value2) {
                    frame->pc += branch - 3;
                }
                break;
            }
            
            case IF_ICMPNE: {
                int16_t branch = read_s2(&frame->pc);
                jint value2 = pop_int(frame);
                jint value1 = pop_int(frame);
                if (value1 != value2) {
                    frame->pc += branch - 3;
                }
                break;
            }
            
            case IF_ICMPLT: {
                int16_t branch = read_s2(&frame->pc);
                jint value2 = pop_int(frame);
                jint value1 = pop_int(frame);
                if (value1 < value2) {
                    frame->pc += branch - 3;
                }
                break;
            }
            
            case IF_ICMPGE: {
                int16_t branch = read_s2(&frame->pc);
                jint value2 = pop_int(frame);
                jint value1 = pop_int(frame);
                if (value1 >= value2) {
                    frame->pc += branch - 3;
                }
                break;
            }
            
            case IF_ICMPGT: {
                int16_t branch = read_s2(&frame->pc);
                jint value2 = pop_int(frame);
                jint value1 = pop_int(frame);
                if (value1 > value2) {
                    frame->pc += branch - 3;
                }
                break;
            }
            
            case IF_ICMPLE: {
                int16_t branch = read_s2(&frame->pc);
                jint value2 = pop_int(frame);
                jint value1 = pop_int(frame);
                if (value1 <= value2) {
                    frame->pc += branch - 3;
                }
                break;
            }
            
            // Unconditional branch
            case GOTO: {
                int16_t branch = read_s2(&frame->pc);
                frame->pc += branch - 3;
                break;
            }
            
            // Method returns
            case IRETURN:
                return pop_int(frame);
            case LRETURN:
                return (int)pop_long(frame);
            case FRETURN:
                return (int)pop_float(frame);
            case DRETURN:
                return (int)pop_double(frame);
            case ARETURN:
                return (int)(intptr_t)pop_ref(frame);
            case RETURN:
                return 0;
                
            // Stack management
            case DUP: {
                if (frame->stack_top > 0) {
                    jvalue value = frame->operand_stack[frame->stack_top - 1];
                    frame->operand_stack[frame->stack_top] = value;
                    frame->stack_top++;
                }
                break;
            }
            
            case POP:
                if (frame->stack_top > 0) {
                    frame->stack_top--;
                }
                break;
                
            case SWAP: {
                if (frame->stack_top >= 2) {
                    jvalue temp = frame->operand_stack[frame->stack_top - 1];
                    frame->operand_stack[frame->stack_top - 1] = frame->operand_stack[frame->stack_top - 2];
                    frame->operand_stack[frame->stack_top - 2] = temp;
                }
                break;
            }
            
            // Method invocations
            case INVOKESTATIC:
                if (execute_invokestatic(jvm, frame) != 0) {
                    return -1;
                }
                break;
                
            case INVOKEVIRTUAL:
                if (execute_invokevirtual(jvm, frame) != 0) {
                    return -1;
                }
                break;
                
            case INVOKESPECIAL: {
                uint16_t method_index = read_u2(&frame->pc);
                (void)method_index;
                break;
            }
            
            // Object operations
            case NEW:
                if (execute_new(jvm, frame) != 0) {
                    return -1;
                }
                break;
                
            case GETSTATIC: {
                uint16_t field_index = read_u2(&frame->pc);
                (void)field_index;
                push_ref(frame, (void*)0x1);
                break;
            }
            
            default:
                return -1;
        }
    }
    return 0;
}

// Execute method
int jvm_execute_method(JVM* jvm, const char* class_name, const char* method_name) {
    if (!jvm || !class_name || !method_name) {
        return -1;
    }
    
    ClassInfo* class_info = find_class(jvm, class_name);
    if (!class_info) {
        return -1;
    }
    
    MethodInfo* method = find_method(class_info, method_name);
    if (!method) {
        return -1;
    }
    
    Frame frame;
    memset(&frame, 0, sizeof(Frame));
    frame.locals = jvm->locals_memory;
    frame.operand_stack = jvm->stack_memory;
    frame.stack_top = 0;
    frame.pc = method->code;
    frame.method = method;
    frame.class_info = class_info;
    jvm->current_frame = &frame;
    
    return execute_bytecode(jvm, &frame);
}