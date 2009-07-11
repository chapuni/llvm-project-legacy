// RUN: clang-cc -emit-llvm %s -o %t

void p(const char*, ...);

@interface NSArray
+(NSArray*) arrayWithObjects: (id) first, ...;
-(unsigned) count;
@end
@interface NSString
-(const char*) cString;
@end

#define S(n) @#n
#define L1(n) S(n+0),S(n+1)
#define L2(n) L1(n+0),L1(n+2)
#define L3(n) L2(n+0),L2(n+4)
#define L4(n) L3(n+0),L3(n+8)
#define L5(n) L4(n+0),L4(n+16)
#define L6(n) L5(n+0),L5(n+32)

void t0() {
  NSArray *array = [NSArray arrayWithObjects: L1(0), (void*)0];

  p("array.length: %d\n", [array count]);
  unsigned index = 0;
  for (NSString *i in array) {
    p("element %d: %s\n", index++, [i cString]);
  }
}

void t1() {
  NSArray *array = [NSArray arrayWithObjects: L6(0), (void*)0];

  p("array.length: %d\n", [array count]);
  unsigned index = 0;
  for (NSString *i in array) {
    index++;
    if (index == 10)
      continue;
    p("element %d: %s\n", index, [i cString]);
    if (index == 55)
      break;
  }
}
