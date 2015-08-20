// RUN: %python %S/check_clang_tidy.py %s readability-identifier-naming %t \
// RUN:   -config='{CheckOptions: [ \
// RUN:     {key: readability-identifier-naming.AbstractClassCase, value: CamelCase}, \
// RUN:     {key: readability-identifier-naming.AbstractClassPrefix, value: 'A'}, \
// RUN:     {key: readability-identifier-naming.ClassCase, value: CamelCase}, \
// RUN:     {key: readability-identifier-naming.ClassPrefix, value: 'C'}, \
// RUN:     {key: readability-identifier-naming.ClassConstantCase, value: CamelCase}, \
// RUN:     {key: readability-identifier-naming.ClassConstantPrefix, value: 'k'}, \
// RUN:     {key: readability-identifier-naming.ClassMemberCase, value: CamelCase}, \
// RUN:     {key: readability-identifier-naming.ClassMethodCase, value: camelBack}, \
// RUN:     {key: readability-identifier-naming.ConstantCase, value: UPPER_CASE}, \
// RUN:     {key: readability-identifier-naming.ConstantSuffix, value: '_CST'}, \
// RUN:     {key: readability-identifier-naming.ConstexprFunctionCase, value: lower_case}, \
// RUN:     {key: readability-identifier-naming.ConstexprMethodCase, value: lower_case}, \
// RUN:     {key: readability-identifier-naming.ConstexprVariableCase, value: lower_case}, \
// RUN:     {key: readability-identifier-naming.EnumCase, value: CamelCase}, \
// RUN:     {key: readability-identifier-naming.EnumPrefix, value: 'E'}, \
// RUN:     {key: readability-identifier-naming.EnumConstantCase, value: UPPER_CASE}, \
// RUN:     {key: readability-identifier-naming.FunctionCase, value: camelBack}, \
// RUN:     {key: readability-identifier-naming.GlobalConstantCase, value: UPPER_CASE}, \
// RUN:     {key: readability-identifier-naming.GlobalFunctionCase, value: CamelCase}, \
// RUN:     {key: readability-identifier-naming.GlobalVariableCase, value: lower_case}, \
// RUN:     {key: readability-identifier-naming.GlobalVariablePrefix, value: 'g_'}, \
// RUN:     {key: readability-identifier-naming.InlineNamespaceCase, value: lower_case}, \
// RUN:     {key: readability-identifier-naming.LocalConstantCase, value: CamelCase}, \
// RUN:     {key: readability-identifier-naming.LocalConstantPrefix, value: 'k'}, \
// RUN:     {key: readability-identifier-naming.LocalVariableCase, value: lower_case}, \
// RUN:     {key: readability-identifier-naming.MemberCase, value: CamelCase}, \
// RUN:     {key: readability-identifier-naming.MemberPrefix, value: 'm_'}, \
// RUN:     {key: readability-identifier-naming.ConstantMemberCase, value: lower_case}, \
// RUN:     {key: readability-identifier-naming.PrivateMemberPrefix, value: '__'}, \
// RUN:     {key: readability-identifier-naming.ProtectedMemberPrefix, value: '_'}, \
// RUN:     {key: readability-identifier-naming.PublicMemberCase, value: lower_case}, \
// RUN:     {key: readability-identifier-naming.MethodCase, value: camelBack}, \
// RUN:     {key: readability-identifier-naming.PrivateMethodPrefix, value: '__'}, \
// RUN:     {key: readability-identifier-naming.ProtectedMethodPrefix, value: '_'}, \
// RUN:     {key: readability-identifier-naming.NamespaceCase, value: lower_case}, \
// RUN:     {key: readability-identifier-naming.ParameterCase, value: camelBack}, \
// RUN:     {key: readability-identifier-naming.ParameterPrefix, value: 'a_'}, \
// RUN:     {key: readability-identifier-naming.ConstantParameterCase, value: camelBack}, \
// RUN:     {key: readability-identifier-naming.ConstantParameterPrefix, value: 'i_'}, \
// RUN:     {key: readability-identifier-naming.ParameterPackCase, value: camelBack}, \
// RUN:     {key: readability-identifier-naming.PureFunctionCase, value: lower_case}, \
// RUN:     {key: readability-identifier-naming.PureMethodCase, value: camelBack}, \
// RUN:     {key: readability-identifier-naming.StaticConstantCase, value: UPPER_CASE}, \
// RUN:     {key: readability-identifier-naming.StaticVariableCase, value: camelBack}, \
// RUN:     {key: readability-identifier-naming.StaticVariablePrefix, value: 's_'}, \
// RUN:     {key: readability-identifier-naming.StructCase, value: lower_case}, \
// RUN:     {key: readability-identifier-naming.TemplateParameterCase, value: UPPER_CASE}, \
// RUN:     {key: readability-identifier-naming.TemplateTemplateParameterCase, value: CamelCase}, \
// RUN:     {key: readability-identifier-naming.TemplateUsingCase, value: lower_case}, \
// RUN:     {key: readability-identifier-naming.TemplateUsingPrefix, value: 'u_'}, \
// RUN:     {key: readability-identifier-naming.TypeTemplateParameterCase, value: camelBack}, \
// RUN:     {key: readability-identifier-naming.TypeTemplateParameterSuffix, value: '_t'}, \
// RUN:     {key: readability-identifier-naming.TypedefCase, value: lower_case}, \
// RUN:     {key: readability-identifier-naming.TypedefSuffix, value: '_t'}, \
// RUN:     {key: readability-identifier-naming.UnionCase, value: CamelCase}, \
// RUN:     {key: readability-identifier-naming.UnionPrefix, value: 'U'}, \
// RUN:     {key: readability-identifier-naming.UsingCase, value: lower_case}, \
// RUN:     {key: readability-identifier-naming.ValueTemplateParameterCase, value: camelBack}, \
// RUN:     {key: readability-identifier-naming.VariableCase, value: lower_case}, \
// RUN:     {key: readability-identifier-naming.VirtualMethodCase, value: UPPER_CASE}, \
// RUN:     {key: readability-identifier-naming.VirtualMethodPrefix, value: 'v_'}, \
// RUN:     {key: readability-identifier-naming.IgnoreFailedSplit, value: 0} \
// RUN:   ]}' -- -std=c++11 -fno-delayed-template-parsing

// FIXME: There should be more test cases for checking that references to class
// FIXME: name, declaration contexts, forward declarations, etc, are correctly
// FIXME: checked and renamed

namespace FOO_NS {
// CHECK-MESSAGES: :[[@LINE-1]]:1: warning: invalid case style for namespace 'FOO_NS' [readability-identifier-naming]
// CHECK-FIXES: {{^}}namespace foo_ns {{{$}}
inline namespace InlineNamespace {
// CHECK-MESSAGES: :[[@LINE-1]]:1: warning: invalid case style for inline namespace 'InlineNamespace'
// CHECK-FIXES: {{^}}inline namespace inline_namespace {{{$}}

#define BLA int FOO_bar
BLA;
// CHECK-MESSAGES: :[[@LINE-1]]:1: warning: invalid case style for global variable 'FOO_bar'
// NO fix expected as FOO_bar is from macro expansion

enum my_enumeration {
// CHECK-MESSAGES: :[[@LINE-1]]:1: warning: invalid case style for enum 'my_enumeration'
// CHECK-FIXES: {{^}}enum EMyEnumeration {{{$}}
    MyConstant = 1,
// CHECK-MESSAGES: :[[@LINE-1]]:5: warning: invalid case style for enum constant 'MyConstant'
// CHECK-FIXES: {{^}}    MY_CONSTANT = 1,{{$}}
    your_CONST = 1,
// CHECK-MESSAGES: :[[@LINE-1]]:5: warning: invalid case style for enum constant 'your_CONST'
// CHECK-FIXES: {{^}}    YOUR_CONST = 1,{{$}}
    THIS_ConstValue = 1,
// CHECK-MESSAGES: :[[@LINE-1]]:5: warning: invalid case style for enum constant 'THIS_ConstValue'
// CHECK-FIXES: {{^}}    THIS_CONST_VALUE = 1,{{$}}
};

constexpr int ConstExpr_variable = MyConstant;
// CHECK-MESSAGES: :[[@LINE-1]]:1: warning: invalid case style for constexpr variable 'ConstExpr_variable'
// CHECK-FIXES: {{^}}constexpr int const_expr_variable = MY_CONSTANT;{{$}}

class my_class {
// CHECK-MESSAGES: :[[@LINE-1]]:1: warning: invalid case style for class 'my_class'
// CHECK-FIXES: {{^}}class CMyClass {{{$}}
    my_class();

  const int MEMBER_one_1 = ConstExpr_variable;
// CHECK-MESSAGES: :[[@LINE-1]]:3: warning: invalid case style for constant member 'MEMBER_one_1'
// CHECK-FIXES: {{^}}  const int member_one_1 = const_expr_variable;{{$}}
  int member2 = 2;
// CHECK-MESSAGES: :[[@LINE-1]]:3: warning: invalid case style for private member 'member2'
// CHECK-FIXES: {{^}}  int __member2 = 2;{{$}}

private:
    int private_member = 3;
// CHECK-MESSAGES: :[[@LINE-1]]:5: warning: invalid case style for private member 'private_member'
// CHECK-FIXES: {{^}}    int __private_member = 3;{{$}}

protected:
    int ProtMember;
// CHECK-MESSAGES: :[[@LINE-1]]:5: warning: invalid case style for protected member 'ProtMember'
// CHECK-FIXES: {{^}}    int _ProtMember;{{$}}

public:
    int PubMem;
// CHECK-MESSAGES: :[[@LINE-1]]:5: warning: invalid case style for public member 'PubMem'
// CHECK-FIXES: {{^}}    int pub_mem;{{$}}

    static const int classConstant;
// CHECK-MESSAGES: :[[@LINE-1]]:5: warning: invalid case style for class constant 'classConstant'
// CHECK-FIXES: {{^}}    static const int kClassConstant;{{$}}
    static int ClassMember_2;
// CHECK-MESSAGES: :[[@LINE-1]]:5: warning: invalid case style for class member 'ClassMember_2'
// CHECK-FIXES: {{^}}    static int ClassMember2;{{$}}
};

const int my_class::classConstant = 4;
// CHECK-MESSAGES: :[[@LINE-1]]:1: warning: invalid case style for class constant 'classConstant'
// CHECK-FIXES: {{^}}const int my_class::kClassConstant = 4;{{$}}
// FIXME: The fixup should reflect class name fixups as well:
// FIXME: {{^}}const int CMyClass::kClassConstant = 4;{{$}}

int my_class::ClassMember_2 = 5;
// CHECK-MESSAGES: :[[@LINE-1]]:1: warning: invalid case style for class member 'ClassMember_2'
// CHECK-FIXES: {{^}}int my_class::ClassMember2 = 5;{{$}}
// FIXME: The fixup should reflect class name fixups as well:
// FIXME: {{^}}int CMyClass::ClassMember2 = 5;{{$}}

const int global_Constant = 6;
// CHECK-MESSAGES: :[[@LINE-1]]:1: warning: invalid case style for global constant 'global_Constant'
// CHECK-FIXES: {{^}}const int GLOBAL_CONSTANT = 6;{{$}}
int Global_variable = 7;
// CHECK-MESSAGES: :[[@LINE-1]]:1: warning: invalid case style for global variable 'Global_variable'
// CHECK-FIXES: {{^}}int g_global_variable = 7;{{$}}

void global_function(int PARAMETER_1, int const CONST_parameter) {
// CHECK-MESSAGES: :[[@LINE-1]]:1: warning: invalid case style for global function 'global_function'
// CHECK-MESSAGES: :[[@LINE-2]]:22: warning: invalid case style for parameter 'PARAMETER_1'
// CHECK-MESSAGES: :[[@LINE-3]]:39: warning: invalid case style for constant parameter 'CONST_parameter'
// CHECK-FIXES: {{^}}void GlobalFunction(int a_parameter1, int const i_constParameter) {{{$}}
    static const int THIS_static_ConsTant = 4;
// CHECK-MESSAGES: :[[@LINE-1]]:5: warning: invalid case style for static constant 'THIS_static_ConsTant'
// CHECK-FIXES: {{^}}    static const int THIS_STATIC_CONS_TANT = 4;{{$}}
    static int THIS_static_variable;
// CHECK-MESSAGES: :[[@LINE-1]]:5: warning: invalid case style for static variable 'THIS_static_variable'
// CHECK-FIXES: {{^}}    static int s_thisStaticVariable;{{$}}
    int const local_Constant = 3;
// CHECK-MESSAGES: :[[@LINE-1]]:5: warning: invalid case style for local constant 'local_Constant'
// CHECK-FIXES: {{^}}    int const kLocalConstant = 3;{{$}}
    int LOCAL_VARIABLE;
// CHECK-MESSAGES: :[[@LINE-1]]:5: warning: invalid case style for local variable 'LOCAL_VARIABLE'
// CHECK-FIXES: {{^}}    int local_variable;{{$}}

    int LOCAL_Array__[] = {0, 1, 2};
// CHECK-MESSAGES: :[[@LINE-1]]:5: warning: invalid case style for local variable 'LOCAL_Array__'
// CHECK-FIXES: {{^}}    int local_array[] = {0, 1, 2};{{$}}

    for (auto _ : LOCAL_Array__) {
    }
}

template<typename ... TYPE_parameters>
// CHECK-MESSAGES: :[[@LINE-1]]:10: warning: invalid case style for type template parameter 'TYPE_parameters'
// CHECK-FIXES: {{^}}template<typename ... typeParameters_t>{{$}}
void Global_Fun(TYPE_parameters... PARAMETER_PACK) {
// CHECK-MESSAGES: :[[@LINE-1]]:1: warning: invalid case style for global function 'Global_Fun'
// CHECK-MESSAGES: :[[@LINE-2]]:17: warning: invalid case style for parameter pack 'PARAMETER_PACK'
// CHECK-FIXES: {{^}}void GlobalFun(TYPE_parameters... parameterPack) {{{$}}
    global_function(1, 2);
// CHECK-FIXES: {{^}}    GlobalFunction(1, 2);{{$}}
    FOO_bar = Global_variable;
// CHECK-FIXES: {{^}}    FOO_bar = g_global_variable;{{$}}
// NO fix expected for FOO_bar declared in macro expansion
}

template<template<typename> class TPL_parameter, int COUNT_params, typename ... TYPE_parameters>
// CHECK-MESSAGES: :[[@LINE-1]]:10: warning: invalid case style for template template parameter 'TPL_parameter'
// CHECK-MESSAGES: :[[@LINE-2]]:50: warning: invalid case style for value template parameter 'COUNT_params'
// CHECK-MESSAGES: :[[@LINE-3]]:68: warning: invalid case style for type template parameter 'TYPE_parameters'
// CHECK-FIXES: {{^}}template<template<typename> class TplParameter, int countParams, typename ... typeParameters_t>{{$}}
class test_CLASS {
// CHECK-MESSAGES: :[[@LINE-1]]:1: warning: invalid case style for class 'test_CLASS'
// CHECK-FIXES: {{^}}class CTestClass {{{$}}
};

class abstract_class {
// CHECK-MESSAGES: :[[@LINE-1]]:1: warning: invalid case style for abstract class 'abstract_class'
// CHECK-FIXES: {{^}}class AAbstractClass {{{$}}
    virtual ~abstract_class() = 0;
    virtual void VIRTUAL_METHOD();
// CHECK-MESSAGES: :[[@LINE-1]]:5: warning: invalid case style for virtual method 'VIRTUAL_METHOD'
// CHECK-FIXES: {{^}}    virtual void v_VIRTUAL_METHOD();{{$}}
    void non_Virtual_METHOD() {}
// CHECK-MESSAGES: :[[@LINE-1]]:5: warning: invalid case style for private method 'non_Virtual_METHOD'
// CHECK-FIXES: {{^}}    void __non_Virtual_METHOD() {}{{$}}
    static void CLASS_METHOD() {}
// CHECK-MESSAGES: :[[@LINE-1]]:5: warning: invalid case style for class method 'CLASS_METHOD'
// CHECK-FIXES: {{^}}    static void classMethod() {}{{$}}

    constexpr int CST_expr_Method() { return 2; }
// CHECK-MESSAGES: :[[@LINE-1]]:5: warning: invalid case style for constexpr method 'CST_expr_Method'
// CHECK-FIXES: {{^}}    constexpr int cst_expr_method() { return 2; }{{$}}

private:
    void PRIVate_Method();
// CHECK-MESSAGES: :[[@LINE-1]]:5: warning: invalid case style for private method 'PRIVate_Method'
// CHECK-FIXES: {{^}}    void __PRIVate_Method();{{$}}
protected:
    void protected_Method();
// CHECK-MESSAGES: :[[@LINE-1]]:5: warning: invalid case style for protected method 'protected_Method'
// CHECK-FIXES: {{^}}    void _protected_Method();{{$}}
public:
    void public_Method();
// CHECK-MESSAGES: :[[@LINE-1]]:5: warning: invalid case style for method 'public_Method'
// CHECK-FIXES: {{^}}    void publicMethod();{{$}}
};

constexpr int CE_function() { return 3; }
// CHECK-MESSAGES: :[[@LINE-1]]:1: warning: invalid case style for constexpr function 'CE_function'
// CHECK-FIXES: {{^}}constexpr int ce_function() { return 3; }{{$}}

struct THIS___Structure {
// CHECK-MESSAGES: :[[@LINE-1]]:1: warning: invalid case style for struct 'THIS___Structure'
// CHECK-FIXES: {{^}}struct this_structure {{{$}}
    THIS___Structure();
// FIXME: There should be a fixup for the constructor as well
// FIXME: {{^}}    this_structure();{{$}}

  union __MyUnion_is_wonderful__ {};
// CHECK-MESSAGES: :[[@LINE-1]]:3: warning: invalid case style for union '__MyUnion_is_wonderful__'
// CHECK-FIXES: {{^}}  union UMyUnionIsWonderful {};{{$}}
};

typedef THIS___Structure struct_type;
// CHECK-MESSAGES: :[[@LINE-1]]:1: warning: invalid case style for typedef 'struct_type'
// CHECK-FIXES: {{^}}typedef THIS___Structure struct_type_t;{{$}}
// FIXME: The fixup should reflect structure name fixups as well:
// FIXME: {{^}}typedef this_structure struct_type_t;{{$}}

static void static_Function() {
// CHECK-MESSAGES: :[[@LINE-1]]:1: warning: invalid case style for function 'static_Function'
// CHECK-FIXES: {{^}}static void staticFunction() {{{$}}
}

}
}
