// RUN: %clang_cc1 -triple x86_64-apple-darwin11 -fsyntax-only -fobjc-arc -fblocks -Wdealloc-in-category -verify %s
// RUN: not %clang_cc1 -triple x86_64-apple-darwin11 -fsyntax-only -fobjc-arc -fblocks -Wdealloc-in-category -fdiagnostics-parseable-fixits %s 2>&1 | FileCheck %s
// rdar://11987838

@protocol NSObject
- dealloc; // expected-error {{return type must be correctly specified as 'void' under ARC, instead of 'id'}}
// CHECK: fix-it:"{{.*}}":{6:3-6:3}:"(void)"
@end

@protocol Foo <NSObject> @end

@interface Root <Foo>
@end

@interface Baz : Root {
}
@end

@implementation Baz
-  (id) dealloc { // expected-error {{return type must be correctly specified as 'void' under ARC, instead of 'id'}}
// CHECK: fix-it:"{{.*}}":{20:5-20:7}:"void"
}

@end

// rdar://15397430
@interface Base
- (void)dealloc;
@end

@interface Subclass : Base
@end 

@interface Subclass (CAT)
- (void)dealloc;
@end

@implementation Subclass (CAT) // expected-note {{declared here}}
- (void)dealloc { // expected-warning {{decalloc is being overridden in category}}
}
@end
