// class_loader.h - Java class file loader
#ifndef CLASS_LOADER_H
#define CLASS_LOADER_H

#include "jvm.h"
#include <stdint.h>

// Java .class file magic number
#define CLASS_MAGIC 0xCAFEBABE

// Attribute types
#define ATTR_CODE "Code"
#define ATTR_CONSTANT_VALUE "ConstantValue"
#define ATTR_SOURCE_FILE "SourceFile"

// Structure for reading .class file data
typedef struct {
    uint8_t* data;
    size_t size;
    size_t pos;
} ClassReader;

// Attribute information
typedef struct {
    uint16_t name_index;
    uint32_t length;
    uint8_t* info;
} AttributeInfo;

// Field information
typedef struct {
    uint16_t access_flags;
    uint16_t name_index;
    uint16_t descriptor_index;
    uint16_t attributes_count;
    AttributeInfo* attributes;
} FieldInfo;

// Extended method information for class loader
typedef struct {
    uint16_t access_flags;
    uint16_t name_index;
    uint16_t descriptor_index;
    uint16_t attributes_count;
    AttributeInfo* attributes;
    
    // Extracted information from Code attribute
    uint16_t max_stack;
    uint16_t max_locals;
    uint32_t code_length;
    uint8_t* code;
} LoadedMethodInfo;

// Loaded class structure
typedef struct {
    uint32_t magic;
    uint16_t minor_version;
    uint16_t major_version;
    uint16_t constant_pool_count;
    ConstantPoolEntry* constant_pool;
    uint16_t access_flags;
    uint16_t this_class;
    uint16_t super_class;
    uint16_t interfaces_count;
    uint16_t* interfaces;
    uint16_t fields_count;
    FieldInfo* fields;
    uint16_t methods_count;
    LoadedMethodInfo* methods;
    uint16_t attributes_count;
    AttributeInfo* attributes;
} LoadedClass;

// Public API functions
int load_class_file(const char* filename, LoadedClass* loaded_class);
int convert_to_jvm_class(const LoadedClass* loaded_class, ClassInfo* class_info);
void free_loaded_class(LoadedClass* loaded_class);
void free_jvm_class(ClassInfo* class_info);
char* read_utf8_string(const ClassInfo* class_info, uint16_t index);

#endif // CLASS_LOADER_H