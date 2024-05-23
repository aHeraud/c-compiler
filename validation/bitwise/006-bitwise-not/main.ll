; ModuleID = 'module'
source_filename = "module"

@0 = global [4 x i8] c"%d\0A\00"
@1 = global [4 x i8] c"%d\0A\00"
@2 = global [4 x i8] c"%d\0A\00"

define i32 @main() {
block_0:
  %0 = alloca i32, align 4
  store i32 0, ptr %0, align 4
  %1 = alloca i32, align 4
  %2 = load i32, ptr %0, align 4
  %3 = xor i32 %2, -1
  store i32 %3, ptr %1, align 4
  %4 = load i32, ptr %1, align 4
  %5 = sext i32 %4 to i64
  %6 = inttoptr i64 %5 to ptr
  %7 = call i32 (ptr, ...) @printf(ptr @0, ptr %6)
  store i32 -252645136, ptr %0, align 4
  %8 = load i32, ptr %0, align 4
  %9 = xor i32 %8, -1
  store i32 %9, ptr %1, align 4
  %10 = load i32, ptr %1, align 4
  %11 = sext i32 %10 to i64
  %12 = inttoptr i64 %11 to ptr
  %13 = call i32 (ptr, ...) @printf(ptr @1, ptr %12)
  %14 = alloca i1, align 1
  store i1 false, ptr %14, align 1
  %15 = alloca i1, align 1
  %16 = load i1, ptr %14, align 1
  %17 = xor i1 %16, true
  store i1 %17, ptr %15, align 1
  %18 = load i1, ptr %15, align 1
  %19 = zext i1 %18 to i64
  %20 = inttoptr i64 %19 to ptr
  %21 = call i32 (ptr, ...) @printf(ptr @2, ptr %20)
  ret i32 0
}

declare i32 @printf(ptr, ...)
