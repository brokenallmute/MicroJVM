MicroJVM - Simple Java Virtual Machine

A lightweight educational JVM implementation in C that runs basic Java programs.

## What it does

- Loads .class files
- Executes Java bytecode
- Supports basic operations (math, variables, methods)
- Handles System.out.println and Scanner input

## Quick Start

### Build
```
make
```

### Run
```
./jvm_runner YourClass.class
```

## What Java features work

✅ Basic math (+, -, *, /, %)\
✅ Variables (int, float, double, long)\
✅ Method calls\
✅ System.out.print/println\
✅ Scanner.nextInt/nextLine\
✅ Simple control flow (if, loops)\
\
❌ Objects and classes\
❌ Arrays\
❌ Exception handling\
❌ Garbage collection

## Project Files

```
MicroJVM/
├── main.c            # Program entry point
├── jvm.c/.h          # Core JVM engine  
├── class_loader.c/h  # Loads .class files
├── native_methods.c  # System.out and Scanner
└── Makefile          # Build script
```

## Example Programs

### Hello World

```
public class HelloWorld {
    public static void main(String[] args) {
        System.out.println("Hello World!");
    }
}
```

### Simple Calculator

```
public class Calculator {
    public static int add(int a, int b) {
        return a + b;
    }
    
    public static void main(String[] args) {
        int result = add(5, 3);
        System.out.println(result);
    }
}
```

## Troubleshooting

**"Class file not found"**
→ Check file path and make sure .class file exists

**"Method not found"**  
→ Check method name spelling

## Clean Build

```
make clean
```
