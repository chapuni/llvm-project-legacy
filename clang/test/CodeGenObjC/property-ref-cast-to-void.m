// RUN: %clang_cc1 -triple x86_64-apple-darwin9 -fobjc-fragile-abi -emit-llvm -o - %s | FileCheck %s
// RUN: %clang_cc1 -x objective-c++ -triple x86_64-apple-darwin9 -fobjc-fragile-abi -emit-llvm -o - %s | FileCheck %s

// rdar: // 8399655
@interface TestClass
@property (readonly) int myProperty;
- (int)myProperty;
- (double)myGetter;
@end

void FUNC () {
    TestClass *obj;
    (void)obj.myProperty; 
    (void)obj.myGetter; 
}

// CHECK: call i32 bitcast
// CHECK: call double bitcast
