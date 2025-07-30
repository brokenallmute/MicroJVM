#ifndef JVM_H
#define JVM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Core constants
#define MAX_STACK_SIZE 2048
#define MAX_LOCALS_SIZE 512
#define MAX_CODE_SIZE 8192
#define MAX_CONSTANT_POOL_SIZE 256
#define MAX_CLASSES 32
#define MAX_STRING_POOL 256
#define MAX_STRING_LENGTH 1024

// Java data types
typedef int32_t jint;
typedef int64_t jlong;
typedef float jfloat;
typedef double jdouble;
typedef uint16_t jchar;
typedef int16_t jshort;
typedef int8_t jbyte;
typedef uint8_t jboolean;

// Stack value type
typedef union {
    jint i;
    jlong l;
    jfloat f;
    jdouble d;
    void* ref;
} jvalue;

// Constant pool types
enum ConstantType {
    CONST_CLASS = 7,
    CONST_FIELDREF = 9,
    CONST_METHODREF = 10,
    CONST_STRING = 8,
    CONST_INTEGER = 3,
    CONST_FLOAT = 4,
    CONST_LONG = 5,
    CONST_DOUBLE = 6,
    CONST_UTF8 = 1
};

// Constant pool entry
typedef struct {
    uint8_t tag;
    union {
        struct {
            uint16_t string_index;
        } class_info;
        struct {
            uint16_t class_index;
            uint16_t name_and_type_index;
        } ref_info;
        struct {
            jint value;
        } integer_info;
        struct {
            jfloat value;
        } float_info;
        struct {
            const char* bytes;
            uint16_t length;
        } utf8_info;
    };
} ConstantPoolEntry;

// Method information
typedef struct {
    uint16_t access_flags;
    const char* name;
    const char* descriptor;
    uint16_t max_stack;
    uint16_t max_locals;
    uint32_t code_length;
    uint8_t* code;
} MethodInfo;

// Class information
typedef struct {
    const char* name;
    uint16_t constant_pool_count;
    ConstantPoolEntry* constant_pool;
    uint16_t methods_count;
    MethodInfo* methods;
} ClassInfo;

// Execution frame
typedef struct {
    jvalue* locals;
    jvalue* operand_stack;
    uint16_t stack_top;
    uint8_t* pc;
    MethodInfo* method;
    ClassInfo* class_info;
} Frame;

// String structure
typedef struct {
    char* data;
    size_t length;
    size_t capacity;
} JString;

// String pool
typedef struct {
    JString strings[MAX_STRING_POOL];
    size_t count;
} StringPool;

// Forward declaration for NativeMethod
struct JVM;

// Native method function type
typedef int (*NativeMethod)(struct JVM* jvm, jvalue* args, int arg_count);

// Native method entry
typedef struct {
    const char* class_name;
    const char* method_name;
    const char* descriptor;
    NativeMethod function;
} NativeMethodEntry;

// Main JVM structure
typedef struct JVM {
    ClassInfo classes[MAX_CLASSES];
    uint16_t classes_count;
    Frame* current_frame;
    jvalue stack_memory[MAX_STACK_SIZE];
    jvalue locals_memory[MAX_LOCALS_SIZE];
    uint8_t heap[8192];
    size_t heap_used;
    StringPool string_pool;
    NativeMethodEntry* native_methods;
    size_t native_methods_count;
} JVM;

// Additional opcodes
enum AdditionalOpCodes {
    GETSTATIC = 0xb2,
    PUTSTATIC = 0xb3,
    GETFIELD = 0xb4,
    PUTFIELD = 0xb5,
    INVOKEINTERFACE = 0xb9,
    INVOKESPECIAL = 0xb7
};

// Main opcodes
enum OpCode {
    // Constants
    NOP = 0x00,
    ACONST_NULL = 0x01,
    ICONST_M1 = 0x02,
    ICONST_0 = 0x03,
    ICONST_1 = 0x04,
    ICONST_2 = 0x05,
    ICONST_3 = 0x06,
    ICONST_4 = 0x07,
    ICONST_5 = 0x08,
    LCONST_0 = 0x09,
    LCONST_1 = 0x0a,
    FCONST_0 = 0x0b,
    FCONST_1 = 0x0c,
    FCONST_2 = 0x0d,
    DCONST_0 = 0x0e,
    DCONST_1 = 0x0f,
    
    // Load constants
    BIPUSH = 0x10,
    SIPUSH = 0x11,
    LDC = 0x12,
    
    // Load from locals
    ILOAD = 0x15,
    LLOAD = 0x16,
    FLOAD = 0x17,
    DLOAD = 0x18,
    ALOAD = 0x19,
    ILOAD_0 = 0x1a,
    ILOAD_1 = 0x1b,
    ILOAD_2 = 0x1c,
    ILOAD_3 = 0x1d,
    ALOAD_0 = 0x2a,
    ALOAD_1 = 0x2b,
    ALOAD_2 = 0x2c,
    ALOAD_3 = 0x2d,
    
    // Store to locals
    ISTORE = 0x36,
    LSTORE = 0x37,
    FSTORE = 0x38,
    DSTORE = 0x39,
    ASTORE = 0x3a,
    ISTORE_0 = 0x3b,
    ISTORE_1 = 0x3c,
    ISTORE_2 = 0x3d,
    ISTORE_3 = 0x3e,
    ASTORE_0 = 0x4b,
    ASTORE_1 = 0x4c,
    ASTORE_2 = 0x4d,
    ASTORE_3 = 0x4e,
    
    // Stack operations
    DUP = 0x59,
    POP = 0x57,
    SWAP = 0x5f,
    
    // Arithmetic operations
    IADD = 0x60,
    LADD = 0x61,
    FADD = 0x62,
    DADD = 0x63,
    ISUB = 0x64,
    LSUB = 0x65,
    FSUB = 0x66,
    DSUB = 0x67,
    IMUL = 0x68,
    LMUL = 0x69,
    FMUL = 0x6a,
    DMUL = 0x6b,
    IDIV = 0x6c,
    LDIV = 0x6d,
    FDIV = 0x6e,
    DDIV = 0x6f,
    IREM = 0x70,
    LREM = 0x71,
    FREM = 0x72,
    DREM = 0x73,
    INEG = 0x74,
    LNEG = 0x75,
    FNEG = 0x76,
    DNEG = 0x77,
    
    // Bitwise operations
    IAND = 0x7e,
    IOR = 0x80,
    IXOR = 0x82,
    
    // Type conversions
    I2L = 0x85,
    I2F = 0x86,
    I2D = 0x87,
    L2I = 0x88,
    L2F = 0x89,
    L2D = 0x8a,
    F2I = 0x8b,
    F2L = 0x8c,
    F2D = 0x8d,
    D2I = 0x8e,
    D2L = 0x8f,
    D2F = 0x90,
    
    // Comparisons
    LCMP = 0x94,
    FCMPL = 0x95,
    FCMPG = 0x96,
    DCMPL = 0x97,
    DCMPG = 0x98,
    
    // Conditional branches
    IFEQ = 0x99,
    IFNE = 0x9a,
    IFLT = 0x9b,
    IFGE = 0x9c,
    IFGT = 0x9d,
    IFLE = 0x9e,
    IF_ICMPEQ = 0x9f,
    IF_ICMPNE = 0xa0,
    IF_ICMPLT = 0xa1,
    IF_ICMPGE = 0xa2,
    IF_ICMPGT = 0xa3,
    IF_ICMPLE = 0xa4,
    
    // Control flow
    GOTO = 0xa7,
    
    // Method returns
    IRETURN = 0xac,
    LRETURN = 0xad,
    FRETURN = 0xae,
    DRETURN = 0xaf,
    ARETURN = 0xb0,
    RETURN = 0xb1,
    
    // Method invocations
    INVOKEVIRTUAL = 0xb6,
    INVOKESTATIC = 0xb8,
    
    // Object operations
    NEW = 0xbb
};

// Core JVM API functions
int jvm_init(JVM* jvm);
int jvm_load_class(JVM* jvm, const ClassInfo* class_info);
int jvm_execute_method(JVM* jvm, const char* class_name, const char* method_name);
void jvm_destroy(JVM* jvm);

// String and native method functions
int jvm_register_native_method(JVM* jvm, const char* class_name,
                              const char* method_name, const char* descriptor,
                              NativeMethod function);
JString* jvm_create_string(JVM* jvm, const char* str);
void jvm_print_string(JVM* jvm, JString* str);
int jvm_read_int(JVM* jvm);
char* jvm_read_line(JVM* jvm);

// System.out native methods
int native_system_out_print(JVM* jvm, jvalue* args, int arg_count);
int native_system_out_println(JVM* jvm, jvalue* args, int arg_count);
int native_system_out_print_int(JVM* jvm, jvalue* args, int arg_count);
int native_system_out_println_int(JVM* jvm, jvalue* args, int arg_count);
int native_system_out_println_void(JVM* jvm, jvalue* args, int arg_count);

// Scanner native methods
int native_scanner_init(JVM* jvm, jvalue* args, int arg_count);
int native_scanner_next_int(JVM* jvm, jvalue* args, int arg_count);
int native_scanner_next_line(JVM* jvm, jvalue* args, int arg_count);

// Register standard native methods
void register_standard_native_methods(JVM* jvm);

#endif // JVM_H