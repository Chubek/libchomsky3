/*
 * AzmaIDL grammar for dparser
 *
 * Practical production-oriented draft.
 * Covers:
 *   - modules/imports
 *   - namespaces
 *   - annotations
 *   - const/alias
 *   - enums
 *   - structs
 *   - interfaces
 *   - methods
 *   - types, including generic type arguments
 *   - values / literals
 *   - records and lists
 *   - expression-style constant values
 *
 * Notes:
 *   1. Keywords are expressed literally.
 *   2. IDENT and literal tokens are regex-style rules.
 *   3. whitespace is defined explicitly so comments are skipped.
 */


/* =========================
   Whitespace / comments
   ========================= */

whitespace
    : (
          "[ \t\r\n]+"
        | "//[^\n]*"
        | "/\\*([^*]|\\*+[^*/])*\\*+/"
      )*
    ;


/* =========================
   Start rule
   ========================= */

file
    : bom? module_decl? import_decl* declaration*
    ;

bom
    : "\357\273\277"
    ;


/* =========================
   Top-level declarations
   ========================= */

declaration
    : annotated_decl
    ;

annotated_decl
    : annotation* declaration_core
    ;

declaration_core
    : namespace_decl
    | enum_decl
    | struct_decl
    | interface_decl
    | alias_decl
    | const_decl
    | empty_decl
    ;

empty_decl
    : ';'
    ;


/* =========================
   Module / import / namespace
   ========================= */

module_decl
    : 'module' qualified_name ';'
    ;

import_decl
    : 'import' string_lit ';'
    ;

namespace_decl
    : 'namespace' IDENT namespace_body
    ;

namespace_body
    : ';'
    | '{' declaration* '}'
    ;


/* =========================
   Annotations
   ========================= */

annotation
    : '@' qualified_name annotation_args?
    ;

annotation_args
    : '(' annotation_arg_list? ')'
    ;

annotation_arg_list
    : annotation_arg (',' annotation_arg)* comma_opt
    ;

annotation_arg
    : IDENT '=' const_expr
    | const_expr
    ;

comma_opt
    : ','
    |
    ;


/* =========================
   Alias / const
   ========================= */

alias_decl
    : 'alias' IDENT '=' type_ref ';'
    ;

const_decl
    : 'const' type_ref IDENT '=' const_expr ';'
    ;


/* =========================
   Enum
   ========================= */

enum_decl
    : 'enum' IDENT enum_base_opt enum_body
    ;

enum_base_opt
    : ':' type_ref
    |
    ;

enum_body
    : '{' enum_member_list_opt '}'
    ;

enum_member_list_opt
    : enum_member_list
    |
    ;

enum_member_list
    : enum_member (',' enum_member)* comma_opt
    ;

enum_member
    : annotation* IDENT enum_member_value_opt
    ;

enum_member_value_opt
    : '=' const_expr
    |
    ;


/* =========================
   Struct
   ========================= */

struct_decl
    : 'struct' IDENT type_params_opt struct_body
    ;

struct_body
    : '{' struct_member* '}'
    ;

struct_member
    : annotation* field_decl
    | annotation* const_decl
    | empty_decl
    ;

field_decl
    : type_ref IDENT field_default_opt ';'
    ;

field_default_opt
    : '=' const_expr
    |
    ;


/* =========================
   Interface
   ========================= */

interface_decl
    : 'interface' IDENT type_params_opt interface_inheritance_opt interface_body
    ;

interface_inheritance_opt
    : ':' type_ref_list
    |
    ;

interface_body
    : '{' interface_member* '}'
    ;

interface_member
    : annotation* method_decl
    | annotation* const_decl
    | empty_decl
    ;

method_decl
    : 'fn' IDENT type_params_opt '(' param_list_opt ')' return_type_opt throws_opt ';'
    ;

return_type_opt
    : '->' type_ref
    |
    ;

throws_opt
    : 'throws' type_ref_list
    |
    ;

param_list_opt
    : param_list
    |
    ;

param_list
    : param (',' param)* comma_opt
    ;

param
    : param_dir_opt type_ref IDENT param_default_opt
    ;

param_dir_opt
    : 'in'
    | 'out'
    | 'inout'
    |
    ;

param_default_opt
    : '=' const_expr
    |
    ;


/* =========================
   Generic parameters / arguments
   ========================= */

type_params_opt
    : '<' type_param_list '>'
    |
    ;

type_param_list
    : type_param (',' type_param)* comma_opt
    ;

type_param
    : IDENT type_param_constraint_opt
    ;

type_param_constraint_opt
    : ':' type_ref_list
    |
    ;

type_arg_list
    : type_ref (',' type_ref)* comma_opt
    ;


/* =========================
   Types
   ========================= */

type_ref
    : union_type
    ;

type_ref_list
    : type_ref (',' type_ref)*
    ;

union_type
    : postfix_type ('|' postfix_type)*
    ;

postfix_type
    : primary_type postfix_type_suffix*
    ;

postfix_type_suffix
    : '?'
    | array_suffix
    ;

array_suffix
    : '[' array_len_opt ']'
    ;

array_len_opt
    : const_expr
    |
    ;

primary_type
    : builtin_type
    | qualified_type
    | '(' type_ref ')'
    ;

qualified_type
    : qualified_name type_args_opt
    ;

type_args_opt
    : '<' type_arg_list '>'
    |
    ;

builtin_type
    : 'bool'
    | 'i8'
    | 'i16'
    | 'i32'
    | 'i64'
    | 'u8'
    | 'u16'
    | 'u32'
    | 'u64'
    | 'f32'
    | 'f64'
    | 'string'
    | 'bytes'
    | 'any'
    | 'void'
    ;


/* =========================
   Qualified names
   ========================= */

qualified_name
    : IDENT ('.' IDENT)*
    ;


/* =========================
   Constant expressions
   ========================= */

const_expr
    : conditional_expr
    ;

conditional_expr
    : logical_or_expr conditional_tail_opt
    ;

conditional_tail_opt
    : '?' const_expr ':' const_expr
    |
    ;

logical_or_expr
    : logical_and_expr ('||' logical_and_expr)*
    ;

logical_and_expr
    : bit_or_expr ('&&' bit_or_expr)*
    ;

bit_or_expr
    : bit_xor_expr ('|' bit_xor_expr)*
    ;

bit_xor_expr
    : bit_and_expr ('^' bit_and_expr)*
    ;

bit_and_expr
    : equality_expr ('&' equality_expr)*
    ;

equality_expr
    : relational_expr (('==' | '!=') relational_expr)*
    ;

relational_expr
    : shift_expr (('<' | '<=' | '>' | '>=') shift_expr)*
    ;

shift_expr
    : additive_expr (('<<' | '>>') additive_expr)*
    ;

additive_expr
    : multiplicative_expr (('+' | '-') multiplicative_expr)*
    ;

multiplicative_expr
    : unary_expr (('*' | '/' | '%') unary_expr)*
    ;

unary_expr
    : ('+' | '-' | '!' | '~') unary_expr
    | postfix_expr
    ;

postfix_expr
    : primary_expr postfix_expr_suffix*
    ;

postfix_expr_suffix
    : '.' IDENT
    | call_suffix
    | index_suffix
    ;

call_suffix
    : '(' argument_list_opt ')'
    ;

index_suffix
    : '[' const_expr ']'
    ;

argument_list_opt
    : argument_list
    |
    ;

argument_list
    : argument (',' argument)* comma_opt
    ;

argument
    : IDENT '=' const_expr
    | const_expr
    ;


/* =========================
   Primary expressions / values
   ========================= */

primary_expr
    : literal
    | qualified_name
    | list_literal
    | record_literal
    | '(' const_expr ')'
    ;

literal
    : bool_lit
    | null_lit
    | integer_lit
    | float_lit
    | string_lit
    | bytes_lit
    ;

bool_lit
    : 'true'
    | 'false'
    ;

null_lit
    : 'null'
    ;

list_literal
    : '[' element_list_opt ']'
    ;

element_list_opt
    : element_list
    |
    ;

element_list
    : const_expr (',' const_expr)* comma_opt
    ;

record_literal
    : '{' field_value_list_opt '}'
    ;

field_value_list_opt
    : field_value_list
    |
    ;

field_value_list
    : field_value (',' field_value)* comma_opt
    ;

field_value
    : IDENT ':' const_expr
    ;


/* =========================
   Lexical rules
   ========================= */

IDENT
    : "[A-Za-z_][A-Za-z0-9_]*"
    ;

integer_lit
    : dec_integer_lit
    | hex_integer_lit
    | oct_integer_lit
    | bin_integer_lit
    ;

dec_integer_lit
    : "0|[1-9][0-9_]*"
    ;

hex_integer_lit
    : "0[xX][0-9A-Fa-f_]+"
    ;

oct_integer_lit
    : "0[oO][0-7_]+"
    ;

bin_integer_lit
    : "0[bB][01_]+"
    ;

float_lit
    : "[0-9][0-9_]*\\.[0-9_]*([eE][+-]?[0-9_]+)?"
    | "\\.[0-9][0-9_]*([eE][+-]?[0-9_]+)?"
    | "[0-9][0-9_]*[eE][+-]?[0-9_]+"
    ;

string_lit
    : dq_string_lit
    | sq_string_lit
    ;

dq_string_lit
    : "\"([^\"\\\\]|\\\\.)*\""
    ;

sq_string_lit
    : "'([^'\\\\]|\\\\.)*'"
    ;

bytes_lit
    : "b\"([^\"\\\\]|\\\\.)*\""
    | "b'([^'\\\\]|\\\\.)*'"
    ;
