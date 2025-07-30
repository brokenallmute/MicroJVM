#define _POSIX_C_SOURCE 200809L

#include "class_loader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Buffer reading functions
static uint8_t read_u1(ClassReader* reader) {
    if (reader->pos >= reader->size) {
        return 0;
    }
    return reader->data[reader->pos++];
}

static uint16_t read_u2(ClassReader* reader) {
    if (reader->pos + 1 >= reader->size) {
        return 0;
    }
    uint16_t value = (reader->data[reader->pos] << 8) | reader->data[reader->pos + 1];
    reader->pos += 2;
    return value;
}

static uint32_t read_u4(ClassReader* reader) {
    if (reader->pos + 3 >= reader->size) {
        return 0;
    }
    uint32_t value = (reader->data[reader->pos] << 24) |
                     (reader->data[reader->pos + 1] << 16) |
                     (reader->data[reader->pos + 2] << 8) |
                     reader->data[reader->pos + 3];
    reader->pos += 4;
    return value;
}

static void read_bytes(ClassReader* reader, uint8_t* buffer, size_t count) {
    if (reader->pos + count > reader->size) {
        return;
    }
    memcpy(buffer, reader->data + reader->pos, count);
    reader->pos += count;
}

// Extract UTF-8 string from constant pool
char* read_utf8_string(const ClassInfo* class_info, uint16_t index) {
    if (index == 0 || index >= class_info->constant_pool_count) {
        return NULL;
    }

    ConstantPoolEntry* entry = &class_info->constant_pool[index];
    if (entry->tag != CONST_UTF8) {
        return NULL;
    }

    char* str = malloc(entry->utf8_info.length + 1);
    if (str) {
        memcpy(str, entry->utf8_info.bytes, entry->utf8_info.length);
        str[entry->utf8_info.length] = '\0';
    }
    return str;
}

// Parse constant pool entries
static int parse_constant_pool(ClassReader* reader, LoadedClass* loaded_class) {
    loaded_class->constant_pool_count = read_u2(reader);
    if (loaded_class->constant_pool_count == 0) {
        return -1;
    }

    loaded_class->constant_pool = calloc(loaded_class->constant_pool_count,
                                        sizeof(ConstantPoolEntry));
    if (!loaded_class->constant_pool) {
        return -1;
    }

    // Index 0 is reserved, start from 1
    for (uint16_t i = 1; i < loaded_class->constant_pool_count; i++) {
        ConstantPoolEntry* entry = &loaded_class->constant_pool[i];
        entry->tag = read_u1(reader);

        switch (entry->tag) {
            case CONST_UTF8: {
                uint16_t length = read_u2(reader);
                char* bytes = malloc(length + 1);
                if (!bytes) {
                    return -1;
                }
                read_bytes(reader, (uint8_t*)bytes, length);
                bytes[length] = '\0';
                entry->utf8_info.length = length;
                entry->utf8_info.bytes = bytes;
                break;
            }
            case CONST_INTEGER:
                entry->integer_info.value = (jint)read_u4(reader);
                break;
            case CONST_FLOAT: {
                uint32_t bits = read_u4(reader);
                entry->float_info.value = *(jfloat*)&bits;
                break;
            }
            case CONST_LONG:
                // Long occupies 2 slots in constant pool
                entry->utf8_info.length = read_u4(reader);
                entry->utf8_info.bytes = (char*)(uintptr_t)read_u4(reader);
                i++; // Skip next slot
                break;
            case CONST_DOUBLE:
                // Double occupies 2 slots in constant pool
                entry->utf8_info.length = read_u4(reader);
                entry->utf8_info.bytes = (char*)(uintptr_t)read_u4(reader);
                i++; // Skip next slot
                break;
            case CONST_CLASS:
                entry->class_info.string_index = read_u2(reader);
                break;
            case CONST_STRING:
                entry->class_info.string_index = read_u2(reader);
                break;
            case CONST_FIELDREF:
                entry->ref_info.class_index = read_u2(reader);
                entry->ref_info.name_and_type_index = read_u2(reader);
                break;
            case CONST_METHODREF:
                entry->ref_info.class_index = read_u2(reader);
                entry->ref_info.name_and_type_index = read_u2(reader);
                break;
            case 12: // CONST_NAME_AND_TYPE
                entry->ref_info.class_index = read_u2(reader);
                entry->ref_info.name_and_type_index = read_u2(reader);
                break;
            default:
                // Skip unknown types
                read_u2(reader);
                break;
        }
    }
    return 0;
}

// Parse attribute information
static int parse_attributes(ClassReader* reader, const LoadedClass* loaded_class,
                           uint16_t* count, AttributeInfo** attributes) {
    (void) loaded_class;
    *count = read_u2(reader);
    if (*count == 0) {
        *attributes = NULL;
        return 0;
    }

    *attributes = calloc(*count, sizeof(AttributeInfo));
    if (!*attributes) {
        return -1;
    }

    for (uint16_t i = 0; i < *count; i++) {
        AttributeInfo* attr = &(*attributes)[i];
        attr->name_index = read_u2(reader);
        attr->length = read_u4(reader);
        
        if (attr->length > 0) {
            attr->info = malloc(attr->length);
            if (!attr->info) {
                return -1;
            }
            read_bytes(reader, attr->info, attr->length);
        }
    }
    return 0;
}

// Parse method information
static int parse_methods(ClassReader* reader, LoadedClass* loaded_class) {
    loaded_class->methods_count = read_u2(reader);
    if (loaded_class->methods_count == 0) {
        return 0;
    }

    loaded_class->methods = calloc(loaded_class->methods_count,
                                  sizeof(LoadedMethodInfo));
    if (!loaded_class->methods) {
        return -1;
    }

    for (uint16_t i = 0; i < loaded_class->methods_count; i++) {
        LoadedMethodInfo* method = &loaded_class->methods[i];
        method->access_flags = read_u2(reader);
        method->name_index = read_u2(reader);
        method->descriptor_index = read_u2(reader);

        if (parse_attributes(reader, loaded_class, &method->attributes_count,
                           &method->attributes) != 0) {
            return -1;
        }

        // Look for Code attribute
        for (uint16_t j = 0; j < method->attributes_count; j++) {
            AttributeInfo* attr = &method->attributes[j];
            ClassInfo temp_class_info = {
                .constant_pool = loaded_class->constant_pool,
                .constant_pool_count = loaded_class->constant_pool_count
            };
            
            char* attr_name = read_utf8_string(&temp_class_info, attr->name_index);
            if (attr_name && strcmp(attr_name, ATTR_CODE) == 0) {
                // Parse Code attribute
                ClassReader code_reader = {
                    .data = attr->info,
                    .size = attr->length,
                    .pos = 0
                };
                
                method->max_stack = read_u2(&code_reader);
                method->max_locals = read_u2(&code_reader);
                method->code_length = read_u4(&code_reader);
                
                if (method->code_length > 0) {
                    method->code = malloc(method->code_length);
                    if (method->code) {
                        read_bytes(&code_reader, method->code, method->code_length);
                    }
                }
            }
            if (attr_name) {
                free(attr_name);
            }
        }
    }
    return 0;
}

// Load .class file from disk
int load_class_file(const char* filename, LoadedClass* loaded_class) {
    if (!filename || !loaded_class) {
        return -1;
    }

    memset(loaded_class, 0, sizeof(LoadedClass));
    FILE* file = fopen(filename, "rb");
    if (!file) {
        return -1;
    }

    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    if (file_size <= 0) {
        fclose(file);
        return -1;
    }

    // Read file into memory
    uint8_t* data = malloc(file_size);
    if (!data) {
        fclose(file);
        return -1;
    }

    if (fread(data, 1, file_size, file) != (size_t)file_size) {
        free(data);
        fclose(file);
        return -1;
    }
    fclose(file);

    ClassReader reader = {
        .data = data,
        .size = file_size,
        .pos = 0
    };

    // Parse .class file format
    loaded_class->magic = read_u4(&reader);
    if (loaded_class->magic != CLASS_MAGIC) {
        free(data);
        return -1;
    }

    loaded_class->minor_version = read_u2(&reader);
    loaded_class->major_version = read_u2(&reader);

    // Parse constant pool
    if (parse_constant_pool(&reader, loaded_class) != 0) {
        free(data);
        return -1;
    }

    loaded_class->access_flags = read_u2(&reader);
    loaded_class->this_class = read_u2(&reader);
    loaded_class->super_class = read_u2(&reader);

    // Interfaces
    loaded_class->interfaces_count = read_u2(&reader);
    if (loaded_class->interfaces_count > 0) {
        loaded_class->interfaces = calloc(loaded_class->interfaces_count, sizeof(uint16_t));
        for (uint16_t i = 0; i < loaded_class->interfaces_count; i++) {
            loaded_class->interfaces[i] = read_u2(&reader);
        }
    }

    // Fields
    loaded_class->fields_count = read_u2(&reader);
    if (loaded_class->fields_count > 0) {
        loaded_class->fields = calloc(loaded_class->fields_count, sizeof(FieldInfo));
        for (uint16_t i = 0; i < loaded_class->fields_count; i++) {
            FieldInfo* field = &loaded_class->fields[i];
            field->access_flags = read_u2(&reader);
            field->name_index = read_u2(&reader);
            field->descriptor_index = read_u2(&reader);
            
            if (parse_attributes(&reader, loaded_class, &field->attributes_count,
                               &field->attributes) != 0) {
                free(data);
                return -1;
            }
        }
    }

    // Methods
    if (parse_methods(&reader, loaded_class) != 0) {
        free(data);
        return -1;
    }

    // Class attributes
    parse_attributes(&reader, loaded_class, &loaded_class->attributes_count,
                    &loaded_class->attributes);

    free(data);
    return 0;
}

// Convert loaded class to JVM format
int convert_to_jvm_class(const LoadedClass* loaded_class, ClassInfo* class_info) {
    if (!loaded_class || !class_info) {
        return -1;
    }

    memset(class_info, 0, sizeof(ClassInfo));

    // Get class name
    if (loaded_class->this_class > 0 &&
        loaded_class->this_class < loaded_class->constant_pool_count) {
        ConstantPoolEntry* class_entry = &loaded_class->constant_pool[loaded_class->this_class];
        if (class_entry->tag == CONST_CLASS) {
            ClassInfo temp_class_info = {
                .constant_pool = loaded_class->constant_pool,
                .constant_pool_count = loaded_class->constant_pool_count
            };
            class_info->name = read_utf8_string(&temp_class_info, class_entry->class_info.string_index);
        }
    }

    if (!class_info->name) {
        class_info->name = strdup("UnknownClass");
    }

    // Copy constant pool
    class_info->constant_pool_count = loaded_class->constant_pool_count;
    if (loaded_class->constant_pool_count > 0) {
        class_info->constant_pool = malloc(loaded_class->constant_pool_count *
                                         sizeof(ConstantPoolEntry));
        memcpy(class_info->constant_pool, loaded_class->constant_pool,
               loaded_class->constant_pool_count * sizeof(ConstantPoolEntry));
    }

    // Convert methods
    class_info->methods_count = loaded_class->methods_count;
    if (loaded_class->methods_count > 0) {
        class_info->methods = calloc(loaded_class->methods_count, sizeof(MethodInfo));
        for (uint16_t i = 0; i < loaded_class->methods_count; i++) {
            const LoadedMethodInfo* src = &loaded_class->methods[i];
            MethodInfo* dst = &class_info->methods[i];

            dst->access_flags = src->access_flags;
            ClassInfo temp_class_info = {
                .constant_pool = loaded_class->constant_pool,
                .constant_pool_count = loaded_class->constant_pool_count
            };
            dst->name = read_utf8_string(&temp_class_info, src->name_index);
            dst->descriptor = read_utf8_string(&temp_class_info, src->descriptor_index);
            dst->max_stack = src->max_stack;
            dst->max_locals = src->max_locals;
            dst->code_length = src->code_length;

            if (src->code_length > 0 && src->code) {
                dst->code = malloc(src->code_length);
                memcpy(dst->code, src->code, src->code_length);
            }
        }
    }
    return 0;
}

// Free loaded class memory
void free_loaded_class(LoadedClass* loaded_class) {
    if (!loaded_class) {
        return;
    }

    // Free constant pool
    if (loaded_class->constant_pool) {
        for (uint16_t i = 1; i < loaded_class->constant_pool_count; i++) {
            ConstantPoolEntry* entry = &loaded_class->constant_pool[i];
            if (entry->tag == CONST_UTF8 && entry->utf8_info.bytes) {
                free((void*)entry->utf8_info.bytes);
            }
        }
        free(loaded_class->constant_pool);
    }

    // Free methods
    if (loaded_class->methods) {
        for (uint16_t i = 0; i < loaded_class->methods_count; i++) {
            LoadedMethodInfo* method = &loaded_class->methods[i];
            if (method->code) {
                free(method->code);
            }
            if (method->attributes) {
                for (uint16_t j = 0; j < method->attributes_count; j++) {
                    if (method->attributes[j].info) {
                        free(method->attributes[j].info);
                    }
                }
                free(method->attributes);
            }
        }
        free(loaded_class->methods);
    }

    // Free fields
    if (loaded_class->fields) {
        for (uint16_t i = 0; i < loaded_class->fields_count; i++) {
            FieldInfo* field = &loaded_class->fields[i];
            if (field->attributes) {
                for (uint16_t j = 0; j < field->attributes_count; j++) {
                    if (field->attributes[j].info) {
                        free(field->attributes[j].info);
                    }
                }
                free(field->attributes);
            }
        }
        free(loaded_class->fields);
    }

    if (loaded_class->interfaces) {
        free(loaded_class->interfaces);
    }

    // Free class attributes
    if (loaded_class->attributes) {
        for (uint16_t i = 0; i < loaded_class->attributes_count; i++) {
            if (loaded_class->attributes[i].info) {
                free(loaded_class->attributes[i].info);
            }
        }
        free(loaded_class->attributes);
    }

    memset(loaded_class, 0, sizeof(LoadedClass));
}

void free_jvm_class(ClassInfo* class_info) {
    if (!class_info) {
        return;
    }

    if (class_info->name) {
        free((void*)class_info->name);
    }

    if (class_info->constant_pool) {
        free(class_info->constant_pool);
    }

    if (class_info->methods) {
        for (uint16_t i = 0; i < class_info->methods_count; i++) {
            MethodInfo* method = &class_info->methods[i];
            if (method->name) {
                free((void*)method->name);
            }
            if (method->descriptor) {
                free((void*)method->descriptor);
            }
            if (method->code) {
                free(method->code);
            }
        }
        free(class_info->methods);
    }

    memset(class_info, 0, sizeof(ClassInfo));
}